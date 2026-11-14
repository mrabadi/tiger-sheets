#include "tigersheets/workbook_links.hpp"

#include <pugixml.hpp>
#include <zip.h>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <functional>
#include <memory>
#include <set>
#include <stdexcept>

namespace tigersheets {
namespace {
namespace fs = std::filesystem;
struct LinkFailure : std::runtime_error {
    std::string code, state;
    LinkFailure(std::string c, std::string s, const std::string& message)
        : std::runtime_error(message), code(std::move(c)), state(std::move(s)) {}
};
bool nonlocalTarget(const std::string& target) {
    if (target.starts_with("//") || target.starts_with("\\\\")) return true;
    const auto colon = target.find(':');
    if (colon == std::string::npos) return false;
    return colon > 0 && std::all_of(target.begin(), target.begin() + static_cast<std::ptrdiff_t>(colon),
        [](unsigned char c) { return std::isalnum(c) || c == '+' || c == '.' || c == '-'; });
}
std::string decodeEscapes(const std::string& target) {
    std::string decoded;
    for (std::size_t i = 0; i < target.size(); ++i) {
        if (target[i] == '%') {
            unsigned int value = 0;
            if (i + 2 >= target.size()) throw LinkFailure("external_link_invalid_uri", "blocked", "Invalid escaped file URI");
            const auto parsed = std::from_chars(target.data() + i + 1, target.data() + i + 3, value, 16);
            if (parsed.ec != std::errc() || parsed.ptr != target.data() + i + 3 || value == 0)
                throw LinkFailure("external_link_invalid_uri", "blocked", "Invalid escaped file URI");
            decoded += static_cast<char>(value); i += 2;
        } else decoded += target[i];
    }
    return decoded;
}
std::string localTarget(std::string target, bool escapedUri) {
    std::string lower = target;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower.starts_with("file://")) {
        target.erase(0, 7);
        const auto slash = target.find('/');
        const auto host = target.substr(0, slash);
        if (host != "" && host != "localhost") throw LinkFailure("external_link_nonlocal", "blocked", "Remote file URI hosts are not local workbook sources");
        if (slash == std::string::npos) throw LinkFailure("external_link_invalid_uri", "blocked", "Local file URI has no path");
        target.erase(0, slash);
        escapedUri = true;
    }
    if (escapedUri) target = decodeEscapes(target);
    if (target.find('\0') != std::string::npos || nonlocalTarget(target))
        throw LinkFailure("external_link_nonlocal", "blocked", "Choose a local file to relocate this workbook link");
    std::replace(target.begin(), target.end(), '\\', '/');
    return target;
}
std::uintmax_t expandedSize(const fs::path& path, std::uintmax_t remaining) {
    int error = 0;
    auto* archive = zip_open(path.c_str(), ZIP_RDONLY, &error);
    if (!archive) throw LinkFailure("external_link_invalid", "invalid", "Linked workbook is not a valid ZIP package");
    std::unique_ptr<zip_t, decltype(&zip_discard)> guard(archive, zip_discard);
    const auto count = zip_get_num_entries(archive, 0);
    if (count < 0 || count > 20000) throw LinkFailure("external_link_limit", "limited", "Linked workbook contains too many package members");
    std::uintmax_t total = 0;
    for (zip_int64_t index = 0; index < count; ++index) {
        zip_stat_t stat; zip_stat_init(&stat);
        if (zip_stat_index(archive, static_cast<zip_uint64_t>(index), 0, &stat) < 0)
            throw LinkFailure("external_link_invalid", "invalid", "Linked workbook has invalid package metadata");
        if (stat.size > remaining - total) throw LinkFailure("external_link_limit", "limited", "Linked workbook expanded-data budget was exceeded");
        total += stat.size;
    }
    return total;
}
std::string localName(const char* name) {
    const std::string value(name);
    const auto colon = value.find(':');
    return colon == std::string::npos ? value : value.substr(colon + 1);
}
pugi::xml_node child(pugi::xml_node parent, const char* name) {
    for (auto node : parent.children()) if (localName(node.name()) == name) return node;
    return {};
}
std::string attribute(pugi::xml_node node, const char* name) {
    for (const auto attr : node.attributes()) if (localName(attr.name()) == name) return attr.value();
    return {};
}
void parse(pugi::xml_document& document, const std::string& xml) {
    if (!document.load_buffer(xml.data(), xml.size())) throw std::runtime_error("Invalid external workbook XML");
}
std::string packageTarget(const std::string& owner, const std::string& target) {
    const auto result = (target.starts_with('/') ? fs::path(target.substr(1)) : fs::path(owner).parent_path() / target).lexically_normal().generic_string();
    if (result.starts_with("../") || result.starts_with('/')) throw std::runtime_error("Unsafe external link package target");
    return result;
}
std::string relationshipsPart(const std::string& part) {
    const fs::path path(part);
    return (path.parent_path() / "_rels" / (path.filename().string() + ".rels")).generic_string();
}
std::pair<int, int> coordinate(const std::string& reference) {
    int column = 0, row = 0;
    std::size_t position = 0;
    while (position < reference.size() && reference[position] >= 'A' && reference[position] <= 'Z') {
        column = column * 26 + reference[position++] - 'A' + 1;
        if (column > 16384) throw std::runtime_error("External cache column is invalid");
    }
    const auto parsed = std::from_chars(reference.data() + position, reference.data() + reference.size(), row);
    if (parsed.ec != std::errc() || parsed.ptr != reference.data() + reference.size() || row < 1 || row > 1048576 || column < 1)
        throw std::runtime_error("External cache cell reference is invalid");
    return {row, column};
}
Json readCache(pugi::xml_node book) {
    const auto data = child(book, "sheetDataSet");
    if (!data || !data.first_child()) return nullptr;
    Json cached = {{"revision", 0}, {"sheets", Json::array()}};
    for (auto name : child(book, "sheetNames").children()) {
        cached["sheets"].push_back({{"id", "cached-" + std::to_string(cached["sheets"].size())},
            {"name", attribute(name, "val")}, {"cells", Json::array()}});
    }
    for (auto sheet : data.children()) {
        const auto index = sheet.attribute("sheetId").as_int(-1);
        if (index < 0 || static_cast<std::size_t>(index) >= cached["sheets"].size())
            throw std::runtime_error("External cache sheet index is invalid");
        for (auto row : sheet.children()) for (auto cell : row.children()) {
            const auto [r, c] = coordinate(attribute(cell, "r"));
            const auto type = attribute(cell, "t");
            auto input = std::string(child(cell, "v").text().as_string());
            if (type == "str") input = "'" + input;
            else if (type == "b") {
                if (input != "0" && input != "1") throw std::runtime_error("Invalid cached boolean");
                input = input == "0" ? "FALSE" : "TRUE";
            } else if (type.empty() || type == "n") {
                if (!input.empty()) {
                    double number = 0;
                    const auto parsed = std::from_chars(input.data(), input.data() + input.size(), number);
                    if (parsed.ec != std::errc() || parsed.ptr != input.data() + input.size() || !std::isfinite(number))
                        throw std::runtime_error("Invalid cached number");
                }
            } else if (type == "e") {
                const std::set<std::string> errors{"#NULL!", "#DIV/0!", "#VALUE!", "#REF!", "#NAME?", "#NUM!", "#N/A", "#SPILL!", "#CALC!"};
                if (!errors.contains(input)) throw std::runtime_error("Unsupported cached error value");
            } else throw std::runtime_error("Unsupported external cache cell type");
            cached["sheets"][static_cast<std::size_t>(index)]["cells"].push_back({{"row", r}, {"col", c}, {"input", input}});
        }
    }
    return cached;
}
Json discover(const Json& workbook, const WorkbookPackage* package, Json& diagnostics) {
    Json links = Json::array();
    if (package && package->entries.contains("xl/workbook.xml") && package->entries.contains("xl/_rels/workbook.xml.rels")) {
        pugi::xml_document document, relationships;
        parse(document, package->entries.at("xl/workbook.xml"));
        parse(relationships, package->entries.at("xl/_rels/workbook.xml.rels"));
        int index = 0;
        for (auto reference : child(document.document_element(), "externalReferences").children()) {
            ++index;
            try {
                const auto id = attribute(reference, "id");
                std::string part;
                for (auto relation : relationships.document_element().children())
                    if (attribute(relation, "Id") == id && attribute(relation, "Type").ends_with("/externalLink"))
                        part = packageTarget("xl/workbook.xml", attribute(relation, "Target"));
                if (part.empty() || !package->entries.contains(part)) throw std::runtime_error("External workbook relationship is missing");
                pugi::xml_document external, externalRelationships;
                parse(external, package->entries.at(part));
                const auto book = child(external.document_element(), "externalBook");
                if (!book) throw std::runtime_error("Only external workbook links are supported");
                const auto rels = relationshipsPart(part);
                if (!package->entries.contains(rels)) throw std::runtime_error("External workbook path relationship is missing");
                parse(externalRelationships, package->entries.at(rels));
                std::string target;
                for (auto relation : externalRelationships.document_element().children())
                    if (attribute(relation, "Id") == attribute(book, "id") && attribute(relation, "Type").ends_with("/externalLinkPath"))
                        target = attribute(relation, "Target");
                if (target.empty()) throw std::runtime_error("External workbook path is missing");
                links.push_back({{"index", index}, {"name", fs::path(decodeEscapes(target)).filename().string()},
                    {"path", target}, {"uriEncoded", true}, {"enabled", true}});
                if (auto cached = readCache(book); !cached.is_null()) links.back()["cachedWorkbook"] = std::move(cached);
            } catch (const std::exception& error) {
                diagnostics.push_back({{"code", "external_link_metadata"}, {"index", index}, {"message", error.what()}});
                if (links.size() < static_cast<std::size_t>(index))
                    links.push_back({{"index", index}, {"name", "Unavailable-link-" + std::to_string(index) + ".xlsx"},
                        {"path", ""}, {"enabled", false}, {"metadataInvalid", true}});
            }
        }
    }
    if (workbook.contains("links") && workbook["links"].is_array()) {
        for (const auto& link : workbook["links"]) {
            bool replaced = false;
            for (auto& current : links)
                if (link.contains("index") ? current.contains("index") && current["index"] == link["index"] : current.value("path", "") == link.value("path", "")) {
                    const bool changedPath = link.contains("path") && link["path"] != current["path"];
                    current.update(link);
                    if (changedPath && !link.contains("uriEncoded")) current["uriEncoded"] = false;
                    replaced = true; break;
                }
            if (!replaced) links.push_back(link);
        }
    }
    return links;
}

std::string normalizeIndexes(const std::string& input, const std::map<int, std::string>& names) {
    std::string output;
    std::size_t position = 0;
    while (position < input.size()) {
        const auto start = position;
        if (input[position] == '"') {
            ++position;
            while (position < input.size()) {
                if (input[position++] == '"') {
                    if (position < input.size() && input[position] == '"') { ++position; continue; }
                    break;
                }
            }
            output += input.substr(start, position - start); continue;
        }
        std::string name;
        if (input[position] == '\'') {
            ++position;
            while (position < input.size()) {
                if (input[position] == '\'') {
                    ++position;
                    if (position < input.size() && input[position] == '\'') { name += '\''; ++position; continue; }
                    break;
                }
                name += input[position++];
            }
        } else if (std::isalnum(static_cast<unsigned char>(input[position])) || input[position] == '_' || input[position] == '[') {
            while (position < input.size() && (std::isalnum(static_cast<unsigned char>(input[position])) || std::string("_.[]$").find(input[position]) != std::string::npos)) name += input[position++];
        } else { output += input[position++]; continue; }
        if (position < input.size() && input[position] == '!' && name.starts_with('[')) {
            const auto close = name.find(']');
            int index = 0;
            if (close != std::string::npos) {
                const auto parsed = std::from_chars(name.data() + 1, name.data() + close, index);
                if (parsed.ec == std::errc() && parsed.ptr == name.data() + close && names.contains(index)) {
                    name = "[" + names.at(index) + "]" + name.substr(close + 1);
                    output += '\'';
                    for (const char c : name) { output += c; if (c == '\'') output += '\''; }
                    output += "'!"; ++position; continue;
                }
            }
        }
        output += input.substr(start, position - start);
    }
    return output;
}
void normalizeWorkbook(Json& workbook, const Json& links) {
    std::map<int, std::string> names;
    for (const auto& link : links) if (link.contains("index") && !link.value("metadataInvalid", false)) names[link["index"].get<int>()] = link.at("name").get<std::string>();
    if (names.empty()) return;
    for (auto& sheet : workbook.at("sheets")) for (auto& cell : sheet.at("cells")) {
        const auto input = cell.at("input").get<std::string>();
        if (input.starts_with('=')) cell["input"] = normalizeIndexes(input, names);
    }
}
}

WorkbookLinkResolution prepareWorkbookLinksOwned(Json workbook, const WorkbookPackage* source, const WorkbookLinkOptions& options) {
    WorkbookLinkResolution result;
    std::size_t attemptedFiles = 0;
    std::uintmax_t chargedBytes = 0, expandedBytes = 0;
    std::set<std::string> activePaths;
    std::map<std::string, Json> preparedWorkbooks;
    if (source && !source->sourcePath.empty()) activePaths.insert(fs::weakly_canonical(source->sourcePath).string());
    std::function<Json(Json, const WorkbookPackage*, std::size_t)> resolve;
    resolve = [&](Json snapshot, const WorkbookPackage* package, std::size_t depth) {
        Json links = Json::array();
        try { links = discover(snapshot, package, result.diagnostics); }
        catch (const std::exception& error) {
            result.diagnostics.push_back({{"code", "external_link_metadata"}, {"message", error.what()}});
        }
        const auto directory = package ? fs::path(package->sourcePath).parent_path() : fs::current_path();
        for (auto& link : links) {
            link.erase("workbook");
            if (link.value("name", "").empty()) link["name"] = fs::path(link.at("path").get<std::string>()).filename().string();
            bool retained = false;
            if (!options.refresh && link.value("enabled", true) && options.retainedLinks.is_array()) {
                for (const auto& previous : options.retainedLinks) {
                    if (!previous.is_object() || !previous.contains("workbook") || !previous.value("enabled", false)) continue;
                    const auto previousPath = previous.value("originalPath", previous.value("path", ""));
                    if (previousPath != link.value("path", "") || previous.value("name", "") != link.value("name", "")) continue;
                    link["workbook"] = previous["workbook"];
                    link["originalPath"] = previousPath;
                    link["path"] = previous.at("path");
                    if (previous.contains("resolvedPath")) link["resolvedPath"] = previous["resolvedPath"];
                    link["enabled"] = true; link["state"] = "retained";
                    retained = true; break;
                }
            }
            if (retained) continue;
            if (options.refresh && link.value("enabled", true)) {
                try {
                    if (depth >= options.maxDepth)
                        throw LinkFailure("external_link_limit", "limited", "Linked workbook depth limit was reached");
                    const auto original = link.at("path").get<std::string>();
                    const auto relocated = options.relocatedPaths.find(original);
                    const auto target = localTarget(relocated != options.relocatedPaths.end() ? relocated->second : original,
                        relocated == options.relocatedPaths.end() && link.value("uriEncoded", false));
                    const auto requested = fs::path(target);
                    const auto path = fs::weakly_canonical(requested.is_absolute() ? requested : directory / requested);
                    if (activePaths.contains(path.string()))
                        throw LinkFailure("external_link_cycle", "cycle", "A linked workbook dependency returns to an active workbook");
                    if (const auto cached = preparedWorkbooks.find(path.string()); cached != preparedWorkbooks.end()) {
                        link["workbook"] = cached->second;
                        link["enabled"] = true; link["state"] = "refreshed";
                        link["originalPath"] = original; link["path"] = path.string(); link["resolvedPath"] = path.string();
                        continue;
                    }
                    if (attemptedFiles >= options.maxWorkbooks)
                        throw LinkFailure("external_link_limit", "limited", "Linked workbook source-file limit was reached");
                    if (!fs::is_regular_file(path)) throw LinkFailure("external_link_missing", "missing", "Linked workbook file was not found");
                    const auto length = fs::file_size(path);
                    if (length > options.maxTotalBytes - chargedBytes)
                        throw LinkFailure("external_link_limit", "limited", "Linked workbook source-byte budget was exceeded");
                    chargedBytes += length;
                    ++attemptedFiles;
                    expandedBytes += expandedSize(path, options.maxExpandedBytes - expandedBytes);
                    const auto loaded = readWorkbook(path.string());
                    ++result.filesRead;
                    result.bytesRead += length;
                    activePaths.insert(path.string());
                    try { link["workbook"] = resolve(loaded.workbook, &loaded, depth + 1); }
                    catch (...) { activePaths.erase(path.string()); throw; }
                    activePaths.erase(path.string());
                    preparedWorkbooks[path.string()] = link["workbook"];
                    link["enabled"] = true;
                    link["originalPath"] = original;
                    link["path"] = path.string();
                    link["resolvedPath"] = path.string();
                    link["state"] = "refreshed";
                } catch (const LinkFailure& error) {
                    link["enabled"] = false;
                    link["state"] = link.contains("cachedWorkbook") ? "cached" : error.state;
                    result.diagnostics.push_back({{"code", error.code},
                        {"path", link.at("path")}, {"message", error.what()}});
                } catch (const std::exception& error) {
                    link["enabled"] = false;
                    link["state"] = link.contains("cachedWorkbook") ? "cached" : "invalid";
                    result.diagnostics.push_back({{"code", "external_link_invalid"},
                        {"path", link.at("path")}, {"message", error.what()}});
                }
            } else {
                link["enabled"] = false;
                link["state"] = link.contains("cachedWorkbook") ? "cached" : "unavailable";
                result.diagnostics.push_back({{"code", link.contains("cachedWorkbook") ? "external_link_cached" : "external_link_unavailable"},
                    {"path", link.at("path")}, {"message", link.contains("cachedWorkbook") ? "Using saved external workbook values without reading the source" : "No saved external workbook values are available"}});
            }
        }
        normalizeWorkbook(snapshot, links);
        snapshot["links"] = links;
        return snapshot;
    };
    result.snapshot = resolve(std::move(workbook), source, 0);
    result.links = result.snapshot["links"];
    for (auto& link : result.links) {
        link.erase("workbook");
        if (link.contains("originalPath")) link["path"] = link["originalPath"];
    }
    return result;
}
WorkbookLinkResolution prepareWorkbookLinks(const Json& workbook, const WorkbookPackage* source, const WorkbookLinkOptions& options) {
    return prepareWorkbookLinksOwned(workbook, source, options);
}
} // namespace tigersheets

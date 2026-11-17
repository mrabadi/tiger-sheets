#pragma once

#include "tigersheets/ooxml.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>

namespace tigersheets {

struct LiveWorkbookSnapshot {
    std::string name,path;
    std::shared_ptr<const Json> workbook;
};

struct WorkbookLinkOptions {
    bool refresh = false;
    // Previous resolution.snapshot["links"], kept only in memory by the host.
    Json retainedLinks = Json::array();
    // Captured on the host UI thread. Workers only read these immutable roots.
    std::vector<LiveWorkbookSnapshot> liveWorkbooks;
    std::map<std::string, std::string> relocatedPaths;
    std::size_t maxWorkbooks = 16;
    std::size_t maxDepth = 4;
    std::uintmax_t maxTotalBytes = 64 * 1024 * 1024;
    std::uintmax_t maxExpandedBytes = 128 * 1024 * 1024;
};

struct WorkbookLinkResolution {
    Json snapshot;
    Json links = Json::array();
    Json diagnostics = Json::array();
    std::size_t filesRead = 0;
    std::uintmax_t bytesRead = 0;
};

// Source files are read only when refresh is explicit. The returned snapshot
// is transient; preserve the caller's canonical workbook for editing/saving.
WorkbookLinkResolution prepareWorkbookLinks(const Json& workbook,
    const WorkbookPackage* source = nullptr, const WorkbookLinkOptions& options = {});
// Isolated workers may transfer an owned immutable capture without cloning all
// cells. The caller's moved-from root is no longer used after this handoff.
WorkbookLinkResolution prepareWorkbookLinksOwned(Json workbook,
    const WorkbookPackage* source = nullptr, const WorkbookLinkOptions& options = {});

} // namespace tigersheets

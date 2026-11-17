#include "tigersheets/external_links_ooxml.hpp"
#include "tigersheets/calculation.hpp"
#include <pugixml.hpp>
#include <algorithm>
#include <charconv>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <stdexcept>
namespace tigersheets {
namespace {
constexpr const char* sheetNs="http://schemas.openxmlformats.org/spreadsheetml/2006/main";
constexpr const char* relNs="http://schemas.openxmlformats.org/officeDocument/2006/relationships";
constexpr const char* packageNs="http://schemas.openxmlformats.org/package/2006/relationships";
std::string local(const char* value){std::string s=value;const auto colon=s.find(':');return colon==std::string::npos?s:s.substr(colon+1);}
pugi::xml_node child(pugi::xml_node parent,const char* name){for(auto value:parent.children())if(local(value.name())==name)return value;return {};}
std::string attr(pugi::xml_node node,const char* name){for(auto a:node.attributes())if(local(a.name())==name)return a.value();return {};}
std::string tag(pugi::xml_node parent,const char* name){const std::string s=parent.name();const auto colon=s.find(':');return colon==std::string::npos?name:s.substr(0,colon+1)+name;}
std::string xml(const pugi::xml_document& document){std::ostringstream out;document.save(out,"",pugi::format_raw,pugi::encoding_utf8);return out.str();}
void parse(pugi::xml_document& document,const std::string& bytes){if(!document.load_buffer(bytes.data(),bytes.size()))throw std::runtime_error("Invalid native external-link XML");}
std::string lower(std::string value){std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return value;}
void validText(const std::string& value){for(unsigned char c:value)if(c<32&&c!='\t'&&c!='\n'&&c!='\r')throw std::runtime_error("External workbook metadata contains an invalid XML character");}
std::string fileTarget(std::string path){
 validText(path);std::replace(path.begin(),path.end(),'\\','/');if(path.starts_with("file:"))return path;
 const bool windows=path.size()>2&&std::isalpha(static_cast<unsigned char>(path[0]))&&path[1]==':'&&path[2]=='/';
 if(path.find("://")!=std::string::npos||path.starts_with("//"))throw std::runtime_error("New external workbook links require a local file path");
 std::string out=path.starts_with('/')?"file://":windows?"file:///":"";constexpr char hex[]="0123456789ABCDEF";
 for(unsigned char c:path){if(std::isalnum(c)||c=='/'||c==':'||c=='-'||c=='_'||c=='.'||c=='~')out+=static_cast<char>(c);else{out+='%';out+=hex[c>>4U];out+=hex[c&15U];}}return out;
}
std::string localPath(std::string path,bool escaped){
 if(lower(path).starts_with("file://")){path.erase(0,7);if(path.starts_with("localhost/"))path.erase(0,9);if(!path.starts_with('/'))return {};escaped=true;}
 if(path.find("://")!=std::string::npos||path.starts_with("//")||(path.size()>1&&path[1]==':'))return {};
 if(escaped){std::string decoded;for(std::size_t i=0;i<path.size();++i){if(path[i]=='%'){unsigned value=0;if(i+2>=path.size())throw std::runtime_error("Invalid external workbook URI");const auto result=std::from_chars(path.data()+i+1,path.data()+i+3,value,16);if(result.ec!=std::errc()||result.ptr!=path.data()+i+3||value==0)throw std::runtime_error("Invalid external workbook URI");decoded+=static_cast<char>(value);i+=2;}else decoded+=path[i];}path=std::move(decoded);}
 return path;
}
std::filesystem::path resolvedPath(const std::string& path,bool escaped,const std::string& workbookPath){const auto decoded=localPath(path,escaped);if(decoded.empty())return {};auto source=std::filesystem::path(decoded);if(source.is_relative()){if(workbookPath.empty())return {};source=std::filesystem::path(workbookPath).parent_path()/source;}return std::filesystem::weakly_canonical(source);}
std::string column(int index){std::string result;while(index){--index;result.insert(result.begin(),static_cast<char>('A'+index%26));index/=26;}return result;}
std::string numeric(const Json& value){if(value.is_number_integer()||value.is_number_unsigned())return value.dump();char bytes[64];const auto result=std::to_chars(bytes,bytes+sizeof(bytes),value.get<double>());if(result.ec!=std::errc())throw std::runtime_error("Invalid external numeric cache");return {bytes,result.ptr};}
std::string normalizedFormula(const std::string& input,const std::map<std::string,std::string>& names){
 std::string out;std::size_t pos=0;while(pos<input.size()){
  const auto start=pos;if(input[pos]=='"'){++pos;while(pos<input.size()){if(input[pos++]=='"'){if(pos<input.size()&&input[pos]=='"'){++pos;continue;}break;}}out+=input.substr(start,pos-start);continue;}
  std::string token;if(input[pos]=='\''){++pos;while(pos<input.size()){if(input[pos]=='\''){++pos;if(pos<input.size()&&input[pos]=='\''){token+='\'';++pos;continue;}break;}token+=input[pos++];}}
  else if(input[pos]=='['||std::isalnum(static_cast<unsigned char>(input[pos]))||input[pos]=='_'){while(pos<input.size()&&(std::isalnum(static_cast<unsigned char>(input[pos]))||std::string("_.[]$").find(input[pos])!=std::string::npos))token+=input[pos++];}
  else{out+=input[pos++];continue;}
  const auto close=token.find(']');if(pos<input.size()&&input[pos]=='!'&&token.starts_with('[')&&close!=std::string::npos){const auto found=names.find(lower(token.substr(1,close-1)));if(found!=names.end()){const auto replacement="["+found->second+"]"+token.substr(close+1);out+='\'';for(char c:replacement){out+=c;if(c=='\'')out+=c;}out+="'!";++pos;continue;}}
  out+=input.substr(start,pos-start);
 }return out;
}
struct SourceCache {Json workbook;Json values;};
SourceCache calculateCache(const Json& source){
 WorkbookSession validated(source);const auto snapshot=validated.snapshot();auto calculation=CalculationEngine().calculate(snapshot);
 if(calculation.value("status",std::string())!="ok"||!calculation.value("converged",true))throw std::runtime_error("Cannot calculate external workbook cache");
 Json cached={{"revision",0},{"sheets",Json::array()}};for(const auto& sheet:snapshot.at("sheets"))cached["sheets"].push_back({{"id",sheet.at("id")},{"name",sheet.at("name")},{"cells",Json::array()}});
 for(const auto& value:calculation.at("values")){auto found=std::find_if(cached["sheets"].begin(),cached["sheets"].end(),[&](const Json& s){return s.at("id")==value.at("sheet");});if(found==cached["sheets"].end())continue;const auto kind=value.at("type").get<std::string>();std::string input;
  if(kind=="empty")continue;else if(kind=="number")input=numeric(value.at("value"));else if(kind=="boolean")input=value.at("value").get<bool>()?"TRUE":"FALSE";else if(kind=="text")input="'"+value.at("value").get<std::string>();else if(kind=="error")input=value.at("value").get<std::string>();else throw std::runtime_error("Invalid external cache value type");
  validText(input);(*found)["cells"].push_back({{"row",value.at("row")},{"col",value.at("col")},{"input",input}});
 }return {std::move(cached),std::move(calculation["values"])};
}
void sourceCache(pugi::xml_node book,const SourceCache& cache){
 for(const auto* name:{"sheetNames","sheetDataSet"})if(auto old=child(book,name))book.remove_child(old);
 const std::string parentName=book.name();const auto colon=parentName.find(':');const auto prefix=colon==std::string::npos?std::string():parentName.substr(0,colon+1);const auto qualified=[&](const char* name){return prefix+name;};
 auto names=book.prepend_child(qualified("sheetNames").c_str()),data=book.append_child(qualified("sheetDataSet").c_str());unsigned index=0;
 for(const auto& sheet:cache.workbook.at("sheets")){const auto name=sheet.at("name").get<std::string>();validText(name);names.append_child(qualified("sheetName").c_str()).append_attribute("val")=name.c_str();auto current=data.append_child(qualified("sheetData").c_str());current.append_attribute("sheetId")=index++;
  std::map<std::pair<int,int>,const Json*> cells;for(const auto& value:cache.values)if(value.at("sheet")==sheet.at("id")&&value.at("type")!="empty")cells[{value.at("row"),value.at("col")}]=&value;
  int previous=0;pugi::xml_node row;for(const auto& [coordinate,value]:cells){const auto [r,c]=coordinate;if(r!=previous){row=current.append_child(qualified("row").c_str());row.append_attribute("r")=r;previous=r;}auto cell=row.append_child(qualified("cell").c_str());cell.append_attribute("r")=(column(c)+std::to_string(r)).c_str();const auto kind=value->at("type").get<std::string>();cell.append_attribute("t")=(kind=="number"?"n":kind=="boolean"?"b":kind=="error"?"e":"str");std::string text;if(kind=="number")text=numeric(value->at("value"));else if(kind=="boolean")text=value->at("value").get<bool>()?"1":"0";else text=value->at("value").get<std::string>();cell.append_child(qualified("v").c_str()).text().set(text.c_str());}
 }
}
}
void restoreNativeExternalFormulaNames(Json& workbook){
 const auto links=workbook.find("links");if(links==workbook.end()||!links->is_array())return;
 std::map<std::string,std::string> names;
 for(const auto& link:*links)if(link.is_object()&&link.contains("index")&&link.at("index").is_number_integer()&&link.at("index")>0&&link.contains("name")&&link.at("name").is_string())names[std::to_string(link.at("index").get<int>())]=link.at("name").get<std::string>();
 if(names.empty())return;for(auto& sheet:workbook.at("sheets"))for(auto& cell:sheet.at("cells")){const auto input=cell.at("input").get<std::string>();if(input.starts_with('='))cell["input"]=normalizedFormula(input,names);}
}
Json writeNativeExternalLinks(std::map<std::string,std::string>& entries,const Json& workbook,const std::map<std::string,std::string>& sheetParts,const Json* resolvedLinks){return writeNativeExternalLinks(entries,workbook,sheetParts,resolvedLinks,{},{});}
Json writeNativeExternalLinks(std::map<std::string,std::string>& entries,const Json& workbook,const std::map<std::string,std::string>& sheetParts,const Json* resolvedLinks,const std::string& savePath,const std::string& originalWorkbookPath){
 if(!workbook.contains("links")||workbook.at("links").empty())return nullptr;
 auto metadata=workbook.at("links");pugi::xml_document native,relationships,types;parse(native,entries.at("xl/workbook.xml"));parse(relationships,entries.at("xl/_rels/workbook.xml.rels"));parse(types,entries.at("[Content_Types].xml"));auto root=native.document_element(),refs=child(root,"externalReferences");
 if(!root.attribute("xmlns:r"))root.append_attribute("xmlns:r")=relNs;
 if(!refs){auto before=child(root,"definedNames");if(!before)before=child(root,"calcPr");refs=before?root.insert_child_before(tag(root,"externalReferences").c_str(),before):root.append_child(tag(root,"externalReferences").c_str());}
 std::vector<std::string> parts;for(auto reference:refs.children()){const auto id=attr(reference,"id");std::string target;for(auto rel:relationships.document_element().children())if(attr(rel,"Id")==id)target=attr(rel,"Target");if(target.empty())throw std::runtime_error("External reference relationship is missing");parts.push_back((std::filesystem::path("xl")/target).lexically_normal().generic_string());}
 std::map<std::string,std::string> names;std::map<std::string,std::string> pathsByName;
 for(auto& link:metadata){const auto name=link.value("name",std::filesystem::path(link.at("path").get<std::string>()).filename().string()),path=link.at("path").get<std::string>();validText(name);if(name.empty()||path.empty())throw std::runtime_error("External workbook links require a name and source path");std::string authoredPath=path;const auto actual=resolvedPath(path,link.value("uriEncoded",false),originalWorkbookPath.empty()?savePath:originalWorkbookPath);if(!savePath.empty()&&!actual.empty()){const auto destination=std::filesystem::weakly_canonical(std::filesystem::path(savePath).parent_path());authoredPath=actual.parent_path()==destination?actual.filename().string():actual.string();link["path"]=authoredPath;link["uriEncoded"]=false;}const auto target=fileTarget(authoredPath),key=lower(name);
  if(pathsByName.contains(key)&&pathsByName.at(key)!=target)throw std::runtime_error("Ambiguous external workbook filename");pathsByName[key]=target;
  int index=link.value("index",0);if(index<0||index>static_cast<int>(parts.size()))index=0;
  if(index==0)for(std::size_t i=0;i<parts.size();++i){const std::filesystem::path part(parts[i]);const auto relPart=(part.parent_path()/"_rels"/(part.filename().string()+".rels")).generic_string();if(!entries.contains(relPart))continue;pugi::xml_document rel;parse(rel,entries.at(relPart));for(auto item:rel.document_element().children())if(attr(item,"Target")==target||(!actual.empty()&&resolvedPath(attr(item,"Target"),true,originalWorkbookPath.empty()?savePath:originalWorkbookPath)==actual))index=static_cast<int>(i+1);}
  bool created=false;if(index==0){index=static_cast<int>(parts.size()+1);const auto part="xl/externalLinks/tigersheetsLink"+std::to_string(index)+".xml";if(entries.contains(part))throw std::runtime_error("External workbook part name collision");parts.push_back(part);const auto id="tigerExternal"+std::to_string(index);auto ref=refs.append_child(tag(refs,"externalReference").c_str());ref.append_attribute("r:id")=id.c_str();auto rel=relationships.document_element().append_child("Relationship");rel.append_attribute("Id")=id.c_str();rel.append_attribute("Type")=(std::string(relNs)+"/externalLink").c_str();rel.append_attribute("Target")=part.substr(3).c_str();auto type=types.document_element().append_child("Override");type.append_attribute("PartName")=("/"+part).c_str();type.append_attribute("ContentType")="application/vnd.openxmlformats-officedocument.spreadsheetml.externalLink+xml";created=true;}
  names[key]=std::to_string(index);link["index"]=index;const auto part=parts.at(static_cast<std::size_t>(index-1));const std::filesystem::path p(part);const auto relPart=(p.parent_path()/"_rels"/(p.filename().string()+".rels")).generic_string();
  const Json* source=nullptr;if(resolvedLinks&&resolvedLinks->is_array())for(const auto& resolved:*resolvedLinks)if(lower(resolved.value("name",std::string()))==key&&resolved.value("originalPath",resolved.value("path",std::string()))==path){if(resolved.value("enabled",true)&&resolved.contains("workbook"))source=&resolved.at("workbook");else if(resolved.contains("cachedWorkbook"))source=&resolved.at("cachedWorkbook");}
  if(!source&&link.contains("cachedWorkbook"))source=&link.at("cachedWorkbook");
  if(created||source){pugi::xml_document external;if(entries.contains(part))parse(external,entries.at(part));else{auto node=external.append_child("externalLink");node.append_attribute("xmlns")=sheetNs;node.append_attribute("xmlns:r")=relNs;}auto book=child(external.document_element(),"externalBook");if(!book){if(!created)throw std::runtime_error("Cannot replace a non-workbook external link");book=external.document_element().append_child("externalBook");}auto relationship=book.attribute("r:id");if(!relationship)relationship=book.append_attribute("r:id");relationship="rId1";
   if(source){auto cached=calculateCache(*source);sourceCache(book,cached);link.erase("cachedWorkbook");}entries[part]=xml(external);
   pugi::xml_document rels;auto r=rels.append_child("Relationships");r.append_attribute("xmlns")=packageNs;auto relation=r.append_child("Relationship");relation.append_attribute("Id")="rId1";relation.append_attribute("Type")=(std::string(relNs)+"/externalLinkPath").c_str();relation.append_attribute("Target")=target.c_str();relation.append_attribute("TargetMode")="External";entries[relPart]=xml(rels);
  }else if(entries.contains(relPart)){
   pugi::xml_document rels;parse(rels,entries.at(relPart));bool changed=false;for(auto relation:rels.document_element().children())if(attr(relation,"Type").ends_with("/externalLinkPath")){relation.attribute("Target")=target.c_str();changed=true;}if(changed)entries[relPart]=xml(rels);
  }
 }
 for(const auto& [id,part]:sheetParts){pugi::xml_document sheet;parse(sheet,entries.at(part));bool changed=false;for(auto row:child(sheet.document_element(),"sheetData").children())for(auto cell:row.children())if(auto formula=child(cell,"f")){const std::string before=formula.text().as_string();const auto after=normalizedFormula(before,names);if(after!=before){formula.text().set(after.c_str());changed=true;}}if(changed)entries[part]=xml(sheet);}
 entries["xl/workbook.xml"]=xml(native);entries["xl/_rels/workbook.xml.rels"]=xml(relationships);entries["[Content_Types].xml"]=xml(types);return metadata;
}
}

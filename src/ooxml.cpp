#include "tigersheets/ooxml.hpp"
#include "tigersheets/pivot_ooxml.hpp"
#include "tigersheets/printing_ooxml.hpp"
#include "tigersheets/references.hpp"
#include "tigersheets/worksheet_layout.hpp"
#include "tigersheets/worksheet_validation.hpp"
#include <zip.h>
#include <pugixml.hpp>
#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <memory>
#include <sstream>
#include <set>
#include <string_view>
#include <stdexcept>
#include <unordered_map>
#include <unistd.h>
#include <fcntl.h>
namespace tigersheets {
namespace {
constexpr const char* ns="http://schemas.openxmlformats.org/spreadsheetml/2006/main";
constexpr const char* relNs="http://schemas.openxmlformats.org/officeDocument/2006/relationships";
using Entries=std::map<std::string,std::string>;
constexpr std::uint64_t maximumMemberBytes=256ULL*1024*1024,maximumExpandedBytes=1024ULL*1024*1024,maximumArchiveBytes=2ULL*1024*1024*1024;
void accountMember(std::uint64_t size,std::uint64_t& total){if(size>maximumMemberBytes||size>maximumExpandedBytes-total)throw std::runtime_error("Workbook expanded size limit exceeded (256 MiB per member, 1 GiB total)");total+=size;}
void validatePackageBudget(const Entries& entries){if(entries.size()>20000)throw std::runtime_error("Too many workbook package members");std::uint64_t total=0;for(const auto& [name,bytes]:entries){if(name.empty()||name.front()=='/'||name.find("../")!=std::string::npos)throw std::runtime_error("Unsafe workbook package member");accountMember(bytes.size(),total);}}
std::string readFile(const std::string& path){std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Cannot open workbook: "+path);file.seekg(0,std::ios::end);const auto length=file.tellg();if(length<0||static_cast<std::uint64_t>(length)>maximumArchiveBytes)throw std::runtime_error("Workbook archive exceeds 2 GiB limit");file.seekg(0);std::string bytes(static_cast<std::size_t>(length),'\0');if(!bytes.empty()&&!file.read(bytes.data(),length))throw std::runtime_error("Cannot read workbook");return bytes;}
std::string hashBytes(const std::string& bytes){
 constexpr std::array<std::uint32_t,64> constants{0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
 std::array<std::uint32_t,8> state{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
 std::vector<unsigned char> data(bytes.begin(),bytes.end());const auto bits=static_cast<std::uint64_t>(data.size())*8U;data.push_back(0x80);while(data.size()%64!=56)data.push_back(0);for(int i=7;i>=0;--i)data.push_back(static_cast<unsigned char>(bits>>(i*8)));
 const auto rotate=[](std::uint32_t x,int n){return (x>>n)|(x<<(32-n));};
 for(std::size_t offset=0;offset<data.size();offset+=64){std::array<std::uint32_t,64> words{};for(std::size_t i=0;i<16;++i)for(std::size_t j=0;j<4;++j)words[i]=(words[i]<<8)|data[offset+i*4+j];for(std::size_t i=16;i<64;++i){const auto a=words[i-15],b=words[i-2];words[i]=words[i-16]+(rotate(a,7)^rotate(a,18)^(a>>3))+words[i-7]+(rotate(b,17)^rotate(b,19)^(b>>10));}auto a=state[0],b=state[1],c=state[2],d=state[3],e=state[4],f=state[5],g=state[6],h=state[7];for(std::size_t i=0;i<64;++i){const auto t1=h+(rotate(e,6)^rotate(e,11)^rotate(e,25))+((e&f)^(~e&g))+constants[i]+words[i];const auto t2=(rotate(a,2)^rotate(a,13)^rotate(a,22))+((a&b)^(a&c)^(b&c));h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;}state[0]+=a;state[1]+=b;state[2]+=c;state[3]+=d;state[4]+=e;state[5]+=f;state[6]+=g;state[7]+=h;}
 std::ostringstream result;result<<std::hex<<std::setfill('0');for(const auto value:state)result<<std::setw(8)<<value;return result.str();
}
std::string local(const char* name){const std::string value(name);const auto colon=value.find(':');return colon==std::string::npos?value:value.substr(colon+1);}
pugi::xml_node child(pugi::xml_node parent,const char* name){for(auto c:parent.children())if(local(c.name())==name)return c;return {};}
std::string xml(const pugi::xml_document& doc){
 std::ostringstream stream;doc.save(stream,"",pugi::format_raw|pugi::format_no_declaration,pugi::encoding_utf8);const auto body=stream.str();std::string encoded;encoded.reserve(body.size());
 for(std::size_t pos=0;pos<body.size();){
  const char* closing=nullptr;if(body.compare(pos,9,"<![CDATA[")==0)closing="]]>";else if(body.compare(pos,4,"<!--")==0)closing="-->";else if(body.compare(pos,2,"<?")==0)closing="?>";
  if(closing){const auto end=body.find(closing,pos+2);const auto count=end==std::string::npos?body.size()-pos:end+std::strlen(closing)-pos;encoded.append(body,pos,count);pos+=count;continue;}
  if(body[pos]=='\r')encoded+="&#13;";else encoded+=body[pos];++pos;
 }
 return "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"+encoded;
}
void parse(pugi::xml_document& doc,const std::string& text){const auto result=doc.load_buffer(text.data(),text.size());if(!result)throw std::runtime_error(std::string("Invalid workbook XML: ")+result.description());}
std::string column(int col){std::string result;while(col>0){--col;result.insert(result.begin(),static_cast<char>('A'+col%26));col/=26;}return result;}
std::pair<int,int> coordinate(const std::string& ref){int col=0,row=0;std::size_t i=0;for(;i<ref.size()&&ref[i]>='A'&&ref[i]<='Z';++i){col=col*26+(ref[i]-'A'+1);if(col>16384)throw std::runtime_error("Invalid cell column");}const auto result=std::from_chars(ref.data()+i,ref.data()+ref.size(),row);if(result.ec!=std::errc()||result.ptr!=ref.data()+ref.size()||row<1||row>1048576||col<1)throw std::runtime_error("Invalid cell reference: "+ref);return {row,col};}
Entries unzip(const std::string& bytes){zip_error_t detail;zip_error_init(&detail);auto* source=zip_source_buffer_create(bytes.data(),bytes.size(),0,&detail);if(!source){zip_error_fini(&detail);throw std::runtime_error("Cannot read ZIP package");}auto* archive=zip_open_from_source(source,ZIP_RDONLY,&detail);if(!archive){zip_source_free(source);zip_error_fini(&detail);throw std::runtime_error("Invalid ZIP package");}zip_error_fini(&detail);std::unique_ptr<zip_t,decltype(&zip_discard)> guard(archive,zip_discard);Entries entries;const auto count=zip_get_num_entries(archive,0);if(count<0||count>20000)throw std::runtime_error("Too many workbook package members");std::uint64_t total=0;for(zip_int64_t i=0;i<count;++i){zip_stat_t stat;zip_stat_init(&stat);if(zip_stat_index(archive,static_cast<zip_uint64_t>(i),0,&stat)<0)throw std::runtime_error("Invalid ZIP member");const std::string name=stat.name;if(name.empty()||name.front()=='/'||name.find("../")!=std::string::npos||entries.contains(name))throw std::runtime_error("Unsafe or duplicate workbook package member");accountMember(stat.size,total);auto* file=zip_fopen_index(archive,static_cast<zip_uint64_t>(i),0);if(!file)throw std::runtime_error("Cannot decode workbook package member");std::string content(static_cast<std::size_t>(stat.size),'\0');zip_uint64_t offset=0;while(offset<stat.size){const auto n=zip_fread(file,content.data()+offset,stat.size-offset);if(n<=0){zip_fclose(file);throw std::runtime_error("Truncated ZIP member");}offset+=static_cast<zip_uint64_t>(n);}if(zip_fclose(file)<0)throw std::runtime_error("Corrupt ZIP member");entries.emplace(name,std::move(content));}return entries;}
std::string targetPath(const std::string& base,const std::string& target){auto path=target.starts_with('/')?std::filesystem::path(target.substr(1)):std::filesystem::path(base).parent_path()/target;const auto result=path.lexically_normal().generic_string();if(result.starts_with("../")||result.starts_with('/'))throw std::runtime_error("Invalid workbook relationship target");return result;}
std::string attrLocal(pugi::xml_node node,const char* key){for(auto attribute:node.attributes())if(local(attribute.name())==key)return attribute.value();return {};}
std::string collectText(pugi::xml_node node){std::string text;for(auto item:node.children()){if(local(item.name())=="t")text+=item.text().as_string();else text+=collectText(item);}return text;}
void setCell(pugi::xml_node cell,const std::string& input);
std::string makeWorksheet(const Json& sheet){
 pugi::xml_document doc;auto root=doc.append_child("worksheet");root.append_attribute("xmlns")=ns;auto data=root.append_child("sheetData");pugi::xml_node row;int previous=-1;
 for(const auto& value:sheet.at("cells")){const auto r=value.at("row").get<int>(),c=value.at("col").get<int>();if(r!=previous){row=data.append_child("row");row.append_attribute("r")=r;previous=r;}auto cell=row.append_child("c");cell.append_attribute("r")=(column(c)+std::to_string(r)).c_str();setCell(cell,value.at("input").get<std::string>());}return xml(doc);
}
void writeZip(const std::string& path,const Entries& entries){int error=0;auto* archive=zip_open(path.c_str(),ZIP_CREATE|ZIP_TRUNCATE,&error);if(!archive)throw std::runtime_error("Cannot create workbook ZIP package");for(const auto& [name,bytes]:entries){auto* source=zip_source_buffer(archive,bytes.data(),bytes.size(),0);if(!source||zip_file_add(archive,name.c_str(),source,ZIP_FL_ENC_UTF_8|ZIP_FL_OVERWRITE)<0){if(source)zip_source_free(source);const std::string message=zip_strerror(archive);zip_discard(archive);throw std::runtime_error("Cannot write workbook member: "+message);}}if(zip_close(archive)<0){const std::string message=zip_strerror(archive);zip_discard(archive);throw std::runtime_error("Cannot complete workbook: "+message);}}

void atomicSave(const std::string& requested,const WorkbookPackage* source,const std::function<void(const std::string&)>& writer){
 const auto destination=std::filesystem::weakly_canonical(requested).string();
 const auto conflict=[&]{if(source&&source->sourcePath==destination&&hashBytes(readFile(destination))!=source->sourceHash)throw std::runtime_error("Workbook changed outside TigerSheets; save a separate copy");};conflict();
 const auto pattern=destination+".tigersheets-XXXXXX";std::vector<char> temporary(pattern.begin(),pattern.end());temporary.push_back('\0');const auto created=mkstemp(temporary.data());if(created<0)throw std::runtime_error("Cannot create temporary workbook");close(created);
 try{writer(temporary.data());const auto descriptor=open(temporary.data(),O_RDONLY|O_CLOEXEC);if(descriptor<0)throw std::runtime_error("Cannot synchronize saved workbook");const auto synced=fsync(descriptor);close(descriptor);if(synced<0)throw std::runtime_error("Cannot synchronize saved workbook");conflict();std::filesystem::rename(temporary.data(),destination);}catch(...){std::filesystem::remove(temporary.data());throw;}
}

std::string prefixed(pugi::xml_node node,const char* name);
pugi::xml_attribute attribute(pugi::xml_node node,const char* name){auto result=node.attribute(name);return result?result:node.append_attribute(name);}
pugi::xml_node ensure(pugi::xml_node node,const char* name){auto result=child(node,name);return result?result:node.append_child(prefixed(node,name).c_str());}
// Native worksheet dimensions and merges remain authoritative even without app metadata.
void readWorksheetLayout(pugi::xml_node root,Json& sheet){
 Json merges=Json::array(),rows=Json::array(),columns=Json::array(),hiddenRows=Json::array();
 for(auto node:child(root,"mergeCells").children())if(local(node.name())=="mergeCell"){const std::string ref=node.attribute("ref").value();const auto colon=ref.find(':');if(colon==std::string::npos)throw std::runtime_error("Invalid native merged range");const auto [r1,c1]=coordinate(ref.substr(0,colon));const auto [r2,c2]=coordinate(ref.substr(colon+1));merges.push_back({{"row1",r1},{"col1",c1},{"row2",r2},{"col2",c2}});}
 for(auto row:child(root,"sheetData").children())if(local(row.name())=="row"&&(row.attribute("ht")||row.attribute("hidden"))){const int index=row.attribute("r").as_int();Json entry={{"index",index}};if(row.attribute("ht")){const auto size=row.attribute("ht").as_double(-1);if(size<0||!std::isfinite(size))throw std::runtime_error("Invalid native row height");if(size>0)entry["size"]=size;else entry["hidden"]=true;}if(row.attribute("hidden"))entry["hidden"]=row.attribute("hidden").as_bool();if(entry.value("hidden",false))hiddenRows.push_back(index);rows.push_back(std::move(entry));}
 std::set<int> seen;for(auto col:child(root,"cols").children())if(local(col.name())=="col"){const int first=col.attribute("min").as_int(),last=col.attribute("max").as_int();if(first<1||last<first||last>16384)throw std::runtime_error("Invalid native column range");for(int index=first;index<=last;++index){if(!seen.insert(index).second)throw std::runtime_error("Overlapping native column definitions");Json entry={{"index",index}};if(col.attribute("width")){const auto size=col.attribute("width").as_double(-1);if(size<0||!std::isfinite(size))throw std::runtime_error("Invalid native column width");if(size>0)entry["size"]=size;else entry["hidden"]=true;}if(col.attribute("hidden"))entry["hidden"]=col.attribute("hidden").as_bool();if(entry.size()>1)columns.push_back(std::move(entry));}}
 if(!merges.empty())sheet["mergedRanges"]=std::move(merges);if(!rows.empty())sheet["rowLayout"]=std::move(rows);if(!columns.empty())sheet["columnLayout"]=std::move(columns);if(!hiddenRows.empty())sheet["hiddenRows"]=std::move(hiddenRows);
}
pugi::xml_node insertBeforeTags(pugi::xml_node root,const char* name,const std::set<std::string>& after){for(auto next:root.children())if(after.contains(local(next.name())))return root.insert_child_before(prefixed(root,name).c_str(),next);return root.append_child(prefixed(root,name).c_str());}
std::optional<Json> nativeListValidation(pugi::xml_node node){
 if(local(node.name())!="dataValidation"||std::string(node.attribute("type").value())!="list")return {};
 for(const auto attr:node.attributes())if(local(attr.name())!="type"&&local(attr.name())!="sqref"&&local(attr.name())!="allowBlank")return {};
 int children=0;for(const auto item:node.children())if(item.type()==pugi::node_element){if(local(item.name())!="formula1")return {};++children;}if(children!=1)return {};
 try{const std::string ref=node.attribute("sqref").value();const auto colon=ref.find(':');const auto [r1,c1]=coordinate(ref.substr(0,colon));const auto [r2,c2]=colon==std::string::npos?std::pair{r1,c1}:coordinate(ref.substr(colon+1));if(r2<r1||c2<c1)return {};
  std::string formula=child(node,"formula1").text().as_string();if(formula.empty())return {};if(formula.front()!='=')formula.insert(formula.begin(),'=');const auto refs=formulaReferences(formula);if(refs.size()!=1||refs[0].start!=1||refs[0].length!=formula.size()-1||refs[0].external||(refs[0].row1!=refs[0].row2&&refs[0].col1!=refs[0].col2)||std::max(refs[0].row2-refs[0].row1,refs[0].col2-refs[0].col1)>=10000)return {};
  return Json{{"type","list"},{"range",{{"row1",r1},{"col1",c1},{"row2",r2},{"col2",c2}}},{"formula1",formula},{"allowBlank",node.attribute("allowBlank").as_bool()}};
 }catch(const std::exception&){return {};}
}
void readWorksheetValidations(pugi::xml_node root,Json& sheet){Json rules=Json::array();for(const auto node:child(root,"dataValidations").children())if(auto rule=nativeListValidation(node))rules.push_back(*rule);if(!rules.empty())sheet["dataValidations"]=std::move(rules);}
void nativeWorksheetValidations(Entries& entries,const Json& book,const std::map<std::string,std::string>& parts,const WorkbookPackage* source){
 for(const auto& sheet:book.at("sheets")){
  Json previous=Json::array();if(source)for(const auto& prior:source->baseline.at("sheets"))if(prior.at("id")==sheet.at("id")){previous=prior.value("dataValidations",Json::array());break;}const auto rules=sheet.value("dataValidations",Json::array());if(rules==previous)continue;
  const auto part=parts.at(sheet.at("id").get<std::string>());pugi::xml_document doc;parse(doc,entries.at(part));auto root=doc.document_element();auto container=child(root,"dataValidations");if(!container&&!rules.empty())container=insertBeforeTags(root,"dataValidations",{"hyperlinks","printOptions","pageMargins","pageSetup","headerFooter","rowBreaks","colBreaks","customProperties","cellWatches","ignoredErrors","smartTags","drawing","legacyDrawing","legacyDrawingHF","picture","oleObjects","controls","webPublishItems","tableParts","extLst"});
  for(auto node=container.first_child();node;){const auto next=node.next_sibling();if(auto rule=nativeListValidation(node);rule&&std::find(previous.begin(),previous.end(),*rule)!=previous.end())container.remove_child(node);node=next;}
  for(const auto& rule:rules){const auto& range=rule.at("range");const auto first=column(range.at("col1"))+std::to_string(range.at("row1").get<int>()),last=column(range.at("col2"))+std::to_string(range.at("row2").get<int>());auto node=container.append_child(prefixed(root,"dataValidation").c_str());node.append_attribute("type")="list";node.append_attribute("allowBlank")=rule.value("allowBlank",true);node.append_attribute("sqref")=(first==last?first:first+":"+last).c_str();auto formula=rule.at("formula1").get<std::string>();if(formula.starts_with('='))formula.erase(0,1);node.append_child(prefixed(root,"formula1").c_str()).text().set(formula.c_str());}
  if(container){unsigned count=0;for(const auto node:container.children())if(local(node.name())=="dataValidation")++count;if(count)attribute(container,"count")=count;else root.remove_child(container);}entries[part]=xml(doc);
 }
}
void nativeWorksheetLayouts(Entries& entries,const Json& book,const std::map<std::string,std::string>& parts,const WorkbookPackage* source){
 for(const auto& sheet:book.at("sheets")){
  const Json* before=nullptr;if(source)for(const auto& prior:source->baseline.at("sheets"))if(prior.at("id")==sheet.at("id")){before=&prior;break;}
  const auto changed=[&](const char* key){return sheet.value(key,Json::array())!=(before?before->value(key,Json::array()):Json::array());};
  const bool structural=before&&before->value("structuralRevision",0ULL)!=sheet.value("structuralRevision",0ULL);if(!changed("mergedRanges")&&!changed("rowLayout")&&!changed("columnLayout")&&!structural)continue;
  const auto part=parts.at(sheet.at("id").get<std::string>());pugi::xml_document doc;parse(doc,entries.at(part));auto root=doc.document_element();
  if(changed("mergedRanges")){for(auto merge=child(root,"mergeCells");merge;merge=child(root,"mergeCells"))root.remove_child(merge);const auto merges=sheet.value("mergedRanges",Json::array());if(!merges.empty()){auto container=insertBeforeTags(root,"mergeCells",{"phoneticPr","conditionalFormatting","dataValidations","hyperlinks","printOptions","pageMargins","pageSetup","headerFooter","rowBreaks","colBreaks","customProperties","cellWatches","ignoredErrors","smartTags","drawing","legacyDrawing","legacyDrawingHF","picture","oleObjects","controls","webPublishItems","tableParts","extLst"});container.append_attribute("count")=static_cast<unsigned>(merges.size());for(const auto& range:merges){const auto ref=column(range.at("col1"))+std::to_string(range.at("row1").get<int>())+":"+column(range.at("col2"))+std::to_string(range.at("row2").get<int>());container.append_child(prefixed(root,"mergeCell").c_str()).append_attribute("ref")=ref.c_str();}}}
  if(changed("rowLayout")||structural){auto data=child(root,"sheetData");if(!data)data=insertBeforeTags(root,"sheetData",{"sheetCalcPr","sheetProtection","protectedRanges","scenarios","autoFilter","sortState","dataConsolidate","customSheetViews","mergeCells","phoneticPr","conditionalFormatting","dataValidations","hyperlinks","printOptions","pageMargins","pageSetup","drawing","extLst"});for(auto row:data.children()){row.remove_attribute("ht");row.remove_attribute("hidden");row.remove_attribute("customHeight");if(structural)row.remove_attribute("spans");}for(const auto& entry:sheet.value("rowLayout",Json::array())){const int index=entry.at("index");pugi::xml_node row;for(auto current:data.children())if(current.attribute("r").as_int()==index){row=current;break;}if(!row){for(auto current:data.children())if(current.attribute("r").as_int()>index){row=data.insert_child_before(prefixed(root,"row").c_str(),current);break;}if(!row)row=data.append_child(prefixed(root,"row").c_str());row.append_attribute("r")=index;}if(entry.contains("size")){attribute(row,"ht")=entry.at("size").get<double>();attribute(row,"customHeight")=1;}if(entry.contains("hidden"))attribute(row,"hidden")=entry.at("hidden").get<bool>();}}
  if(changed("columnLayout")||structural){auto prior=child(root,"cols");std::map<int,pugi::xml_node> originals;for(auto item:prior.children())if(local(item.name())=="col")for(int i=item.attribute("min").as_int();i<=item.attribute("max").as_int()&&i<=16384;++i)originals[i]=item;
   auto container=prior?root.insert_child_before(prefixed(root,"cols").c_str(),prior):insertBeforeTags(root,"cols",{"sheetData","sheetCalcPr","sheetProtection","protectedRanges","scenarios","autoFilter","sortState","mergeCells","phoneticPr","conditionalFormatting","dataValidations","hyperlinks","printOptions","pageMargins","pageSetup","drawing","extLst"});
   std::map<int,Json> dimensions;for(const auto& entry:sheet.value("columnLayout",Json::array()))dimensions[entry.at("index")]=entry;
   std::set<int> indices;for(const auto& [index,node]:originals)indices.insert(index);for(const auto& [index,entry]:dimensions)indices.insert(index);
   for(const int index:indices){auto node=originals.contains(index)?container.append_copy(originals.at(index)):container.append_child(prefixed(root,"col").c_str());attribute(node,"min")=index;attribute(node,"max")=index;node.remove_attribute("width");node.remove_attribute("customWidth");node.remove_attribute("hidden");if(dimensions.contains(index)){const auto& entry=dimensions.at(index);if(entry.contains("size")){attribute(node,"width")=entry.at("size").get<double>();attribute(node,"customWidth")=1;}if(entry.contains("hidden"))attribute(node,"hidden")=entry.at("hidden").get<bool>();}if(std::distance(node.attributes_begin(),node.attributes_end())==2)container.remove_child(node);}
   if(prior)root.remove_child(prior);if(!container.first_child())root.remove_child(container);
  }
  if(structural){for(auto dimension=child(root,"dimension");dimension;dimension=child(root,"dimension"))root.remove_child(dimension);}
  entries[part]=xml(doc);
 }
}
std::string stylePart(const Entries& entries){if(entries.contains("xl/_rels/workbook.xml.rels")){pugi::xml_document rel;parse(rel,entries.at("xl/_rels/workbook.xml.rels"));for(auto node:rel.document_element().children())if(std::string(node.attribute("Type").value()).ends_with("/styles"))return targetPath("xl/workbook.xml",node.attribute("Target").value());}return "xl/styles.xml";}
std::vector<Json> readStyles(const Entries& entries){
 std::vector<Json> result;const auto part=stylePart(entries);if(!entries.contains(part))return result;pugi::xml_document doc;parse(doc,entries.at(part));const auto root=doc.document_element();
 std::vector<pugi::xml_node> fonts,fills;for(auto font:child(root,"fonts").children())fonts.push_back(font);for(auto fill:child(root,"fills").children())fills.push_back(fill);
 std::map<int,std::string> formats{{0,"General"},{1,"0"},{2,"0.00"},{3,"#,##0"},{4,"#,##0.00"},{9,"0%"},{10,"0.00%"},{14,"mm-dd-yy"},{49,"@"}};for(auto format:child(root,"numFmts").children())formats[format.attribute("numFmtId").as_int()]=format.attribute("formatCode").value();
 for(auto xf:child(root,"cellXfs").children()){
  Json style{{"ooxmlStyleId",result.size()}};const auto fontId=xf.attribute("fontId").as_uint(),fillId=xf.attribute("fillId").as_uint();
  if(fontId<fonts.size()){const auto font=fonts[fontId];for(const auto* property:{"bold","italic","underline"}){const char* tag=std::string(property)=="bold"?"b":std::string(property)=="italic"?"i":"u";const auto flag=child(font,tag);if(flag)style[property]=std::string(flag.attribute("val").value())!="0";}
   if(child(font,"name"))style["fontFamily"]=child(font,"name").attribute("val").value();
   if(child(font,"sz"))style["fontSize"]=child(font,"sz").attribute("val").as_double();
   const auto color=child(font,"color").attribute("rgb");if(color){const std::string rgb=color.value();if(rgb.size()>=6)style["color"]="#"+rgb.substr(rgb.size()-6);}}
  if(fillId<fills.size()){const auto color=child(child(fills[fillId],"patternFill"),"fgColor").attribute("rgb");if(color){const std::string rgb=color.value();if(rgb.size()>=6)style["fill"]="#"+rgb.substr(rgb.size()-6);}}
  const auto format=xf.attribute("numFmtId").as_int();if(formats.contains(format))style["numberFormat"]=formats.at(format);const auto align=child(xf,"alignment");if(align.attribute("horizontal"))style["alignment"]=align.attribute("horizontal").value();if(align.attribute("wrapText"))style["wrap"]=align.attribute("wrapText").as_bool();result.push_back(std::move(style));
 }return result;
}
void nativeStyles(Entries& entries,const Json& book,const std::map<std::string,std::string>& parts){
 bool needed=false;for(const auto& sheet:book.at("sheets"))for(const auto& cell:sheet.at("cells"))if(cell.contains("style"))needed=true;if(!needed)return;
 const auto part=stylePart(entries);pugi::xml_document doc;if(entries.contains(part))parse(doc,entries.at(part));else parse(doc,R"(<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><fonts count="1"><font><sz val="11"/><name val="Calibri"/></font></fonts><fills count="2"><fill><patternFill patternType="none"/></fill><fill><patternFill patternType="gray125"/></fill></fills><borders count="1"><border><left/><right/><top/><bottom/><diagonal/></border></borders><cellStyleXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0"/></cellStyleXfs><cellXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0"/></cellXfs><cellStyles count="1"><cellStyle name="Normal" xfId="0" builtinId="0"/></cellStyles></styleSheet>)");
 auto root=doc.document_element(),fonts=child(root,"fonts"),fills=child(root,"fills"),xfs=child(root,"cellXfs");std::vector<pugi::xml_node> fontList,xfList;for(auto item:fonts.children())fontList.push_back(item);for(auto item:xfs.children())xfList.push_back(item);std::map<std::string,int> assigned;
 for(const auto& sheet:book.at("sheets")){
  if(std::none_of(sheet.at("cells").begin(),sheet.at("cells").end(),[](const Json& cell){return cell.contains("style");}))continue;
  pugi::xml_document worksheet;parse(worksheet,entries.at(parts.at(sheet.at("id").get<std::string>())));auto data=child(worksheet.document_element(),"sheetData");
  std::unordered_map<std::string_view,pugi::xml_node> nativeCells;nativeCells.reserve(sheet.at("cells").size());for(auto row:data.children())for(auto cell:row.children())if(local(cell.name())=="c")nativeCells.emplace(cell.attribute("r").value(),cell);
  for(const auto& value:sheet.at("cells")){if(!value.contains("style"))continue;const auto& style=value.at("style");const auto key=style.dump();int index=0;
   if(assigned.contains(key))index=assigned.at(key);else{
    const auto base=style.value("ooxmlStyleId",0);auto xf=(base>=0&&static_cast<std::size_t>(base)<xfList.size())?xfs.append_copy(xfList[static_cast<std::size_t>(base)]):xfs.append_child(prefixed(root,"xf").c_str());
    attribute(xf,"xfId")=0;const auto fontId=xf.attribute("fontId").as_uint();auto font=fontId<fontList.size()?fonts.append_copy(fontList[fontId]):fonts.append_child(prefixed(root,"font").c_str());
    for(const auto& pair:std::array<std::pair<const char*,const char*>,3>{{{"bold","b"},{"italic","i"},{"underline","u"}}})if(style.contains(pair.first)){for(auto flag=child(font,pair.second);flag;flag=child(font,pair.second))font.remove_child(flag);if(style[pair.first].get<bool>())font.append_child(prefixed(root,pair.second).c_str());}
    if(style.contains("fontFamily"))attribute(ensure(font,"name"),"val")=style["fontFamily"].get<std::string>().c_str();
    if(style.contains("fontSize"))attribute(ensure(font,"sz"),"val")=style["fontSize"].get<double>();
    if(style.contains("color")){auto color=ensure(font,"color");color.remove_attributes();auto rgb=style["color"].get<std::string>();if(rgb.empty())attribute(color,"auto")=1;else{if(rgb.starts_with('#'))rgb.erase(0,1);attribute(color,"rgb")=(rgb.size()==6?"FF"+rgb:rgb).c_str();}}
    attribute(xf,"fontId")=static_cast<unsigned>(std::distance(fonts.begin(),fonts.end())-1);attribute(xf,"applyFont")=1;
    if(style.contains("fill")&&style.at("fill")==""){attribute(xf,"fillId")=0;attribute(xf,"applyFill")=1;}else if(style.contains("fill")){auto fill=fills.append_child(prefixed(root,"fill").c_str());auto pattern=fill.append_child(prefixed(root,"patternFill").c_str());pattern.append_attribute("patternType")="solid";auto rgb=style["fill"].get<std::string>();if(rgb.starts_with('#'))rgb.erase(0,1);pattern.append_child(prefixed(root,"fgColor").c_str()).append_attribute("rgb")=(rgb.size()==6?"FF"+rgb:rgb).c_str();pattern.append_child(prefixed(root,"bgColor").c_str()).append_attribute("indexed")=64;attribute(xf,"fillId")=static_cast<unsigned>(std::distance(fills.begin(),fills.end())-1);attribute(xf,"applyFill")=1;}
    if(style.contains("numberFormat")){auto formats=child(root,"numFmts");if(!formats)formats=root.prepend_child(prefixed(root,"numFmts").c_str());int formatId=164;for(auto format:formats.children())formatId=std::max(formatId,format.attribute("numFmtId").as_int()+1);auto format=formats.append_child(prefixed(root,"numFmt").c_str());format.append_attribute("numFmtId")=formatId;format.append_attribute("formatCode")=style["numberFormat"].get<std::string>().c_str();attribute(formats,"count")=static_cast<unsigned>(std::distance(formats.begin(),formats.end()));attribute(xf,"numFmtId")=formatId;attribute(xf,"applyNumberFormat")=1;}
    if(style.contains("alignment")||style.contains("wrap")){auto alignment=ensure(xf,"alignment");if(style.contains("alignment")){const auto horizontal=style["alignment"].get<std::string>();attribute(alignment,"horizontal")=(horizontal.empty()?"general":horizontal).c_str();}if(style.contains("wrap"))attribute(alignment,"wrapText")=style["wrap"].get<bool>();attribute(xf,"applyAlignment")=1;}
    index=static_cast<int>(std::distance(xfs.begin(),xfs.end())-1);assigned[key]=index;
   }
   const auto ref=column(value.at("col").get<int>())+std::to_string(value.at("row").get<int>());if(const auto target=nativeCells.find(ref);target!=nativeCells.end())attribute(target->second,"s")=index;
  }entries[parts.at(sheet.at("id").get<std::string>())]=xml(worksheet);
 }
 attribute(fonts,"count")=static_cast<unsigned>(std::distance(fonts.begin(),fonts.end()));attribute(fills,"count")=static_cast<unsigned>(std::distance(fills.begin(),fills.end()));attribute(xfs,"count")=static_cast<unsigned>(std::distance(xfs.begin(),xfs.end()));entries[part]=xml(doc);
 pugi::xml_document relationships;parse(relationships,entries.at("xl/_rels/workbook.xml.rels"));bool present=false;for(auto rel:relationships.document_element().children())if(std::string(rel.attribute("Type").value()).ends_with("/styles"))present=true;if(!present){auto rel=relationships.document_element().append_child("Relationship");rel.append_attribute("Id")="tigerStyles";rel.append_attribute("Type")=(std::string(relNs)+"/styles").c_str();rel.append_attribute("Target")=std::filesystem::path(part).lexically_relative("xl").generic_string().c_str();}entries["xl/_rels/workbook.xml.rels"]=xml(relationships);
 pugi::xml_document types;parse(types,entries.at("[Content_Types].xml"));present=false;for(auto node:types.document_element().children())if(("/"+part)==node.attribute("PartName").value())present=true;if(!present){auto node=types.document_element().append_child("Override");node.append_attribute("PartName")=("/"+part).c_str();node.append_attribute("ContentType")="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml";}entries["[Content_Types].xml"]=xml(types);
}

std::string relationshipPart(const std::string& part){const std::filesystem::path path(part);return (path.parent_path()/"_rels"/(path.filename().string()+".rels")).generic_string();}
void addType(Entries& entries,const std::string& part,const char* type){pugi::xml_document doc;parse(doc,entries.at("[Content_Types].xml"));for(auto node:doc.document_element().children())if(("/"+part)==node.attribute("PartName").value())return;auto node=doc.document_element().append_child(prefixed(doc.document_element(),"Override").c_str());node.append_attribute("PartName")=("/"+part).c_str();node.append_attribute("ContentType")=type;entries["[Content_Types].xml"]=xml(doc);}
std::string uniqueRelation(pugi::xml_node root,const std::string& prefix){auto id=prefix;bool used=true;while(used){used=false;for(auto node:root.children())if(id==node.attribute("Id").value()){id+='x';used=true;}}return id;}
void relationDocument(pugi::xml_document& doc,const Entries& entries,const std::string& part){if(entries.contains(part))parse(doc,entries.at(part));else doc.append_child("Relationships").append_attribute("xmlns")="http://schemas.openxmlformats.org/package/2006/relationships";}
std::string sheetFormula(const std::string& name,int r1,int c1,int r2,int c2){std::string quoted="'";for(const auto c:name){quoted+=c;if(c=='\'')quoted+='\'';}quoted+="'!$"+column(c1)+"$"+std::to_string(r1);if(r1!=r2||c1!=c2)quoted+=":$"+column(c2)+"$"+std::to_string(r2);return quoted;}
std::string chartXml(const Json& chart,const Json& sheet){
 const auto type=chart.value("type",std::string("line"));const std::set<std::string> supported{"line","column","bar","scatter","area","pie","doughnut"};if(!supported.contains(type))throw std::runtime_error("Native chart export is not yet available for "+type);
 const auto& range=chart.at("range");const auto r1=range.at("row1").get<int>(),c1=range.at("col1").get<int>(),r2=range.at("row2").get<int>(),c2=range.at("col2").get<int>();if(r1<1||r2<=r1||r2>1048576||c1<1||c2<=c1||c2>16384)throw std::runtime_error("Chart needs a header, categories and at least one value column");
 pugi::xml_document doc;auto space=doc.append_child("c:chartSpace");space.append_attribute("xmlns:c")="http://schemas.openxmlformats.org/drawingml/2006/chart";space.append_attribute("xmlns:a")="http://schemas.openxmlformats.org/drawingml/2006/main";space.append_attribute("xmlns:r")=relNs;auto main=space.append_child("c:chart");const auto title=chart.value("title",std::string());if(!title.empty()){auto rich=main.append_child("c:title").append_child("c:tx").append_child("c:rich");rich.append_child("a:bodyPr");rich.append_child("a:lstStyle");auto paragraph=rich.append_child("a:p");paragraph.append_child("a:r").append_child("a:t").text().set(title.c_str());}
 auto plot=main.append_child("c:plotArea");plot.append_child("c:layout");const std::string tag=(type=="column"||type=="bar")?"c:barChart":"c:"+type+"Chart";auto chartNode=plot.append_child(tag.c_str());if(type=="column"||type=="bar"){chartNode.append_child("c:barDir").append_attribute("val")=type=="bar"?"bar":"col";chartNode.append_child("c:grouping").append_attribute("val")="clustered";}else if(type=="scatter")chartNode.append_child("c:scatterStyle").append_attribute("val")="lineMarker";else if(type=="line"||type=="area")chartNode.append_child("c:grouping").append_attribute("val")="standard";
 const auto name=sheet.at("name").get<std::string>();for(int col=c1+1;col<=c2;++col){auto series=chartNode.append_child("c:ser");series.append_child("c:idx").append_attribute("val")=col-c1-1;series.append_child("c:order").append_attribute("val")=col-c1-1;auto reference=series.append_child("c:tx").append_child("c:strRef");reference.append_child("c:f").text().set(sheetFormula(name,r1,col,r1,col).c_str());auto category=series.append_child(type=="scatter"?"c:xVal":"c:cat").append_child(type=="scatter"?"c:numRef":"c:strRef");category.append_child("c:f").text().set(sheetFormula(name,r1+1,c1,r2,c1).c_str());auto values=series.append_child(type=="scatter"?"c:yVal":"c:val").append_child("c:numRef");values.append_child("c:f").text().set(sheetFormula(name,r1+1,col,r2,col).c_str());}
 if(type=="doughnut")chartNode.append_child("c:holeSize").append_attribute("val")=50;
 if(type!="pie"&&type!="doughnut"){chartNode.append_child("c:axId").append_attribute("val")=1001;chartNode.append_child("c:axId").append_attribute("val")=1002;for(int axis=0;axis<2;++axis){auto node=plot.append_child(axis==0&&type!="scatter"?"c:catAx":"c:valAx");node.append_child("c:axId").append_attribute("val")=1001+axis;node.append_child("c:scaling").append_child("c:orientation").append_attribute("val")="minMax";node.append_child("c:axPos").append_attribute("val")=axis==0?"b":"l";node.append_child("c:crossAx").append_attribute("val")=axis==0?1002:1001;node.append_child("c:crosses").append_attribute("val")="autoZero";}}
 main.append_child("c:plotVisOnly").append_attribute("val")=1;return xml(doc);
}
std::set<std::string> managedChartParts(const Entries& entries,const Json& baseline){
 std::set<std::string> names,parts;for(const auto& chart:baseline.value("charts",Json::array())){names.insert("TigerSheets:"+chart.at("id").get<std::string>());if(chart.contains("native")&&chart.at("native").contains("objectName"))names.insert(chart.at("native").at("objectName").get<std::string>());}for(const auto& [part,bytes]:entries){if(!part.starts_with("xl/drawings/")||!part.ends_with(".xml")||!entries.contains(relationshipPart(part)))continue;pugi::xml_document drawing,relations;parse(drawing,bytes);parse(relations,entries.at(relationshipPart(part)));for(auto anchor:drawing.document_element().children()){const auto frame=child(anchor,"graphicFrame"),properties=child(child(frame,"nvGraphicFramePr"),"cNvPr");if(!names.contains(properties.attribute("name").value()))continue;const auto object=child(child(child(frame,"graphic"),"graphicData"),"chart");const auto id=attrLocal(object,"id");for(auto relation:relations.document_element().children())if(id==relation.attribute("Id").value()&&std::string(relation.attribute("Type").value()).ends_with("/chart")&&std::string(relation.attribute("TargetMode").value())!="External")parts.insert(targetPath(part,relation.attribute("Target").value()));}}
 return parts;
}
void clearManagedChartAnchors(Entries& entries,const Json& baseline){
 const auto charts=baseline.value("charts",Json::array());if(!charts.is_array()||charts.empty())return;auto removedParts=managedChartParts(entries,baseline);std::set<std::string> names;for(const auto& chart:charts){names.insert("TigerSheets:"+chart.at("id").get<std::string>());if(chart.contains("native")&&chart.at("native").contains("objectName"))names.insert(chart.at("native").at("objectName").get<std::string>());}
 for(auto& [part,bytes]:entries){if(!part.starts_with("xl/drawings/")||!part.ends_with(".xml"))continue;pugi::xml_document doc;parse(doc,bytes);auto root=doc.document_element();bool changed=false;std::set<std::string> removedRelations;
  for(auto anchor=root.first_child();anchor;){const auto next=anchor.next_sibling();const auto frame=child(anchor,"graphicFrame"),properties=child(child(frame,"nvGraphicFramePr"),"cNvPr");if(names.contains(properties.attribute("name").value())){const auto chart=child(child(child(frame,"graphic"),"graphicData"),"chart");removedRelations.insert(attrLocal(chart,"id"));root.remove_child(anchor);changed=true;}anchor=next;}
  if(!changed)continue;
  const auto relPart=relationshipPart(part);if(entries.contains(relPart)){pugi::xml_document rels;parse(rels,entries.at(relPart));std::function<bool(pugi::xml_node,const std::string&)> used=[&](pugi::xml_node node,const std::string& id){if(attrLocal(node,"id")==id)return true;for(const auto child:node.children())if(used(child,id))return true;return false;};auto relationships=rels.document_element();for(auto rel=relationships.first_child();rel;){const auto next=rel.next_sibling();const std::string id=rel.attribute("Id").value();if(removedRelations.contains(id)&&!used(root,id))relationships.remove_child(rel);rel=next;}entries[relPart]=xml(rels);}
  bytes=xml(doc);
 }
 for(const auto& [part,bytes]:entries)if(part.ends_with(".rels")){pugi::xml_document relations;parse(relations,bytes);const std::filesystem::path relationship(part);const auto base=(relationship.parent_path().parent_path()/relationship.stem()).generic_string();for(auto relation:relations.document_element().children())if(std::string(relation.attribute("Type").value()).ends_with("/chart")&&std::string(relation.attribute("TargetMode").value())!="External")removedParts.erase(targetPath(base,relation.attribute("Target").value()));}
 if(!removedParts.empty()){pugi::xml_document types;parse(types,entries.at("[Content_Types].xml"));auto root=types.document_element();for(const auto& part:removedParts){entries.erase(part);entries.erase(relationshipPart(part));for(auto node=root.first_child();node;){const auto next=node.next_sibling();if("/"+part==node.attribute("PartName").value())root.remove_child(node);node=next;}}entries["[Content_Types].xml"]=xml(types);}
}
void nativeCharts(Entries& entries,const Json& book,const std::map<std::string,std::string>& parts,const std::string& path){
 if(!book.contains("charts")||!book["charts"].is_array())return;
 int index=0;
 for(const auto& chart:book["charts"]){++index;const auto id=chart.at("sheet").get<std::string>();const Json* sheet=nullptr;for(const auto& candidate:book.at("sheets"))if(candidate.at("id")==id)sheet=&candidate;if(!sheet)throw std::runtime_error("Chart worksheet was not found");
  auto chartPart="xl/charts/tigersheets-chart"+std::to_string(index)+".xml";while(entries.contains(chartPart))chartPart.insert(chartPart.size()-4,"x");entries[chartPart]=chart.contains("pivotId")?nativePivotChartXml(book,chart,path):chartXml(chart,*sheet);addType(entries,chartPart,"application/vnd.openxmlformats-officedocument.drawingml.chart+xml");
  const auto worksheetPart=parts.at(id),sheetRelationsPart=relationshipPart(worksheetPart);pugi::xml_document worksheet,sheetRelations;parse(worksheet,entries.at(worksheetPart));relationDocument(sheetRelations,entries,sheetRelationsPart);auto root=worksheet.document_element();auto drawing=child(root,"drawing");std::string drawingPart;
  if(drawing){const auto rid=attrLocal(drawing,"id");for(auto rel:sheetRelations.document_element().children())if(rid==rel.attribute("Id").value())drawingPart=targetPath(worksheetPart,rel.attribute("Target").value());if(drawingPart.empty()||!entries.contains(drawingPart))throw std::runtime_error("Original worksheet drawing is missing");}
  else{drawingPart="xl/drawings/tigersheets-drawing"+std::to_string(index)+".xml";while(entries.contains(drawingPart))drawingPart.insert(drawingPart.size()-4,"x");const auto rid=uniqueRelation(sheetRelations.document_element(),"tigerDrawing");auto rel=sheetRelations.document_element().append_child(prefixed(sheetRelations.document_element(),"Relationship").c_str());rel.append_attribute("Id")=rid.c_str();rel.append_attribute("Type")=(std::string(relNs)+"/drawing").c_str();rel.append_attribute("Target")=std::filesystem::path(drawingPart).lexically_relative(std::filesystem::path(worksheetPart).parent_path()).generic_string().c_str();drawing=root.append_child(prefixed(root,"drawing").c_str());drawing.append_attribute("r:id")=rid.c_str();if(!root.attribute("xmlns:r"))root.append_attribute("xmlns:r")=relNs;}
  pugi::xml_document picture,pictureRelations;if(entries.contains(drawingPart))parse(picture,entries.at(drawingPart));else{auto dr=picture.append_child("xdr:wsDr");dr.append_attribute("xmlns:xdr")="http://schemas.openxmlformats.org/drawingml/2006/spreadsheetDrawing";dr.append_attribute("xmlns:a")="http://schemas.openxmlformats.org/drawingml/2006/main";}
  auto dr=picture.document_element();if(!dr.attribute("xmlns:xdr"))dr.append_attribute("xmlns:xdr")="http://schemas.openxmlformats.org/drawingml/2006/spreadsheetDrawing";if(!dr.attribute("xmlns:a"))dr.append_attribute("xmlns:a")="http://schemas.openxmlformats.org/drawingml/2006/main";std::uint64_t maximumObjectId=0;std::function<void(pugi::xml_node)> objectIds=[&](pugi::xml_node node){if(local(node.name())=="cNvPr")maximumObjectId=std::max(maximumObjectId,static_cast<std::uint64_t>(node.attribute("id").as_ullong()));for(auto nested:node.children())objectIds(nested);};objectIds(dr);if(maximumObjectId>=4294967295ULL)throw std::runtime_error("Drawing object IDs are exhausted");const auto drawingName="TigerSheets:"+chart.at("id").get<std::string>();for(auto anchor=dr.first_child();anchor;){const auto next=anchor.next_sibling();const auto props=child(child(child(anchor,"graphicFrame"),"nvGraphicFramePr"),"cNvPr");if(drawingName==props.attribute("name").value())dr.remove_child(anchor);anchor=next;}
  const auto drawingRelsPart=relationshipPart(drawingPart);relationDocument(pictureRelations,entries,drawingRelsPart);const auto rid=uniqueRelation(pictureRelations.document_element(),"tigerChart");auto rel=pictureRelations.document_element().append_child(prefixed(pictureRelations.document_element(),"Relationship").c_str());rel.append_attribute("Id")=rid.c_str();rel.append_attribute("Type")=(std::string(relNs)+"/chart").c_str();rel.append_attribute("Target")=std::filesystem::path(chartPart).lexically_relative(std::filesystem::path(drawingPart).parent_path()).generic_string().c_str();
  const auto anchorData=chart.value("anchor",Json::object());auto anchor=dr.append_child("xdr:oneCellAnchor"),from=anchor.append_child("xdr:from");from.append_child("xdr:col").text().set(std::max(0,anchorData.value("col",6)-1));from.append_child("xdr:colOff").text().set(static_cast<long long>(anchorData.value("offsetX",0))*9525);from.append_child("xdr:row").text().set(std::max(0,anchorData.value("row",2)-1));from.append_child("xdr:rowOff").text().set(static_cast<long long>(anchorData.value("offsetY",0))*9525);auto extent=anchor.append_child("xdr:ext");extent.append_attribute("cx")=static_cast<long long>(anchorData.value("width",560))*9525;extent.append_attribute("cy")=static_cast<long long>(anchorData.value("height",300))*9525;auto frame=anchor.append_child("xdr:graphicFrame");frame.append_attribute("macro")="";auto nv=frame.append_child("xdr:nvGraphicFramePr"),props=nv.append_child("xdr:cNvPr");props.append_attribute("id")=static_cast<unsigned>(maximumObjectId+1);props.append_attribute("name")=drawingName.c_str();nv.append_child("xdr:cNvGraphicFramePr");auto xfrm=frame.append_child("xdr:xfrm");auto off=xfrm.append_child("a:off");off.append_attribute("x")=0;off.append_attribute("y")=0;auto ext=xfrm.append_child("a:ext");ext.append_attribute("cx")=0;ext.append_attribute("cy")=0;auto graphic=frame.append_child("a:graphic").append_child("a:graphicData");graphic.append_attribute("uri")="http://schemas.openxmlformats.org/drawingml/2006/chart";auto object=graphic.append_child("c:chart");object.append_attribute("xmlns:c")="http://schemas.openxmlformats.org/drawingml/2006/chart";object.append_attribute("xmlns:r")=relNs;object.append_attribute("r:id")=rid.c_str();anchor.append_child("xdr:clientData");
  entries[drawingPart]=xml(picture);entries[drawingRelsPart]=xml(pictureRelations);entries[worksheetPart]=xml(worksheet);entries[sheetRelationsPart]=xml(sheetRelations);addType(entries,drawingPart,"application/vnd.openxmlformats-officedocument.drawing+xml");
 }
}
void readManagedChartOffsets(Json& book,const Entries& entries,const std::map<std::string,std::string>& parts){
 if(!book.contains("charts")||!book["charts"].is_array())return;
 for(auto& chart:book["charts"]){
  if(!chart.is_object()||!chart.contains("id")||!chart["id"].is_string()||!chart.contains("sheet")||!chart["sheet"].is_string())continue;
  const auto worksheetPart=parts.find(chart["sheet"].get<std::string>());if(worksheetPart==parts.end())continue;
  const auto sheetRelationsPart=relationshipPart(worksheetPart->second);if(!entries.contains(sheetRelationsPart))continue;
  pugi::xml_document worksheet,relations;parse(worksheet,entries.at(worksheetPart->second));parse(relations,entries.at(sheetRelationsPart));
  const auto drawing=child(worksheet.document_element(),"drawing");if(!drawing)continue;
  const auto relationId=attrLocal(drawing,"id");std::string drawingPart;
  for(auto rel:relations.document_element().children())if(relationId==rel.attribute("Id").value()&&std::string(rel.attribute("TargetMode").value())!="External")drawingPart=targetPath(worksheetPart->second,rel.attribute("Target").value());
  if(drawingPart.empty()||!entries.contains(drawingPart))continue;
  pugi::xml_document picture;parse(picture,entries.at(drawingPart));
  for(auto anchor:picture.document_element().children()){
   if(local(anchor.name())!="oneCellAnchor")continue;
   const auto properties=child(child(child(anchor,"graphicFrame"),"nvGraphicFramePr"),"cNvPr");
   if("TigerSheets:"+chart["id"].get<std::string>()!=properties.attribute("name").value())continue;
   const auto from=child(anchor,"from");if(!from)continue;
   if(chart.contains("anchor")&&!chart["anchor"].is_object())continue;
   if(!chart.contains("anchor"))chart["anchor"]=Json::object();
   for(const auto& [nativeField,field]:{std::pair{"colOff","offsetX"},std::pair{"rowOff","offsetY"}}){
    const auto node=child(from,nativeField);std::int64_t emu=0;
    if(node){
     std::string_view value=node.text().as_string();while(!value.empty()&&std::isspace(static_cast<unsigned char>(value.front())))value.remove_prefix(1);while(!value.empty()&&std::isspace(static_cast<unsigned char>(value.back())))value.remove_suffix(1);
     const auto parsed=std::from_chars(value.data(),value.data()+value.size(),emu);
     if(value.empty()||parsed.ec!=std::errc()||parsed.ptr!=value.data()+value.size()||emu<0||emu>9525000000LL)throw std::runtime_error(std::string("Native chart anchor ")+field+" is outside supported pixel bounds");
    }
    chart["anchor"][field]=static_cast<int>((emu+4762)/9525);
   }
   break;
  }
 }
}
void nativeMetadata(Entries& entries,const Json& book,const std::map<std::string,std::string>& parts){
 pugi::xml_document doc;parse(doc,entries.at("xl/workbook.xml"));auto root=doc.document_element();if(book.contains("names")){auto names=child(root,"definedNames");if(!names)names=root.append_child(prefixed(root,"definedNames").c_str());for(auto node=names.first_child();node;){const auto next=node.next_sibling();if(!node.attribute("localSheetId")&&!std::string(node.attribute("name").value()).starts_with("_xlnm."))names.remove_child(node);node=next;}for(const auto& [name,value]:book["names"].items()){if(!value.is_string())continue;auto node=names.append_child(prefixed(root,"definedName").c_str());node.append_attribute("name")=name.c_str();auto formula=value.get<std::string>();if(formula.starts_with('='))formula.erase(0,1);node.text().set(formula.c_str());}}
 if(book.contains("settings")){const auto& settings=book["settings"];auto properties=child(root,"workbookPr");if(!properties)properties=root.prepend_child(prefixed(root,"workbookPr").c_str());if(settings.contains("date1904"))attribute(properties,"date1904")=settings["date1904"].get<bool>();auto calc=ensure(root,"calcPr");if(settings.contains("calcMode"))attribute(calc,"calcMode")=settings["calcMode"].get<std::string>().c_str();if(settings.contains("iterationEnabled"))attribute(calc,"iterate")=settings["iterationEnabled"].get<bool>();if(settings.contains("maxIterations"))attribute(calc,"iterateCount")=settings["maxIterations"].get<int>();if(settings.contains("maxChange"))attribute(calc,"iterateDelta")=settings["maxChange"].get<double>();}entries["xl/workbook.xml"]=xml(doc);
 Json metadata=Json::object(),fingerprints=Json::object();for(const auto* key:{"charts","pivots","scenarios","financialModels","links","vba"})if(book.contains(key)){if(std::string(key)=="vba")metadata[key]={{"enabled",book[key].value("enabled",false)}};else metadata[key]=book[key];}if(metadata.empty())return;for(const auto& [part,bytes]:entries)if(part.starts_with("xl/"))fingerprints[part]=hashBytes(bytes);Json identities=Json::object();for(const auto& [id,part]:parts)identities[part]=id;entries["tigersheets/metadata.json"]=Json{{"version",1},{"metadata",metadata},{"fingerprints",fingerprints},{"sheetIdentities",std::move(identities)}}.dump();addType(entries,"tigersheets/metadata.json","application/json");
}

bool macroExtension(const std::string& path){auto ext=std::filesystem::path(path).extension().string();std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return ext==".xlsm";}
void nativeFileType(Entries& entries,const std::string& path){pugi::xml_document document;parse(document,entries.at("[Content_Types].xml"));for(auto node:document.document_element().children())if(std::string(node.attribute("PartName").value())=="/xl/workbook.xml")attribute(node,"ContentType")=macroExtension(path)?"application/vnd.ms-excel.sheet.macroEnabled.main+xml":"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml";entries["[Content_Types].xml"]=xml(document);}

bool sameContent(Json a,Json b){a.erase("revision");b.erase("revision");return a==b;}
std::string prefixed(pugi::xml_node node,const char* name){while(node.parent()&&node.parent().type()!=pugi::node_document)node=node.parent();const std::string rootName=node.name();const auto colon=rootName.find(':');return colon==std::string::npos?name:rootName.substr(0,colon+1)+name;}
void setCell(pugi::xml_node cell,const std::string& input){
 for(auto current=cell.first_child();current;){const auto next=current.next_sibling();const auto name=local(current.name());if(name=="f"||name=="v"||name=="is")cell.remove_child(current);current=next;}
 cell.remove_attribute("t");if(input.empty())return;
 if(input.front()=='='){cell.append_child(prefixed(cell,"f").c_str()).text().set(input.substr(1).c_str());return;}
 const bool escaped=input.front()=='\'';const auto textValue=escaped?input.substr(1):input;auto upper=textValue;std::transform(upper.begin(),upper.end(),upper.begin(),[](unsigned char c){return static_cast<char>(std::toupper(c));});
 if(!escaped&&(upper=="TRUE"||upper=="FALSE")){cell.append_attribute("t")="b";cell.append_child(prefixed(cell,"v").c_str()).text().set(upper=="TRUE"?1:0);return;}
 double number=0;const auto parsed=std::from_chars(input.data(),input.data()+input.size(),number);
 if(!escaped&&parsed.ec==std::errc()&&parsed.ptr==input.data()+input.size()&&std::isfinite(number)){cell.append_child(prefixed(cell,"v").c_str()).text().set(input.c_str());return;}
 cell.append_attribute("t")="inlineStr";auto text=cell.append_child(prefixed(cell,"is").c_str()).append_child(prefixed(cell,"t").c_str());text.append_attribute("xml:space")="preserve";text.text().set(textValue.c_str());
}
void validateXmlString(const std::string& text,const std::string& context){
 for(std::size_t pos=0;pos<text.size();){
  const auto first=static_cast<unsigned char>(text[pos]);std::uint32_t scalar=0;std::size_t length=0;
  if(first<0x80){scalar=first;length=1;}
  else if(first>=0xc2&&first<=0xdf){scalar=first&0x1fU;length=2;}
  else if(first>=0xe0&&first<=0xef){scalar=first&0x0fU;length=3;}
  else if(first>=0xf0&&first<=0xf4){scalar=first&0x07U;length=4;}
  else throw std::runtime_error("Malformed UTF-8 in "+context);
  if(text.size()-pos<length)throw std::runtime_error("Truncated UTF-8 in "+context);
  for(std::size_t offset=1;offset<length;++offset){const auto next=static_cast<unsigned char>(text[pos+offset]);if((next&0xc0U)!=0x80U)throw std::runtime_error("Malformed UTF-8 continuation in "+context);scalar=(scalar<<6U)|(next&0x3fU);}
  if((length==2&&scalar<0x80)||(length==3&&scalar<0x800)||(length==4&&scalar<0x10000))throw std::runtime_error("Overlong UTF-8 in "+context);
  if(scalar<0x20&&scalar!=9&&scalar!=10&&scalar!=13)throw std::runtime_error("Unsupported XML control character (byte "+std::to_string(scalar)+") in "+context);
  if(!((scalar==9||scalar==10||scalar==13)||(scalar>=0x20&&scalar<=0xd7ff)||(scalar>=0xe000&&scalar<=0xfffd)||(scalar>=0x10000&&scalar<=0x10ffff)))throw std::runtime_error("Invalid XML Unicode scalar value in "+context);
  pos+=length;
 }
}
void validateAuthoredCellXml(const Json& book){
 for(const auto& sheet:book.at("sheets")){
  const auto name=sheet.at("name").get<std::string>();validateXmlString(name,"worksheet name");
  for(const auto& cell:sheet.at("cells")){
   const auto context=name+"!"+column(cell.at("col").get<int>())+std::to_string(cell.at("row").get<int>());validateXmlString(cell.at("input").get<std::string>(),"cell input at "+context);
   if(cell.contains("style"))for(const auto* field:{"fontFamily","numberFormat","alignment","fill","color"})if(cell["style"].contains(field)){
    const auto text=cell["style"][field].get<std::string>();validateXmlString(text,std::string("cell style ")+field+" at "+context);
    if(std::string_view(field)=="fill"||std::string_view(field)=="color"){
     std::string_view rgb(text);if(rgb.starts_with('#'))rgb.remove_prefix(1);const bool valid=text.empty()||((rgb.size()==6||rgb.size()==8)&&std::all_of(rgb.begin(),rgb.end(),[](char ch){return (ch>='0'&&ch<='9')||(ch>='a'&&ch<='f')||(ch>='A'&&ch<='F');}));
     if(!valid)throw std::runtime_error(std::string("Cell style ")+field+" requires 6-digit RGB or 8-digit ARGB hexadecimal text (optional #) at "+context);
    }
   }
  }
 }
 if(book.contains("names"))for(const auto& [name,value]:book["names"].items())if(value.is_string()){validateXmlString(name,"defined-name key");validateXmlString(value.get<std::string>(),"defined-name formula "+name);}
 if(book.contains("charts"))for(const auto& chart:book["charts"])for(const auto* field:{"id","title"})if(chart.contains(field))validateXmlString(chart[field].get<std::string>(),std::string("chart ")+field);
 if(book.contains("pivots")){std::function<void(const Json&)> validatePivotText=[&](const Json& value){if(value.is_string())validateXmlString(value.get<std::string>(),"native pivot metadata/cache");else if(value.is_array()||value.is_object())for(const auto& child:value)validatePivotText(child);};for(const auto& pivot:book.at("pivots"))if(pivot.value("version",0)==1)validatePivotText(pivot);}
 if(book.contains("settings")&&book["settings"].contains("calcMode"))validateXmlString(book["settings"]["calcMode"].get<std::string>(),"calculation mode");
}
std::string textInput(std::string value){auto upper=value;std::transform(upper.begin(),upper.end(),upper.begin(),[](unsigned char c){return static_cast<char>(std::toupper(c));});if(value.empty()||upper=="TRUE"||upper=="FALSE"||std::isdigit(static_cast<unsigned char>(value[0]))||value.find_first_of("'=+-.@$\xC2\xE2\xC2")==0)return "'"+value;return value;}

std::string patchSheet(const std::string& bytes,const Json& before,const Json& after){
 pugi::xml_document doc;parse(doc,bytes);auto root=doc.document_element();auto data=child(root,"sheetData");if(!data)data=root.append_child(prefixed(root,"sheetData").c_str());
 std::map<std::pair<int,int>,Json> oldCells,newCells;for(const auto& cell:before.at("cells"))oldCells[{cell.at("row").get<int>(),cell.at("col").get<int>()}]=cell;for(const auto& cell:after.at("cells"))newCells[{cell.at("row").get<int>(),cell.at("col").get<int>()}]=cell;
 std::set<std::pair<int,int>> all;for(const auto& [key,value]:oldCells)all.insert(key);for(const auto& [key,value]:newCells)all.insert(key);bool sharedFormulaChanged=false;
 for(const auto& key:all){const auto prior=oldCells.find(key),next=newCells.find(key);if(prior!=oldCells.end()&&next!=newCells.end()&&prior->second==next->second)continue;
  const bool sameInput=prior!=oldCells.end()&&next!=newCells.end()&&prior->second.at("input")==next->second.at("input");if(sameInput&&next->second.contains("style"))continue;
  pugi::xml_node row;for(auto current:data.children())if(current.attribute("r").as_int()==key.first){row=current;break;}
  if(!row){for(auto current:data.children())if(current.attribute("r").as_int()>key.first){row=data.insert_child_before(prefixed(root,"row").c_str(),current);break;}if(!row)row=data.append_child(prefixed(root,"row").c_str());row.append_attribute("r")=key.first;}
  const auto ref=column(key.second)+std::to_string(key.first);pugi::xml_node cell;for(auto current:row.children())if(current.attribute("r").value()==ref){cell=current;break;}
  if(!cell){for(auto current:row.children())if(coordinate(current.attribute("r").value()).second>key.second){cell=row.insert_child_before(prefixed(root,"c").c_str(),current);break;}if(!cell)cell=row.append_child(prefixed(root,"c").c_str());cell.append_attribute("r")=ref.c_str();}
  if(next!=newCells.end()&&!next->second.contains("style"))cell.remove_attribute("s");if(sameInput)continue;
  if(std::string(child(cell,"f").attribute("t").value())=="shared")sharedFormulaChanged=true;
  if(next==newCells.end()){if(child(cell,"extLst"))throw std::runtime_error("Cannot remove a cell containing unsupported extension data");row.remove_child(cell);continue;}setCell(cell,next->second.at("input").get<std::string>());
 }
 if(sharedFormulaChanged)for(auto row:data.children())for(auto cell:row.children()){const auto formula=child(cell,"f");if(std::string(formula.attribute("t").value())=="shared"){const auto key=coordinate(cell.attribute("r").value());const auto value=newCells.find(key);if(value!=newCells.end())setCell(cell,value->second.at("input").get<std::string>());}}
 return xml(doc);
}
void validateStructuralPreservation(const WorkbookPackage& source,const Json& book){
 for(const auto& after:book.at("sheets"))for(const auto& before:source.baseline.at("sheets"))if(after.at("id")==before.at("id")){if(after.value("structuralRevision",0ULL)!=before.value("structuralRevision",0ULL))preflightWorksheetStructure(source,after.at("id"));if(after.value("mergedRanges",Json::array())!=before.value("mergedRanges",Json::array()))preflightWorksheetLayout(source,after.at("id"),after.value("mergedRanges",Json::array()));}

 const auto& before=source.baseline.at("sheets");const auto& after=book.at("sheets");bool namesChanged=false,identityChanged=before.size()!=after.size();
 for(std::size_t index=0;index<before.size();++index){const auto& sheet=before[index];const auto found=std::find_if(after.begin(),after.end(),[&](const auto& item){return item.at("id")==sheet.at("id");});if(found==after.end()){namesChanged=true;identityChanged=true;continue;}if(found->at("name")!=sheet.at("name"))namesChanged=true;if(static_cast<std::size_t>(std::distance(after.begin(),found))!=index)identityChanged=true;}
 if(!namesChanged&&!identityChanged)return;
 const auto managedPivots=ownedNativePivotParts(source);if(source.baseline.contains("opaquePivots")&&!source.baseline.at("opaquePivots").empty())throw std::runtime_error("Cannot save worksheet structural changes while imported pivots are opaque.");
 const auto managedCharts=managedChartParts(source.entries,source.baseline);
 for(const auto& [part,bytes]:source.entries){
  if(part.starts_with("xl/tables/")||(part.starts_with("xl/pivotTables/")&&!managedPivots.contains(part))||(part.starts_with("xl/pivotCache/")&&!managedPivots.contains(part))||part.starts_with("xl/externalLinks/")||part=="xl/connections.xml")throw std::runtime_error("Cannot save worksheet structural changes while imported table, pivot or external-reference parts are opaque.");
  if(namesChanged&&part.starts_with("xl/worksheets/")&&part.ends_with(".xml")){pugi::xml_document doc;parse(doc,bytes);std::function<bool(pugi::xml_node)> opaqueFormula=[&](pugi::xml_node node){const auto tag=local(node.name());if(tag=="formula"||tag=="formula1"||tag=="formula2"||(tag=="f"&&local(node.parent().name())!="c"))return true;for(const auto item:node.children())if(opaqueFormula(item))return true;return false;};if(opaqueFormula(doc.document_element()))throw std::runtime_error("Cannot save worksheet rename/deletion while validation or conditional-format formulas are opaque.");}
  if(part=="xl/workbook.xml"){pugi::xml_document doc;parse(doc,bytes);for(const auto name:child(doc.document_element(),"definedNames").children())if(name.attribute("localSheetId")&&!supportedNativePrintName(source,name.attribute("name").value(),name.attribute("localSheetId").value(),name.text().as_string()))throw std::runtime_error("Cannot save worksheet structural changes while worksheet-scoped defined names are preserved but not editable.");}
  if(part.starts_with("xl/charts/")&&part.ends_with(".xml")&&!managedCharts.contains(part))throw std::runtime_error("Cannot save worksheet rename/deletion while imported chart references are opaque. Keep worksheet identities unchanged to preserve this package.");
 }
}
Entries preservedEntries(const WorkbookPackage& source,const Json& book,std::map<std::string,std::string>& parts){
 validateStructuralPreservation(source,book);
 auto entries=source.entries;parts=source.sheetParts;pugi::xml_document document,relations,content;parse(document,entries.at("xl/workbook.xml"));parse(relations,entries.at("xl/_rels/workbook.xml.rels"));parse(content,entries.at("[Content_Types].xml"));auto root=document.document_element(),sheets=child(root,"sheets");
 std::map<std::string,Json> old;for(const auto& sheet:source.baseline.at("sheets"))old[sheet.at("id").get<std::string>()]=sheet;
 auto scopedNames=child(root,"definedNames");for(auto name=scopedNames.first_child();name;){auto next=name.next_sibling();if(name.attribute("localSheetId")&&supportedNativePrintName(source,name.attribute("name").value(),name.attribute("localSheetId").value(),name.text().as_string())){const auto previous=name.attribute("localSheetId").as_uint();const auto& previousSheet=source.baseline.at("sheets").at(previous);const auto found=std::find_if(book.at("sheets").begin(),book.at("sheets").end(),[&](const Json& value){return value.at("id")==previousSheet.at("id");});if(found==book.at("sheets").end())scopedNames.remove_child(name);else{const auto index=static_cast<unsigned>(std::distance(book.at("sheets").begin(),found));name.attribute("localSheetId")=index;const std::string formula=name.text().as_string();std::string qualifier="'";for(char ch:found->at("name").get<std::string>()){qualifier+=ch;if(ch=='\'')qualifier+='\'';}qualifier+="'!";name.text().set((qualifier+formula.substr(formula.rfind('!')+1)).c_str());}}name=next;}
 std::set<std::string> retainedIds;for(const auto& sheet:book.at("sheets"))retainedIds.insert(sheet.at("id").get<std::string>());
 for(const auto& [id,oldSheet]:old)if(!retainedIds.contains(id)){
  const auto oldPart=parts.at(id);for(auto node=sheets.first_child();node;){const auto next=node.next_sibling();const auto rid=attrLocal(node,"id");for(auto rel=relations.document_element().first_child();rel;){const auto nextRel=rel.next_sibling();if(rid==rel.attribute("Id").value()&&targetPath("xl/workbook.xml",rel.attribute("Target").value())==oldPart){relations.document_element().remove_child(rel);sheets.remove_child(node);break;}rel=nextRel;}node=next;}parts.erase(id);
 }
 int maximumId=0;for(auto sheet:sheets.children())maximumId=std::max(maximumId,sheet.attribute("sheetId").as_int());
 for(const auto& sheet:book.at("sheets")){
  const auto id=sheet.at("id").get<std::string>();const auto existing=parts.find(id);
  if(existing!=parts.end()){
   pugi::xml_node node;for(auto current:sheets.children()){const auto relationId=attrLocal(current,"id");for(auto rel:relations.document_element().children())if(relationId==rel.attribute("Id").value()&&targetPath("xl/workbook.xml",rel.attribute("Target").value())==existing->second)node=current;}
   if(!node)throw std::runtime_error("Cannot locate worksheet in original package");
   node.attribute("name")=sheet.at("name").get<std::string>().c_str();sheets.append_move(node);
   if(!old.contains(id))throw std::runtime_error("Missing editable worksheet baseline");
   if(old.at(id)!=sheet)entries[existing->second]=patchSheet(entries.at(existing->second),old.at(id),sheet);
  }else{
   ++maximumId;auto target="worksheets/tigersheets-"+std::to_string(maximumId)+".xml";while(entries.contains("xl/"+target)){++maximumId;target="worksheets/tigersheets-"+std::to_string(maximumId)+".xml";}
   auto relationId="tigerSheet"+std::to_string(maximumId);bool used=true;while(used){used=false;for(auto rel:relations.document_element().children())if(relationId==rel.attribute("Id").value()){relationId+='x';used=true;}}
   auto node=sheets.append_child(prefixed(root,"sheet").c_str());node.append_attribute("name")=sheet.at("name").get<std::string>().c_str();node.append_attribute("sheetId")=maximumId;node.append_attribute("r:id")=relationId.c_str();if(!root.attribute("xmlns:r"))root.append_attribute("xmlns:r")=relNs;
   auto rel=relations.document_element().append_child("Relationship");rel.append_attribute("Id")=relationId.c_str();rel.append_attribute("Type")=(std::string(relNs)+"/worksheet").c_str();rel.append_attribute("Target")=target.c_str();
   auto over=content.document_element().append_child("Override");over.append_attribute("PartName")=("/xl/"+target).c_str();over.append_attribute("ContentType")="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml";parts[id]="xl/"+target;entries["xl/"+target]=makeWorksheet(sheet);
  }
 }
 auto calc=child(root,"calcPr");if(!calc)calc=root.append_child(prefixed(root,"calcPr").c_str());auto flag=calc.attribute("fullCalcOnLoad");if(!flag)flag=calc.append_attribute("fullCalcOnLoad");flag=1;
 entries["xl/workbook.xml"]=xml(document);entries["xl/_rels/workbook.xml.rels"]=xml(relations);entries["[Content_Types].xml"]=xml(content);return entries;
}
}
void preflightWorksheetStructure(const WorkbookPackage& source,const std::string& sheetId){
 const auto found=source.sheetParts.find(sheetId);if(found==source.sheetParts.end())return;const auto worksheet=source.entries.find(found->second);if(worksheet==source.entries.end())throw std::runtime_error("Original worksheet part is missing");
 for(const auto& [part,bytes]:source.entries){
  if(part.starts_with("xl/tables/")||part.starts_with("xl/pivotTables/")||part.starts_with("xl/pivotCache/")||part.starts_with("xl/externalLinks/")||part.starts_with("xl/charts/")||part=="xl/connections.xml")throw std::runtime_error("Cannot insert or delete rows/columns while imported tables, charts, pivots or external-reference parts need preservation");
  if(part=="xl/workbook.xml"){pugi::xml_document doc;parse(doc,bytes);for(const auto node:child(doc.document_element(),"definedNames").children())if(node.attribute("localSheetId"))throw std::runtime_error("Cannot insert or delete rows/columns while native worksheet-scoped names are preserved");}
  if(part.starts_with("xl/worksheets/")&&part.ends_with(".xml")){pugi::xml_document referenced;parse(referenced,bytes);std::function<bool(pugi::xml_node)> opaque=[&](pugi::xml_node node){const auto tag=local(node.name());if(tag=="formula"||tag=="formula1"||tag=="formula2"||(tag=="f"&&local(node.parent().name())!="c"))return true;for(const auto nested:node.children())if(opaque(nested))return true;return false;};if(opaque(referenced.document_element()))throw std::runtime_error("Cannot insert or delete rows/columns while opaque validation or conditional formulas may reference this worksheet");}
 }
 std::set<int> richSharedStrings;if(source.entries.contains("xl/sharedStrings.xml")){pugi::xml_document shared;parse(shared,source.entries.at("xl/sharedStrings.xml"));int index=0;for(const auto item:shared.document_element().children())if(local(item.name())=="si"){if(child(item,"r")||child(item,"rPh"))richSharedStrings.insert(index);++index;}}
 pugi::xml_document doc;parse(doc,worksheet->second);const auto root=doc.document_element();const std::set<std::string> supported={"sheetPr","dimension","sheetViews","sheetFormatPr","cols","sheetData","sheetCalcPr","mergeCells","phoneticPr","printOptions","pageMargins","pageSetup","headerFooter"};
 for(const auto node:root.children())if(!supported.contains(local(node.name())))throw std::runtime_error("Cannot insert or delete rows/columns while native "+local(node.name())+" references are not editable");
 for(const auto view:child(root,"sheetViews").children())if(child(view,"pane"))throw std::runtime_error("Unfreeze or remove the imported split pane before inserting or deleting rows/columns");
 for(const auto row:child(root,"sheetData").children()){for(const auto attr:row.attributes())if(local(attr.name())!="r"&&local(attr.name())!="ht"&&local(attr.name())!="customHeight"&&local(attr.name())!="hidden"&&local(attr.name())!="spans")throw std::runtime_error("Cannot rebase unsupported native row formatting or grouping");for(const auto cell:row.children()){
  const auto formula=child(cell,"f");const std::string type=formula.attribute("t").value(),valueType=cell.attribute("t").value();if(child(cell,"extLst")||(!type.empty()&&type!="normal"&&type!="shared"))throw std::runtime_error("Cannot rebase native array/data-table formulas or cell extensions");
  if((!formula&&(valueType=="e"||valueType=="d"))||child(child(cell,"is"),"r")||child(child(cell,"is"),"rPh")||(valueType=="s"&&richSharedStrings.contains(child(cell,"v").text().as_int(-1))))throw std::runtime_error("Cannot rebase imported error, ISO-date or rich-text cells without preserving their native value semantics");
  for(const auto attr:cell.attributes())if(local(attr.name())!="r"&&local(attr.name())!="s"&&local(attr.name())!="t")throw std::runtime_error("Cannot rebase native cell metadata or phonetic annotations");
 }}
 for(const auto col:child(root,"cols").children())for(const auto attr:col.attributes())if(local(attr.name())!="min"&&local(attr.name())!="max"&&local(attr.name())!="width"&&local(attr.name())!="customWidth"&&local(attr.name())!="hidden")throw std::runtime_error("Cannot rebase unsupported native column formatting or grouping");
}
void preflightWorksheetLayout(const WorkbookPackage& source,const std::string& sheetId,const Json& mergedRanges){
 const auto found=source.sheetParts.find(sheetId);if(found==source.sheetParts.end())return;const Json* original=nullptr;for(const auto& sheet:source.baseline.at("sheets"))if(sheet.at("id")==sheetId){original=&sheet;break;}const auto existing=original?original->value("mergedRanges",Json::array()):Json::array();bool adding=false;for(const auto& merge:mergedRanges)if(std::find(existing.begin(),existing.end(),merge)==existing.end())adding=true;if(!adding)return;
 const auto worksheet=source.entries.find(found->second);if(worksheet==source.entries.end())throw std::runtime_error("Original worksheet part is missing");pugi::xml_document doc;parse(doc,worksheet->second);const auto root=doc.document_element();if(child(root,"tableParts"))throw std::runtime_error("Cannot merge cells on a worksheet containing an imported table whose ownership is not editable");if(child(root,"sheetProtection"))throw std::runtime_error("Cannot merge cells on an imported protected worksheet");
 if(child(root,"pivotTableParts")&&source.baseline.contains("opaquePivots")&&!source.baseline.at("opaquePivots").empty())throw std::runtime_error("Cannot merge cells on a worksheet containing an unsupported native PivotTable");
 const auto filter=child(root,"autoFilter");if(filter){const std::string ref=filter.attribute("ref").value();const auto colon=ref.find(':');const auto [r1,c1]=coordinate(ref.substr(0,colon));const auto [r2,c2]=coordinate(colon==std::string::npos?ref:ref.substr(colon+1));for(const auto& merge:mergedRanges)if(merge.at("row1")<=r2&&merge.at("row2")>=r1&&merge.at("col1")<=c2&&merge.at("col2")>=c1)throw std::runtime_error("Cannot merge cells inside an imported AutoFilter range");}
}
WorkbookPackage readWorkbook(const std::string& path){
 const auto bytes=readFile(path);WorkbookPackage result;result.entries=unzip(bytes);result.sourcePath=std::filesystem::weakly_canonical(path).string();result.sourceHash=hashBytes(bytes);
 if(!result.entries.contains("xl/workbook.xml")||!result.entries.contains("xl/_rels/workbook.xml.rels"))throw std::runtime_error("Package contains no supported Excel workbook");
 pugi::xml_document book,relationships;parse(book,result.entries.at("xl/workbook.xml"));parse(relationships,result.entries.at("xl/_rels/workbook.xml.rels"));
 std::map<std::string,std::string> targets;for(auto relation:relationships.document_element().children())if(!std::string(relation.attribute("TargetMode").value()).starts_with("External"))targets[relation.attribute("Id").value()]=targetPath("xl/workbook.xml",relation.attribute("Target").value());
 std::vector<std::string> shared;if(result.entries.contains("xl/sharedStrings.xml")){pugi::xml_document strings;parse(strings,result.entries.at("xl/sharedStrings.xml"));for(auto item:strings.document_element().children())if(local(item.name())=="si")shared.push_back(collectText(item));}
 const auto styles=readStyles(result.entries);
 result.workbook={{"revision",0},{"sheets",Json::array()}};
 for(auto sheet:child(book.document_element(),"sheets").children()){
  const auto id="sheet-"+std::string(sheet.attribute("sheetId").value());const auto part=targets.find(attrLocal(sheet,"id"));if(part==targets.end()||!result.entries.contains(part->second))throw std::runtime_error("Worksheet relationship is missing");result.sheetParts[id]=part->second;
  Json model{{"id",id},{"name",sheet.attribute("name").value()},{"cells",Json::array()}};pugi::xml_document document;parse(document,result.entries.at(part->second));
  std::map<std::string,std::tuple<int,int,std::string>> sharedFormulas;
  for(auto row:child(document.document_element(),"sheetData").children())for(auto cell:row.children()){const auto formula=child(cell,"f");if(std::string(formula.attribute("t").value())=="shared"&&std::strlen(formula.text().as_string())>0){const auto [r,c]=coordinate(cell.attribute("r").value());sharedFormulas[formula.attribute("si").value()]={r,c,formula.text().as_string()};}}
  for(auto row:child(document.document_element(),"sheetData").children())for(auto cell:row.children())if(local(cell.name())=="c"){
   const auto [r,c]=coordinate(cell.attribute("r").value());std::string input;const auto formula=child(cell,"f");if(formula){input="="+std::string(formula.text().as_string());if(std::string(formula.attribute("t").value())=="shared"&&input=="="){const auto master=sharedFormulas.find(formula.attribute("si").value());if(master==sharedFormulas.end())throw std::runtime_error("Shared formula has no master");input=shiftFormula("="+std::get<2>(master->second),r-std::get<0>(master->second),c-std::get<1>(master->second));}}else{const std::string type=cell.attribute("t").value();if(type=="inlineStr")input=collectText(child(cell,"is"));else if(type=="s"){const auto index=child(cell,"v").text().as_int(-1);if(index<0||static_cast<std::size_t>(index)>=shared.size())throw std::runtime_error("Invalid shared-string index");input=shared[static_cast<std::size_t>(index)];}else if(type=="b")input=child(cell,"v").text().as_int()==0?"FALSE":"TRUE";else input=child(cell,"v").text().as_string();}
   const std::string nativeType=cell.attribute("t").value();if(!formula&&(nativeType=="inlineStr"||nativeType=="s"||nativeType=="str"))input=textInput(std::move(input));
   Json parsedCell{{"row",r},{"col",c},{"input",input}};if(cell.attribute("s")){const auto style=cell.attribute("s").as_uint();if(style>=styles.size())throw std::runtime_error("Cell refers to missing style");parsedCell["style"]=styles[style];}model["cells"].push_back(std::move(parsedCell));
  }
  readWorksheetLayout(document.document_element(),model);readWorksheetValidations(document.document_element(),model);result.workbook["sheets"].push_back(std::move(model));
 }
 // Unsupported native list sources remain in their original XML. Only expose
 // editable rules after all worksheet names are available for resolution.
 for(auto& sheet:result.workbook["sheets"])if(sheet.contains("dataValidations")){
  auto& rules=sheet["dataValidations"];rules.erase(std::remove_if(rules.begin(),rules.end(),[&](const Json& rule){try{makeWorksheetListValidation(result.workbook,sheet.at("id").get<std::string>(),rule.at("range"),rule.at("formula1").get<std::string>(),rule.at("allowBlank").get<bool>());return false;}catch(const std::exception&){return true;}}),rules.end());if(rules.empty())sheet.erase("dataValidations");
 }
 Json names=Json::object();for(auto node:child(book.document_element(),"definedNames").children())if(!node.attribute("localSheetId"))names[node.attribute("name").value()]="="+std::string(node.text().as_string());if(!names.empty())result.workbook["names"]=std::move(names);
 const auto properties=child(book.document_element(),"workbookPr"),calc=child(book.document_element(),"calcPr");result.workbook["settings"]={{"date1904",properties.attribute("date1904").as_bool()},{"calcMode",calc.attribute("calcMode").as_string("auto")},{"iterationEnabled",calc.attribute("iterate").as_bool()},{"maxIterations",calc.attribute("iterateCount").as_int(100)},{"maxChange",calc.attribute("iterateDelta").as_double(0.001)}};
 if(result.entries.contains("tigersheets/metadata.json")){
  try{
   const auto metadata=Json::parse(result.entries.at("tigersheets/metadata.json"));bool valid=metadata.value("version",0)==1;const auto fingerprints=metadata.value("fingerprints",Json::object());for(const auto& [part,digest]:fingerprints.items())if(!result.entries.contains(part)||hashBytes(result.entries.at(part))!=digest.get<std::string>())valid=false;
   if(valid){
    // XML remains authoritative. Restore application identities only when every
    // fingerprint and the complete native worksheet-part mapping still match.
    if(metadata.contains("sheetIdentities")){
     const auto& identities=metadata.at("sheetIdentities");if(!identities.is_object()||identities.size()!=result.sheetParts.size())throw std::runtime_error("Invalid saved worksheet identity map");std::map<std::string,std::string> restored;std::set<std::string> used;
     for(const auto& [id,part]:result.sheetParts){const auto stable=identities.at(part).get<std::string>();if(stable.empty()||stable.size()>128||stable.find('\0')!=std::string::npos||!used.insert(stable).second)throw std::runtime_error("Invalid saved worksheet identity");restored[stable]=part;}
     for(auto& sheet:result.workbook.at("sheets"))sheet["id"]=identities.at(result.sheetParts.at(sheet.at("id").get<std::string>()));result.sheetParts=std::move(restored);
    }
    for(const auto& [key,value]:metadata.at("metadata").items())if(key=="charts"||key=="pivots"||key=="scenarios"||key=="financialModels"||key=="links"||key=="vba")result.workbook[key]=value;
   }
  }catch(const std::exception&){/* Optional app metadata never invalidates an otherwise readable workbook. */}
 }
 readManagedChartOffsets(result.workbook,result.entries,result.sheetParts);
 for(const auto& [part,bytes]:result.entries)if(part.ends_with("vbaProject.bin")){result.workbook["vba"]={{"enabled",false},{"modules",Json::array()},{"projectPresent",true},{"sourceAvailable",false}};break;}
 readNativePivots(result);readNativePrintSettings(result);
 if(result.workbook.contains("financialModels"))for(const auto& definition:result.workbook.at("financialModels"))if(definition.value("version",0)==1&&definition.contains("results")&&definition.contains("output")){
  const auto& range=definition.at("output");for(auto& sheet:result.workbook.at("sheets"))if(sheet.at("id")==range.at("sheet"))for(auto& cell:sheet.at("cells"))if(cell.at("row")>=range.at("row1")&&cell.at("row")<=range.at("row2")&&cell.at("col")>=range.at("col1")&&cell.at("col")<=range.at("col2"))cell["financialModelOwner"]=definition.at("id");
 }
 WorkbookSession validated(result.workbook);result.workbook=validated.snapshot();result.baseline=result.workbook;return result;
}
void writeWorkbook(const std::string& path,const Json& workbook,WorkbookPackage* source){
 WorkbookSession validated(workbook);const auto canonical=validated.snapshot();validateAuthoredCellXml(canonical);Entries entries;
 if(canonical.contains("vba")&&canonical["vba"].is_object()&&canonical["vba"].value("projectPresent",false)&&(!source||source->entries.empty()))throw std::runtime_error("Preserving an imported VBA project requires its original WorkbookPackage; the semantic workbook alone cannot retain macro bytes.");
 if(canonical.contains("vba")&&canonical["vba"].is_object()&&canonical["vba"].contains("modules")&&!canonical["vba"]["modules"].empty())throw std::runtime_error("Saving authored or edited VBA modules requires native MS-OVBA project authoring, which is not available. Original macro projects remain preserved; keep edited source in TigerSheets recovery.");
 if(source&&!source->entries.empty()){
  if(!macroExtension(path)){for(const auto& [part,bytes]:source->entries)if(part.ends_with("vbaProject.bin"))throw std::runtime_error("This workbook contains a VBA project. Save it as .xlsm to preserve its macros.");}
  const auto charts=canonical.find("charts");const bool renamedPivotCharts=std::filesystem::path(path).filename()!=std::filesystem::path(source->sourcePath).filename()&&charts!=canonical.end()&&std::any_of(charts->begin(),charts->end(),[](const Json& chart){return chart.contains("pivotId");});
  if(!renamedPivotCharts&&!source->sourcePath.empty()&&sameContent(canonical,source->baseline)&&macroExtension(path)==macroExtension(source->sourcePath)){const auto original=readFile(source->sourcePath);if(hashBytes(original)!=source->sourceHash)throw std::runtime_error("Workbook source changed outside TigerSheets; reopen it before making an unchanged copy");atomicSave(path,source,[&](const auto& temporary){std::ofstream output(temporary,std::ios::binary|std::ios::trunc);if(!output||!output.write(original.data(),static_cast<std::streamsize>(original.size())))throw std::runtime_error("Cannot copy workbook");});if(source->sourcePath!=std::filesystem::weakly_canonical(path).string()){source->sourcePath=std::filesystem::weakly_canonical(path).string();source->sourceHash=hashBytes(original);}return;}
  std::map<std::string,std::string> parts;entries=preservedEntries(*source,canonical,parts);nativeWorksheetLayouts(entries,canonical,parts,source);nativeWorksheetValidations(entries,canonical,parts,source);nativeStyles(entries,canonical,parts);writeNativePivots(entries,canonical,parts,source);clearManagedChartAnchors(entries,source->baseline);nativeCharts(entries,canonical,parts,path);writeNativePrintSettings(entries,canonical,parts);nativeMetadata(entries,canonical,parts);nativeFileType(entries,path);validatePackageBudget(entries);atomicSave(path,source,[&](const auto& temporary){writeZip(temporary,entries);});source->entries=entries;source->sheetParts=std::move(parts);source->workbook=canonical;source->baseline=canonical;source->sourcePath=std::filesystem::weakly_canonical(path).string();source->sourceHash=hashBytes(readFile(path));return;
 }
 pugi::xml_document book,rels,types;auto root=book.append_child("workbook");root.append_attribute("xmlns")=ns;root.append_attribute("xmlns:r")=relNs;auto sheets=root.append_child("sheets");auto calc=root.append_child("calcPr");calc.append_attribute("fullCalcOnLoad")=1;
 auto relationships=rels.append_child("Relationships");relationships.append_attribute("xmlns")="http://schemas.openxmlformats.org/package/2006/relationships";
 auto contentTypes=types.append_child("Types");contentTypes.append_attribute("xmlns")="http://schemas.openxmlformats.org/package/2006/content-types";
 auto extension=contentTypes.append_child("Default");extension.append_attribute("Extension")="rels";extension.append_attribute("ContentType")="application/vnd.openxmlformats-package.relationships+xml";extension=contentTypes.append_child("Default");extension.append_attribute("Extension")="xml";extension.append_attribute("ContentType")="application/xml";
 auto override=contentTypes.append_child("Override");override.append_attribute("PartName")="/xl/workbook.xml";override.append_attribute("ContentType")="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml";
 int index=0;for(const auto& sheet:canonical.at("sheets")){++index;const auto id="rId"+std::to_string(index),target="worksheets/sheet"+std::to_string(index)+".xml";auto node=sheets.append_child("sheet");node.append_attribute("name")=sheet.at("name").get<std::string>().c_str();node.append_attribute("sheetId")=index;node.append_attribute("r:id")=id.c_str();auto relation=relationships.append_child("Relationship");relation.append_attribute("Id")=id.c_str();relation.append_attribute("Type")=(std::string(relNs)+"/worksheet").c_str();relation.append_attribute("Target")=target.c_str();entries["xl/"+target]=makeWorksheet(sheet);override=contentTypes.append_child("Override");override.append_attribute("PartName")=("/xl/"+target).c_str();override.append_attribute("ContentType")="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml";}
 entries["xl/workbook.xml"]=xml(book);entries["xl/_rels/workbook.xml.rels"]=xml(rels);entries["[Content_Types].xml"]=xml(types);entries["_rels/.rels"]="<?xml version=\"1.0\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/></Relationships>";
 std::map<std::string,std::string> parts;int sheetIndex=0;for(const auto& sheet:canonical["sheets"]){++sheetIndex;parts[sheet["id"].get<std::string>()]="xl/worksheets/sheet"+std::to_string(sheetIndex)+".xml";}nativeWorksheetLayouts(entries,canonical,parts,nullptr);nativeWorksheetValidations(entries,canonical,parts,nullptr);nativeStyles(entries,canonical,parts);writeNativePivots(entries,canonical,parts);nativeCharts(entries,canonical,parts,path);writeNativePrintSettings(entries,canonical,parts);nativeMetadata(entries,canonical,parts);nativeFileType(entries,path);
 validatePackageBudget(entries);atomicSave(path,nullptr,[&](const auto& temporary){writeZip(temporary,entries);});if(source){*source=readWorkbook(path);source->sheetParts=parts;source->workbook=canonical;source->baseline=canonical;}
}
}

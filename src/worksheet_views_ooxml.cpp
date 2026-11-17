#include "tigersheets/worksheet_views_ooxml.hpp"
#include "tigersheets/references.hpp"
#include <pugixml.hpp>
#include <sstream>
#include <stdexcept>
#include <algorithm>
namespace tigersheets {
namespace {
std::string local(const char* name){const std::string text(name);const auto colon=text.find(':');return colon==std::string::npos?text:text.substr(colon+1);}
pugi::xml_node child(pugi::xml_node parent,const char* name){for(auto node:parent.children())if(local(node.name())==name)return node;return {};}
std::string qualified(pugi::xml_node root,const char* name){const std::string original(root.name());const auto colon=original.find(':');return colon==std::string::npos?name:original.substr(0,colon+1)+name;}
pugi::xml_node ensure(pugi::xml_node parent,const char* name){auto node=child(parent,name);return node?node:parent.append_child(qualified(parent,name).c_str());}
pugi::xml_attribute attr(pugi::xml_node node,const char* name){auto value=node.attribute(name);return value?value:node.append_attribute(name);}
void parse(pugi::xml_document& doc,const std::string& bytes){if(!doc.load_buffer(bytes.data(),bytes.size()))throw std::runtime_error("Cannot read native workbook view XML");}
std::string xml(const pugi::xml_document& doc){std::ostringstream out;doc.save(out,"",pugi::format_raw);return out.str();}
bool validReference(const std::string& text,bool single){const auto refs=formulaReferences("="+text);return refs.size()==1&&refs[0].start==1&&refs[0].length==text.size()&&!refs[0].external&&refs[0].sheet.empty()&&(!single||(refs[0].row1==refs[0].row2&&refs[0].col1==refs[0].col2));}
bool validSelections(const std::string& text){std::istringstream words(text);std::string word;bool any=false;while(words>>word){if(!validReference(word,false))return false;any=true;}return any;}
pugi::xml_node primaryView(pugi::xml_node root){auto views=child(root,"sheetViews");for(auto view:views.children())if(local(view.name())=="sheetView"&&view.attribute("workbookViewId").as_int()==0)return view;return {};}
}
void readNativeWorkbookViews(WorkbookPackage& package){
 if(const auto found=package.entries.find("xl/workbook.xml");found!=package.entries.end()){pugi::xml_document doc;parse(doc,found->second);const auto view=child(child(doc.document_element(),"bookViews"),"workbookView");if(view.attribute("activeTab")){const int index=view.attribute("activeTab").as_int();if(index>=0&&index<static_cast<int>(package.workbook.at("sheets").size()))package.workbook["activeSheetIndex"]=index;}}
 for(auto& sheet:package.workbook.at("sheets")){
  const auto part=package.sheetParts.find(sheet.at("id").get<std::string>());if(part==package.sheetParts.end())continue;const auto found=package.entries.find(part->second);if(found==package.entries.end())continue;pugi::xml_document doc;parse(doc,found->second);const auto view=primaryView(doc.document_element());if(!view)continue;
  Json state=Json::object();const std::string top=view.attribute("topLeftCell").value();if(validReference(top,true))state["topLeftCell"]=top;const int zoom=view.attribute("zoomScale").as_int();if(zoom>=25&&zoom<=400)state["zoomScale"]=zoom;
  auto selected=child(view,"selection");const auto pane=child(view,"pane");if(pane.attribute("activePane"))for(auto node:view.children())if(local(node.name())=="selection"&&std::string(node.attribute("pane").value())==pane.attribute("activePane").value())selected=node;
  const std::string active=selected.attribute("activeCell").value(),ranges=selected.attribute("sqref").value();if(validReference(active,true))state["activeCell"]=active;if(validSelections(ranges))state["selection"]=ranges;if(!state.empty())sheet["nativeView"]=std::move(state);
 }
}
void writeNativeWorkbookViews(std::map<std::string,std::string>& entries,const Json& workbook,const std::map<std::string,std::string>& parts){
 if(workbook.contains("activeSheetIndex")){const auto& value=workbook.at("activeSheetIndex");if(!value.is_number_integer()||value<0||value>=workbook.at("sheets").size())throw std::runtime_error("Invalid active worksheet view");pugi::xml_document doc;parse(doc,entries.at("xl/workbook.xml"));auto root=doc.document_element(),views=child(root,"bookViews");if(!views){const auto sheets=child(root,"sheets");views=sheets?root.insert_child_before(qualified(root,"bookViews").c_str(),sheets):root.append_child(qualified(root,"bookViews").c_str());}attr(ensure(views,"workbookView"),"activeTab")=value.get<int>();entries["xl/workbook.xml"]=xml(doc);}
 for(std::size_t index=0;index<workbook.at("sheets").size();++index){const auto& sheet=workbook.at("sheets")[index];if(!sheet.contains("nativeView"))continue;const auto& state=sheet.at("nativeView");if(!state.is_object())throw std::runtime_error("Invalid worksheet view");for(const auto* key:{"activeCell","topLeftCell"})if(state.contains(key)&&(!state.at(key).is_string()||!validReference(state.at(key).get<std::string>(),true)))throw std::runtime_error("Invalid worksheet view cell");if(state.contains("selection")&&(!state.at("selection").is_string()||!validSelections(state.at("selection").get<std::string>())))throw std::runtime_error("Invalid worksheet view selection");if(state.contains("zoomScale")&&(!state.at("zoomScale").is_number_integer()||state.at("zoomScale")<25||state.at("zoomScale")>400))throw std::runtime_error("Invalid worksheet view zoom");
  const auto& part=parts.at(sheet.at("id").get<std::string>());pugi::xml_document doc;parse(doc,entries.at(part));auto root=doc.document_element(),view=primaryView(root);if(!view){auto views=child(root,"sheetViews");if(!views){const auto dimension=child(root,"dimension");const auto properties=child(root,"sheetPr");views=dimension?root.insert_child_after(qualified(root,"sheetViews").c_str(),dimension):properties?root.insert_child_after(qualified(root,"sheetViews").c_str(),properties):root.prepend_child(qualified(root,"sheetViews").c_str());}view=views.append_child(qualified(root,"sheetView").c_str());attr(view,"workbookViewId")=0;}
  for(const auto* key:{"topLeftCell"})if(state.contains(key))attr(view,key)=state.at(key).get<std::string>().c_str();if(state.contains("zoomScale"))attr(view,"zoomScale")=state.at("zoomScale").get<int>();if(workbook.contains("activeSheetIndex"))attr(view,"tabSelected")=index==workbook.at("activeSheetIndex").get<std::size_t>();
  auto selected=child(view,"selection");const auto pane=child(view,"pane");if(pane.attribute("activePane"))for(auto node:view.children())if(local(node.name())=="selection"&&std::string(node.attribute("pane").value())==pane.attribute("activePane").value())selected=node;if(!selected)selected=view.append_child(qualified(root,"selection").c_str());if(state.contains("activeCell"))attr(selected,"activeCell")=state.at("activeCell").get<std::string>().c_str();if(state.contains("selection"))attr(selected,"sqref")=state.at("selection").get<std::string>().c_str();entries[part]=xml(doc);
 }
}
}

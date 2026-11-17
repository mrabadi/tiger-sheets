#pragma once
#include "native_action_replay.hpp"
#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/worksheet_view.hpp"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QLineEdit>
#include <QTemporaryDir>
#include <stdexcept>

namespace tigersheets::testing {
namespace popup_composition_test {
inline void require(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
inline Json step(const char* id,const Json& action){return {{"id",id},{"videoTime","synthetic acceptance"},{"evidence",{{"kind","acceptance"},{"reference","Actual native modal and owned currency-popup composition; not a source-video gesture"}}},{"action",action}};}
}
inline void nativeModalPopupComposition(){
 using namespace popup_composition_test;
 QTemporaryDir temporary;const auto requested=qEnvironmentVariable("TIGERSHEETS_POPUP_COMPOSITION_OUTPUT");const QString output=requested.isEmpty()?temporary.path():requested;
 MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();
 auto* view=window.findChild<WorksheetView*>("worksheet.grid");require(view,"actual native worksheet exists");auto* model=dynamic_cast<WorksheetModel*>(view->model());require(model&&model->setData(model->index(0,0),"5648",Qt::EditRole),"prepare independent native numeric value before modal capture");view->setFocus();QApplication::processEvents();const auto before=window.currentSession().snapshot();
 NativeActionReplay replay(window,output);
 const auto report=replay.run({{"steps",Json::array({step("modal-popup-composition",{{"type","modal"},{"open",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",{"Ctrl+1"}}}},{"steps",Json::array({
  step("modal-popup-number-page",{{"type","mouse_tab"},{"target","formatCells.tabs"},{"tab","Number"}}),
  step("modal-popup-accounting",{{"type","key_sequence"},{"target","formatCells.categoryList"},{"keys",{"Home","Down","Down","Down"}}}),
  step("modal-popup-open-symbol",{{"type","mouse"},{"target","formatCells.currencySymbol"},{"gesture","click"}}),
  step("modal-popup-overlap-capture",{{"type","key_sequence"},{"target","formatCells.dialog"},{"keys",Json::array()}}),
  step("modal-popup-dismiss-only-list",{{"type","key_sequence"},{"target","formatCells.currencySymbol"},{"keys",{"Escape"}}}),
  step("modal-popup-cancel-dialog",{{"type","key_sequence"},{"target","formatCells.dialog"},{"keys",{"Escape"}}})
 })}})})}});
 require(report.at("executionComplete")==true,"actual Format Cells and owned Symbol popup open, capture and cancel successfully: "+report.dump());
 require(window.currentSession().snapshot()==before,"capturing and cancelling the actual modal popup preserves workbook/history");
 const auto& children=report.at("steps").at(0).at("children");const Json* capture=nullptr;
 for(const auto& child:children)if(child.at("id")=="modal-popup-overlap-capture")capture=&child;
 require(capture&&capture->contains("sourceCompositeScreenshot")&&capture->contains("ownedWindowScreenshots")&&capture->contains("modalScreenshot"),"separate actual popup/modal grabs and a disclosed composed capture exist");
 const Json* popup=nullptr;const Json* modal=nullptr;
 for(const auto& owned:capture->at("ownedWindowScreenshots")){
  if(owned.at("windowType")==static_cast<int>(Qt::Popup))popup=&owned;
  if(owned.at("objectName")=="formatCells.dialog")modal=&owned;
 }
 require(popup&&modal,"the actual currency dropdown and its real modal owner are separately captured");
 const auto image=[&](const std::string& filename){return QImage(QDir(output).filePath(QString::fromStdString(filename))).convertToFormat(QImage::Format_ARGB32);};
 const QImage composite=image(capture->at("sourceCompositeScreenshot")),popupPixels=image(popup->at("screenshot"));
 require(!composite.isNull()&&!popupPixels.isNull(),"actual modal-popup composition pixels are readable");
 const QPoint position(popup->at("position").at(0).get<int>(),popup->at("position").at(1).get<int>());const QRect popupBounds(position,popupPixels.size());const QRect modalBounds(QPoint(modal->at("position").at(0).get<int>(),modal->at("position").at(1).get<int>()),QSize(modal->at("size").at(0).get<int>(),modal->at("size").at(1).get<int>()));const QRect overlap=popupBounds.intersected(modalBounds).intersected(composite.rect());
 require(overlap.width()>100&&overlap.height()>100,"the real dropdown materially overlaps its modal instead of testing a disjoint region");
 int mismatched=0;
 for(int y=overlap.top();y<=overlap.bottom();++y)for(int x=overlap.left();x<=overlap.right();++x)if(composite.pixelColor(x,y)!=popupPixels.pixelColor(x-position.x(),y-position.y()))++mismatched;
 const Json result={{"modalOpenAndCancelled",true},{"workbookUnchanged",true},{"overlapPixels",overlap.width()*overlap.height()},{"mismatchedPopupPixels",mismatched},{"popupPosition",popup->at("position")},{"popupSize",popup->at("size")},{"composite",capture->at("sourceCompositeScreenshot")},{"popup",popup->at("screenshot")},{"modal",modal->at("screenshot")}};
 QFile evidence(QDir(output).filePath("modal-popup-pixel-verification.json"));require(evidence.open(QIODevice::WriteOnly),"pixel verification evidence is writable");evidence.write(QByteArray::fromStdString(result.dump(2)));evidence.close();
 require(mismatched==0,"actual owned dropdown must paint above its modal in the source composite; mismatched popup pixels="+std::to_string(mismatched));
}
inline void nativeSelectionReleaseCaptureFreshness(){
 using namespace popup_composition_test;
 QTemporaryDir temporary;const auto requested=qEnvironmentVariable("TIGERSHEETS_RELEASE_CAPTURE_OUTPUT");const QString output=requested.isEmpty()?temporary.path():requested;
 MainWindow window;window.resize(1280,720);window.show();window.activateWindow();QApplication::processEvents();
 auto* view=window.findChild<WorksheetView*>("worksheet.grid");auto* name=window.findChild<QLineEdit*>("formula.name");require(view&&name,"actual worksheet and Name Box exist before release capture");auto* model=dynamic_cast<WorksheetModel*>(view->model());require(model,"actual native worksheet model exists");
 for(const auto& [column,value]:std::vector<std::pair<int,int>>{{6,1267},{7,1056},{8,1420}})require(model->setData(model->index(22,column),QString::number(value),Qt::EditRole),"prepare native selected values");
 if(!window.property("ribbon.collapsed").toBool())require(window.executeCommand("view.ribbon"),"collapse the actual native ribbon");
 view->setCurrentIndex(model->index(23,6));view->scrollTo(model->index(3,0),QAbstractItemView::PositionAtTop);view->setFocus();QApplication::processEvents();const auto before=window.currentSession().snapshot();NativeActionReplay replay(window,output);
 const auto held=replay.run({{"steps",Json::array({step("capture-drag-press",{{"type","mouse_cell"},{"address","G23"},{"gesture","press"}}),step("capture-drag-expand",{{"type","mouse_cell"},{"address","I24"},{"gesture","move"}}),step("capture-drag-reduce",{{"type","mouse_cell"},{"address","I23"},{"gesture","move"}}),step("capture-drag-held",{{"type","key_sequence"},{"target","worksheet.grid"},{"keys",Json::array()}})})}});
 require(held.at("executionComplete")==true&&name->text()=="1R x 3C","real held drag exposes dimensions before release");
 auto release=step("capture-drag-release",{{"type","mouse_cell"},{"address","I23"},{"gesture","release"}});release["expect"]={{"nameBoxText","G23"}};const auto report=replay.run({{"steps",Json::array({release})}});
 require(report.at("executionComplete")==true&&name->text()=="G23","actual release restores the native control text without changing its selection");
 const auto& captured=report.at("steps").back();require(captured.at("after").at("nameBoxText")=="G23","the read-only release observation reads the real restored control text");
 const QImage whole(QDir(output).filePath(QString::fromStdString(captured.at("screenshot").get<std::string>())));require(!whole.isNull(),"actual release screenshot exists");
 QApplication::processEvents();const QImage current=name->grab().toImage();const QPoint position=name->mapTo(&window,QPoint());require(!current.isNull()&&whole.rect().contains(QRect(position,current.size())),"native Name Box pixels are contained in the actual window capture");
 int mismatched=0;for(int y=1;y<current.height()-1;++y)for(int x=1;x<current.width()-1;++x)if(whole.pixelColor(position.x()+x,position.y()+y)!=current.pixelColor(x,y))++mismatched;
 require(window.currentSession().snapshot()==before,"held drag, release and read-only capture preserve workbook/history");
 const Json result={{"actualControlText",name->text().toStdString()},{"observedControlText",captured.at("after").at("nameBoxText")},{"mismatchedNameBoxPixels",mismatched},{"workbookUnchanged",true},{"screenshot",captured.at("screenshot")}};QFile evidence(QDir(output).filePath("selection-release-pixel-verification.json"));require(evidence.open(QIODevice::WriteOnly),"release pixel verification evidence is writable");evidence.write(QByteArray::fromStdString(result.dump(2)));evidence.close();
 require(mismatched==0,"actual release screenshot must paint the current native Name Box text rather than stale held dimensions; mismatched pixels="+std::to_string(mismatched));
}
}

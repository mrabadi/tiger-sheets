#include "native_action_replay.hpp"
#include "native_document_context.hpp"
#include "native_annotation_overlay.hpp"
#include "native_presentation_builds.hpp"
#include "native_document_selection.hpp"
#include "native_document_text_toolbar.hpp"
#include "native_video_caption.hpp"
#include "tigersheets/main_window.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/references.hpp"
#include <QApplication>
#include <QAbstractScrollArea>
#include <QCryptographicHash>
#include <QClipboard>
#include <QMimeData>
#include <QCheckBox>
#include <QDir>
#include <QDialog>
#include <QElapsedTimer>
#include <QFile>
#include <QKeySequence>
#include <QLineEdit>
#include <QLabel>
#include <QMouseEvent>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPainter>
#include <QRegularExpression>
#include <QRadioButton>
#include <QScrollBar>
#include <QStatusBar>
#include <QStyleOptionButton>
#include <QTableView>
#include <QTabWidget>
#include <QTabBar>
#include <QTest>
#include <QToolTip>
#include <QToolButton>
#include <QWindow>
#include <QTimer>
#include <QWheelEvent>
#include <cmath>
#include <stdexcept>

namespace tigersheets::testing {
namespace {
void require(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
bool allSourceVisual(const Json& record){
 if(record.at("evidence").at("kind")!="visual")return false;
 if(record.contains("children"))for(const auto& child:record.at("children"))if(!allSourceVisual(child))return false;
 return true;
}
NativeDocumentContext* documentContext(){
 NativeDocumentContext* result=nullptr;for(auto* widget:QApplication::topLevelWidgets())if(auto* candidate=dynamic_cast<NativeDocumentContext*>(widget)){require(!result,"Replay has multiple external PDF contexts");result=candidate;}return result;
}
QWidget* videoCaption(){
 QWidget* result=nullptr;for(auto* widget:QApplication::topLevelWidgets())if(widget->objectName()=="replay.videoCaption"&&widget->isVisible()){require(!result,"Replay has multiple publisher captions");result=widget;}return result;
}
QWidget* visibleTarget(MainWindow* window,const Json& action){
 const auto name=QString::fromStdString(action.at("target").get<std::string>());QWidget* found=nullptr;
 if(name=="@active_modal")return QApplication::activeModalWidget();
 if(name=="@active_window")return window;
 if(name=="@context_window")return documentContext();
 QWidget* scope=QApplication::activeModalWidget();if(!scope)scope=name.startsWith("context.")&&documentContext()?static_cast<QWidget*>(documentContext()):static_cast<QWidget*>(window);
 if(scope->objectName()==name&&scope->isVisible())found=scope;
 for(auto* candidate:scope->findChildren<QWidget*>(name))if(candidate->isVisible()){
  require(!found,"Replay target is ambiguous: "+name.toStdString());found=candidate;
 }
 return found;
}
Json cellObservation(WorksheetModel& model,const std::string& address){
 const auto match=QRegularExpression("^([A-Z]+)([1-9][0-9]*)$").match(QString::fromStdString(address));require(match.hasMatch(),"Invalid checkpoint cell address");
 const int row=match.captured(2).toInt()-1,col=columnNumber(match.captured(1).toStdString())-1;auto cell=model.copyRange(row,col,row,col).at("cells").at(0).at(0);cell["address"]=address;cell["display"]=model.data(model.index(row,col),Qt::DisplayRole).toString().toStdString();
 if(!cell.contains("value"))if(const auto number=model.numericValue(row,col)){cell["value"]=*number;cell["valueType"]="number";}
 return cell;
}
}
NativeActionReplay::NativeActionReplay(MainWindow& window,QString outputDirectory)
 :window_(&window),outputDirectory_(std::move(outputDirectory)){
 require(QDir().mkpath(outputDirectory_),"Cannot create replay evidence directory");
}
void NativeActionReplay::configureWindow(MainWindow* window){
 for(const auto& [source,rect]:windowGeometryOverrides_)if(source==window){const auto actual=window->geometry();if(actual.size()!=rect.size()||std::abs(actual.x()-rect.x())>2||std::abs(actual.y()-rect.y())>2)window->setGeometry(rect);return;}
 if(replayWindowSize_.isValid())window->resize(replayWindowSize_);
}
Json NativeActionReplay::observe() const {
 require(window_,"Replay workbook window has closed");
 auto* grid=window_->findChild<QTableView*>("worksheet.grid");
 auto* ribbon=window_->findChild<QTabWidget*>("workspace.ribbon");
 auto* model=grid?dynamic_cast<WorksheetModel*>(grid->model()):nullptr;
 require(model&&ribbon,"Replay requires a native worksheet and ribbon");
 const auto current=grid->currentIndex();
 const auto& book=window_->currentSession().snapshotView();
 auto* formula=window_->findChild<QLineEdit*>("formula.input");
 auto* nameBox=window_->findChild<QLineEdit*>("formula.name");
 auto* pointHint=window_->findChild<QLabel*>("worksheet.pointRangeHint");
 auto* quickAnalysis=window_->findChild<QToolButton*>("worksheet.quickAnalysis");
 auto* document=window_->findChild<QLabel*>("workspace.documentName");int windowCount=0;
 auto* externalEditor=window_->findChild<QPlainTextEdit*>("replay.externalTextEditor");
 for(auto* widget:QApplication::topLevelWidgets())if(widget->isVisible()&&dynamic_cast<MainWindow*>(widget))++windowCount;
 std::string sheetName;for(const auto& sheet:book.at("sheets"))if(sheet.at("id")==model->sheetId().toStdString()){sheetName=sheet.at("name").get<std::string>();break;}
 Json result={{"revision",book.at("revision")},
         {"sheet",model->sheetId().toStdString()},
         {"sheetName",sheetName},
         {"row",current.row()+1},{"column",current.column()+1},
         {"topRow",std::max(1,grid->rowAt(0)+1)},{"firstColumn",std::max(1,grid->columnAt(0)+1)},
         {"topRowOffset",grid->rowViewportPosition(std::max(0,grid->rowAt(0)))},{"firstColumnOffset",grid->columnViewportPosition(std::max(0,grid->columnAt(0)))},
         {"ribbon",ribbon->tabText(ribbon->currentIndex()).toStdString()},
         {"showGridLines",grid->showGrid()},
         {"documentName",document?document->text().toStdString():std::string()},{"windowCount",windowCount},
         {"externalText",externalEditor?externalEditor->toPlainText().toStdString():std::string()},
         {"modal",QApplication::activeModalWidget()?QApplication::activeModalWidget()->objectName().toStdString():std::string()},
         {"formulaDraft",formula?formula->text().toStdString():std::string()},
         {"nameBoxText",nameBox?nameBox->text().toStdString():std::string()},
         {"pointRangeHint",{{"visible",pointHint&&pointHint->isVisible()},{"text",pointHint?pointHint->text().toStdString():std::string()}}},
         {"quickAnalysisVisible",quickAnalysis?Json(quickAnalysis->isVisible()):Json()},
         {"statusBarMessage",window_->statusBar()->currentMessage().toStdString()},
         {"toolTip",QToolTip::isVisible()?QToolTip::text().toStdString():std::string()},
         {"windowGeometry",{window_->geometry().x(),window_->geometry().y(),window_->width(),window_->height()}},
         {"workbookSha256",QCryptographicHash::hash(QByteArray::fromStdString(book.dump()),QCryptographicHash::Sha256).toHex().toStdString()}};for(const auto& [source,rect]:windowGeometryOverrides_)if(source==window_)result["requestedWindowGeometry"]={rect.x(),rect.y(),rect.width(),rect.height()};if(auto* context=documentContext())result["externalDocument"]=context->observation();return result;
}
Json NativeActionReplay::dispatch(const Json& action){
 if(action.at("type")=="window_geometry"){
  const auto& bounds=action.at("rect");require(bounds.is_array()&&bounds.size()==4,"Replay source window requires four client-geometry integers");for(const auto& part:bounds)require(part.is_number_integer(),"Replay source window coordinates must be integers");const QRect rect(bounds[0].get<int>(),bounds[1].get<int>(),bounds[2].get<int>(),bounds[3].get<int>());require(std::abs(rect.x())<=32768&&std::abs(rect.y())<=32768&&rect.width()>=100&&rect.height()>=100&&rect.width()<=16384&&rect.height()<=16384,"Replay source window geometry is outside the bounded native range");bool stored=false;for(auto& [source,value]:windowGeometryOverrides_)if(source==window_){value=rect;stored=true;break;}if(!stored)windowGeometryOverrides_.push_back({window_,rect});if(window_->isMaximized())window_->showNormal();configureWindow(window_);return {{"sourceWindowGeometryAdapter","Observed outer source border mapped to native Qt client geometry; original window-manager drag/decorations are adapted."}};
 }
 if(action.at("type")=="video_caption"||action.at("type")=="document_text_toolbar"){
  auto* surface=visibleTarget(window_,action);require(surface&&surface->isVisible(),"Replay source chrome surface is unavailable");const auto operation=action.at("operation").get<std::string>();const bool caption=action.at("type")=="video_caption";
  if(operation=="clear"){if(caption){if(auto* actual=videoCaption())clearVideoCaption(actual->parentWidget());}else clearDocumentTextToolbar(surface);return {{caption?"videoCaptionVisible":"documentTextToolbarVisible",false}};}
  require(operation=="show","Replay source chrome operation is invalid");const auto& rect=action.at("rect");require(rect.is_array()&&rect.size()==4,"Replay source chrome bounds require four integers");for(const auto& part:rect)require(part.is_number_integer(),"Replay source chrome coordinates must be integers");const QRect bounds(rect[0].get<int>(),rect[1].get<int>(),rect[2].get<int>(),rect[3].get<int>());require(bounds.isValid()&&surface->rect().contains(bounds),"Replay source chrome lies outside the actual native surface");
  if(caption){if(auto* actual=videoCaption())clearVideoCaption(actual->parentWidget());QStringList lines;for(const auto& line:action.at("lines"))lines.push_back(QString::fromStdString(line.get<std::string>()));showVideoCaption(surface,bounds,lines,action.value("fontPixelSize",42));return {{"videoCaptionVisible",true},{"videoCaptionLines",action.at("lines")}};}
  const auto kind=action.at("kind").get<std::string>();showDocumentTextToolbar(surface,bounds,QString::fromStdString(kind),action.value("opacity",1.0));return {{"documentTextToolbarVisible",true},{"documentTextToolbarKind",kind}};
 }
 if(action.at("type")=="document_context"){
  auto* context=documentContext();const auto operation=action.at("operation").get<std::string>();
  if(operation=="open"){if(!context)context=new NativeDocumentContext(window_);context->openPdf(QString::fromStdString(action.at("path").get<std::string>()));}
  else if(operation=="hide"){require(context,"Replay document context is not open");context->hide();window_->activateWindow();return {{"externalAppAdapter","read-only PDF"}};}
  else if(operation=="close"){require(context,"Replay document context is not open");context->close();delete context;window_->activateWindow();return {{"externalAppAdapter","read-only PDF"}};}
  else require(operation=="activate"&&context,"Replay document context is unavailable");
  context->resize(replayWindowSize_.isValid()?replayWindowSize_:window_->size());context->show();context->raise();context->activateWindow();return {{"externalAppAdapter","read-only PDF"}};
 }
 if(action.at("type")=="presentation_builds"){
  auto* surface=visibleTarget(window_,action);require(surface&&surface->isVisible(),"Replay presentation surface is unavailable");const auto operation=action.at("operation").get<std::string>();
  if(operation=="set"){std::vector<std::vector<QRect>> stages;for(const auto& stage:action.at("masks")){std::vector<QRect> rectangles;for(const auto& rect:stage){require(rect.is_array()&&rect.size()==4,"Replay presentation mask must have four coordinates");rectangles.emplace_back(rect[0].get<int>(),rect[1].get<int>(),rect[2].get<int>(),rect[3].get<int>());}stages.push_back(std::move(rectangles));}setPresentationBuilds(surface,stages);}
  else if(operation=="advance")advancePresentationBuild(surface,action.value("reverse",false));
  else if(operation=="clear")clearPresentationBuilds(surface);else throw std::runtime_error("Replay presentation operation is invalid");
  return {{"presentationStage",surface->property("replay.presentationStage").toInt()},{"presentationStageCount",surface->property("replay.presentationStageCount").toInt()}};
 }
 if(action.at("type")=="document_image_selection"){
  auto* surface=visibleTarget(window_,action);require(surface&&surface->isVisible(),"Replay document image surface is unavailable");const auto operation=action.at("operation").get<std::string>();
  if(operation=="clear"){clearDocumentSelection(surface);return {{"documentImageSelected",false}};}
  require(operation=="select"||operation=="artwork","Replay document image operation is invalid");auto rectangle=[&](const char* field){const auto& rect=action.at(field);require(rect.is_array()&&rect.size()==4,"Replay document image rectangle needs four coordinates");for(const auto& part:rect)require(part.is_number_integer(),"Replay document image coordinates must be integers");const QRect result(rect[0].get<int>(),rect[1].get<int>(),rect[2].get<int>(),rect[3].get<int>());require(result.isValid()&&surface->rect().contains(result),"Replay document image region is outside the original PDF raster");return result;};if(operation=="artwork")selectDocumentArtwork(surface,rectangle("rect"));else selectDocumentImage(surface,rectangle("rect"),rectangle("toolbarRect"));return {{"documentImageSelected",true}};
 }
 if(action.at("type")=="annotation"){
  auto* target=visibleTarget(window_,action);require(target&&target->isVisible(),"Replay annotation surface is unavailable");QWidget* surface=target;if(auto* area=qobject_cast<QAbstractScrollArea*>(target))surface=area->viewport();const auto operation=action.at("operation").get<std::string>();
  QRect tabAnchor;
  if(action.contains("tabName")){
   auto* tabs=qobject_cast<QTabBar*>(target);require(tabs&&action.at("tabName").is_string(),"Replay annotation tab anchor requires actual native tabs and a text name");
   const auto name=QString::fromStdString(action.at("tabName").get<std::string>());int found=-1;
   for(int index=0;index<tabs->count();++index)if(tabs->tabText(index)==name){require(found<0,"Replay annotation tab name is ambiguous");found=index;}
   require(found>=0,"Replay annotation tab is unavailable: "+name.toStdString());const auto rectangle=tabs->tabRect(found);
   require(tabs->isTabVisible(found)&&rectangle.isValid()&&tabs->rect().contains(rectangle.center()),"Replay annotation tab is outside the visible native tabs");
   surface=window_;tabAnchor=QRect(tabs->mapTo(surface,rectangle.topLeft()),rectangle.size());
  }
  if(operation=="clear"){clearAnnotations(surface);return Json::object();}
  if(operation=="press"||operation=="move"||operation=="release"){
   QPoint position;
   if(tabAnchor.isValid()){
    const auto& point=action.at("tabPoint");require(point.is_array()&&point.size()==2,"Replay normalized tab point requires two numbers");
    for(const auto& value:point)require(value.is_number()&&std::isfinite(value.get<double>())&&std::abs(value.get<double>())<=4,"Replay normalized tab point is outside the bounded native range");
    position=tabAnchor.topLeft()+QPoint(qRound(point[0].get<double>()*(tabAnchor.width()-1)),qRound(point[1].get<double>()*(tabAnchor.height()-1)));
    require(surface->rect().contains(position),"Replay tab point is outside the actual native window");
   }else{
    const auto& point=action.at("point");require(point.is_array()&&point.size()==2,"Replay annotation point requires two coordinates");position=QPoint(point[0].get<int>(),point[1].get<int>());
   }
   if(operation=="press")beginAnnotation(surface,position,QColor(QString::fromStdString(action.value("color",std::string("#0000FF")))),action.value("lineWidth",3));else if(operation=="move")moveAnnotation(surface,position);else endAnnotation(surface,position);return {{"annotationGesture",operation},{"annotationPoint",{position.x(),position.y()}}};
  }
  require(operation=="rectangle","Replay annotation operation is invalid");QRect rectangle;
  if(tabAnchor.isValid()){
   rectangle=tabAnchor;
   if(action.contains("padding")){
    const auto& padding=action.at("padding");require(padding.is_array()&&padding.size()==4,"Replay tab padding requires four signed integers");
    for(const auto& value:padding)require(value.is_number_integer()&&value.get<int>()>=-1024&&value.get<int>()<=1024,"Replay tab padding is outside the bounded native range");
    rectangle.adjust(-padding[0].get<int>(),-padding[1].get<int>(),padding[2].get<int>(),padding[3].get<int>());
   }
  }
  else if(action.contains("cellRange")){auto* grid=qobject_cast<QTableView*>(target);require(grid,"Cell annotation requires the actual worksheet view");const auto range=QString::fromStdString(action.at("cellRange").get<std::string>());const auto match=QRegularExpression("^([A-Z]+)([1-9][0-9]*)(?::([A-Z]+)([1-9][0-9]*))?$").match(range);require(match.hasMatch(),"Replay annotation range is invalid");auto start=grid->model()->index(match.captured(2).toInt()-1,columnNumber(match.captured(1).toStdString())-1);auto end=match.captured(3).isEmpty()?start:grid->model()->index(match.captured(4).toInt()-1,columnNumber(match.captured(3).toStdString())-1);require(start.isValid()&&end.isValid(),"Replay annotation range is outside the worksheet");rectangle=grid->visualRect(start).united(grid->visualRect(end));}
  else{const auto& rect=action.at("rect");require(rect.is_array()&&rect.size()==4,"Replay annotation requires a native surface rectangle");rectangle=QRect(rect[0].get<int>(),rect[1].get<int>(),rect[2].get<int>(),rect[3].get<int>());}
  const QColor color(QString::fromStdString(action.value("color",std::string("#0000FF"))));require(color.isValid()&&rectangle.isValid()&&surface->rect().contains(rectangle),"Replay annotation is outside the visible surface");annotateRectangle(surface,rectangle,color,action.value("lineWidth",3));return {{"annotationRect",{rectangle.x(),rectangle.y(),rectangle.width(),rectangle.height()}}};
 }
 if(action.at("type")=="external_text_editor"){
  auto* editor=window_->findChild<QPlainTextEdit*>("replay.externalTextEditor");const auto operation=action.at("operation").get<std::string>();
  if(operation=="open"){
   require(!editor,"Replay external text adapter is already open");editor=new QPlainTextEdit(window_);editor->setWindowFlag(Qt::Window);editor->setObjectName("replay.externalTextEditor");editor->setWindowTitle("External text editor — source replay adapter");editor->resize(720,480);
  }else if(operation=="close"){
   require(editor,"Replay external text adapter is not open");editor->close();delete editor;window_->activateWindow();return {{"externalAppAdapter","QPlainTextEdit"}};
  }else require(operation=="activate"&&editor,"Replay external text adapter is unavailable");
  editor->show();editor->raise();editor->activateWindow();editor->setFocus();return {{"externalAppAdapter","QPlainTextEdit"}};
 }
 if(action.at("type")=="activate_window"){
  MainWindow* found=nullptr;const auto wanted=action.at("documentName").get<std::string>();
  for(auto* widget:QApplication::topLevelWidgets())if(auto* candidate=dynamic_cast<MainWindow*>(widget);candidate&&candidate->isVisible()){
   auto* name=candidate->findChild<QLabel*>("workspace.documentName");
   if(name&&name->text().toStdString()==wanted){require(!found,"Replay window name is ambiguous: "+wanted);found=candidate;}
  }
  require(found,"Replay workbook window is unavailable: "+wanted);
  QPointer<QWidget> focusToRestore;auto* focus=QApplication::focusWidget();if(window_==found&&focus&&focus->window()==found)focusToRestore=focus;
  window_=found;configureWindow(found);found->raise();found->activateWindow();
  if(focusToRestore&&focusToRestore->isVisible())focusToRestore->setFocus();else if(auto* grid=found->findChild<QTableView*>("worksheet.grid"))grid->setFocus();
  return Json::object();
 }
 if(action.at("type")=="modal"){
  if(action.value("dialogSequence",false)){
   require(action.at("steps").is_array()&&!action.at("steps").empty(),"Sequential modal replay requires explicit dialog steps");
   window_->activateWindow();QApplication::processEvents();
   const QPointer<QWidget> previousModal=QApplication::activeModalWidget();Json children=Json::array();std::string failure;std::size_t next=0;bool entered=false,executing=false;
   QTimer driver,watchdog;watchdog.setSingleShot(true);
   QObject::connect(&driver,&QTimer::timeout,&driver,[&]{
    if(executing)return;auto* modal=QApplication::activeModalWidget();if(!modal||modal==previousModal)return;
    entered=true;
    if(!failure.empty()||next>=action.at("steps").size()){
     if(failure.empty())failure="Sequential modal replay opened an unrecorded dialog";
     if(auto* dialog=qobject_cast<QDialog*>(modal))dialog->reject();else modal->close();return;
    }
    executing=true;
    try{children.push_back(execute(action.at("steps").at(next++)));if(children.back().at("status")!="passed")failure="Nested sequential modal replay step did not pass";}
    catch(const std::exception& error){failure=error.what();}
    executing=false;
    if(!failure.empty())if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))dialog->reject();
    if(next==action.at("steps").size()&&!QApplication::activeModalWidget())watchdog.stop();
   });
   QObject::connect(&watchdog,&QTimer::timeout,&watchdog,[&]{failure="Sequential modal replay timed out";if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))dialog->reject();});
   driver.start(0);watchdog.start(5000);dispatch(action.at("open"));driver.stop();watchdog.stop();
   require(entered,"Replay opening action did not enter a modal event loop");
   if(next!=action.at("steps").size()&&failure.empty())failure="Sequential modal replay ended before all dialog steps";
   Json result={{"children",children}};if(!failure.empty())result["nestedError"]=failure;return result;
  }
  QObject conflictScope;bool replacingClipboard=false,clipboardWasReplaced=false;const bool clipboardConflict=action.value("clipboardOwnershipConflict",false);
  if(clipboardConflict){auto* clipboard=QApplication::clipboard();QObject::connect(clipboard,&QClipboard::dataChanged,&conflictScope,[&,clipboard]{if(!replacingClipboard&&clipboard->mimeData()->hasFormat("application/x-tigersheets-cells+json")){replacingClipboard=true;clipboardWasReplaced=true;clipboard->setText("External application clipboard ownership — source replay boundary adapter");replacingClipboard=false;}});}
  window_->activateWindow();QApplication::processEvents();
  Json children=Json::array();std::string failure;bool entered=false;QPointer<QWidget> previousModal=QApplication::activeModalWidget();
  QTimer driver,watchdog;QObject::connect(&driver,&QTimer::timeout,&driver,[&]{
   if(!QApplication::activeModalWidget()||QApplication::activeModalWidget()==previousModal)return;
   driver.stop();
   entered=true;
   try{
    require(QApplication::activeModalWidget(),"Replay opening action did not display a modal dialog");
    if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))QObject::connect(dialog,&QDialog::finished,&watchdog,&QTimer::stop);
    for(const auto& child:action.at("steps")){
     children.push_back(execute(child));
     if(children.back().at("status")!="passed"){failure="Nested modal replay step did not pass";break;}
    }
    if(QApplication::activeModalWidget()&&failure.empty())failure="Modal replay did not close its dialog";
   }catch(const std::exception& error){failure=error.what();}
   if(!failure.empty())if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))dialog->reject();
  });
  watchdog.setSingleShot(true);QObject::connect(&watchdog,&QTimer::timeout,&watchdog,[&]{failure="Modal replay timed out";if(auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget()))dialog->reject();});
  driver.start(0);watchdog.start(5000);
  dispatch(action.at("open"));
  require(entered,"Replay opening action did not enter a modal event loop");
  Json result={{"children",children}};if(clipboardConflict)result["clipboardOwnershipReplaced"]=clipboardWasReplaced;if(!failure.empty())result["nestedError"]=failure;return result;
 }
 if(action.at("type")=="text"){
  auto* target=visibleTarget(window_,action);
  require(target&&target->isVisible()&&target->isEnabled(),"Replay text target is unavailable");
  if(target->window()==window_&&!window_->isActiveWindow()&&!QApplication::activeModalWidget()){window_->activateWindow();QApplication::processEvents();}
  const auto text=QString::fromStdString(action.at("text").get<std::string>());
  require(QString::fromLatin1(text.toLatin1())==text,"Replay text requires a Unicode input action");
  target->setFocus();QTest::keyClicks(target,text);return Json::object();
 }
 if(action.at("type")=="mouse_cell"){
  auto* grid=window_->findChild<QTableView*>("worksheet.grid");
  require(grid&&grid->isVisible()&&grid->isEnabled(),"Replay worksheet pointer target is unavailable");
  const auto address=QString::fromStdString(action.at("address").get<std::string>());
  const auto match=QRegularExpression("^([A-Z]+)([1-9][0-9]*)$").match(address);
  require(match.hasMatch(),"Replay pointer cell address is invalid");
  const auto index=grid->model()->index(match.captured(2).toInt()-1,columnNumber(match.captured(1).toStdString())-1);
  const auto rect=grid->visualRect(index);
  require(index.isValid()&&grid->viewport()->rect().contains(rect.center()),"Replay pointer cell is outside the visible worksheet: "+address.toStdString());
  const auto gesture=action.value("gesture",std::string("click"));
  if(gesture=="click")QTest::mouseClick(grid->viewport(),Qt::LeftButton,Qt::NoModifier,rect.center());
  else if(gesture=="press"){QTest::mousePress(grid->viewport(),Qt::LeftButton,Qt::NoModifier,rect.center());pointerPressed_=true;}
  else if(gesture=="move"){
   QMouseEvent event(QEvent::MouseMove,QPointF(rect.center()),QPointF(grid->viewport()->mapToGlobal(rect.center())),Qt::NoButton,pointerPressed_?Qt::LeftButton:Qt::NoButton,Qt::NoModifier);
   QApplication::sendEvent(grid->viewport(),&event);
  }
  else if(gesture=="release"){QTest::mouseRelease(grid->viewport(),Qt::LeftButton,Qt::NoModifier,rect.center());pointerPressed_=false;}
  else throw std::runtime_error("Replay pointer gesture is invalid: "+gesture);
  return Json::object();
 }
 if(action.at("type")=="mouse"||action.at("type")=="mouse_tab"){
  auto* target=visibleTarget(window_,action);
  require(target&&target->isVisible()&&target->isEnabled(),"Replay widget pointer target is unavailable");
  if(action.contains("childAccessibleName")){
   const auto name=QString::fromStdString(action.at("childAccessibleName").get<std::string>());QWidget* child=nullptr;
   for(auto* candidate:target->findChildren<QWidget*>())if(candidate->isVisible()&&candidate->accessibleName()==name){require(!child,"Replay accessible child is ambiguous: "+name.toStdString());child=candidate;}
   require(child&&child->isEnabled(),"Replay accessible child is unavailable: "+name.toStdString());target=child;
  }
  QPoint position=target->rect().center();
  if(qobject_cast<QCheckBox*>(target)||qobject_cast<QRadioButton*>(target)){
   QStyleOptionButton option;option.initFrom(target);option.rect=target->rect();
   position=target->style()->subElementRect(qobject_cast<QCheckBox*>(target)?QStyle::SE_CheckBoxIndicator:QStyle::SE_RadioButtonIndicator,&option,target).center();
  }
  if(action.at("type")=="mouse_tab"){
   auto* tabs=qobject_cast<QTabBar*>(target);if(auto* pages=qobject_cast<QTabWidget*>(target))tabs=pages->tabBar();
   require(tabs,"Replay tab pointer target is not a tab bar");const auto title=QString::fromStdString(action.at("tab").get<std::string>());int found=-1;
   for(int index=0;index<tabs->count();++index)if(tabs->tabText(index)==title){require(found<0,"Replay tab label is ambiguous");found=index;}
   require(found>=0&&tabs->isTabEnabled(found)&&tabs->isTabVisible(found),"Replay tab is unavailable: "+title.toStdString());
   target=tabs;position=tabs->tabRect(found).center();
  }
  const auto gesture=action.value("gesture",std::string("click"));
  if(gesture=="click")QTest::mouseClick(target,Qt::LeftButton,Qt::NoModifier,position);
  else if(gesture=="double_click")QTest::mouseDClick(target,Qt::LeftButton,Qt::NoModifier,position);
  else if(gesture=="hover"){
   require(!action.contains("dwellMs")||(action.at("dwellMs").is_number_integer()&&action.at("dwellMs")>=0&&action.at("dwellMs")<=5000),"Replay hover dwell must be a bounded integer from0to5000milliseconds");
   auto* nativeWindow=target->window()->windowHandle();require(nativeWindow,"Replay hover target has no native window");
   QTest::mouseMove(nativeWindow,nativeWindow->mapFromGlobal(target->mapToGlobal(position)));if(const int dwell=action.value("dwellMs",0);dwell>0)QTest::qWait(dwell);
  }
  else throw std::runtime_error("Replay widget pointer gesture is invalid: "+gesture);
  return Json::object();
 }
 if(action.at("type")=="wheel"){
  auto* target=visibleTarget(window_,action);
  require(target&&target->isVisible()&&target->isEnabled(),"Replay wheel target is unavailable");
  if(auto* area=qobject_cast<QAbstractScrollArea*>(target))target=area->viewport();
  const auto& delta=action.at("angleDelta");require(delta.is_array()&&delta.size()==2&&delta[0].is_number_integer()&&delta[1].is_number_integer(),"Replay wheel requires two integer angle deltas");
  const auto local=target->rect().center();
  QWheelEvent event(QPointF(local),QPointF(target->mapToGlobal(local)),QPoint(),QPoint(delta[0].get<int>(),delta[1].get<int>()),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
  QApplication::sendEvent(target,&event);return Json::object();
 }
 if(action.at("type")=="wheel_to_cell"){
  auto* grid=window_->findChild<QTableView*>("worksheet.grid");require(grid&&grid->isVisible(),"Replay scroll worksheet is unavailable");
  const auto address=QString::fromStdString(action.at("address").get<std::string>());const auto match=QRegularExpression("^([A-Z]+)([1-9][0-9]*)$").match(address);
  require(match.hasMatch(),"Replay scroll cell address is invalid");const auto index=grid->model()->index(match.captured(2).toInt()-1,columnNumber(match.captured(1).toStdString())-1);require(index.isValid(),"Replay scroll cell is outside the worksheet");
  Json events=Json::array();
  auto position=[&]{return Json{{"topRow",std::max(1,grid->rowAt(0)+1)},{"firstColumn",std::max(1,grid->columnAt(0)+1)}};};
  for(int attempt=0;attempt<256;++attempt){
   const auto point=grid->visualRect(index).center();const auto viewport=grid->viewport()->rect();if(viewport.contains(point))return {{"wheelEvents",events}};
   const int x=point.x()<viewport.left()?480:point.x()>viewport.right()?-480:0,y=point.y()<viewport.top()?480:point.y()>viewport.bottom()?-480:0;const auto before=position();
   dispatch({{"type","wheel"},{"target","worksheet.grid"},{"angleDelta",{x,y}}});QApplication::processEvents();const auto after=position();
   events.push_back({{"angleDelta",{x,y}},{"before",before},{"after",after}});require(before!=after,"Native wheel cannot expose the requested worksheet cell: "+address.toStdString());
  }
  throw std::runtime_error("Native wheel navigation exceeded the bounded replay limit");
 }
 if(action.at("type")=="scroll_view"){
  auto* area=qobject_cast<QAbstractScrollArea*>(visibleTarget(window_,action));require(area&&area->isVisible(),"Replay native scroll area is unavailable");Json events=Json::array();
  for(const auto& [field,vertical]:{std::pair{"topPixel",true},std::pair{"leftPixel",false}})if(action.contains(field)){
   auto* bar=vertical?area->verticalScrollBar():area->horizontalScrollBar();require(action.at(field).is_number_integer(),"Replay pixel scroll position is invalid");const int wanted=action.at(field).get<int>();require(wanted>=bar->minimum()&&wanted<=bar->maximum(),"Replay pixel scroll position is outside the native range");int stationary=0;bool reached=false;
   for(int attempt=0;attempt<4096;++attempt){const int before=bar->value();if(before==wanted){reached=true;break;}const int delta=(before<wanted?-1:1)*(std::abs(before-wanted)>90?120:1);const Json angle=vertical?Json::array({0,delta}):Json::array({delta,0});dispatch({{"type","wheel"},{"target",action.at("target")},{"angleDelta",angle}});QApplication::processEvents();events.push_back({{"axis",vertical?"vertical":"horizontal"},{"angleDelta",angle},{"beforeValue",before},{"afterValue",bar->value()}});stationary=before==bar->value()?stationary+1:0;require(stationary<128,"Native wheel cannot reach the requested pixel offset");}require(reached,"Native pixel navigation exceeded the bounded limit");
  }
  return {{"wheelEvents",events}};
 }
 if(action.at("type")=="horizontal_view"||action.at("type")=="vertical_view"){
  const bool vertical=action.at("type")=="vertical_view";auto* grid=window_->findChild<QTableView*>("worksheet.grid");require(grid&&grid->isVisible(),"Replay scroll worksheet is unavailable");auto* bar=vertical?grid->verticalScrollBar():grid->horizontalScrollBar();Json keys=Json::array(),events=Json::array();
  auto first=[&]{return vertical?grid->rowAt(0)+1:grid->columnAt(0)+1;};const int count=vertical?grid->model()->rowCount():grid->model()->columnCount();
  auto edge=[&](bool end){QTest::keyClick(bar,end?Qt::Key_End:Qt::Key_Home);QApplication::processEvents();keys.push_back(end?"End":"Home");};
  if(action.contains("edge")){const auto requested=action.at("edge").get<std::string>();require(requested=="home"||requested=="end","Replay scrollbar edge is invalid");edge(requested=="end");}
  const auto coordinate=vertical?"topRow":"firstColumn";
  require(!action.contains("topRowOffset")||(vertical&&action.contains("topRow")&&action.at("topRowOffset").is_number_integer()),"Replay partial-row offset requires an integer offset and topRow");
  if(action.contains(coordinate)){
   int wanted=0;if(vertical){require(action.at(coordinate).is_number_integer(),"Replay top row is invalid");wanted=action.at(coordinate).get<int>();}else{const auto name=action.at(coordinate).get<std::string>();require(QRegularExpression("^[A-Z]+$").match(QString::fromStdString(name)).hasMatch(),"Replay first column is invalid");wanted=columnNumber(name);}require(wanted>=1&&wanted<=count,"Replay scroll position is outside the worksheet");
   const int wantedOffset=vertical?action.value("topRowOffset",0):0;require(!vertical||(wantedOffset<=0&&wantedOffset>-grid->rowHeight(wanted-1)),"Replay partial-row offset must lie within the actual native row height");
   if(wanted==1||std::abs(first()-wanted)>32)edge(wanted>count/2);
   int stationary=0;bool reached=false;
   for(int attempt=0;attempt<4096;++attempt){
    const int current=first(),offset=vertical?grid->rowViewportPosition(wanted-1):grid->columnViewportPosition(wanted-1);if(current==wanted&&offset==wantedOffset){reached=true;break;}const int before=bar->value();const int direction=current==wanted?(offset<wantedOffset?1:-1):(current<wanted?-1:1);const int delta=direction*(std::abs(current-wanted)>20?120:1);const Json angle=vertical?Json::array({0,delta}):Json::array({delta,0});
    dispatch({{"type","wheel"},{"target","worksheet.grid"},{"angleDelta",angle}});QApplication::processEvents();
    events.push_back({{"angleDelta",angle},{"beforeValue",before},{"afterValue",bar->value()},{coordinate,first()}});stationary=before==bar->value()?stationary+1:0;require(stationary<64,"Native wheel cannot reach the requested scroll position");
   }
   require(reached,"Native scrollbar navigation exceeded the bounded replay limit");
  }
  return {{"nativeScrollbarKeys",keys},{"wheelEvents",events}};
 }
 if(action.at("type")=="modifier_release"){
  auto* target=visibleTarget(window_,action);
  require(target&&target->isVisible()&&target->isEnabled(),"Replay modifier release target is unavailable");
  require(action.at("keys").is_array(),"Replay modifier release requires a key list");
  QList<Qt::Key> keys;
  for(const auto& item:action.at("keys")){
   require(item.is_string(),"Replay modifier release key is invalid");
   const auto text=item.get<std::string>();
   if(text=="Shift")keys.push_back(Qt::Key_Shift);
   else if(text=="Ctrl"||text=="Control")keys.push_back(Qt::Key_Control);
   else if(text=="Alt")keys.push_back(Qt::Key_Alt);
   else if(text=="Meta")keys.push_back(Qt::Key_Meta);
   else throw std::runtime_error("Replay modifier release key is invalid: "+text);
  }
  for(const auto key:keys){QTest::keyRelease(target,key);QApplication::processEvents();}
  return Json::object();
 }
 if(action.at("type")=="key_sequence"){
  QPointer<QWidget> target=visibleTarget(window_,action);
  require(target&&target->isVisible()&&target->isEnabled(),"Replay key target is unavailable");
  const QPointer<QWidget> requested=target;
  auto popupReceiver=[&]() -> QWidget* {
   auto* popup=QApplication::activePopupWidget();
   if(!requested||!popup||!popup->isVisible())return nullptr;
   // Native popup windows retain QObject ownership across a QWidget window boundary.
   bool owned=false;for(QObject* parent=popup;parent;parent=parent->parent())if(parent==requested){owned=true;break;}
   if(!owned)return nullptr;
   auto* focused=QApplication::focusWidget();
   if(focused&&(focused==popup||popup->isAncestorOf(focused))&&focused->isVisible()&&focused->isEnabled())return focused;
   if(auto* retained=popup->focusWidget();retained&&retained->isVisible()&&retained->isEnabled())return retained;
   return popup;
  };
  require(!action.contains("releaseModifiers")||action.at("releaseModifiers").is_boolean(),"Replay modifier release flag must be boolean");
  const bool releaseModifiers=action.value("releaseModifiers",true);
  if(action.at("keys").empty())return Json::object();
  if(target->window()==window_&&!window_->isActiveWindow()&&!QApplication::activeModalWidget()){window_->activateWindow();QApplication::processEvents();}
  if(auto* popup=popupReceiver())target=popup;
  target->setFocus();
  for(const auto& item:action.at("keys")){
   if(auto* popup=popupReceiver())target=popup;
   require(target&&target->isVisible()&&target->isEnabled(),"Replay key target became unavailable");
   const QPointer<QWidget> focusBefore=QApplication::focusWidget();
   const auto text=QString::fromStdString(item.get<std::string>());
   if(text=="Alt")QTest::keyClick(target,Qt::Key_Alt);
   else{
    const auto sequence=QKeySequence::fromString(text,QKeySequence::PortableText);
    require(sequence.count()==1&&sequence[0].key()!=Qt::Key_unknown,"Replay key is invalid: "+text.toStdString());
    const auto modifiers=sequence[0].keyboardModifiers();
    if(releaseModifiers){
     QTest::keyClick(target,sequence[0].key(),modifiers);
     for(const auto& [modifier,key]:{std::pair{Qt::MetaModifier,Qt::Key_Meta},std::pair{Qt::AltModifier,Qt::Key_Alt},std::pair{Qt::ShiftModifier,Qt::Key_Shift},std::pair{Qt::ControlModifier,Qt::Key_Control}})
      if(modifiers.testFlag(modifier))QTest::keyRelease(target,key);
    }else{
     QTest::keyPress(target,sequence[0].key(),modifiers);
     QTest::keyRelease(target,sequence[0].key());
    }
   }
   QApplication::processEvents();
   if(auto* active=dynamic_cast<MainWindow*>(QApplication::activeWindow());active&&active->isVisible()){window_=active;configureWindow(active);}
   if(!window_||!window_->isVisible()){MainWindow* remaining=nullptr;for(auto* widget:QApplication::topLevelWidgets())if(auto* candidate=dynamic_cast<MainWindow*>(widget);candidate&&candidate->isVisible()){require(!remaining,"Native close left multiple windows without an active target");remaining=candidate;}require(remaining,"Native close left no workbook window");window_=remaining;remaining->activateWindow();if(auto* grid=remaining->findChild<QTableView*>("worksheet.grid"))grid->setFocus();QApplication::processEvents();}
   if(auto* focused=QApplication::focusWidget();focused&&focused!=focusBefore&&focused->isVisible()&&focused->isEnabled())target=focused;
  }
  return Json::object();
 }
 throw std::runtime_error("Unsupported native UI replay action: "+action.at("type").get<std::string>());
}
Json NativeActionReplay::execute(const Json& step){
 Json record={{"id",step.at("id")},{"videoTime",step.at("videoTime")},
              {"evidence",step.at("evidence")},{"action",step.at("action")},
              {"expected",step.value("expect",Json::object())},{"before",observe()}};
 const auto kind=step.at("evidence").at("kind").get<std::string>();
 if(kind!="visual"&&kind!="acceptance"){
  record["status"]="pending";record["reason"]="Source action lacks visual verification";
 }else try{
  const QPointer<QWidget> initialModal=QApplication::activeModalWidget();QTimer unexpectedModal;
  QObject::connect(&unexpectedModal,&QTimer::timeout,&unexpectedModal,[&]{
   auto* modal=QApplication::activeModalWidget();if(!modal||modal==initialModal)return;
   if(!record.contains("unexpectedModals"))record["unexpectedModals"]=Json::array();
   const auto number=record["unexpectedModals"].size();const auto suffix=number==0?QString(".unexpected-modal.png"):QString(".unexpected-modal-%1.png").arg(number+1);
   const auto file=QString::fromStdString(step.at("id").get<std::string>())+suffix;modal->grab().save(QDir(outputDirectory_).filePath(file));
   const Json observed={{"objectName",modal->objectName().toStdString()},{"windowTitle",modal->windowTitle().toStdString()},{"text",qobject_cast<QMessageBox*>(modal)?qobject_cast<QMessageBox*>(modal)->text().toStdString():std::string()}};
   if(number==0){record["unexpectedModalScreenshot"]=file.toStdString();record["unexpectedModal"]=observed;}
   auto retained=observed;retained["screenshot"]=file.toStdString();record["unexpectedModals"].push_back(std::move(retained));
   if(auto* dialog=qobject_cast<QDialog*>(modal))dialog->reject();else modal->close();
  });
  if(step.at("action").at("type")!="modal")unexpectedModal.start(1);
  record.update(dispatch(step.at("action")));unexpectedModal.stop();QApplication::processEvents();
  require(!record.contains("unexpectedModal"),"Source action opened an undeclared native modal; actual warning pixels/text were retained before bounded native dismissal");
  require(!record.contains("nestedError"),record.value("nestedError",std::string()));
  const auto expected=step.value("expect",Json::object());bool waitForCalculation=false;if(expected.contains("cells"))for(const auto& cell:expected.at("cells"))if(cell.contains("value")||cell.contains("display")||cell.contains("valueType"))waitForCalculation=true;
  QElapsedTimer elapsed;elapsed.start();for(;;){try{verify(expected);break;}catch(const std::exception&){if(!waitForCalculation||elapsed.elapsed()>=5000)throw;QTest::qWait(10);}}
  record["status"]="passed";
 }catch(const std::exception& error){record["status"]="failed";record["error"]=error.what();}
 if(record.at("expected").contains("cells")){
  auto* grid=window_->findChild<QTableView*>("worksheet.grid");auto* model=dynamic_cast<WorksheetModel*>(grid->model());record["observedCells"]=Json::array();for(const auto& cell:record.at("expected").at("cells"))record["observedCells"].push_back(cellObservation(*model,cell.at("address")));
 }
 record["after"]=observe();
 const auto file=QString::fromStdString(step.at("id").get<std::string>())+".png";
 require(window_->grab().save(QDir(outputDirectory_).filePath(file)),"Cannot save native UI checkpoint");
 record["screenshot"]=file.toStdString();
 if(auto* editor=window_->findChild<QPlainTextEdit*>("replay.externalTextEditor");editor&&editor->isVisible()){
  const auto externalFile=QString::fromStdString(step.at("id").get<std::string>())+".external.png";
  require(editor->grab().save(QDir(outputDirectory_).filePath(externalFile)),"Cannot save external text adapter checkpoint");record["externalScreenshot"]=externalFile.toStdString();
 }
 if(auto* context=documentContext();context&&context->isVisible()){const auto contextFile=QString::fromStdString(step.at("id").get<std::string>())+".context.png";require(context->grab().save(QDir(outputDirectory_).filePath(contextFile)),"Cannot save actual document context checkpoint");record["contextScreenshot"]=contextFile.toStdString();}
 if(auto* modal=QApplication::activeModalWidget()){
  const auto modalFile=QString::fromStdString(step.at("id").get<std::string>())+".modal.png";
  require(modal->grab().save(QDir(outputDirectory_).filePath(modalFile)),"Cannot save native modal checkpoint");
  record["modalScreenshot"]=modalFile.toStdString();
 }
 QWidget* foreground=window_;if(auto* context=documentContext();context&&context->isVisible()&&context->isActiveWindow())foreground=context;
 QList<QWidget*> surfaces;auto* caption=videoCaption();auto* modal=QApplication::activeModalWidget();auto ownedByWindow=[&](QObject* widget){for(auto* owner=widget->parent();owner;owner=owner->parent())if(owner==window_)return true;return false;};
 for(auto* widget:QApplication::topLevelWidgets())if(widget!=window_&&widget->isVisible()&&(widget==caption||widget==modal||(ownedByWindow(widget)&&(widget->windowType()==Qt::Popup||widget->windowType()==Qt::ToolTip))))surfaces.push_back(widget);
 auto depth=[](QWidget* widget){int result=0;for(auto* parent=widget->parentWidget();parent;parent=parent->parentWidget())++result;return result;};auto priority=[&](QWidget* widget){if(widget==caption)return 3;if(widget->windowType()==Qt::ToolTip)return 2;for(auto* owner=static_cast<QObject*>(widget);owner;owner=owner->parent())if(owner==modal)return 1;return 0;};std::stable_sort(surfaces.begin(),surfaces.end(),[&](QWidget* a,QWidget* b){return priority(a)!=priority(b)?priority(a)<priority(b):depth(a)<depth(b);});
 if(!surfaces.isEmpty()){
  auto composite=foreground->grab();QPainter painter(&composite);Json captures=Json::array();int index=0;
  for(auto* surface:surfaces){const auto pixels=surface->grab();const auto position=surface->mapToGlobal(QPoint())-foreground->mapToGlobal(QPoint());painter.drawPixmap(position,pixels);const auto surfaceFile=QString::fromStdString(step.at("id").get<std::string>())+QString(".owned-window-%1.png").arg(index++);require(pixels.save(QDir(outputDirectory_).filePath(surfaceFile)),"Cannot save actual native owned popup/tooltip pixels");captures.push_back({{"objectName",surface->objectName().toStdString()},{"screenshot",surfaceFile.toStdString()},{"position",{position.x(),position.y()}},{"size",{surface->width(),surface->height()}},{"windowType",static_cast<int>(surface->windowType())}});
   if(surface==caption){const auto captionFile=QString::fromStdString(step.at("id").get<std::string>())+".caption.png";require(pixels.save(QDir(outputDirectory_).filePath(captionFile)),"Cannot save actual native publisher caption");record["captionScreenshot"]=captionFile.toStdString();}
  }
  painter.end();record["ownedWindowScreenshots"]=captures;const auto compositeFile=QString::fromStdString(step.at("id").get<std::string>())+".source-composite.png";require(composite.save(QDir(outputDirectory_).filePath(compositeFile)),"Cannot save disclosed native owned-window composition");record["sourceCompositeScreenshot"]=compositeFile.toStdString();record["sourceComposition"]={{"baseSurface",foreground->objectName().toStdString()},{"method","Actual foreground native workbook or read-only PDF, owned popup/keytip windows, active modal and publisher caption grabs composed at their real global-coordinate offsets. Separate native grabs are retained; original MainWindow screenshot is unchanged."}};
 }
 bool sourceGeometry=false;for(const auto& [source,rect]:windowGeometryOverrides_)if(source==window_){sourceGeometry=true;break;}
 if(sourceGeometry&&replayWindowSize_.isValid()){
  QPixmap canvas(replayWindowSize_);canvas.fill(QColor("#D9D9D9"));QPainter painter(&canvas);Json sources=Json::array();auto draw=[&](QWidget* widget){const auto at=widget->mapToGlobal(QPoint());painter.drawPixmap(at,widget->grab());sources.push_back({{"objectName",widget->objectName().toStdString()},{"position",{at.x(),at.y()}},{"size",{widget->width(),widget->height()}}});};for(auto* widget:QApplication::topLevelWidgets())if(widget!=window_&&widget!=foreground&&widget->isVisible()&&(dynamic_cast<MainWindow*>(widget)||dynamic_cast<NativeDocumentContext*>(widget)))draw(widget);draw(window_);if(foreground!=window_)draw(foreground);for(auto* surface:surfaces)draw(surface);painter.end();const auto canvasFile=QString::fromStdString(step.at("id").get<std::string>())+".window-canvas.png";require(canvas.save(QDir(outputDirectory_).filePath(canvasFile)),"Cannot save observed native window-position canvas");record["windowCanvasScreenshot"]=canvasFile.toStdString();record["windowCanvasComposition"]={{"method","Actual visible native workbook/document and popup grabs at their global positions on the source-size canvas, with the active document above its underlying workbook. Source outer Windows border/decorations and hidden original desktop background are adapted; no worksheet pixels or values are synthesized."},{"nativeWindows",sources}};
 }
 return record;
}
void NativeActionReplay::verify(const Json& expected) const {
 const auto actual=observe();
 for(const auto& [key,value]:expected.items()){
  if(key=="externalDocument"){require(actual.contains(key),"Replay document context is unavailable");for(const auto& [field,wanted]:value.items())require(actual.at(key).contains(field)&&actual.at(key).at(field)==wanted,"Replay document context mismatch: "+field);continue;}
  if(key=="cells"){
   auto* grid=window_->findChild<QTableView*>("worksheet.grid");
   auto* model=dynamic_cast<WorksheetModel*>(grid->model());
   for(const auto& cell:value){
    const auto address=cell.at("address").get<std::string>();const auto observed=cellObservation(*model,address);
    if(cell.contains("input"))require(observed.at("input")==cell.at("input"),"Replay cell input mismatch: "+address);
    if(cell.contains("style"))for(const auto& [name,style]:cell.at("style").items())require(observed.at("style").value(name,name=="bold"?Json(false):Json())==style,"Replay cell style mismatch: "+address+" "+name);
    if(cell.contains("display")){if(observed.at("input").get_ref<const std::string&>().starts_with('='))require(observed.contains("value"),"Replay calculated display is pending: "+address);require(observed.at("display")==cell.at("display"),"Replay cell display mismatch: "+address);}
    if(cell.contains("valueTolerance")){
     const auto& bounds=cell.at("valueTolerance");require(bounds.is_object()&&!bounds.empty(),"Replay numeric tolerance must specify finite nonnegative bounds");
     for(const auto& [name,bound]:bounds.items())require((name=="absolute"||name=="relative")&&bound.is_number()&&std::isfinite(bound.get<double>())&&bound.get<double>()>=0,"Replay numeric tolerance bound is invalid");
     require(cell.contains("value")&&cell.at("value").is_number()&&observed.contains("value")&&observed.at("value").is_number(),"Replay numeric tolerance requires actual and expected numeric values: "+address);
     const double actual=observed.at("value").get<double>(),wanted=cell.at("value").get<double>();require(std::isfinite(actual)&&std::isfinite(wanted),"Replay numeric tolerance requires finite values");
     const double limit=std::max(bounds.value("absolute",0.0),bounds.value("relative",0.0)*std::max(std::abs(actual),std::abs(wanted)));require(std::abs(actual-wanted)<=limit,"Replay cell value outside tolerance: "+address);
    }else if(cell.contains("value"))require(observed.contains("value")&&observed.at("value")==cell.at("value"),"Replay cell value mismatch: "+address);
    if(cell.contains("valueType"))require(observed.contains("valueType")&&observed.at("valueType")==cell.at("valueType"),"Replay cell valueType mismatch: "+address);
   }
   continue;
  }
  require(actual.contains(key)&&actual.at(key)==value,"Replay checkpoint mismatch: "+key);
 }
}
Json NativeActionReplay::run(const Json& manifest){
 if(manifest.contains("windowSize")){
  const auto& size=manifest.at("windowSize");require(size.is_array()&&size.size()==2&&size[0].is_number_integer()&&size[1].is_number_integer()&&size[0]>0&&size[1]>0,"Replay windowSize requires positive integer width and height");
  replayWindowSize_=QSize(size[0].get<int>(),size[1].get<int>());configureWindow(window_);QApplication::processEvents();
 }
 Json report={{"version",1},{"sourceVideo",manifest.value("sourceVideo",std::string())},
              {"scope",manifest.value("scope",std::string())},{"steps",Json::array()},
              {"executionComplete",true},{"complete",false},
              {"sourceReviewComplete",manifest.value("sourceReviewComplete",false)}};
 bool allVisual=!manifest.at("steps").empty();
 QFile trace(QDir(outputDirectory_).filePath("action-trace.jsonl"));
 require(trace.open(QIODevice::WriteOnly|QIODevice::Truncate),"Cannot write replay trace");
 for(const auto& step:manifest.at("steps")){
  auto record=execute(step);if(record.at("status")!="passed")report["executionComplete"]=false;
  allVisual=allVisual&&allSourceVisual(record);
  report["steps"].push_back(record);trace.write(QByteArray::fromStdString(record.dump()+"\n"));trace.flush();
  if(record["status"]=="failed")break;
 }
 report["complete"]=report.at("executionComplete")==true&&allVisual&&report.at("sourceReviewComplete")==true;
 QFile summary(QDir(outputDirectory_).filePath("action-replay.json"));
 require(summary.open(QIODevice::WriteOnly|QIODevice::Truncate),"Cannot write replay report");
 summary.write(QByteArray::fromStdString(report.dump(2)+"\n"));
 return report;
}
}

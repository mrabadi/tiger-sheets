#include "tigersheets/main_window.hpp"
#include "tigersheets/worksheet_find_replace.hpp"
#include "tigersheets/biff.hpp"
#include "tigersheets/command_icons.hpp"
#include "tigersheets/paste_special.hpp"
#include "tigersheets/tutorial_shortcuts.hpp"
#include "tigersheets/external_reference_entry.hpp"
#include "tigersheets/quick_analysis.hpp"
#include "tigersheets/worksheet_error_indicator.hpp"
#include "tigersheets/review_ribbon.hpp"
#include "tigersheets/view_ribbon.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/worksheet_view.hpp"
#include "tigersheets/worksheet_sheet_bar.hpp"
#include "tigersheets/worksheet_context_menu.hpp"
#include "tigersheets/worksheet_edit_actions.hpp"
#include "tigersheets/worksheet_cell_format.hpp"
#include "tigersheets/worksheet_layout.hpp"
#include "tigersheets/worksheet_validation_ui.hpp"
#include "tigersheets/ribbon_page.hpp"
#include "tigersheets/formula_editor.hpp"
#include "tigersheets/formula_bar_controls.hpp"
#include "tigersheets/calculation.hpp"
#include "tigersheets/calculation_scheduler.hpp"
#include "tigersheets/workbook_links.hpp"
#include "tigersheets/live_workbooks.hpp"
#include "tigersheets/printing.hpp"
#include "tigersheets/print_settings.hpp"
#include "tigersheets/references.hpp"
#include "tigersheets/ooxml.hpp"
#include "tigersheets/csv.hpp"
#include "tigersheets/theme.hpp"
#include "tigersheets/chart.hpp"
#include "tigersheets/chart_editor.hpp"
#include "tigersheets/codex.hpp"
#include "tigersheets/codex_controls.hpp"
#include "tigersheets/workbook_tools.hpp"
#include "tigersheets/analysis.hpp"
#include "tigersheets/recovery.hpp"
#include "tigersheets/vba.hpp"
#include "tigersheets/vba_ui.hpp"
#include "tigersheets/solver.hpp"
#include "tigersheets/modeling_ui.hpp"
#include "tigersheets/financial_model_pane.hpp"
#include "tigersheets/financial_workspace.hpp"
#include "tigersheets/pivot.hpp"
#include "tigersheets/pivot_conversion.hpp"
#include "tigersheets/pivot_fields_pane.hpp"
#include "tigersheets/report_worker.hpp"
#include "tigersheets/snapshot_capture.hpp"
#include "tigersheets/selection_statistics.hpp"
#include "tigersheets/formatting.hpp"
#include <QAction>
#include <algorithm>
#include <atomic>
#include <charconv>
#include <cmath>
#include <map>
#include <set>
#include <QFileInfo>
#include <QApplication>
#include <QElapsedTimer>
#include <QVariantMap>
#include <QClipboard>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFontComboBox>
#include <QFormLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMenuBar>
#include <QMenu>
#include <QScopedValueRollback>
#include <QScopeGuard>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSlider>
#include <QPainter>
#include <QMouseEvent>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabBar>
#include <QTextBrowser>
#include <QTextCharFormat>
#include <QTimer>
#include <QTemporaryFile>
#include <QThread>
#include <QPointer>
#include <QToolButton>
#include <QUuid>
#include <optional>
#include <QVBoxLayout>
#include <QTableView>
#include <QLineEdit>
#include <QTabWidget>
#include <QHeaderView>
static void initializeTigerResources(){Q_INIT_RESOURCE(tiger_sheets);}
namespace tigersheets {
namespace {
class WorksheetRibbonTabs final:public QTabBar {
public:
 explicit WorksheetRibbonTabs(QWidget* parent):QTabBar(parent){}
 QSize tabSizeHint(int index)const override{auto size=QTabBar::tabSizeHint(index);size.setHeight(32);return size;}
};
class WorksheetRibbon final:public QTabWidget {
public:
 explicit WorksheetRibbon(QWidget* parent):QTabWidget(parent){setTabBar(new WorksheetRibbonTabs(this));}
};
int numberFormatCategory(const std::string& format){
 const auto code=QString::fromStdString(format).trimmed();
 if(code.isEmpty()||code.compare("General",Qt::CaseInsensitive)==0)return 0;
 if(code=="@")return 6;
 if(dateNumberFormat(format)){bool quoted=false;for(int i=0;i<code.size();++i){const auto ch=code[i];if(ch=='\\'){++i;continue;}if(ch=='"'){quoted=!quoted;continue;}if(quoted&&ch.isLetter())return 7;}return 5;}
 const auto isCurrency=[](QChar ch){return ch=='$'||ch==QChar(0x20ac)||ch==QChar(0x00a3)||ch==QChar(0x00a5);};
 bool quoted=false,spacing=false,percent=false,currency=false;
 for(int i=0;i<code.size();++i){
  const auto ch=code[i];if(ch=='"'){quoted=!quoted;continue;}
  if(ch=='\\'){if(i+1<code.size())currency|=isCurrency(code[++i]);continue;}
  if(!quoted&&ch=='['){const auto close=code.indexOf(']',i+1);if(close<0)break;auto directive=code.mid(i+1,close-i-1);if(directive.startsWith('$')){directive.remove(0,1);const auto locale=directive.lastIndexOf('-');if(locale>=0)directive.truncate(locale);currency|=!directive.isEmpty();}i=close;continue;}
  currency|=isCurrency(ch);if(quoted)continue;
  if(ch=='_'||ch=='*'){spacing=true;++i;continue;}if(ch=='%')percent=true;
 }
 if(percent)return 4;
 static const QRegularExpression zeroDash(";\\s*(?:\"-\"|-)\\s*$");
 if((currency&&spacing)||(code.count(';')>=2&&zeroDash.match(code).hasMatch()))return 3;
 if(currency)return 2;
 static const QRegularExpression number("^[0#?,]+(?:\\.[0#?]+)?(?:;(?:-?[0#?,]+(?:\\.[0#?]+)?|\\([0#?,]+(?:\\.[0#?]+)?\\)))?$");
 return number.match(code).hasMatch()?1:7;
}
struct CodexToolJob {
 std::shared_ptr<Json> request;
 WorkbookTools::ProposalState previous;
 std::shared_ptr<const Json> operations;
 std::unique_ptr<WorkbookSession::PreparedTransaction> prepared;
 Json reply,returned;
 std::function<void(Json)> completion;
 std::atomic<bool> cancelled{false};
 std::uint64_t serial=0,revision=0,context=0;
 std::string workbookId,error;
};
// Large captured workbooks, returned caches and retired previews are released
// away from the event loop, including cancellation and stale-result paths.
std::shared_ptr<CodexToolJob> newCodexToolJob(){return {new CodexToolJob,[](CodexToolJob* job){auto* thread=QThread::create([job]{delete job;});QObject::connect(thread,&QThread::finished,thread,&QObject::deleteLater);thread->start();}};}
}
struct MainWindow::State {
 WorkbookSession session;WorksheetModel* model{};QTableView* grid{};QLineEdit* formula{};QLineEdit* name{};
 QTabWidget* ribbon{};QTabBar* sheets{};QComboBox* numberFormats{};QLabel* saveState{};QLabel* saveIndicator{};QLabel* selection{};QLabel* filename{};QTimer* timer{};SelectionStatistics* statistics{};std::uint64_t selectionSerial=0;
 QFontComboBox* fontFamily{};QSpinBox* fontSize{};QToolButton* boldControl{};QToolButton* italicControl{};QToolButton* underlineControl{};bool ribbonCollapsed=false;
 void presentRibbon(MainWindow& window,bool peek=false){
  if(!ribbon||!ribbon->currentWidget())return;auto* stack=ribbon->findChild<QStackedWidget*>();if(!stack)return;auto* page=dynamic_cast<RibbonPage*>(ribbon->currentWidget());const int overflow=page?page->overflowHeight():0;const int pageHeight=page&&page->property("ribbon.pageHeight").isValid()?page->property("ribbon.pageHeight").toInt():72;
  if(ribbonCollapsed){
   window.setProperty("ribbon.collapsed",true);ribbon->setFixedHeight(ribbon->tabBar()->sizeHint().height());
   if(stack->windowType()!=Qt::Popup)stack->setWindowFlags(Qt::Popup|Qt::FramelessWindowHint);
   stack->setObjectName("ribbon.peek");window.setProperty("ribbon.peek",peek);
   if(peek){ribbon->currentWidget()->show();stack->setFixedSize(ribbon->width(),(page&&page->property("ribbon.contentHeight").isValid()?page->property("ribbon.contentHeight").toInt():pageHeight-ribbon->tabBar()->sizeHint().height())+overflow);stack->move(ribbon->tabBar()->mapToGlobal(QPoint(0,ribbon->tabBar()->height())));stack->show();anchorRibbonPopup(*ribbon,*stack);}else stack->hide();
  }else{
   window.setProperty("ribbon.collapsed",false);window.setProperty("ribbon.peek",false);stack->setMinimumSize(0,0);stack->setMaximumSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX);if(stack->windowType()==Qt::Popup)stack->setWindowFlags(Qt::Widget);stack->setObjectName("qt_tabwidget_stackedwidget");ribbon->setFixedHeight(pageHeight+overflow);ribbon->currentWidget()->show();stack->show();if(ribbon->layout())ribbon->layout()->activate();
  }
 }
 struct SheetPosition {QModelIndex current;QItemSelection selection;int horizontal=0,vertical=0;int topRow=0,topColumn=0,zoom=100;bool native=false;};
 std::map<QString,SheetPosition> sheetPositions;
 std::map<std::uint64_t,std::pair<QString,SheetPosition>> editPositions;
 bool restoringHistory=false;
 std::map<QString,Json> loadedSheetViews;
 QString loadedActiveSheet;
 QString copySheet;QItemSelectionRange copyRange;bool copyPasting=false,publishingClipboard=false;Json clipboardFallback;
 QDockWidget* dock{};CodexClient* codex{};CodexControls* codexControls{};QTextBrowser* transcript{};QPlainTextEdit* composer{};QLabel* connection{};QPushButton* codexConnection{};QPushButton* codexSend{};QPushButton* codexStop{};QWidget* previewBar{};
 std::optional<WorkbookPackage> package;std::string originalBytes;QString path,error;Json calculated=Json::object();std::optional<std::uint64_t> previewContext;bool dirty=false,showFormulas=false;
 QTableView* frozen{};CalculationScheduler* scheduler{};Json retainedLinks=Json::array();std::uint64_t contextGeneration=0;QLabel* linkStatus{};
 std::shared_ptr<LiveWorkbookRegistration> liveSource;bool liveInvalidationPending=false;
 QDockWidget* pivotDock{};PivotFieldsPane* pivotPane{};SnapshotCapture* calcCapture{};std::uint64_t calcSerial=0;SnapshotCapture* reportCapture{};ReportWorker* reportWorker{};QPushButton* reportStop{};
 std::string reportConversionId;Json reportDefinition,reportPrefix=Json::array();std::shared_ptr<Json> reportBook,reportInputs;QPointer<QThread> reportPublication;std::uint64_t reportRevision=0,reportContext=0,reportSerial=0;
 std::unique_ptr<WorkbookTools> workbookTools;
 QDockWidget* financialDock{};FinancialModelPane* financialPane{};FinancialWorkspace* financialWorkspace{};std::string financialOutputSheet,financialAction;QString financialSummary;Json financialSelection=Json::object();
 SnapshotCapture* toolCapture{};ReportWorker* toolWorker{};QPointer<QThread> toolPublication;std::shared_ptr<CodexToolJob> toolJob;std::uint64_t toolSerial=0;
 std::unique_ptr<RecoveryDriver> recovery;std::string recoveryPath;std::string recoveryId=QUuid::createUuid().toString().toStdString();
 std::vector<ChartWidget*> charts;
 Json functionCatalog,availableFunctions;
 WorksheetValidationUi* validation{};
 QStringList completionNames,completionFunctions;
 void refreshCompletions(const Json& book){
  completionNames.clear();completionFunctions.clear();
  const QRegularExpression lambda("^=?\\s*LAMBDA\\s*\\(",QRegularExpression::CaseInsensitiveOption);
  const auto names=book.value("names",Json::object());
  for(const auto& [key,value]:names.items()){
   const auto name=QString::fromStdString(key);
   if(value.is_string()&&lambda.match(QString::fromStdString(value.get<std::string>())).hasMatch())completionFunctions<<name;else completionNames<<name;
  }
  const auto vba=book.value("vba",Json::object());
  if(vba.value("enabled",false)){
   const QRegularExpression declaration("^\\s*(?:Public\\s+)?(?:Static\\s+)?Function\\s+([A-Za-z_][A-Za-z0-9_]*)\\s*\\(",QRegularExpression::CaseInsensitiveOption|QRegularExpression::MultilineOption);
   for(const auto& module:vba.value("modules",Json::array())){auto matches=declaration.globalMatch(QString::fromStdString(module.value("source",std::string())));while(matches.hasNext())completionFunctions<<matches.next().captured(1);}
  }
  if(auto* editor=dynamic_cast<FormulaLineEdit*>(formula))editor->setWorkbookCompletions(completionNames,completionFunctions);
 }
 QItemSelectionRange range()const{auto r=grid->selectionModel()->selection();auto i=grid->currentIndex();if(!i.isValid())i=model->index(0,0);return r.isEmpty()?QItemSelectionRange(i,i):r.front();}
 void syncHeaderLayout(MainWindow& window){
  auto* header=saveIndicator->parentWidget();auto* layout=header->layout();auto* menus=window.menuBar();auto* identity=window.findChild<QWidget*>("workspace.identity");
  const auto controls=header->findChildren<QToolButton*>(QString(),Qt::FindDirectChildrenOnly);for(auto* control:controls){if(!control->property("workspace.normalButtonStyle").isValid())control->setProperty("workspace.normalButtonStyle",int(control->toolButtonStyle()));control->setToolButtonStyle(Qt::ToolButtonStyle(control->property("workspace.normalButtonStyle").toInt()));}
  filename->show();layout->setContentsMargins(4,4,8,4);layout->setSpacing(4);layout->invalidate();int menuWidth=0;for(auto* action:menus->actions())if(action->isVisible())menuWidth+=std::max(menus->actionGeometry(action).width(),menus->fontMetrics().horizontalAdvance(action->text())+20);
  const int available=window.width()-(identity?identity->sizeHint().width():0)-menuWidth-16;
  if(layout->sizeHint().width()>available){filename->hide();layout->setContentsMargins(2,4,2,4);layout->setSpacing(2);for(auto* control:controls)control->setToolButtonStyle(Qt::ToolButtonIconOnly);}
  layout->invalidate();layout->activate();header->setFixedWidth(layout->sizeHint().width());menus->setCornerWidget(header,Qt::TopRightCorner);
 }
 void syncSaveState(MainWindow& window,const QString& detail={}){
  const bool sourceIdentityChanged=window.property("workbook.path").toString()!=path||window.property("workbook.name").toString()!=filename->text();
  window.setProperty("workbook.path",path);window.setProperty("workbook.name",filename->text());
  if(sourceIdentityChanged&&liveSource)notifyLiveWorkbookChanged(liveSource.get());
  const bool unsaved=dirty||path.isEmpty();const auto text=dirty?QString("Unsaved changes"):path.isEmpty()?QString("Not saved"):QString("Saved locally");
  const auto description=path.isEmpty()?QString("This workbook has not been saved to disk. Use Save to choose a file."):dirty?QString("Changes to %1 have not been saved.").arg(QDir::toNativeSeparators(path)):QString("Saved to %1").arg(QDir::toNativeSeparators(path));
  saveIndicator->setText(text);saveIndicator->setAccessibleName("Workbook save status: "+text);saveIndicator->setAccessibleDescription(description);saveIndicator->setToolTip(description);
  saveIndicator->setStyleSheet(unsaved?"QLabel { color: #743d00; background: #fff0c2; border: 1px solid #d9a12e; border-radius: 3px; padding: 3px 8px; font-weight: 600; }":"QLabel { color: #245648; background: #e7efe6; border: 1px solid #cbdcc8; border-radius: 3px; padding: 3px 8px; font-weight: 600; }");
  saveIndicator->ensurePolished();saveIndicator->setMinimumWidth(saveIndicator->sizeHint().width());
  saveState->setText(detail.isEmpty()?QString("Ready"):detail);filename->setToolTip(path.isEmpty()?filename->text():QDir::toNativeSeparators(path));window.setWindowFilePath(path);window.setWindowTitle(filename->text()+"[*] — TigerSheets");window.setWindowModified(unsaved);syncHeaderLayout(window);
 }
 void refreshCopyOutline(){static_cast<WorksheetView*>(grid)->setCopyRange(copySheet,copyRange);if(frozen)static_cast<WorksheetView*>(frozen)->setCopyRange(copySheet,copyRange);}
 void clearCopyOutline(){copySheet.clear();copyRange=QItemSelectionRange();clipboardFallback=Json();refreshCopyOutline();if(saveState&&saveState->text()=="Select destination and press ENTER or choose Paste")saveState->setText("Ready");}
 void syncFontControls(){if(!fontFamily||!fontSize)return;const auto current=grid->currentIndex();const auto style=current.isValid()?model->cellStyle(current.row(),current.column()):Json::object();const QSignalBlocker familyBlocked(fontFamily),sizeBlocked(fontSize);const auto declaredFamily=QString::fromStdString(style.value("fontFamily",std::string("Calibri")));fontFamily->setCurrentFont(QFont(declaredFamily));fontFamily->setEditText(declaredFamily);const auto entryFontSize=grid->property("worksheet.entryFontSize");const auto* sizeEditor=fontSize->findChild<QLineEdit*>();const bool sizeDraft=sizeEditor&&(fontSize->hasFocus()||sizeEditor->hasFocus())&&sizeEditor->isModified()&&fontSize->cleanText()!=QString::number(fontSize->value());if(!sizeDraft)fontSize->setValue(qRound(entryFontSize.isValid()?entryFontSize.toDouble():style.value("fontSize",11.0)));for(const auto& entry:{std::pair{boldControl,"bold"},std::pair{italicControl,"italic"},std::pair{underlineControl,"underline"}})if(entry.first){const QSignalBlocker blocked(entry.first);entry.first->setChecked(style.value(entry.second,false));}}
 void syncNumberFormat(){const QSignalBlocker blocked(numberFormats);const auto current=grid->currentIndex();numberFormats->setCurrentIndex(current.isValid()?numberFormatCategory(model->cellStyle(current.row(),current.column()).value("numberFormat",std::string("General"))):0);syncFontControls();}
 void syncFormulaBar(){if(formula->isModified()||static_cast<WorksheetView*>(grid)->activeCellEditor())return;const auto current=grid->currentIndex();if(!current.isValid())return;const auto text=model->data(current,Qt::EditRole).toString();if(formula->text()!=text)formula->setText(text);}
 static Json nativeSheetView(const SheetPosition& position){
  auto address=[](int row,int column){return columnName(column+1)+std::to_string(row+1);};std::string selected;for(const auto& range:position.selection){if(!selected.empty())selected+=' ';const auto first=address(range.top(),range.left()),last=address(range.bottom(),range.right());selected+=first==last?first:first+":"+last;}const auto current=position.current.isValid()?address(position.current.row(),position.current.column()):std::string("A1");return {{"activeCell",current},{"selection",selected.empty()?current:selected},{"topLeftCell",address(position.topRow,position.topColumn)},{"zoomScale",position.zoom}};
 }
 SheetPosition captureSheetPosition()const{
  const auto* zoom=grid->window()->findChild<QSlider*>("worksheet.zoom");return {grid->currentIndex(),grid->selectionModel()->selection(),grid->horizontalScrollBar()->value(),grid->verticalScrollBar()->value(),std::max(0,grid->rowAt(0)),std::max(0,grid->columnAt(0)),zoom?zoom->value():100,false};
 }
 void restoreSheetPosition(const SheetPosition& position){
  if(auto* zoom=grid->window()->findChild<QSlider*>("worksheet.zoom"))zoom->setValue(position.zoom);
  grid->setCurrentIndex(position.current);grid->selectionModel()->select(position.selection,QItemSelectionModel::ClearAndSelect);int horizontal=position.horizontal;if(position.native){auto* header=grid->horizontalHeader();horizontal=header->sectionPosition(position.topColumn);if(grid->horizontalScrollMode()==QAbstractItemView::ScrollPerItem){const int visual=header->visualIndex(position.topColumn);horizontal=visual;if(header->hiddenSectionCount())for(int index=0;index<visual;++index)if(header->isSectionHidden(header->logicalIndex(index)))--horizontal;}}grid->horizontalScrollBar()->setValue(std::max(0,horizontal));grid->verticalScrollBar()->setValue(position.native?grid->verticalHeader()->sectionPosition(position.topRow):position.vertical);
  syncFormulaBar();syncNumberFormat();if(position.current.isValid())name->setText(QString::fromStdString(columnName(position.current.column()+1))+QString::number(position.current.row()+1));
 }
 void switchSheet(const QString& id){
  if(id==model->sheetId())return;
  sheetPositions[model->sheetId()]=captureSheetPosition();
  model->setSheetId(id);const auto found=sheetPositions.find(id);const SheetPosition position=found==sheetPositions.end()?SheetPosition{model->index(0,0),QItemSelection(model->index(0,0),model->index(0,0))}:found->second;restoreSheetPosition(position);
 }
 void restoreWorkbookViews(){
  const auto& book=session.snapshotView();sheetPositions.clear();loadedSheetViews.clear();
  for(const auto& sheet:book.at("sheets"))if(sheet.contains("nativeView")){
   const auto& view=sheet.at("nativeView");const auto active=formulaReferences("="+view.value("activeCell",std::string("A1"))),top=formulaReferences("="+view.value("topLeftCell",std::string("A1")));SheetPosition position;position.current=active.empty()?model->index(0,0):model->index(active.front().row1-1,active.front().col1-1);position.selection=QItemSelection(position.current,position.current);position.native=true;position.zoom=view.value("zoomScale",100);if(!top.empty()){position.topRow=top.front().row1-1;position.topColumn=top.front().col1-1;}const auto refs=formulaReferences("="+view.value("selection",view.value("activeCell",std::string("A1"))));if(!refs.empty()){position.selection.clear();for(const auto& ref:refs)position.selection.select(model->index(ref.row1-1,ref.col1-1),model->index(ref.row2-1,ref.col2-1));}sheetPositions[QString::fromStdString(sheet.at("id").get<std::string>())]=std::move(position);
  }
  const int active=std::clamp(book.value("activeSheetIndex",0),0,static_cast<int>(book.at("sheets").size())-1);const auto id=QString::fromStdString(book.at("sheets")[active].at("id").get<std::string>());model->setSheetId(id);const auto found=sheetPositions.find(id);restoreSheetPosition(found==sheetPositions.end()?SheetPosition{model->index(0,0),QItemSelection(model->index(0,0),model->index(0,0))}:found->second);
  loadedActiveSheet=id;for(const auto& sheet:book.at("sheets")){const auto sheetId=QString::fromStdString(sheet.at("id").get<std::string>());const auto position=sheetPositions.find(sheetId);loadedSheetViews[sheetId]=nativeSheetView(sheetId==id?captureSheetPosition():position==sheetPositions.end()?SheetPosition{model->index(0,0),QItemSelection(model->index(0,0),model->index(0,0))}:position->second);}
 }
 Json snapshotWithViews(){
  sheetPositions[model->sheetId()]=captureSheetPosition();auto book=session.snapshot();int index=0;for(auto& sheet:book.at("sheets")){const auto id=QString::fromStdString(sheet.at("id").get<std::string>());if(id==model->sheetId()&&(id!=loadedActiveSheet||(book.contains("activeSheetIndex")&&book.at("activeSheetIndex")!=index)))book["activeSheetIndex"]=index;const auto found=sheetPositions.find(id);if(found!=sheetPositions.end()){const auto view=nativeSheetView(found->second);const auto loaded=loadedSheetViews.find(id);if(loaded==loadedSheetViews.end()||loaded->second!=view){auto saved=sheet.value("nativeView",Json::object());saved.update(view);sheet["nativeView"]=std::move(saved);}}++index;}return book;
 }
 const Json& bookView()const{const auto* preview=session.previewView();return preview?*preview:session.snapshotView();}
 void rebuildSheets(){QSignalBlocker block(sheets);while(sheets->count())sheets->removeTab(0);int i=0,current=0;const auto& book=bookView();for(const auto& sh:book["sheets"]){sheets->addTab(QString::fromStdString(sh["name"].get<std::string>()));sheets->setTabData(i,QString::fromStdString(sh["id"].get<std::string>()));if(sh["id"]==model->sheetId().toStdString())current=i;++i;}sheets->setCurrentIndex(current);refreshCompletions(book);}
 void refreshFinancialFields(){if(!financialPane)return;Json sheetList=Json::array();for(const auto& sheet:session.snapshotView().at("sheets"))sheetList.push_back({{"id",sheet.at("id")},{"name",sheet.at("name")}});const auto selected=range();Json selection={{"sheet",model->sheetId().toStdString()},{"row",selected.top()+1},{"col",selected.left()+1}};if(const auto* cell=session.cellAt(model->sheetId().toStdString(),selected.top()+1,selected.left()+1)){const auto raw=cell->at("input").get<std::string>();double number=0;const auto parsed=std::from_chars(raw.data(),raw.data()+raw.size(),number);if(raw.empty()){selection["type"]="blank";selection["value"]=nullptr;}else if(raw.starts_with('\'')){selection["type"]="string";selection["value"]=raw.substr(1);}else if(raw=="TRUE"||raw=="FALSE"){selection["type"]="boolean";selection["value"]=raw=="TRUE";}else if(parsed.ec==std::errc()&&parsed.ptr==raw.data()+raw.size()&&std::isfinite(number)){selection["type"]="number";selection["value"]=number;}else if(!raw.starts_with('=')){selection["type"]="string";selection["value"]=raw;}}financialSelection=selection;const auto definitions=session.snapshotView().find("financialModels");static const Json empty=Json::array();financialPane->setWorkbookContext(sheetList,model->sheetId(),selection,definitions==session.snapshotView().end()?empty:*definitions);financialPane->setIterationSettings(session.snapshotView().value("settings",Json::object()));}
 void format(const Json& style){auto r=range();Json ops=Json::array();const auto id=model->sheetId().toStdString();if(r.height()==MaxRows&&r.width()==MaxColumns){for(const auto& sheet:session.snapshotView().at("sheets"))if(sheet.at("id")==id){auto defaults=sheet.value("defaultStyle",Json::object());defaults.update(style);Json layout={{"type","set_sheet_layout"},{"sheet",id},{"defaultStyle",defaults}};if(style.contains("fontSize")||style.contains("fontFamily"))layout["defaultRowHeight"]=worksheetFontDefaultRowHeight(*grid,defaults);ops.push_back(std::move(layout));for(const auto& cell:sheet.at("cells"))ops.push_back({{"type","set_style"},{"sheet",id},{"row",cell.at("row")},{"col",cell.at("col")},{"style",style}});break;}}else{if(static_cast<long long>(r.height())*r.width()>100000)return;for(int row=r.top();row<=r.bottom();++row)for(int col=r.left();col<=r.right();++col)ops.push_back({{"type","set_style"},{"sheet",id},{"row",row+1},{"col",col+1},{"style",style}});}model->apply(ops);}
};
MainWindow::MainWindow(QWidget* p):QMainWindow(p),state_(std::make_unique<State>()){
 initializeTigerResources();auto& s=*state_;setWindowTitle("Book1.xlsx — TigerSheets");resize(1380,900);setMinimumSize(820,600);applyAppTheme(this);
 QIcon icon;for(int size:{16,24,32,48,64,128,256,512})icon.addFile(QString(":/icons/tiger-sheets-%1.png").arg(size),QSize(size,size));setWindowIcon(icon);
 auto* body=new QWidget(this);auto* layout=new QVBoxLayout(body);layout->setContentsMargins(0,0,0,0);layout->setSpacing(0);
 auto makeButton=[this](QWidget* parent,QString text,std::function<void()> fn){auto* b=new QToolButton(parent);b->setText(text);b->setIcon(spreadsheetCommandIcon(text));b->setIconSize(QSize(18,18));b->setToolButtonStyle(Qt::ToolButtonIconOnly);const auto title=text=="↶"?QString("Undo"):text=="↷"?QString("Redo"):text;b->setAccessibleName(title);b->setToolTip(title);b->setAutoRaise(true);b->setMinimumHeight(24);connect(b,&QToolButton::clicked,this,fn);return b;};
 auto* menus=menuBar();menus->setNativeMenuBar(false);menus->setObjectName("workspace.menubar");menus->setFixedHeight(34);
 auto* identity=new QWidget(menus);identity->setObjectName("workspace.identity");auto* identityLayout=new QHBoxLayout(identity);identityLayout->setContentsMargins(10,4,10,4);identityLayout->setSpacing(7);auto* mascot=new QLabel(identity);mascot->setObjectName("workspace.logo");mascot->setPixmap(icon.pixmap(28,28));mascot->setAccessibleName("TigerSheets");identityLayout->addWidget(mascot);auto* brand=new QLabel("TigerSheets",identity);brand->setObjectName("workspace.brand");identityLayout->addWidget(brand);menus->setCornerWidget(identity,Qt::TopLeftCorner);
 auto* header=new QWidget(menus);header->setObjectName("workspace.header");auto* hl=new QHBoxLayout(header);hl->setContentsMargins(4,4,8,4);hl->setSpacing(4);s.filename=new QLabel("Book1.xlsx",header);s.filename->setObjectName("workspace.documentName");s.filename->setMaximumWidth(150);hl->addWidget(s.filename);s.saveIndicator=new QLabel(header);s.saveIndicator->setObjectName("workspace.saveState");hl->addWidget(s.saveIndicator);
 for(auto entry:{std::pair{"Open","open"},std::pair{"↶","undo"},std::pair{"↷","redo"},std::pair{"Save","save"},std::pair{"Find","find"},std::pair{"Codex","codex.toggle"}}){auto* b=makeButton(header,entry.first,[this,c=QString(entry.second)]{executeCommand(c);});b->setObjectName("command."+QString(entry.second));if(QString(entry.second)=="save")b->setProperty("role","primary");hl->addWidget(b);}menus->setCornerWidget(header,Qt::TopRightCorner);
 s.ribbon=new WorksheetRibbon(body);s.ribbon->setObjectName("workspace.ribbon");s.ribbon->setFixedHeight(72);s.ribbon->setDocumentMode(true);layout->addWidget(s.ribbon);
 auto tab=[&](QString title){auto* page=new RibbonPage;page->overflowChanged=[this,page](bool){if(state_->ribbon->currentWidget()==page&&!state_->ribbonCollapsed)state_->ribbon->setFixedHeight((page->property("ribbon.pageHeight").isValid()?page->property("ribbon.pageHeight").toInt():72)+page->overflowHeight());};s.ribbon->addTab(page,title);return page->contentLayout();};
 connect(s.ribbon,&QTabWidget::currentChanged,this,[this](int){state_->presentRibbon(*this,property("ribbon.peek").toBool());});
 auto group=[&](QHBoxLayout* row,QString title){auto* w=new QWidget;auto* col=new QVBoxLayout(w);col->setContentsMargins(3,0,7,0);col->setSpacing(1);auto* controls=new QHBoxLayout;controls->setSpacing(3);col->addLayout(controls);auto* caption=new QLabel(title);caption->setAlignment(Qt::AlignHCenter);caption->setStyleSheet("font-size:10px;color:#65756d;");col->addWidget(caption);row->addWidget(w);return controls;};
 auto add=[&](QHBoxLayout* row,QString label,QString command){row->addWidget(makeButton(body,label,[this,command]{executeCommand(command);}));};
 auto* home=tab("Home");auto* clip=group(home,"Clipboard");add(clip,"Paste","paste");add(clip,"Copy","copy");add(clip,"Cut","cut");add(clip,"▾","paste.special");
 auto* fonts=group(home,"Font");auto* family=new QFontComboBox;s.fontFamily=family;family->setObjectName("ribbon.fontFamily");family->setAccessibleName("Font family");family->setMaximumWidth(140);fonts->addWidget(family);auto* size=new QSpinBox;s.fontSize=size;size->setObjectName("ribbon.fontSize");size->setAccessibleName("Font size");size->setRange(6,96);size->setKeyboardTracking(false);size->setValue(11);size->setMaximumWidth(60);fonts->addWidget(size);connect(family,&QFontComboBox::currentFontChanged,this,[&s](QFont f){s.format({{"fontFamily",f.family().toStdString()}});});connect(size,qOverload<int>(&QSpinBox::valueChanged),this,[&s](int v){s.format({{"fontSize",v}});});for(const auto& entry:{std::pair{"B","style.bold"},std::pair{"I","style.italic"},std::pair{"U","style.underline"}}){auto* control=makeButton(body,entry.first,[this,command=QString(entry.second)]{executeCommand(command);});control->setCheckable(true);control->setObjectName("command."+QString(entry.second));fonts->addWidget(control);if(QString(entry.first)=="B")s.boldControl=control;else if(QString(entry.first)=="I")s.italicControl=control;else s.underlineControl=control;}add(fonts,"A","style.color");add(fonts,"Fill","style.fill");
 auto* align=group(home,"Alignment");add(align,"Left","align.left");add(align,"Center","align.center");add(align,"Right","align.right");add(align,"Wrap","style.wrap");
 auto* number=group(home,"Number");auto* formats=new QComboBox;s.numberFormats=formats;formats->setObjectName("ribbon.numberFormat");formats->setAccessibleName("Number format");formats->addItems({"General","Number","Currency","Accounting","Percent","Date","Text","Custom…"});number->addWidget(formats);add(number,"%","number.percent");connect(formats,qOverload<int>(&QComboBox::activated),this,[this,&s](int index){const char* fs[]={"General","#,##0.00","$#,##0.00",nullptr,"0.0%","m/d/yyyy","@"};if(index<7)s.format({{"numberFormat",index==3?accountingNumberFormat(2):std::string(fs[index])}});else executeCommand("cells.format",{{"category","Custom"}});s.syncNumberFormat();});
 auto* formatOptions=makeButton(body,"Format…",[this]{executeCommand("cells.format");});formatOptions->setObjectName("ribbon.numberFormatOptions");formatOptions->setAccessibleName("Number format options");number->addWidget(formatOptions);
 auto* edit=group(home,"Editing");add(edit,"Σ","autosum");add(edit,"Fill ↓","fill.down");home->addStretch();
 auto* insert=tab("Insert");auto* cg=group(insert,"Charts");add(cg,"Insert chart","chart.insert");add(cg,"Remove charts","chart.clear");auto* sg=group(insert,"Sheets");add(sg,"New sheet","sheet.add");add(sg,"Rename","sheet.rename");insert->addStretch();
 auto* page=tab("Page Layout");auto* pg=group(page,"Print");add(pg,"Print settings","print.settings");add(pg,"Print","print");add(pg,"Export PDF","export.pdf");page->addStretch();
 auto* formulas=tab("Formulas");auto* fg=group(formulas,"Calculation");add(fg,"Calculate now","calculate");add(fg,"Insert function","function.insert");add(fg,"Define name","name.define");add(fg,"GETPIVOTDATA","pivot.generateGetPivotData");formulas->addStretch();
 auto* data=tab("Data");auto* dg=group(data,"Sort");add(dg,"A → Z","sort.ascending");add(dg,"Z → A","sort.descending");add(dg,"Data Validation","data.validation");auto* ag=group(data,"What-if analysis");add(ag,"Forecast","financial.forecast");add(ag,"Assumptions","financial.assumption_set");add(ag,"Compare scenarios","financial.scenario_comparison");add(ag,"Goal Seek","analysis.goalSeek");add(ag,"Solver","analysis.solver");add(ag,"Scenarios","scenario.manage");add(ag,"PivotTable","pivot.create");add(ag,"Convert summary…","pivot.convert");add(ag,"Data table","analysis.table");add(ag,"Refresh links","links.refresh");data->addStretch();
 auto* review=tab("Review");auto* rg=group(review,"Notes");add(rg,"Cell note","note.edit");review->addStretch();
 auto* view=tab("View");auto* vg=group(view,"Display");add(vg,"Gridlines","view.gridlines");add(vg,"Show formulas","view.formulas");add(vg,"Freeze top row","view.freeze");view->addStretch();
 auto* developer=tab("Developer");auto* dev=group(developer,"Macros");add(dev,"Visual Basic","vba.editor");add(dev,"Record macro","vba.record");developer->addStretch();
 auto* formulaBar=new QWidget(body);formulaBar->setFixedHeight(44);auto* fl=new QHBoxLayout(formulaBar);fl->setContentsMargins(6,8,6,10);fl->setSpacing(5);s.name=new QLineEdit("A1");s.name->setFixedHeight(26);s.name->setObjectName("formula.name");s.name->setMaximumWidth(88);fl->addWidget(s.name);auto* fx=makeButton(formulaBar,"ƒx",[this]{executeCommand("function.insert");});fx->setObjectName("formula.insertFunction");fx->setAccessibleName("Insert Function");fx->setToolTip("Insert Function");fx->setFixedHeight(26);fl->addWidget(fx);
 QFile catalog(":/compatibility/functions.json");if(!catalog.open(QIODevice::ReadOnly))throw std::runtime_error("Cannot load the worksheet function catalog");s.functionCatalog=Json::parse(catalog.readAll().toStdString());s.availableFunctions=CalculationEngine().functions();auto* formulaEditor=new FormulaLineEdit(formulaBar);formulaEditor->setFunctionCatalog(s.functionCatalog,s.availableFunctions);s.formula=formulaEditor;s.formula->setFixedHeight(26);s.formula->setObjectName("formula.input");s.formula->setPlaceholderText("Enter a value or formula");fl->addWidget(s.formula);layout->addWidget(formulaBar);
 s.previewBar=new QWidget(body);auto* pl=new QHBoxLayout(s.previewBar);pl->addWidget(new QLabel("Workbook edit preview"));pl->addStretch();pl->addWidget(makeButton(body,"Accept",[this]{executeCommand("preview.accept");}));pl->addWidget(makeButton(body,"Discard",[this]{executeCommand("preview.discard");}));s.previewBar->setStyleSheet("background:#e7efe6;");layout->addWidget(s.previewBar);s.previewBar->hide();
 state_->model=new WorksheetModel(state_->session,this);state_->grid=new WorksheetView(body);state_->grid->setObjectName("worksheet.grid");state_->grid->setModel(state_->model);static_cast<WorksheetView*>(state_->grid)->setFormulaBar(s.formula);installFormulaBarControls(*this,*static_cast<WorksheetView*>(s.grid),*s.name,*s.formula);layout->addWidget(state_->grid);
 installWorksheetContextMenus(*s.grid,*s.model,[this](const QString& command,const Json& arguments){if(command.startsWith("paste." )&&command!="paste.special")return executeCommand("paste.special",arguments);return executeCommand(command,arguments);});
 static_cast<WorksheetView*>(s.grid)->pointedReference=[this](QModelIndex first,QModelIndex last){const auto& s=*state_;if(first!=last||!s.session.snapshotView().value("settings",Json::object()).value("generateGetPivotData",true))return QString();return QString::fromStdString(pivotReferenceFormula(s.session.snapshotView(),s.model->sheetId().toStdString(),first.row()+1,first.column()+1));};
 static_cast<WorksheetView*>(s.grid)->beforeEditorKey=[](QLineEdit* editor,QKeyEvent* key){auto* formula=dynamic_cast<FormulaLineEdit*>(editor);return formula&&formula->handleFormulaKey(key);};
 static_cast<WorksheetView*>(s.grid)->editorFactory=[this](QWidget* parent){state_->clearCopyOutline();auto* editor=new FormulaLineEdit(parent);editor->setFunctionCatalog(state_->functionCatalog,state_->availableFunctions);editor->setWorkbookCompletions(state_->completionNames,state_->completionFunctions);return editor;};
 connect(s.grid->horizontalHeader(),&QHeaderView::sectionResized,this,[this]{positionCharts();});
 connect(s.grid->verticalHeader(),&QHeaderView::sectionResized,this,[this]{positionCharts();});
 state_->grid->verticalHeader()->setDefaultSectionSize(20);state_->grid->horizontalHeader()->setDefaultSectionSize(64);s.grid->setSelectionMode(QAbstractItemView::ExtendedSelection);s.grid->setWordWrap(false);s.grid->setHorizontalScrollMode(QAbstractItemView::ScrollPerItem);s.grid->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);s.grid->setStyleSheet("QTableView{background:white;gridline-color:#d9d9d9;selection-background-color:#dfece3;selection-color:#202c33;border:0;border-left:1px solid #000000;}QHeaderView::section{background:#f4f5f3;color:#65756d;border:0;border-right:1px solid #e1e7de;border-bottom:1px solid #e1e7de;padding:3px;}QTableView::item{padding:2px 5px;}");s.grid->verticalHeader()->setMinimumWidth(48);
 s.sheets=new QTabBar;s.sheets->setObjectName("workspace.sheets");s.sheets->setExpanding(false);layout->addWidget(createWorksheetSheetBar(*this,*s.grid,*s.sheets));statusBar()->setFixedHeight(21);
 s.linkStatus=new QLabel; s.linkStatus->setObjectName("workspace.linkStatus");s.linkStatus->setAccessibleName("Linked source status");statusBar()->addPermanentWidget(s.linkStatus);s.linkStatus->hide();
 s.saveState=new QLabel("Ready");s.saveState->setObjectName("status.saveState");statusBar()->addWidget(s.saveState);s.selection=new QLabel;s.selection->setObjectName("workspace.selectionStatus");s.statistics=new SelectionStatistics(this);statusBar()->addPermanentWidget(s.selection);
 class StatusZoomSlider final:public QSlider {
  int valueAt(int x)const{const int middle=width()/2;x=std::clamp(x,0,width()-1);return qRound(x<=middle?minimum()+double(100-minimum())*x/std::max(1,middle):100+double(maximum()-100)*(x-middle)/std::max(1,width()-1-middle));}
  int handleAt()const{const int middle=width()/2;return qRound(value()<=100?double(middle)*(value()-minimum())/std::max(1,100-minimum()):middle+double(width()-1-middle)*(value()-100)/std::max(1,maximum()-100));}
 protected:
  void paintEvent(QPaintEvent*)override{QPainter painter(this);painter.setPen(QColor("#444444"));const int middle=height()/2;painter.drawLine(0,middle,width()-1,middle);painter.fillRect(handleAt()-2,middle-5,5,11,QColor("#444444"));}
  void mousePressEvent(QMouseEvent* event)override{if(event->button()!=Qt::LeftButton){QSlider::mousePressEvent(event);return;}setSliderDown(true);setValue(valueAt(qRound(event->position().x())));event->accept();}
  void mouseMoveEvent(QMouseEvent* event)override{if(!isSliderDown()){QSlider::mouseMoveEvent(event);return;}setValue(valueAt(qRound(event->position().x())));event->accept();}
  void mouseReleaseEvent(QMouseEvent* event)override{if(event->button()!=Qt::LeftButton){QSlider::mouseReleaseEvent(event);return;}setValue(valueAt(qRound(event->position().x())));setSliderDown(false);event->accept();}
 public:explicit StatusZoomSlider(QWidget* parent):QSlider(Qt::Horizontal,parent){setFocusPolicy(Qt::NoFocus);setSingleStep(10);setPageStep(10);}
 };
 auto* zoomBar=new QWidget(statusBar());zoomBar->setObjectName("status.zoomControls");zoomBar->setFixedSize(189,16);auto* zoom=new StatusZoomSlider(zoomBar);zoom->setObjectName("worksheet.zoom");zoom->setAccessibleName("Worksheet zoom");zoom->setRange(25,400);zoom->setValue(100);zoom->setGeometry(17,0,100,16);
 auto zoomButton=[&](const QString& name,const QString& caption,bool positive,int left){auto* button=new QToolButton(zoomBar);button->setObjectName(name);button->setAccessibleName(caption);button->setToolTip(caption);button->setFocusPolicy(Qt::NoFocus);button->setGeometry(left,0,16,16);button->setStyleSheet("QToolButton{border:0;padding:0;background:transparent;}QToolButton:hover{background:#dedede;}QToolButton:pressed{background:#c9c9c9;}");QPixmap pixels(16,16);pixels.fill(Qt::transparent);QPainter painter(&pixels);painter.setPen(QColor("#444444"));painter.drawLine(4,7,11,7);if(positive)painter.drawLine(7,4,7,11);painter.end();button->setIcon(QIcon(pixels));button->setIconSize(QSize(16,16));connect(button,&QToolButton::clicked,this,[zoom,positive]{zoom->setValue(std::clamp(zoom->value()+(positive?10:-10),zoom->minimum(),zoom->maximum()));});return button;};
 auto* zoomOut=zoomButton("status.zoomOut","Zoom Out",false,0);auto* zoomIn=zoomButton("status.zoomIn","Zoom In",true,118);auto* zoomLabel=new QLabel("100%",zoomBar);zoomLabel->setObjectName("status.zoom");zoomLabel->setGeometry(137,0,48,16);zoomLabel->setContentsMargins(7,0,0,0);auto zoomFont=zoomLabel->font();zoomFont.setPixelSize(11);zoomLabel->setFont(zoomFont);zoomLabel->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);statusBar()->setSizeGripEnabled(false);statusBar()->addPermanentWidget(zoomBar);
 connect(zoom,&QSlider::valueChanged,this,[zoom,zoomOut,zoomIn](int value){zoomOut->setEnabled(value>zoom->minimum());zoomIn->setEnabled(value<zoom->maximum());});
 connect(zoom,&QSlider::valueChanged,this,[&s,zoomLabel](int v){zoomLabel->setText(QString::number(v)+"%");double rowPoints=15.0,columnWidth=spreadsheetColumnWidth(8.43,SpreadsheetNormalDigitWidth);for(const auto& sheet:s.model->workbookView().at("sheets"))if(sheet.at("id")==s.model->sheetId().toStdString()){rowPoints=sheet.value("defaultRowHeight",15.0);columnWidth=sheet.value("defaultColumnWidth",columnWidth);break;}s.grid->verticalHeader()->setDefaultSectionSize(qRound(rowPoints*s.grid->logicalDpiY()/72.0*v/100.0));s.grid->horizontalHeader()->setDefaultSectionSize(qRound(spreadsheetColumnPixels(columnWidth,SpreadsheetNormalDigitWidth)*v/100.0));});
 setCentralWidget(body);state_->grid->setCurrentIndex(state_->model->index(0,0));
 connect(state_->formula,&QLineEdit::returnPressed,this,[this]{state_->model->setData(state_->grid->currentIndex(),state_->formula->text(),Qt::EditRole);state_->formula->setModified(false);state_->grid->setFocus();});
 connect(state_->formula,&QLineEdit::editingFinished,this,[this]{if(state_->formula->isModified())commitEditor();});
 connect(state_->grid->selectionModel(),&QItemSelectionModel::currentChanged,this,[this](const QModelIndex&i){if(!i.isValid())return;if(!static_cast<WorksheetView*>(state_->grid)->hasReferenceSheetEntry())state_->formula->setText(state_->model->data(i,Qt::EditRole).toString());state_->name->setText(QString::fromStdString(columnName(i.column()+1))+QString::number(i.row()+1));state_->syncNumberFormat();});
 connect(s.model,&QAbstractItemModel::dataChanged,this,[this]{state_->syncNumberFormat();state_->syncFormulaBar();positionCharts();});
 connect(s.model,&QAbstractItemModel::modelReset,this,[this]{state_->syncNumberFormat();state_->syncFormulaBar();});
 connect(QApplication::clipboard(),&QClipboard::dataChanged,this,[this]{if(!state_->publishingClipboard)state_->clearCopyOutline();});
 connect(s.formula,&QLineEdit::textEdited,this,[this]{state_->clearCopyOutline();});
 connect(s.grid->selectionModel(),&QItemSelectionModel::selectionChanged,this,[this]{updateSelectionStatus();});
 connect(s.name,&QLineEdit::returnPressed,this,[&s]{auto text=s.name->text().split(':');QRegularExpression rx("^\\$?([A-Za-z]+)\\$?([1-9][0-9]*)$");auto a=rx.match(text[0]),b=rx.match(text.value(1,text[0]));if(!a.hasMatch()||!b.hasMatch())return;auto first=s.model->index(a.captured(2).toInt()-1,columnNumber(a.captured(1).toStdString())-1),last=s.model->index(b.captured(2).toInt()-1,columnNumber(b.captured(1).toStdString())-1);if(!first.isValid()||!last.isValid())return;s.grid->setCurrentIndex(first);s.grid->selectionModel()->select(QItemSelection(first,last),QItemSelectionModel::ClearAndSelect);s.grid->scrollTo(first);s.grid->setFocus();});
 connect(s.sheets,&QTabBar::currentChanged,this,[this](int i){
  if(i<0)return;auto& s=*state_;auto* view=static_cast<WorksheetView*>(s.grid);view->entrySheetRequested=[this](const QString& id){for(int tab=0;tab<state_->sheets->count();++tab)if(state_->sheets->tabData(tab).toString()==id){state_->sheets->setCurrentIndex(tab);return;}};view->prepareReferenceSheetChange();s.switchSheet(s.sheets->tabData(i).toString());const auto& book=s.bookView();const auto& result=s.calculated;const auto iteration=result.find("iteration");
  const bool current=result.value("status",std::string())=="ok"&&result.value("revision",std::uint64_t{})==book.at("revision").get<std::uint64_t>()&&result.contains("contextGeneration")&&result.at("contextGeneration")==s.contextGeneration&&(iteration==result.end()||!iteration->is_object()||iteration->value("converged",true));
  bool chartsReady=true;if(book.contains("charts"))for(const auto& chart:book.at("charts"))if(chart.value("sheet",std::string())==s.model->sheetId().toStdString()&&(!result.contains("preparedCharts")||!result.at("preparedCharts").contains(chart.at("id").get<std::string>())))chartsReady=false;
  if(current&&chartsReady){auto cached=std::move(s.calculated);displayCalculation(std::move(cached));return;}
  for(auto* chart:s.charts)delete chart;s.charts.clear();for(const auto& sheet:book.at("sheets"))if(sheet.at("id")==s.model->sheetId().toStdString())static_cast<WorksheetView*>(s.grid)->setCurrentSheetName(QString::fromStdString(sheet.at("name").get<std::string>()));if(current)s.model->setCalculated(s.calculated);updateSelectionStatus();s.timer->start();
 });connect(s.sheets,&QTabBar::tabBarDoubleClicked,this,[this](int tab){if(tab<0)return;state_->sheets->setCurrentIndex(tab);executeCommand("sheet.rename");});
 s.sheets->setContextMenuPolicy(Qt::CustomContextMenu);connect(s.sheets,&QWidget::customContextMenuRequested,this,[this](QPoint point){
  auto& s=*state_;if(point.x()<0||point.y()<0)point=s.sheets->tabRect(s.sheets->currentIndex()).center();const int tab=s.sheets->tabAt(point);if(tab<0)return;s.sheets->setCurrentIndex(tab);auto* menu=new QMenu(s.sheets);menu->setAttribute(Qt::WA_DeleteOnClose);menu->addAction("Rename…",this,[this]{executeCommand("sheet.rename");});auto* remove=menu->addAction("Delete",this,[this]{executeCommand("sheet.delete");});remove->setEnabled(s.sheets->count()>1);menu->popup(s.sheets->mapToGlobal(point));
 });
 s.scheduler=new CalculationScheduler({},this);s.scheduler->onResult=[this](Json result){displayCalculation(std::move(result));};s.timer=new QTimer(this);s.timer->setSingleShot(true);s.timer->setInterval(35);s.calcCapture=new SnapshotCapture(this,1024);connect(s.timer,&QTimer::timeout,this,[this]{queueCalculation();});s.model->onChanged=[this]{auto& s=*state_;const auto& change=s.session.lastChange();
  // Capture the edit selection synchronously, before Enter/Tab navigation.
  // History tokens are transient identities and never enter workbook files.
  const auto token=s.session.undoToken();if(!s.restoringHistory&&token&&!change.value("structure",true)&&!change.at("sheets").empty()&&!s.editPositions.contains(token)){
   s.editPositions.emplace(token,std::pair{s.model->sheetId(),s.captureSheetPosition()});
   if(s.editPositions.size()>1024)for(auto position=s.editPositions.begin();position!=s.editPositions.end();){if(!s.session.hasHistoryToken(position->first))position=s.editPositions.erase(position);else ++position;}
  }
  bool reusedCalculated=false;
  if(!change.value("structure",true)&&!change.value("metadata",false)&&!change.value("calculationInputsChanged",true)){
   const auto revision=s.session.snapshotView().at("revision").get<std::uint64_t>();const auto iteration=s.calculated.find("iteration");
   const bool current=revision>0&&s.calculated.value("revision",Json())==Json(revision-1)&&s.calculated.value("contextGeneration",Json())==Json(s.contextGeneration)&&s.calculated.value("status",std::string())=="ok"&&s.calculated.value("converged",true)&&(iteration==s.calculated.end()||!iteration->is_object()||iteration->value("converged",true));
   if(current){s.calculated["revision"]=revision;reusedCalculated=true;}
   else s.model->setCalculated({{"revision",revision},{"status","stale"},{"values",Json::array()}});
  }
  notifyLiveWorkbookChanged(s.liveSource.get());if(!s.copyPasting)s.clearCopyOutline();++s.selectionSerial;s.statistics->cancel();s.selection->clear();++s.calcSerial;if(s.calcCapture)s.calcCapture->stop();s.scheduler->cancel();s.dirty=true;s.syncSaveState(*this);s.refreshFinancialFields();if(s.pivotPane&&s.reportDefinition.contains("id")&&s.session.snapshotView().contains("pivots"))for(const auto& definition:s.session.snapshotView().at("pivots"))if(definition.value("id",std::string())==s.reportDefinition.at("id")){s.pivotPane->setStatus(definition);break;}if(change.value("structure",true))s.rebuildSheets();else if(change.value("metadata",false))s.refreshCompletions(s.session.snapshotView());if(reusedCalculated){s.timer->stop();updateSelectionStatus();refreshChartPresentation(change);}else s.timer->start();if(s.recovery)try{s.recovery->append(s.recoveryId,s.session.takeRecoveryDelta());}catch(const std::exception&e){statusBar()->showMessage(e.what());}};
 s.codex=new CodexClient(this);
 s.dock=new QDockWidget("Codex",this);s.dock->setObjectName("workspace.codex");s.dock->setAccessibleName("Codex workbook assistant");s.dock->setAllowedAreas(Qt::LeftDockWidgetArea|Qt::RightDockWidgetArea);s.dock->setMinimumWidth(320);
 auto* chat=new QWidget;auto* chatLayout=new QVBoxLayout(chat);chatLayout->setContentsMargins(8,8,8,8);chatLayout->setSpacing(7);
 auto* privacy=new QLabel("Workbook context is shared when you send a message.",chat);privacy->setObjectName("codex.privacy");privacy->setProperty("tone","info");privacy->setWordWrap(true);chatLayout->addWidget(privacy);
 auto* connectionRow=new QHBoxLayout;s.connection=new QLabel("Offline",chat);s.connection->setObjectName("codex.status");connectionRow->addWidget(s.connection);connectionRow->addStretch();
 s.codexStop=new QPushButton("Stop",chat);s.codexStop->setObjectName("codex.stop");s.codexStop->hide();connectionRow->addWidget(s.codexStop);
 s.codexConnection=new QPushButton("Reconnect",chat);s.codexConnection->setObjectName("codex.connection");connectionRow->addWidget(s.codexConnection);chatLayout->addLayout(connectionRow);
 s.codexControls=new CodexControls(chat);chatLayout->addWidget(s.codexControls);
 s.transcript=new QTextBrowser(chat);s.transcript->setObjectName("codex.transcript");s.transcript->setAccessibleName("Codex conversation");s.transcript->setOpenExternalLinks(false);s.transcript->setPlaceholderText("Codex conversation");chatLayout->addWidget(s.transcript,1);
 s.composer=new QPlainTextEdit(chat);s.composer->setObjectName("codex.input");s.composer->setAccessibleName("Message to Codex");s.composer->setAccessibleDescription("Enter sends your message; Shift+Enter starts a new line");s.composer->setPlaceholderText("Ask about or propose changes to this workbook…");s.composer->setMaximumHeight(110);s.composer->setEnabled(false);chatLayout->addWidget(s.composer);
 s.codexSend=new QPushButton("Send",chat);s.codexSend->setObjectName("codex.send");s.codexSend->setProperty("role","primary");s.codexSend->setDefault(true);s.codexSend->setEnabled(false);chatLayout->addWidget(s.codexSend);
 connect(s.codexConnection,&QPushButton::clicked,this,[&s]{if(s.codex->isReady())s.codex->disconnectRuntime();else s.codex->connectRuntime();});
 connect(s.codexStop,&QPushButton::clicked,this,[&s]{s.codex->cancel();});
 connect(s.codexSend,&QPushButton::clicked,this,[&s]{
  if(!s.codex->isReady()||s.codex->isBusy())return;
  const auto prompt=s.composer->toPlainText().trimmed();if(prompt.isEmpty())return;
  if(prompt.size()>65536){s.connection->setText("Message exceeds the 65,536 character limit.");return;}
  s.codex->setModel(s.codexControls->model());s.codex->setReasoningEffort(s.codexControls->reasoningEffort());s.codex->setServiceTier(s.codexControls->serviceTier());
  s.transcript->append("<p><b style=\"color:#2457a6\">You</b><br>"+prompt.toHtmlEscaped().replace('\n',"<br>")+"</p><p><b style=\"color:#166534\">Codex</b></p>");
  auto cursor=s.transcript->textCursor();cursor.movePosition(QTextCursor::End);cursor.insertBlock();cursor.setCharFormat(QTextCharFormat());s.transcript->setTextCursor(cursor);
  s.composer->clear();s.codex->send(prompt,s.session.snapshotView());
 });
 s.dock->setWidget(chat);addDockWidget(Qt::RightDockWidgetArea,s.dock);s.dock->hide();
 s.codex->onModelsChanged=[&s]{s.codexControls->setCatalog(s.codex->models());};
 s.codex->onText=[&s](QString text){auto cursor=s.transcript->textCursor();cursor.movePosition(QTextCursor::End);cursor.setCharFormat(QTextCharFormat());cursor.insertText(text);s.transcript->setTextCursor(cursor);s.transcript->ensureCursorVisible();};
 s.codex->onError=[&s](QString text){s.connection->setToolTip(text);s.transcript->append("<p><b>Error</b><br>"+text.toHtmlEscaped()+"</p>");};
 s.codex->onStatus=[&s](QString text){
  const bool connected=s.codex->isReady(),busy=s.codex->isBusy();s.connection->setText(text);s.codexControls->setConnected(connected);
  s.codexConnection->setText(connected?"Disconnect":"Reconnect");s.codexConnection->setAccessibleName(s.codexConnection->text());s.composer->setEnabled(connected&&!busy);s.codexSend->setEnabled(connected&&!busy);s.codexStop->setVisible(busy);
 };
 s.workbookTools=std::make_unique<WorkbookTools>(s.session,s.availableFunctions);s.workbookTools->calculationContextIsCurrent=[this](std::uint64_t generation){return generation==state_->contextGeneration;};
 s.workbookTools->onPreview=[this,&s](const Json& preview){++s.calcSerial;s.calcCapture->stop();s.scheduler->cancel();s.previewContext=s.contextGeneration;s.model->showPreviewView(preview);s.rebuildSheets();s.timer->start();s.previewBar->show();s.formula->setReadOnly(true);};
 s.workbookTools->calculate=[this,&s](const Json& snapshot){
  WorkbookLinkOptions options;options.retainedLinks=s.retainedLinks;options.liveWorkbooks=captureLiveWorkbooks(s.liveSource.get());auto prepared=prepareWorkbookLinks(snapshot,s.package?&*s.package:nullptr,options);
  const auto& book=prepared.snapshot;const auto macro=book.value("vba",Json::object());std::string source;for(const auto& module:macro.value("modules",Json::array()))source+=module.value("source",std::string())+"\n";
  CalculationEngine engine;auto result=macro.value("enabled",false)&&!source.empty()?engine.calculate(book,[&](const std::string& name,const Json& args){return evaluateVbaFunction(book,source,name,args);}):engine.calculate(book);
  result["contextGeneration"]=s.contextGeneration;
  Json links=Json::array();if(prepared.snapshot.contains("links"))for(const auto& link:prepared.snapshot.at("links"))links.push_back({{"name",link.value("name",std::string())},{"path",link.value("originalPath",link.value("path",std::string()))},{"state",link.value("state",std::string("unavailable"))}});for(const auto& diagnostic:prepared.diagnostics)result["diagnostics"].push_back(diagnostic);result["calculationContext"]={{"generation",s.contextGeneration},{"allowCachedLinks",false},{"links",std::move(links)},{"diagnostics",prepared.diagnostics}};return result;
 };
 s.codex->onToolAsync=[this](QString tool,Json args,std::function<void(Json)> completion){startCodexTool(std::move(tool),std::move(args),std::move(completion));};
 s.codex->onCancelTools=[this]{cancelCodexTool();};
 auto bind=[this,&s](const char* keys,const char* cmd,bool worksheet){auto* action=new QAction(this);action->setShortcut(QKeySequence(keys));action->setShortcutContext(worksheet?Qt::WidgetWithChildrenShortcut:Qt::WindowShortcut);if(worksheet)s.grid->addAction(action);else addAction(action);connect(action,&QAction::triggered,this,[this,command=QString(cmd)]{executeCommand(command);});};
 for(auto x:{std::pair{"Ctrl+C","copy"},std::pair{"Ctrl+X","cut"},std::pair{"Ctrl+V","paste"},std::pair{"Ctrl+Alt+V","paste.special"},std::pair{"Ctrl+1","cells.format"},std::pair{"Ctrl+D","fill.down"},std::pair{"Ctrl+R","fill.right"},std::pair{"Delete","clear"},std::pair{"Ctrl+B","style.bold"},std::pair{"Ctrl+I","style.italic"},std::pair{"Ctrl+U","style.underline"},std::pair{"Ctrl+G","goto"},std::pair{"Ctrl+F","find"},std::pair{"Ctrl+H","replace"},std::pair{"Alt+=","autosum"},std::pair{"Shift+F11","sheet.add"}})bind(x.first,x.second,true);
 for(auto x:{std::pair{"Ctrl+W","close"},std::pair{"Ctrl+S","save"},std::pair{"Ctrl+Shift+S","save.as"},std::pair{"F12","save.as"},std::pair{"Ctrl+O","open"},std::pair{"Ctrl+N","new"},std::pair{"Ctrl+Z","undo"},std::pair{"Ctrl+Y","redo"},std::pair{"F9","calculate"},std::pair{"Alt+F11","vba.editor"},std::pair{"Ctrl+F1","view.ribbon"}})bind(x.first,x.second,false);
 auto* file=menuBar()->addMenu("File");for(auto x:{std::pair{"New","new"},std::pair{"Open…","open"},std::pair{"Save","save"},std::pair{"Save As…","save.as"},std::pair{"Recover workbook…","recover"},std::pair{"Financial model example","template.financial"}}){auto* a=file->addAction(x.first);connect(a,&QAction::triggered,this,[this,c=QString(x.second)]{executeCommand(c);});}
 s.validation=new WorksheetValidationUi(this,*s.model,*s.grid,*s.sheets,[this]{const auto& result=state_->calculated;if(!result.contains("contextGeneration")||result.at("contextGeneration")!=state_->contextGeneration||result.value("revision",Json())!=state_->session.snapshotView().at("revision"))return Json::object();return result;});
 qApp->installEventFilter(this);s.rebuildSheets();try{auto dir=qEnvironmentVariable("TIGERSHEETS_STATE_DIR",QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));if(QDir().mkpath(dir)){s.recoveryPath=(dir+"/recovery.sqlite").toStdString();s.recovery=std::make_unique<RecoveryDriver>(s.recoveryPath);s.recovery->onError=[this](const std::string& message){state_->error=QString::fromStdString(message);statusBar()->showMessage("Recovery: "+state_->error,15000);};s.recovery->seed(s.recoveryId,s.session.snapshot(),std::make_shared<const std::string>(),{});}}catch(const std::exception&e){statusBar()->showMessage(e.what());}s.syncSaveState(*this);installTutorialShortcuts(*this,*s.model,*static_cast<WorksheetView*>(s.grid),*s.ribbon);installExternalReferenceEntry(*this,*static_cast<WorksheetView*>(s.grid),*s.formula);installQuickAnalysis(*this,*s.model,*static_cast<WorksheetView*>(s.grid));installWorksheetErrorIndicator(*this,*s.model,*static_cast<WorksheetView*>(s.grid));installReviewRibbon(*this,*s.ribbon,*s.grid);installViewRibbon(*this,*s.ribbon,*s.grid,*s.model);for(int tab=0;tab<s.ribbon->count();++tab)if(auto* page=dynamic_cast<RibbonPage*>(s.ribbon->widget(tab)))page->compactControls();s.presentRibbon(*this);s.syncHeaderLayout(*this);
 const QPointer<MainWindow> guarded(this);
 s.liveSource=registerLiveWorkbook([guarded]{
  if(!guarded)return LiveWorkbookSnapshot{};const auto& state=*guarded->state_;
  return LiveWorkbookSnapshot{state.filename->text().toStdString(),state.path.toStdString(),state.session.captureSnapshot()};
 },[guarded]{
  if(!guarded||guarded->state_->liveInvalidationPending)return;guarded->state_->liveInvalidationPending=true;
  QMetaObject::invokeMethod(guarded.data(),[guarded]{if(!guarded)return;auto& state=*guarded->state_;state.liveInvalidationPending=false;
   ++state.contextGeneration;++state.calcSerial;if(state.calcCapture)state.calcCapture->stop();state.scheduler->cancel();state.timer->start();
  },Qt::QueuedConnection);
 });
}
MainWindow::~MainWindow(){delete findChild<QObject*>("worksheet.errorIndicatorController");delete findChild<QObject*>("view.controller");delete findChild<QObject*>("formula.controlsController");delete findChild<QObject*>("review.controller");delete findChild<QObject*>("worksheet.quickAnalysisController");delete findChild<QObject*>("tutorial.shortcuts");delete findChild<QObject*>("worksheet.externalReferenceEntry");state_->liveSource.reset();delete state_->validation;state_->validation=nullptr;qApp->removeEventFilter(this);delete state_->financialWorkspace;state_->financialWorkspace=nullptr;cancelCodexTool();if(state_->toolCapture){delete state_->toolCapture;state_->toolCapture=nullptr;}if(state_->toolWorker){delete state_->toolWorker;state_->toolWorker=nullptr;}if(state_->toolPublication)state_->toolPublication->wait();if(state_->calcCapture){state_->calcCapture->stop();delete state_->calcCapture;state_->calcCapture=nullptr;}if(state_->reportCapture){state_->reportCapture->stop();delete state_->reportCapture;state_->reportCapture=nullptr;}if(state_->reportWorker){state_->reportWorker->stop();delete state_->reportWorker;state_->reportWorker=nullptr;}if(state_->reportPublication)state_->reportPublication->wait();state_->timer->stop();state_->scheduler->onResult={};delete state_->scheduler;state_->scheduler=nullptr;for(auto* dialog:findChildren<QDialog*>())if(dialog->objectName()=="vba.editor"||dialog->objectName()=="vba.recorder")delete dialog;state_->model->onChanged={};for(auto* widget:findChildren<QWidget*>())if(auto* page=dynamic_cast<RibbonPage*>(widget))page->overflowChanged={};for(auto* child:findChildren<QObject*>())QObject::disconnect(child,nullptr,this,nullptr);delete state_->codex;state_->codex=nullptr;delete takeCentralWidget();delete state_->model;state_->model=nullptr;}
void MainWindow::resetWorkbookContext(){auto& s=*state_;s.sheetPositions.clear();s.loadedSheetViews.clear();s.loadedActiveSheet.clear();s.clearCopyOutline();if(s.validation)s.validation->reset();invalidateWorksheetContextMenus(*s.grid);if(s.frozen)invalidateWorksheetContextMenus(*s.frozen);executeCommand("financial.cancel");if(s.financialDock)s.financialDock->hide();if(s.financialPane)s.financialPane->resetCachedLinkConsent();if(s.recovery)s.recovery->append(s.recoveryId,s.session.takeRecoveryDelta());s.recoveryId=QUuid::createUuid().toString().toStdString();executeCommand("pivot.cancel");++s.contextGeneration;++s.calcSerial;if(s.calcCapture)s.calcCapture->stop();if(s.pivotDock)s.pivotDock->hide();static_cast<WorksheetView*>(s.grid)->cancelEntry();auto previousCharts=std::move(s.charts);s.charts.clear();for(auto* chart:previousCharts)delete chart;s.timer->stop();s.scheduler->cancel();s.retainedLinks=Json::array();for(auto* dialog:findChildren<QDialog*>())if(dialog->objectName()=="vba.editor"||dialog->objectName()=="vba.recorder")delete dialog;s.model->refresh();s.session.discard();s.previewContext.reset();s.previewBar->hide();s.formula->setReadOnly(false);s.codex->resetWorkbook();s.workbookTools->reset();s.transcript->clear();s.composer->clear();s.codexControls->setCatalog(Json::array());}
void MainWindow::cancelCodexTool(){
 auto& s=*state_;++s.toolSerial;
 auto job=std::move(s.toolJob);if(job)job->cancelled=true;
 if(s.toolCapture)s.toolCapture->stop();if(s.toolWorker)s.toolWorker->stop();
 setProperty("codexToolBusy",bool(s.toolPublication)||(s.toolWorker&&s.toolWorker->running()));
 if(job&&job->completion){auto completion=std::move(job->completion);completion({{"error","Workbook tool cancelled; no candidate was published"}});}
}
void MainWindow::startCodexTool(QString tool,Json arguments,std::function<void(Json)> completion){
 auto& s=*state_;
 if((s.financialWorkspace&&s.financialWorkspace->running())||s.toolJob||s.toolPublication||(s.toolWorker&&s.toolWorker->running())||property("reportBusy").toBool()||(s.reportCapture&&s.reportCapture->running())||(s.reportWorker&&s.reportWorker->running())||s.reportPublication){completion({{"error","A workbook report or tool is already running; stop it before starting another"}});return;}
 if(!s.toolCapture){s.toolCapture=new SnapshotCapture(this,1024);s.toolWorker=new ReportWorker(this);}
 auto job=newCodexToolJob();job->serial=++s.toolSerial;job->revision=s.session.snapshotView().at("revision");job->context=s.contextGeneration;job->workbookId=s.recoveryId;job->previous=s.workbookTools->proposalState();job->completion=std::move(completion);job->request=std::make_shared<Json>(Json{{"version",1},{"kind","workbook_tool"},{"tool",tool.toStdString()},{"args",std::move(arguments)},{"contextGeneration",job->context},{"workbookId",job->workbookId},{"allowCachedLinks",false},{"sourcePath",s.path.toStdString()},{"nativeLinkParts",Json::object()}});s.toolJob=job;setProperty("codexToolBusy",true);
 const std::weak_ptr<CodexToolJob> weak=job;
 auto current=[this,weak]{const auto job=weak.lock();if(!job)return false;const auto& s=*state_;return !job->cancelled&&s.toolJob==job&&s.toolSerial==job->serial&&s.session.snapshotView().at("revision")==job->revision&&s.contextGeneration==job->context&&s.recoveryId==job->workbookId&&s.workbookTools->proposalState().operations==job->previous.operations&&s.codex&&s.codex->isBusy();};
 auto fail=[this,weak](QString message){auto job=weak.lock();if(!job||state_->toolJob!=job)return;state_->toolJob.reset();setProperty("codexToolBusy",false);if(job->completion){auto completion=std::move(job->completion);completion({{"error",message.toStdString()}});}};
 s.toolCapture->failed=fail;s.toolWorker->onError=fail;
 s.toolWorker->onProgress=[this](QString progress){if(progress=="Cancelled"&&!state_->toolJob&&!state_->toolPublication)setProperty("codexToolBusy",false);statusBar()->showMessage(progress);};
 s.toolWorker->onResult=[this,weak,current,fail](Json result){
  auto job=weak.lock();if(!job)return;
  job->returned=std::move(result);auto& response=job->returned;
  if(!current()){fail("Workbook, proposal or linked inputs changed; the tool result was discarded");return;}
  if(response.value("status",std::string())!="ok"){fail("Workbook tool did not complete successfully");return;}
  if(response.at("toolReply").value("status",std::string())!="preview"){
   auto reply=std::move(response.at("toolReply"));state_->toolJob.reset();setProperty("codexToolBusy",false);auto callback=std::move(job->completion);if(callback)callback(std::move(reply));return;
  }
  auto* thread=QThread::create([job]() mutable {
   try{if(job->cancelled)return;job->reply=std::move(job->returned.at("toolReply"));job->operations=std::make_shared<const Json>(std::move(job->returned.at("operations")));job->prepared=WorkbookSession::prepareTransaction(std::move(job->request->at("workbook")),*job->operations,&job->error);}
   catch(const std::exception& error){job->error=error.what();}
   job->request.reset();
  });state_->toolPublication=thread;
  connect(thread,&QThread::finished,this,[this,job,current,fail,thread]{
   auto& s=*state_;if(s.toolPublication==thread)s.toolPublication=nullptr;
   if(!current()){if(s.toolJob==job)fail("Workbook, proposal or linked inputs changed; the tool candidate was discarded");else if(!s.toolJob&&!s.toolPublication)setProperty("codexToolBusy",false);return;}
   if(!job->prepared){fail(QString::fromStdString(job->error.empty()?"Workbook tool candidate was invalid":job->error));return;}
   // Invalidate old preview calculations before replacing the borrowed view.
   ++s.calcSerial;s.calcCapture->stop();s.scheduler->cancel();std::string error;
   if(!s.session.previewPrepared(job->revision,job->prepared,&error)){fail(QString::fromStdString(error));return;}
   if(!s.workbookTools->adoptProposal({job->revision,job->operations},&error)){fail(QString::fromStdString(error));return;}
   s.previewContext=job->context;s.model->showPreviewView(*s.session.previewView());s.rebuildSheets();s.previewBar->show();s.formula->setReadOnly(true);s.timer->start();s.toolJob.reset();setProperty("codexToolBusy",false);auto callback=std::move(job->completion);if(callback)callback(std::move(job->reply));
  });connect(thread,&QThread::finished,thread,&QObject::deleteLater);thread->start();
 };
 s.toolCapture->completed=[this,weak,current,fail](std::shared_ptr<Json> captured){
  auto job=weak.lock();if(!job)return;if(!current()){fail("Workbook changed while preparing tool inputs");return;}(*job->request)["workbook"]=std::move(*captured);
  state_->toolCapture->completed=[this,weak,current,fail](std::shared_ptr<Json> retained){
   auto job=weak.lock();if(!job)return;if(!current()){fail("Linked inputs changed while preparing tool inputs");return;}(*job->request)["retainedLinks"]=std::move(*retained);
   state_->toolCapture->completed=[this,weak,current,fail](std::shared_ptr<Json> operations){
    auto job=weak.lock();if(!job)return;if(!current()){fail("Proposal changed while preparing tool inputs");return;}(*job->request)["previousProposal"]=std::move(*operations);
    if(state_->package)for(const auto& [name,bytes]:state_->package->entries)if(name=="xl/workbook.xml"||name=="xl/_rels/workbook.xml.rels"||name.starts_with("xl/externalLinks/"))(*job->request)["nativeLinkParts"][name]=bytes;
    state_->toolWorker->start(job->request,property("reportWorkerExecutable").toString());
   };
   if(!state_->toolCapture->start(*job->previous.operations,current))fail("Cannot capture the pending proposal");
  };
  if(!state_->toolCapture->start(state_->retainedLinks,current))fail("Cannot capture linked inputs");
 };
 if(!s.toolCapture->start(s.session.snapshotView(),current))fail("Cannot capture workbook inputs");
}
WorkbookSession& MainWindow::currentSession(){return state_->session;}
QString MainWindow::lastError()const{return state_->error;}
void MainWindow::commitEditor(){auto& s=*state_;QString draftError;if(!flushVbaDrafts(this,&draftError))throw std::runtime_error(draftError.toStdString());if(!static_cast<WorksheetView*>(s.grid)->commitPendingEditor())throw std::runtime_error("Cannot accept the current cell edit");if(auto* focused=QApplication::focusWidget();focused&&s.grid->isAncestorOf(focused)){focused->clearFocus();s.grid->setFocus();}}

Json MainWindow::calculationSnapshot()const{const auto& s=*state_;WorkbookLinkOptions options;options.retainedLinks=s.retainedLinks;options.liveWorkbooks=captureLiveWorkbooks(s.liveSource.get());auto prepared=prepareWorkbookLinks(s.bookView(),s.package?&*s.package:nullptr,options);prepared.snapshot["_tigersheetsLinkDiagnostics"]=std::move(prepared.diagnostics);return std::move(prepared.snapshot);}
CalculationContext MainWindow::calculationContext(bool allowCachedLinks)const{
 CalculationContext context;context.allowCachedLinks=allowCachedLinks;context.snapshot=calculationSnapshot();context.generation=state_->contextGeneration;context.generationIsCurrent=[this](std::uint64_t generation){return state_->contextGeneration==generation;};
 context.evaluator=[](const Json& book){const auto vba=book.value("vba",Json::object());std::string source;for(const auto& module:vba.value("modules",Json::array()))source+=module.value("source",std::string())+"\n";CalculationEngine engine;if(!vba.value("enabled",false)||source.empty())return engine.calculate(book);return engine.calculate(book,[&](const std::string& name,const Json& arguments){return evaluateVbaFunction(book,source,name,arguments);});};return context;
}
namespace {
Json reportSheets(const Json& book){Json sheets=Json::array();for(const auto& sheet:book.at("sheets"))sheets.push_back({{"id",sheet.at("id")},{"name",sheet.at("name")}});return sheets;}
Json reportHeaders(const WorkbookSession& session,const Json& source,const Json& calculated,std::uint64_t generation){
 Json fields=Json::array();const auto sheet=source.at("sheet").get<std::string>();const int row=source.at("row1");const bool current=calculated.value("revision",Json())==session.snapshotView().at("revision")&&calculated.value("status",std::string())=="ok"&&calculated.value("contextGeneration",Json())==generation;
 std::map<int,const Json*> results;if(current&&calculated.contains("values"))for(const auto& value:calculated.at("values"))if(value.at("sheet")==sheet&&value.at("row")==row&&value.at("col")>=source.at("col1")&&value.at("col")<=source.at("col2"))results.emplace(value.at("col").get<int>(),&value);
 for(int col=source.at("col1");col<=source.at("col2").get<int>();++col){const auto* cell=session.cellAt(sheet,row,col);auto name=cell?cell->value("input",std::string()):std::string();if(auto value=results.find(col);value!=results.end()){const auto type=value->second->value("type",std::string());if(type!="text"&&type!="string")throw std::runtime_error("Calculated report headers must be nonempty text");name=value->second->at("value").get<std::string>();}else if(name.starts_with('='))throw std::runtime_error("Recalculate before reading formula-based report headers");else if(name.starts_with('\''))name.erase(0,1);fields.push_back({{"name",name},{"numberFormat","General"},{"items",Json::array()}});}return fields;
}
const Json& selectedPivot(const Json& book,const std::string& id){if(book.contains("pivots"))for(const auto& pivot:book.at("pivots"))if(pivot.value("version",0)==1&&pivot.value("id",std::string())==id)return pivot;throw std::runtime_error("Editable native pivot was not found");}
Json editableReport(const Json& definition){Json result=Json::object();for(const auto& [key,value]:definition.items())if(key!="cache"&&key!="layout")result[key]=value;return result;}
Json reportRange(const QString& sheet,const QString& text){const auto parts=text.split(':');const QRegularExpression expression("^\\$?([A-Za-z]{1,3})\\$?([1-9][0-9]*)$");const auto first=expression.match(parts.value(0)),last=expression.match(parts.value(1,parts.value(0)));if(!first.hasMatch()||!last.hasMatch()||parts.size()>2)throw std::runtime_error("Enter a rectangular source such as A1:J100001");return {{"sheet",sheet.toStdString()},{"row1",first.captured(2).toInt()},{"col1",columnNumber(first.captured(1).toUpper().toStdString())},{"row2",last.captured(2).toInt()},{"col2",columnNumber(last.captured(1).toUpper().toStdString())}};}
}
void MainWindow::showPivotFields(const Json& definition){
 auto& s=*state_;
 if(!s.pivotDock){
  s.pivotDock=new QDockWidget("PivotTable fields",this);s.pivotDock->setObjectName("workspace.pivotFields");s.pivotDock->setAccessibleName("PivotTable fields and report filters");s.pivotPane=new PivotFieldsPane(s.pivotDock);s.pivotDock->setWidget(s.pivotPane);addDockWidget(Qt::RightDockWidgetArea,s.pivotDock);tabifyDockWidget(s.dock,s.pivotDock);
  s.pivotPane->applyRequested=[this](const Json& requested){auto definition=state_->reportDefinition;definition.update(requested);executeCommand("pivot.update",{{"definition",definition}});};
  s.pivotPane->insertChartRequested=[this](QString type){executeCommand("pivot.chart",{{"pivotId",state_->reportDefinition.value("id",std::string())},{"type",type.toStdString()}});};
  s.pivotPane->deleteRequested=[this]{executeCommand("pivot.delete",{{"id",state_->reportDefinition.value("id",std::string())}});};
  s.pivotPane->memberSearchRequested=[this](QString field,QString query){try{const auto& pivot=selectedPivot(state_->session.snapshotView(),state_->reportDefinition.at("id").get<std::string>());Json matches=Json::array();std::size_t total=0;for(const auto& descriptor:pivot.at("cache").at("fields"))if(descriptor.at("name")==field.toStdString())for(const auto& item:descriptor.at("items")){const auto label=item.at("type")=="blank"?QString("(blank)"):item.at("value").is_string()?QString::fromStdString(item.at("value").get<std::string>()):QString::fromStdString(item.at("value").dump());if(label.contains(query,Qt::CaseInsensitive)){if(matches.size()<200)matches.push_back(item);++total;}}state_->pivotPane->setMemberResults(field,matches,total);}catch(const std::exception& error){statusBar()->showMessage(error.what(),10000);}};
  s.pivotPane->sourceRequested=[this](QString sheet,QString range){try{auto definition=state_->reportDefinition;try{definition=state_->pivotPane->definition();}catch(const std::exception&){}definition["source"]=reportRange(sheet,range);showPivotFields(definition);}catch(const std::exception& error){state_->error=QString::fromUtf8(error.what());statusBar()->showMessage(state_->error,10000);}};
 }
 const auto& book=s.session.snapshotView();auto fields=definition.contains("cache")?Json():reportHeaders(s.session,definition.at("source"),s.calculated,s.contextGeneration);s.pivotPane->setDefinition(definition,reportSheets(book),definition.contains("cache")?definition.at("cache").at("fields"):fields);s.reportDefinition=editableReport(definition);s.pivotDock->show();s.pivotDock->raise();
}
bool MainWindow::startFinancialModel(const Json& definition,const std::string& action,bool allowCachedLinks){
 auto& s=*state_;if(s.financialWorkspace&&s.financialWorkspace->running())throw std::runtime_error("A financial job is already running; stop it before starting another");commitEditor();if(!s.financialPane)showFinancialFields("forecast");if(!s.financialWorkspace){
  s.financialWorkspace=new FinancialWorkspace(s.session,this);
  s.financialWorkspace->epoch=[this]{return std::pair<std::string,std::uint64_t>{state_->recoveryId,state_->contextGeneration};};
  s.financialWorkspace->retainedLinks=[this]() ->const Json&{return state_->retainedLinks;};
  s.financialWorkspace->mayStart=[this]{return !property("reportBusy").toBool()&&!property("codexToolBusy").toBool()&&!state_->reportPublication;};
  s.financialWorkspace->prepareRequest=[this](Json& request){request["sourcePath"]=state_->path.toStdString();request["nativeLinkParts"]=Json::object();if(state_->package)for(const auto& [name,bytes]:state_->package->entries)if(name=="xl/workbook.xml"||name=="xl/_rels/workbook.xml.rels"||name.starts_with("xl/externalLinks/"))request["nativeLinkParts"][name]=bytes;};
  s.financialWorkspace->onStarting=[this]{auto& s=*state_;++s.calcSerial;s.calcCapture->stop();s.scheduler->cancel();s.timer->stop();s.error.clear();setProperty("financialBusy",true);s.financialPane->setRunState("running","Preparing immutable financial model inputs…");};
  s.financialWorkspace->onProgress=[this](QString message){statusBar()->showMessage(message);};
  s.financialWorkspace->onError=[this](QString message){state_->error=message;setProperty("financialBusy",false);state_->financialPane->setRunState("error",message);statusBar()->showMessage(message,15000);state_->timer->start();};
  s.financialWorkspace->onAnalysis=[this](const Json& analysis){setProperty("financialBusy",false);setProperty("financialAnalysis",QString::fromStdString(analysis.dump()));const auto detail=financialAnalysisSummary(analysis);state_->financialSummary=detail;state_->financialPane->setRunState(analysis.value("success",false)?"complete":"error",detail);if(!analysis.value("success",false))state_->error=detail;else state_->error.clear();state_->timer->start();};
  s.financialWorkspace->onPreview=[this]{auto& s=*state_;++s.calcSerial;s.calcCapture->stop();s.scheduler->cancel();s.previewContext=s.contextGeneration;s.model->showPreviewView(*s.session.previewView());if(!s.financialOutputSheet.empty())s.model->setSheetId(QString::fromStdString(s.financialOutputSheet));else if(s.financialAction=="example")s.model->setSheetId(QString::fromStdString(s.session.previewView()->at("sheets").back().at("id").get<std::string>()));s.rebuildSheets();s.previewBar->show();s.formula->setReadOnly(true);s.financialPane->setRunState("preview",s.financialSummary+"\nAccept applies these changes as one undoable transaction.",true);s.timer->start();};
 }
 s.financialWorkspace->executable=property("reportWorkerExecutable").toString();const bool started=s.financialWorkspace->start(definition,action,allowCachedLinks);if(started){s.financialOutputSheet=definition.value("output",Json::object()).value("sheet",std::string());s.financialAction=action;}return started;
}
void MainWindow::showFinancialFields(const std::string& kind){
 auto& s=*state_;if(!kind.empty()&&(s.session.hasPreview()||(s.financialWorkspace&&s.financialWorkspace->running())))throw std::runtime_error("Accept or discard the current preview and stop active financial work before changing setups");if(!s.financialPane){s.financialDock=new QDockWidget("Forecasts and what-if models",this);s.financialDock->setObjectName("financial.dock");s.financialPane=new FinancialModelPane(s.financialDock);s.financialDock->setWidget(s.financialPane);addDockWidget(Qt::RightDockWidgetArea,s.financialDock);
  s.financialPane->saveRequested=[this](const Json& definition){executeCommand("financial.save",{{"definition",definition},{"useCachedLinks",state_->financialPane->allowCachedLinks()}});};s.financialPane->runRequested=[this](const Json& definition){executeCommand("financial.run",{{"definition",definition},{"useCachedLinks",state_->financialPane->allowCachedLinks()}});};s.financialPane->deleteRequested=[this](const std::string& id){executeCommand("financial.delete",{{"definition",{{"id",id}}}});};s.financialPane->settingsRequested=[this](const Json& settings){executeCommand("financial.settings",{{"definition",{{"kind","iteration"},{"settings",settings}}}});};s.financialPane->exampleRequested=[this]{executeCommand("financial.example");};s.financialPane->acceptRequested=[this]{executeCommand("preview.accept");};s.financialPane->discardRequested=[this]{executeCommand("preview.discard");};s.financialPane->cancelRequested=[this]{executeCommand("financial.cancel");};
 }
 s.refreshFinancialFields();if(!kind.empty()){const auto range=s.range();s.financialPane->setDefinition(defaultFinancialDefinition(kind,s.model->sheetId().toStdString(),s.financialSelection,s.session.snapshotView().value("financialModels",Json::array())));}s.financialDock->show();s.financialDock->raise();
}
bool MainWindow::startReport(Json definition,bool useCachedLinks){
 auto& s=*state_;if(s.session.hasPreview())throw std::runtime_error("Accept or discard the current preview before refreshing reports");commitEditor();
 if(!s.reportCapture){s.reportCapture=new SnapshotCapture(this,1024);s.reportWorker=new ReportWorker(this);s.reportStop=new QPushButton("Stop report",this);s.reportStop->setObjectName("report.stop");s.reportStop->setAccessibleName("Cancel report refresh");statusBar()->addPermanentWidget(s.reportStop);s.reportStop->hide();connect(s.reportStop,&QPushButton::clicked,this,[this]{executeCommand("pivot.cancel");});}
 if((s.financialWorkspace&&s.financialWorkspace->running())||s.reportCapture->running()||s.reportWorker->running()||s.reportPublication||property("codexToolBusy").toBool())throw std::runtime_error("A report is already running; stop it before starting another");
 ++s.calcSerial;s.timer->stop();s.calcCapture->stop();s.scheduler->cancel();
 if(!definition.contains("id"))definition["id"]=QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();if(!definition.contains("name"))definition["name"]="PivotTable1";
 s.reportConversionId=definition.value("conversionLegacyId",std::string());definition.erase("conversionLegacyId");s.reportPrefix=Json::array();
 if(definition.at("output").value("sheet",std::string()).empty()){
  const auto sheetId=QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();auto name=definition.value("newWorksheetName",std::string("Report"));if(name.empty())name="Report";const auto initial=name;int suffix=2;const auto exists=[&](const std::string& candidate){for(const auto& sheet:s.session.snapshotView().at("sheets"))if(QString::fromStdString(sheet.at("name").get<std::string>()).compare(QString::fromStdString(candidate),Qt::CaseInsensitive)==0)return true;return false;};while(exists(name))name=initial.substr(0,25)+" "+std::to_string(suffix++);definition["output"]["sheet"]=sheetId;s.reportPrefix.push_back({{"type","add_sheet"},{"id",sheetId},{"name",name}});
 }
 definition.erase("newWorksheet");definition.erase("newWorksheetName");s.reportDefinition=editableReport(definition);s.reportRevision=s.session.snapshotView().at("revision");s.reportContext=s.contextGeneration;const auto serial=++s.reportSerial;s.reportStop->show();setProperty("reportBusy",true);statusBar()->showMessage("Preparing report inputs…");
 struct WarmBinding {std::optional<std::uint64_t> serial;const Json::object_t* identity=nullptr;};auto warmBinding=std::make_shared<WarmBinding>();
 const auto current=[this,serial,warmBinding]{return state_->reportSerial==serial&&state_->contextGeneration==state_->reportContext&&state_->session.snapshotView().at("revision")==state_->reportRevision&&(!warmBinding->serial||(state_->calcSerial==*warmBinding->serial&&state_->calculated.get_ptr<const Json::object_t*>()==warmBinding->identity));};
 s.reportCapture->failed=[this](QString message){state_->reportBook.reset();state_->reportInputs.reset();state_->reportCapture->completed={};state_->error=message;state_->reportStop->hide();setProperty("reportBusy",false);statusBar()->showMessage(message,10000);};
 s.reportWorker->onError=[this,current,useCachedLinks](QString message){state_->reportInputs.reset();state_->error=message;state_->reportStop->hide();setProperty("reportBusy",false);statusBar()->showMessage(message,15000);if(!useCachedLinks&&message.contains("cached_link_consent_required")&&current()){auto definition=state_->reportDefinition;if(!state_->reportConversionId.empty())definition["conversionLegacyId"]=state_->reportConversionId;for(const auto& operation:state_->reportPrefix)if(operation.at("type")=="add_sheet"){definition["output"]["sheet"]="";definition["newWorksheetName"]=operation.at("name");}QTimer::singleShot(0,this,[this,current,definition]{if(!current())return;if(QMessageBox::question(this,"Use saved linked values?","A linked workbook is unavailable or has only saved values. Use those saved values for this report? The report will retain this source status.",QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)==QMessageBox::Yes)try{startReport(definition,true);}catch(const std::exception& error){state_->error=QString::fromUtf8(error.what());statusBar()->showMessage(state_->error,15000);}});}};
 s.reportWorker->onProgress=[this](QString stage){statusBar()->showMessage(stage);};
 s.reportWorker->onResult=[this,current](Json result){auto& s=*state_;if(!current()){auto* disposal=QThread::create([result=std::move(result),inputs=std::move(s.reportInputs)]() mutable {result=Json();inputs.reset();});connect(disposal,&QThread::finished,disposal,&QObject::deleteLater);disposal->start();s.reportStop->hide();setProperty("reportBusy",false);return;}statusBar()->showMessage("Preparing the report transaction…");
  struct Publication {std::unique_ptr<WorkbookSession::PreparedTransaction> prepared;std::string error,id,sheet;};auto publication=std::make_shared<Publication>();auto inputs=std::move(s.reportInputs);const auto prefix=s.reportPrefix;const auto conversion=s.reportConversionId;const auto expected=s.reportRevision;
  auto* thread=QThread::create([publication,inputs=std::move(inputs),prefix,conversion,result=std::move(result)]() mutable {try{if(!inputs||result.value("status",std::string())!="ok")throw std::runtime_error("Report did not complete successfully");auto operations=prefix;PivotGeneration generation{std::move(result.at("definition")),std::move(result.at("cells"))};publication->id=generation.definition.at("id");publication->sheet=generation.definition.at("output").at("sheet");auto& captured=inputs->at("workbook");if(!conversion.empty()){Json retained=Json::array();for(const auto& pivot:captured.at("pivots"))if(pivot.value("id",std::string())!=conversion)retained.push_back(pivot);operations.push_back({{"type","set_metadata"},{"key","pivots"},{"value",std::move(retained)}});}for(auto& operation:pivotOperations(captured,generation))operations.push_back(std::move(operation));for(const auto& operation:prefix)if(operation.at("type")=="add_sheet"){auto& sheets=captured["sheets"].get_ref<Json::array_t&>();std::erase_if(sheets,[&](const Json& sheet){return sheet.at("id")==operation.at("id");});}publication->prepared=WorkbookSession::prepareTransaction(std::move(captured),operations,&publication->error);}catch(const std::exception& error){publication->error=error.what();}inputs.reset();result=Json();});s.reportPublication=thread;
  connect(thread,&QThread::finished,this,[this,current,expected,publication,thread,conversion]() mutable {auto& s=*state_;QElapsedTimer trace;trace.start();QVariantMap callbackTimes;const auto mark=[&](const char* name){callbackTimes[QString::fromLatin1(name)]=trace.nsecsElapsed()/1e6;};if(s.reportPublication==thread)s.reportPublication=nullptr;try{if(!current()||s.session.hasPreview())throw std::runtime_error("Workbook or linked inputs changed during refresh; the report candidate was discarded");if(!publication->prepared)throw std::runtime_error(publication->error);const bool changed=publication->prepared->changed();std::string error;if(!conversion.empty()){++s.calcSerial;s.calcCapture->stop();s.scheduler->cancel();if(!s.session.previewPrepared(expected,publication->prepared,&error))throw std::runtime_error(error);s.previewContext=s.contextGeneration;s.model->showPreviewView(*s.session.previewView());s.model->setSheetId(QString::fromStdString(publication->sheet));s.rebuildSheets();s.previewBar->show();s.formula->setReadOnly(true);s.timer->start();statusBar()->showMessage("Conversion preview — Accept creates the native report; original summary cells are retained",15000);}else{if(!s.session.commitPrepared(expected,*publication->prepared,&error))throw std::runtime_error(error);s.error.clear();mark("commitPreparedCumulativeMs");if(changed){s.model->refresh();s.model->setSheetId(QString::fromStdString(publication->sheet));mark("modelRefreshCumulativeMs");if(s.model->onChanged)s.model->onChanged();mark("onChangedCumulativeMs");s.rebuildSheets();mark("sheetRebuildCumulativeMs");showPivotFields(selectedPivot(s.session.snapshotView(),publication->id));mark("fieldsPaneCumulativeMs");s.timer->start();}statusBar()->showMessage(changed?"Report refreshed — Undo restores the previous table, cache and charts":"Report is current — no changes to undo",10000);}}catch(const std::exception& error){if(current()){s.error=QString::fromUtf8(error.what());statusBar()->showMessage(s.error,15000);}}mark("totalCumulativeMs");setProperty("reportPublicationTimings",callbackTimes);if(s.reportSerial==thread->property("reportSerial").toULongLong()){s.reportStop->hide();setProperty("reportBusy",false);}auto* disposal=QThread::create([publication=std::move(publication)]() mutable {publication.reset();});connect(disposal,&QThread::finished,disposal,&QObject::deleteLater);disposal->start();});thread->setProperty("reportSerial",QVariant::fromValue<qulonglong>(s.reportSerial));connect(thread,&QThread::finished,thread,&QObject::deleteLater);thread->start();
 };
 s.reportCapture->completed=[this,current,useCachedLinks,warmBinding](std::shared_ptr<Json> captured){
  auto& s=*state_;if(!current()){s.reportCapture->failed("Workbook changed while preparing report inputs");return;}s.reportBook=std::move(captured);
  s.reportCapture->completed=[this,current,useCachedLinks,warmBinding](std::shared_ptr<Json> retained){
   auto& s=*state_;if(!current()){s.reportCapture->failed("Linked inputs changed while preparing report inputs");return;}
   auto request=std::move(s.reportBook);auto workbook=std::move(*request);*request=Json::object();(*request)["version"]=1;(*request)["workbook"]=std::move(workbook);
   for(const auto& prefix:s.reportPrefix)if(prefix.at("type")=="add_sheet")(*request)["workbook"]["sheets"].push_back({{"id",prefix.at("id")},{"name",prefix.at("name")},{"cells",Json::array()}});
   (*request)["definition"]=s.reportDefinition;(*request)["retainedLinks"]=std::move(*retained);(*request)["contextGeneration"]=s.reportContext;(*request)["allowCachedLinks"]=useCachedLinks;(*request)["sourcePath"]=s.path.toStdString();(*request)["nativeLinkParts"]=Json::object();
   bool nativeExternal=false;if(s.package)for(const auto& [name,bytes]:s.package->entries){if(name.starts_with("xl/externalLinks/")||(name=="xl/workbook.xml"&&bytes.find("externalReference")!=std::string::npos)||(name=="xl/_rels/workbook.xml.rels"&&bytes.find("relationships/externalLink")!=std::string::npos))nativeExternal=true;if(name=="xl/workbook.xml"||name=="xl/_rels/workbook.xml.rels"||name.starts_with("xl/externalLinks/"))(*request)["nativeLinkParts"][name]=bytes;}
   s.reportInputs=request;const auto& result=s.calculated;const auto iteration=result.find("iteration");const auto links=(*request)["workbook"].find("links");
   const bool reusable=result.value("status",std::string())=="ok"&&result.value("converged",true)&&result.value("revision",Json())==s.reportRevision&&result.value("contextGeneration",Json())==s.reportContext&&(iteration==result.end()||!iteration->is_object()||iteration->value("converged",true))&&(links==(*request)["workbook"].end()||links->empty())&&(*request)["retainedLinks"].empty()&&!nativeExternal;
   if(!reusable){if(!s.reportWorker->start(std::move(request),property("reportWorkerExecutable").toString()))s.reportWorker->onError(s.reportWorker->error());return;}
   warmBinding->serial=s.calcSerial;warmBinding->identity=s.calculated.get_ptr<const Json::object_t*>();statusBar()->showMessage("Preparing current source values…");
   s.reportCapture->completed=[this,current,request=std::move(request)](std::shared_ptr<Json> calculated){
    auto& s=*state_;s.reportCapture->completed={};if(!current()){s.reportCapture->failed("Workbook changed while preparing current source values");return;}
    struct WarmInputs {std::shared_ptr<Json> request;std::string error;};auto warm=std::make_shared<WarmInputs>();
    auto* thread=QThread::create([warm,request,calculated=std::move(calculated)]() mutable {
     try{
      const auto cancelled=[] {return QThread::currentThread()->isInterruptionRequested();};const auto& book=request->at("workbook");auto cache=buildPivotSourceCache(book,*calculated,request->at("definition"),cancelled);
      auto compact=std::make_shared<Json>(Json::object());(*compact)["version"]=1;(*compact)["contextGeneration"]=request->at("contextGeneration");(*compact)["definition"]=request->at("definition");(*compact)["allowCachedLinks"]=request->at("allowCachedLinks");(*compact)["retainedLinks"]=Json::array();(*compact)["workbook"]={{"revision",book.at("revision")},{"sheets",Json::array()}};
      auto& source=(*compact)["sourceCache"];source["revision"]=book.at("revision");source["contextGeneration"]=request->at("contextGeneration");source["calculation"]={{"revision",calculated->at("revision")},{"contextGeneration",calculated->at("contextGeneration")},{"status",calculated->at("status")},{"converged",true}};source["source"]=request->at("definition").at("source");source["fields"]=std::move(cache.fields);source["records"]=std::move(cache.records);
      for(const auto& sheet:book.at("sheets"))(*compact)["workbook"]["sheets"].push_back({{"id",sheet.at("id")},{"name",sheet.at("name")},{"cells",Json::array()}});
      if(cancelled())throw std::runtime_error("Report refresh cancelled");warm->request=std::move(compact);
     }catch(const std::exception& error){warm->error=error.what();}
    });s.reportPublication=thread;thread->setProperty("reportSerial",QVariant::fromValue<qulonglong>(s.reportSerial));
    connect(thread,&QThread::finished,this,[this,current,warm,thread]() mutable {
     auto& s=*state_;if(s.reportPublication==thread)s.reportPublication=nullptr;
     if(current()&&warm->error.empty()){if(!s.reportWorker->start(std::move(warm->request),property("reportWorkerExecutable").toString()))s.reportWorker->onError(s.reportWorker->error());}
     else if(s.reportSerial==thread->property("reportSerial").toULongLong()){s.reportInputs.reset();s.error=QString::fromStdString(warm->error.empty()?"Workbook or linked inputs changed during refresh; the report candidate was discarded":warm->error);s.reportStop->hide();setProperty("reportBusy",false);statusBar()->showMessage(s.error,10000);}
     auto* disposal=QThread::create([warm=std::move(warm)]() mutable {warm.reset();});connect(disposal,&QThread::finished,disposal,&QObject::deleteLater);disposal->start();
    });connect(thread,&QThread::finished,thread,&QObject::deleteLater);thread->start();
   };
   s.reportCapture->start(s.calculated,current);
  };
  s.reportCapture->start(s.retainedLinks,current);
 };
 return s.reportCapture->start(s.session.snapshotView(),current);
}
bool MainWindow::reportingCommand(const QString& command,const Json& args){
 auto& s=*state_;
 if(command=="pivot.cancel"){const bool hadActiveReport=(s.reportCapture&&s.reportCapture->running())||(s.reportWorker&&s.reportWorker->running())||s.reportPublication;++s.reportSerial;if(s.reportCapture)s.reportCapture->stop();if(s.reportWorker)s.reportWorker->stop();if(s.reportPublication)s.reportPublication->requestInterruption();s.reportBook.reset();s.reportInputs.reset();if(s.reportStop)s.reportStop->hide();setProperty("reportBusy",false);if(hadActiveReport)statusBar()->showMessage("Report stopped; workbook changes were discarded",10000);return true;}
 const auto& book=s.session.snapshotView();
 if(command=="pivot.convert"){std::string legacy=args.value("id",std::string());if(legacy.empty()){QStringList labels;std::vector<std::string> ids;if(book.contains("pivots"))for(const auto& pivot:book.at("pivots"))if(pivot.value("staticSummary",false)){ids.push_back(pivot.at("id"));labels<<QString::fromStdString(pivot.value("rowField",std::string())+" / "+pivot.value("valueField",std::string()));}if(ids.empty())throw std::runtime_error("This workbook has no static summaries to convert");bool accepted=false;const auto selected=QInputDialog::getItem(this,"Convert static summary","Choose a summary to preview as a native PivotTable on a new worksheet. Original summary cells will be retained.",labels,0,false,&accepted);if(!accepted)return false;legacy=ids.at(labels.indexOf(selected));}const auto newId=QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();auto definition=staticPivotConversionDefinition(book,legacy,QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(),newId,"Converted_"+newId.substr(0,8));definition["output"]["sheet"]="";definition["newWorksheetName"]="Converted report";definition["conversionLegacyId"]=legacy;return startReport(std::move(definition),args.value("useCachedLinks",false));}
 std::string id=args.value("id",args.value("pivotId",std::string()));if(id.empty()){const auto index=s.grid->currentIndex();if(book.contains("pivots"))for(const auto& pivot:book.at("pivots"))if(pivot.value("version",0)==1&&pivot.at("output").at("sheet")==s.model->sheetId().toStdString()&&index.row()+1>=pivot.at("output").at("row1")&&index.row()+1<=pivot.at("output").at("row2")&&index.column()+1>=pivot.at("output").at("col1")&&index.column()+1<=pivot.at("output").at("col2")){id=pivot.at("id");break;}}
 if(command=="pivot.fields"){showPivotFields(selectedPivot(book,id));return true;}
 if(command=="pivot.chart"){auto charts=book.value("charts",Json::array());auto chart=pivotChart(book,args.value("pivotId",id),args.value("id",QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString()),args.value("type",std::string("column")));charts.push_back(std::move(chart));return s.model->apply(Json::array({{{"type","set_metadata"},{"key","charts"},{"value",std::move(charts)}}}));}
 if(command=="pivot.delete"){selectedPivot(book,id);int count=0;for(const auto& chart:book.value("charts",Json::array()))count+=chart.value("pivotId",std::string())==id;if(!args.contains("accept")&&QMessageBox::question(this,"Delete report",QString("Delete this PivotTable and its %1 linked chart(s)?").arg(count))!=QMessageBox::Yes)return false;if(!args.value("accept",true))return false;const auto result=s.model->apply(deletePivotOperations(book,id));if(result&&s.pivotDock)s.pivotDock->hide();return result;}
 if(command=="pivot.refresh"||command=="pivot.move"){auto definition=editableReport(selectedPivot(book,id));if(args.contains("output"))definition["output"]=args.at("output");return startReport(std::move(definition),args.value("useCachedLinks",false));}
 if(command=="pivot.update"&&args.contains("definition"))return startReport(editableReport(args.at("definition")),args.value("useCachedLinks",false));
 if(command=="pivot.create"){
  if(args.contains("definition"))return startReport(args.at("definition"),args.value("useCachedLinks",false));auto range=s.range();if(range.height()<2||range.width()<2){int row=1,col=1;for(const auto& sheet:book.at("sheets"))if(sheet.at("id")==s.model->sheetId().toStdString())for(const auto& cell:sheet.at("cells")){row=std::max(row,cell.at("row").get<int>());col=std::max(col,cell.at("col").get<int>());}range=QItemSelectionRange(s.model->index(0,0),s.model->index(row-1,col-1));}
  Json definition={{"name","PivotTable"+std::to_string(book.value("pivots",Json::array()).size()+1)},{"source",{{"sheet",s.model->sheetId().toStdString()},{"row1",range.top()+1},{"col1",range.left()+1},{"row2",range.bottom()+1},{"col2",range.right()+1}}},{"rows",Json::array()},{"columns",Json::array()},{"filters",Json::array()},{"values",Json::array()},{"output",{{"sheet",""},{"row1",1},{"col1",1}}}};const auto fields=reportHeaders(s.session,definition.at("source"),s.calculated,s.contextGeneration);if(!fields.empty())definition["rows"].push_back({{"field",fields[0].at("name")}});if(fields.size()>1)definition["values"].push_back({{"field",fields[1].at("name")},{"aggregate","sum"},{"name","Sum of "+fields[1].at("name").get<std::string>()},{"numberFormat","#,##0.00"}});showPivotFields(definition);return true;
 }
 return false;
}
void MainWindow::updateSelectionStatus(){
 auto& s=*state_;if(!s.statistics||!s.model)return;const auto range=s.range();const auto serial=++s.selectionSerial;const auto revision=s.bookView().at("revision").get<std::uint64_t>();const auto sheet=s.model->sheetId();const auto* preview=s.session.previewView();s.selection->setText("Calculating selection…");s.statistics->start(*s.model,range.top(),range.left(),range.bottom(),range.right(),[this,serial,revision,sheet,preview]{return state_->selectionSerial==serial&&state_->bookView().at("revision")==revision&&state_->model->sheetId()==sheet&&state_->session.previewView()==preview;},[this](SelectionTotals totals){
  if(totals.populated<=1){state_->selection->clear();return;}
  auto display=[&](double value,int precision){if(totals.numberFormat!="General")try{return QString::fromStdString(formatCellValue(value,totals.numberFormat)).trimmed();}catch(const std::exception&){}return QString::number(value,'g',precision);};
  state_->selection->setText(totals.count?QString("Average: %1     Count: %2     Sum: %3").arg(display(totals.sum/totals.count,10)).arg(static_cast<qulonglong>(totals.populated)).arg(display(totals.sum,12)):QString("Count: %1").arg(static_cast<qulonglong>(totals.populated)));
 });
}
void MainWindow::queueCalculation(){
 auto& s=*state_;s.calcCapture->stop();const auto serial=++s.calcSerial,revision=s.session.snapshotView().at("revision").get<std::uint64_t>(),generation=s.contextGeneration;const auto* preview=s.session.previewView();
 const auto current=[this,serial,revision,generation,preview]{return state_->calcSerial==serial&&state_->contextGeneration==generation&&state_->session.snapshotView().at("revision")==revision&&state_->session.previewView()==preview;};
 s.calcCapture->failed=[this,current](QString error){if(current()){state_->error=error;statusBar()->showMessage(error,10000);}};
 s.calcCapture->completed=[this,current,generation](std::shared_ptr<Json> book){if(!current())return;state_->calcCapture->completed=[this,current,generation,book=std::move(book)](std::shared_ptr<Json> retained){if(!current())return;WorkbookPackage parts;parts.sourcePath=state_->path.toStdString();if(state_->package)for(const auto& [name,xml]:state_->package->entries)if(name=="xl/workbook.xml"||name=="xl/_rels/workbook.xml.rels"||name.starts_with("xl/externalLinks/"))parts.entries.emplace(name,xml);auto live=captureLiveWorkbooks(state_->liveSource.get());state_->scheduler->request(std::move(*book),[retained=std::move(retained),parts=std::move(parts),generation,live=std::move(live)](Json captured) mutable {WorkbookLinkOptions options;options.retainedLinks=std::move(*retained);options.liveWorkbooks=std::move(live);auto prepared=prepareWorkbookLinksOwned(std::move(captured),parts.sourcePath.empty()&&parts.entries.empty()?nullptr:&parts,options);prepared.snapshot["_tigersheetsComputedResults"]=true;prepared.snapshot["_tigersheetsContextGeneration"]=generation;prepared.snapshot["_tigersheetsLinkDiagnostics"]=std::move(prepared.diagnostics);return std::move(prepared.snapshot);});};state_->calcCapture->start(state_->retainedLinks,current);};s.calcCapture->start(s.bookView(),current);
}
void MainWindow::recalculateNow(){
 auto& s=*state_;++s.calcSerial;if(s.calcCapture)s.calcCapture->stop();s.scheduler->cancel();s.calculated={{"revision",s.bookView().at("revision")},{"status","stale"},{"values",Json::array()}};s.model->setCalculated(s.calculated);++s.selectionSerial;s.statistics->cancel();s.selection->clear();try{
  auto book=calculationSnapshot();book["_tigersheetsComputedResults"]=true;auto macro=book.value("vba",Json::object());std::string source;for(const auto& module:macro.value("modules",Json::array()))source+=module.value("source",std::string())+"\n";
  CalculationEngine engine;auto result=macro.value("enabled",false)&&!source.empty()?engine.calculate(book,[&](const std::string& name,const Json& args){return evaluateVbaFunction(book,source,name,args);}):engine.calculate(book);
  for(const auto& diagnostic:book.value("_tigersheetsLinkDiagnostics",Json::array()))if(std::find(result["diagnostics"].begin(),result["diagnostics"].end(),diagnostic)==result["diagnostics"].end())result["diagnostics"].push_back(diagnostic);
  result["contextGeneration"]=s.contextGeneration;displayCalculation(std::move(result));
 }catch(const std::exception&e){s.error=QString::fromUtf8(e.what());statusBar()->showMessage(s.error,10000);}
}
void MainWindow::displayCalculation(Json incoming){
 auto& s=*state_;try{const auto& book=s.bookView();
 if(incoming.contains("contextGeneration")&&!incoming["contextGeneration"].is_null()&&incoming["contextGeneration"]!=s.contextGeneration)return;if(incoming.value("revision",std::uint64_t{})!=book.at("revision").get<std::uint64_t>())return;s.calculated=std::move(incoming);const auto& result=s.calculated;
 QStringList linkDetails;bool savedLinks=false,unavailableLinks=false;
 for(const auto& diagnostic:result.value("diagnostics",Json::array()))if(diagnostic.is_object()&&diagnostic.value("code",std::string()).starts_with("external_link_")){
  const auto code=diagnostic.value("code",std::string()),message=diagnostic.value("message",std::string());savedLinks|=code=="external_link_cached";unavailableLinks|=code!="external_link_cached";
  const auto source=diagnostic.value("workbook",diagnostic.value("path",std::string()));linkDetails<<QString::fromStdString(source.empty()?message:source+": "+message);
 }
 linkDetails.removeDuplicates();s.linkStatus->setText(savedLinks?(unavailableLinks?"Saved/unavailable linked sources":"Using saved linked values"):(unavailableLinks?"Linked sources unavailable":""));s.linkStatus->setToolTip(linkDetails.join('\n'));s.linkStatus->setAccessibleDescription(linkDetails.join('\n'));s.linkStatus->setVisible(!linkDetails.isEmpty());
 if(result.value("status",std::string())!="ok"){QStringList errors;for(const auto& diagnostic:result.value("diagnostics",Json::array()))errors<<QString::fromStdString(diagnostic.is_string()?diagnostic.get<std::string>():diagnostic.value("message",diagnostic.dump()));s.model->setCalculated(result);++s.selectionSerial;s.statistics->cancel();s.selection->clear();for(auto* chart:s.charts)delete chart;s.charts.clear();s.error=errors.join("; ");statusBar()->showMessage(s.error);return;}
 s.error.clear();
 for(const auto& sheet:book["sheets"])if(sheet["id"]==s.model->sheetId().toStdString())static_cast<WorksheetView*>(s.grid)->setCurrentSheetName(QString::fromStdString(sheet["name"].get<std::string>()));
  s.model->setCalculated(s.calculated);
  updateSelectionStatus();
  std::vector<std::pair<Json,Json>> prepared;
  if(book.contains("charts"))for(const auto& spec:book.at("charts"))if(spec.value("sheet",std::string())==s.model->sheetId().toStdString())try{const auto id=spec.value("id",std::string());if(s.calculated.contains("preparedCharts")&&s.calculated["preparedCharts"].contains(id)){const auto& chart=s.calculated["preparedCharts"][id];if(chart.contains("error"))throw std::runtime_error(chart.at("error").get<std::string>());prepared.emplace_back(spec,chart.at("data"));}else prepared.emplace_back(spec,chartData(book,s.calculated,spec));}catch(const std::exception& error){s.error=QString::fromUtf8(error.what());statusBar()->showMessage(s.error+" — complete worksheet retained",10000);}
  std::vector<ChartWidget*> retained;
  for(auto& [spec,data]:prepared){
   const auto id=spec.value("id",std::string());const auto objectName="chart."+QString::fromStdString(id);
   auto existing=std::find_if(s.charts.begin(),s.charts.end(),[&](auto* chart){return chart->objectName()==objectName;});
   auto* chart=existing==s.charts.end()?new ChartWidget(s.grid->viewport()):*existing;
   if(existing==s.charts.end()){
    chart->setObjectName(objectName);
    chart->selectionChanged=[this,chart,id](bool selected){if(!selected)return;for(auto* other:state_->charts)if(other!=chart)other->setSelected(false);const auto& book=state_->session.snapshotView();if(book.contains("charts"))for(const auto& spec:book.at("charts"))if(spec.value("id",std::string())==id&&spec.contains("pivotId"))try{showPivotFields(selectedPivot(book,spec.at("pivotId").get<std::string>()));}catch(const std::exception& error){statusBar()->showMessage(error.what(),10000);}};
    chart->geometryCommitted=[this,id](QRect before,QRect after){if(!executeCommand("chart.geometry",{{"id",id},{"x",after.x()},{"y",after.y()},{"width",after.width()},{"height",after.height()},{"beforeX",before.x()},{"beforeY",before.y()}}))recalculateNow();};
    chart->deleteRequested=[this,id]{executeCommand("chart.delete",{{"id",id}});};
    chart->editRequested=[this,id](ChartEditAction action){executeCommand("chart.edit",{{"id",id},{"section",static_cast<int>(action)}});};
   }
   chart->setChart(spec,std::move(data));
   const auto anchor=spec.value("anchor",Json::object());chart->setProperty("anchorRow",anchor.value("row",12)-1);chart->setProperty("anchorColumn",anchor.value("col",4)-1);chart->setProperty("anchorOffsetX",anchor.value("offsetX",0));chart->setProperty("anchorOffsetY",anchor.value("offsetY",0));
   if(!chart->isInteracting()){
    const auto anchor=spec.value("anchor",Json::object());const auto rect=s.grid->visualRect(s.model->index(anchor.value("row",12)-1,anchor.value("col",4)-1));
    chart->setGeometry(rect.x()+anchor.value("offsetX",0),rect.y()+anchor.value("offsetY",0),anchor.value("width",560),anchor.value("height",300));
   }
   chart->show();retained.push_back(chart);
  }
  auto previousCharts=std::move(s.charts);s.charts=std::move(retained);
  for(auto* chart:previousCharts)if(std::find(s.charts.begin(),s.charts.end(),chart)==s.charts.end())delete chart;
 }catch(const std::exception& error){s.error=QString::fromUtf8(error.what());statusBar()->showMessage(s.error,10000);}
}
void MainWindow::positionCharts(){auto& s=*state_;for(auto* chart:s.charts)if(!chart->isInteracting()){const auto cell=s.grid->visualRect(s.model->index(chart->property("anchorRow").toInt(),chart->property("anchorColumn").toInt()));chart->move(cell.x()+chart->property("anchorOffsetX").toInt(),cell.y()+chart->property("anchorOffsetY").toInt());}}
void MainWindow::refreshChartPresentation(const Json& change){
 auto& s=*state_;const auto& book=s.bookView();const auto& changed=change.at("sheets");
 // Widths and row heights only move the existing widgets. Source styling
 // changes labels and axis formats, but does not change their numeric values.
 if(changed.empty()){positionCharts();return;}
 if(!book.contains("charts"))return;
 for(const auto& spec:book.at("charts")){
  // Existing unsupported chart metadata is reported by displayCalculation;
  // a style edit cannot repair it and must not throw after the edit commits.
  if(!spec.contains("range")||!spec.contains("sheet")||!spec.contains("id"))continue;
  const auto& range=spec.at("range");
  const bool affected=std::any_of(changed.begin(),changed.end(),[&](const Json& cells){return cells.at("sheet")==spec.at("sheet")&&cells.at("row1")<=range.at("row2")&&range.at("row1")<=cells.at("row2")&&cells.at("col1")<=range.at("col2")&&range.at("col1")<=cells.at("col2");});
  if(!affected)continue;
  const auto id=spec.at("id").get<std::string>();const auto name="chart."+QString::fromStdString(id);
  auto visible=std::find_if(s.charts.begin(),s.charts.end(),[&](const auto* chart){return chart->objectName()==name;});
  try{
   auto data=chartData(book,s.calculated,spec);
   // Refresh nonvisible sheets too: a later tab switch can reuse this cache.
   s.calculated["preparedCharts"][id]={{"data",std::move(data)}};
   if(visible!=s.charts.end())(*visible)->setChart(spec,s.calculated.at("preparedCharts").at(id).at("data"));
  }catch(const std::exception& error){
   s.calculated["preparedCharts"][id]={{"error",error.what()}};s.error=QString::fromUtf8(error.what());statusBar()->showMessage(s.error+" — complete worksheet retained",10000);
   if(visible!=s.charts.end()){delete *visible;s.charts.erase(visible);}
  }
 }
}
bool MainWindow::openWorkbook(const QString& path){
 try{commitEditor();WorkbookSession candidate;std::optional<WorkbookPackage> package;if(path.endsWith(".csv",Qt::CaseInsensitive))candidate=WorkbookSession(readCsv(path.toStdString()));else{package=readWorkbook(path.toStdString());candidate=WorkbookSession(package->workbook);}QFile original(path);if(!original.open(QIODevice::ReadOnly))throw std::runtime_error("Cannot retain original workbook for recovery");auto bytes=original.readAll().toStdString();resetWorkbookContext();state_->session=std::move(candidate);state_->originalBytes=std::move(bytes);state_->package=std::move(package);state_->path=path;if(state_->recovery){auto base=state_->package?std::move(state_->package->workbook):state_->session.snapshot();state_->session.takeRecoveryDelta();state_->recovery->seed(state_->recoveryId,std::move(base),std::make_shared<const std::string>(state_->originalBytes),path.toStdString());}state_->model->setSheetId({});state_->grid->setCurrentIndex(state_->model->index(0,0));state_->filename->setText(QFileInfo(path).fileName());state_->restoreWorkbookViews();state_->rebuildSheets();state_->dirty=false;state_->syncSaveState(*this);state_->error.clear();state_->timer->start();notifyLiveWorkbookChanged(state_->liveSource.get());return true;}
 catch(const std::exception& e){state_->error=QString::fromUtf8(e.what());return false;}
}
bool MainWindow::saveWorkbook(const QString& path){
 const auto savingMessage=QString("Saving %1...").arg(QFileInfo(path).fileName());
 bool savingStarted=false;
 const auto savingCleanup=qScopeGuard([this,&savingStarted,&savingMessage]{if(savingStarted&&statusBar()->currentMessage()==savingMessage)statusBar()->clearMessage();});
 try{QString saveNotice;if(state_->session.hasPreview())throw std::runtime_error("Accept or discard the preview before saving");commitEditor();if(state_->recovery)try{state_->recovery->flush();}catch(const std::exception& e){statusBar()->showMessage(QString("Recovery: ")+e.what(),10000);}savingStarted=true;statusBar()->showMessage(savingMessage);statusBar()->repaint();if(path.endsWith(".csv",Qt::CaseInsensitive)){auto& s=*state_;if(path==s.path){QFile existing(path);if(!existing.open(QIODevice::ReadOnly)||existing.readAll().toStdString()!=s.originalBytes)throw std::runtime_error("CSV changed outside TigerSheets; export a separate copy");}recalculateNow();const auto book=s.session.snapshot();const auto sheet=s.model->sheetId().toStdString();writeCsv(path.toStdString(),csvValuesSnapshot(book,s.calculated,sheet),sheet);bool simple=book["sheets"].size()==1&&book["sheets"][0]["name"]=="Sheet1";for(const auto* key:{"vba","names","charts","pivots","scenarios","financialModels","links"})if(book.contains(key)&&!book[key].empty())simple=false;for(const auto& sh:book["sheets"]){for(const auto* key:{"mergedRanges","rowLayout","columnLayout","hiddenRows","hiddenColumns"})if(sh.contains(key)&&!sh.at(key).empty())simple=false;for(const auto& cell:sh["cells"]){if(cell.value("input",std::string()).starts_with('=')||(cell.contains("style")&&!cell["style"].empty()))simple=false;}}if(path!=s.path||!simple){if(path==s.path){QFile exported(path);if(!exported.open(QIODevice::ReadOnly))throw std::runtime_error("Cannot retain exported CSV for recovery");s.originalBytes=exported.readAll().toStdString();if(s.recovery){s.recovery->flush();s.recovery->seed(s.recoveryId,book,std::make_shared<const std::string>(s.originalBytes),s.path.toStdString());s.recovery->persist(s.recoveryId);}}s.saveState->setText("CSV exported — save XLSX to retain workbook changes");s.error.clear();return true;}}else{auto package=state_->package.value_or(WorkbookPackage{});if(package.entries.contains("xl/vbaProject.bin")&&!path.endsWith(".xlsm",Qt::CaseInsensitive))throw std::runtime_error("Save this workbook as XLSM to preserve its macro project");recalculateNow();const auto book=state_->snapshotWithViews();const auto& calculated=state_->calculated;const bool current=calculated.value("status",std::string())=="ok"&&calculated.value("revision",Json())==book.at("revision")&&calculated.value("contextGeneration",Json())==Json(state_->contextGeneration)&&(!calculated.contains("iteration")||calculated.at("iteration").value("converged",true));const auto resolvedLinks=current?calculationSnapshot().value("links",Json::array()):Json::array();if(path.endsWith(".xls",Qt::CaseInsensitive))writeBiffWorkbook(path.toStdString(),book,&package);else if(current)writeWorkbookCalculated(path.toStdString(),book,calculated,&package,resolvedLinks);else{writeWorkbook(path.toStdString(),book,&package,resolvedLinks);saveNotice="Saved without recalculated values";if(!state_->error.isEmpty())saveNotice+=": "+state_->error;}state_->package=std::move(package);}QFile saved(path);if(!saved.open(QIODevice::ReadOnly))throw std::runtime_error("Cannot retain saved workbook for recovery");state_->originalBytes=saved.readAll().toStdString();if(path!=state_->path){cancelCodexTool();executeCommand("financial.cancel");executeCommand("pivot.cancel");++state_->contextGeneration;}if(state_->recovery){state_->recovery->erase(state_->recoveryId);auto base=state_->package&&!state_->package->workbook.is_null()?std::move(state_->package->workbook):state_->session.snapshot();state_->session.takeRecoveryDelta();state_->recovery->seed(state_->recoveryId,std::move(base),std::make_shared<const std::string>(state_->originalBytes),path.toStdString());state_->recovery->flush();}state_->path=path;state_->filename->setText(QFileInfo(path).fileName());state_->dirty=false;state_->syncSaveState(*this,saveNotice);if(saveNotice.isEmpty())statusBar()->clearMessage();else statusBar()->showMessage(saveNotice);state_->error.clear();return true;}
 catch(const std::exception& e){state_->error=QString::fromUtf8(e.what());return false;}
}
bool MainWindow::executeCommand(const QString& command,const Json& arguments){
 auto& s=*state_;
 try{
 if(command=="data.validation"){if(!static_cast<WorksheetView*>(s.grid)->commitPendingEditor())throw std::runtime_error("Finish editing the active cell before changing validation.");return s.validation->edit(arguments);}
 if(command.startsWith("cells.")||command.startsWith("rows.")||command.startsWith("columns.")){if(!static_cast<WorksheetView*>(s.grid)->commitPendingEditor())throw std::runtime_error("Finish editing the active cell before changing the worksheet.");return executeWorksheetEdit(this,*s.model,*s.grid,command,arguments,s.package?&*s.package:nullptr);}
 if(command=="financial.forecast"||command=="financial.goal_seek"||command=="financial.solver"||command=="financial.sensitivity"||command=="financial.assumption_set"||command=="financial.scenario"||command=="financial.scenario_comparison"){showFinancialFields(command.mid(10).toStdString());return true;}
 if(command=="financial.open"){showFinancialFields("");return true;}
 if(arguments.empty()&&(command=="analysis.goalSeek"||command=="analysis.solver"||command=="analysis.table"||command=="scenario.manage")){showFinancialFields(command=="analysis.goalSeek"?"goal_seek":command=="analysis.solver"?"solver":command=="analysis.table"?"sensitivity":"scenario");return true;}
 if(command=="financial.example")return startFinancialModel(Json::object(),"example");
 if(command=="financial.save"||command=="financial.run"||command=="financial.apply"||command=="financial.delete"||command=="financial.settings"){return startFinancialModel(arguments.at("definition"),command.mid(10).toStdString(),arguments.value("useCachedLinks",false));}
 if(command=="financial.cancel"){if(s.financialWorkspace)s.financialWorkspace->cancel();s.timer->start();s.error.clear();setProperty("financialBusy",false);if(s.financialPane)s.financialPane->setRunState("cancelled","Financial job stopped; its candidate was discarded");return true;}
 if(command=="open"||command=="template.financial")commitEditor();
 if(command=="pivot.generateGetPivotData"){auto settings=s.session.snapshotView().value("settings",Json::object());const bool enabled=arguments.value("enabled",!settings.value("generateGetPivotData",true));settings["generateGetPivotData"]=enabled;if(!s.model->apply(Json::array({{{"type","set_metadata"},{"key","settings"},{"value",settings}}})))return false;statusBar()->showMessage(enabled?"Formula pointing generates GETPIVOTDATA":"Formula pointing uses cell references",5000);return true;}
 if(command=="links.refresh"){executeCommand("financial.cancel");if(s.financialPane)s.financialPane->resetCachedLinkConsent();executeCommand("pivot.cancel");++s.contextGeneration;++s.calcSerial;if(s.calcCapture)s.calcCapture->stop();s.scheduler->cancel();WorkbookLinkOptions options;options.refresh=true;options.liveWorkbooks=captureLiveWorkbooks(s.liveSource.get());if(arguments.contains("relocatedPaths"))for(const auto& [oldPath,newPath]:arguments["relocatedPaths"].items())options.relocatedPaths[oldPath]=newPath.get<std::string>();auto prepared=prepareWorkbookLinks(s.bookView(),s.package?&*s.package:nullptr,options);s.retainedLinks=prepared.snapshot.at("links");recalculateNow();statusBar()->showMessage(QString("Refreshed %1 local workbook files").arg(prepared.filesRead),10000);return true;}
 if(command=="calculate"){recalculateNow();return true;}
 if(command=="view.ribbon"){s.ribbonCollapsed=!s.ribbonCollapsed;s.presentRibbon(*this);return true;}
 if(command=="view.ribbon.open"){if(s.ribbonCollapsed)s.presentRibbon(*this,true);return true;}
 if(command=="view.ribbon.close"){if(s.ribbonCollapsed)s.presentRibbon(*this);return true;}
 if(command=="recovery.flush"){if(s.recovery)s.recovery->flush();return true;}
 if(command=="recover"){
  if(!s.recovery)throw std::runtime_error("Recovery storage is unavailable");
  s.recovery->flush();const auto records=RecoveryStore(s.recoveryPath).records();if(records.empty())throw std::runtime_error("There are no unsaved workbooks to recover");
  std::string id=arguments.value("id",std::string());if(id.empty()){QStringList choices;for(const auto& record:records)choices<<QString::fromStdString(record.value("sourcePath",std::string()).empty()?record.at("id").get<std::string>():record.at("sourcePath").get<std::string>());bool ok=false;const auto choice=QInputDialog::getItem(this,"Recover workbook","Unsaved workbook",choices,0,false,&ok);if(!ok)return false;id=records[choices.indexOf(choice)].at("id").get<std::string>();}
  auto record=std::find_if(records.begin(),records.end(),[&](const auto& item){return item.at("id")==id;});if(record==records.end())throw std::runtime_error("Unknown recovery record");
  WorkbookSession candidate(record->at("workbook"));std::optional<WorkbookPackage> package;const auto source=record->at("sourceBytes").get<std::string>();
  if(source.starts_with("PK")){QTemporaryFile original;if(!original.open()||original.write(source.data(),static_cast<qint64>(source.size()))!=static_cast<qint64>(source.size())||!original.flush())throw std::runtime_error("Cannot read recovered package");package=readWorkbook(original.fileName().toStdString());package->sourcePath.clear();package->sourceHash.clear();}
  resetWorkbookContext();s.session=std::move(candidate);s.package=std::move(package);s.originalBytes=source;s.path.clear();s.recoveryId=id;if(s.recovery){s.session.takeRecoveryDelta();s.recovery->seed(id,record->at("workbook"),std::make_shared<const std::string>(source),record->value("sourcePath",std::string()));}s.model->setSheetId({});s.grid->setCurrentIndex(s.model->index(0,0));s.rebuildSheets();s.filename->setText("Recovered workbook");s.dirty=true;s.syncSaveState(*this,"Recovered — save a new copy");recalculateNow();return true;
 }

 if(command=="save"||command=="save.as"){
  const bool legacy=s.path.endsWith(".xls",Qt::CaseInsensitive);auto path=command=="save"?s.path:QString();
  if(path.isEmpty()){const auto suggestion=legacy?QFileInfo(s.path).dir().filePath(QFileInfo(s.path).completeBaseName()+".xlsx"):QString("Book1.xlsx");path=QFileDialog::getSaveFileName(this,"Save workbook",suggestion,"Excel workbook (*.xlsx);;Legacy Excel workbook (*.xls);;Macro-enabled workbook (*.xlsm);;CSV (*.csv)");}
  if(path.isEmpty())return false;if(QFileInfo(path).suffix().isEmpty())path+=".xlsx";bool ok=saveWorkbook(path);if(!ok)QMessageBox::warning(this,"Cannot save workbook",s.error);return ok;
 }
 if(command=="open"){auto path=QFileDialog::getOpenFileName(this,"Open workbook",{},"Workbooks (*.xlsx *.xlsm *.xls *.csv)");if(path.isEmpty())return false;if(s.dirty){auto* other=new MainWindow;other->setAttribute(Qt::WA_DeleteOnClose);if(other->openWorkbook(path)){other->show();return true;}other->deleteLater();return false;}return openWorkbook(path);}
 if(command=="close")return close();
 if(command=="new"){auto* other=new MainWindow;other->setAttribute(Qt::WA_DeleteOnClose);other->show();return true;}
 if(command=="template.financial"){if(s.dirty){auto* other=new MainWindow;other->setAttribute(Qt::WA_DeleteOnClose);other->loadFinancialExample();other->show();}else loadFinancialExample();return true;}
 if(command=="undo"||command=="redo"){
  const auto token=command=="undo"?s.session.undoToken():s.session.redoToken();const auto found=s.editPositions.find(token);const auto position=found==s.editPositions.end()?std::optional<std::pair<QString,State::SheetPosition>>():found->second;
  const QScopedValueRollback<bool> restoring(s.restoringHistory,true);const auto ok=command=="undo"?s.session.undo():s.session.redo();
  if(ok){s.previewContext.reset();s.previewBar->hide();s.formula->setReadOnly(false);s.model->refreshAfterChange();if(s.model->onChanged)s.model->onChanged();if(position){bool exists=false;for(const auto& sheet:s.session.snapshotView().at("sheets"))exists|=sheet.at("id")==position->first.toStdString();if(exists){s.switchSheet(position->first);s.restoreSheetPosition(position->second);s.rebuildSheets();}}}
  return ok;
 }
 if(command=="goto"){s.name->setFocus();s.name->selectAll();return true;}
 if(command=="find"||command=="replace"){commitEditor();showWorksheetFindReplace(this,*s.model,*s.grid,command=="replace",[this]{return state_->calculated.value("contextGeneration",Json())==Json(state_->contextGeneration)?state_->calculated:Json::object();});return true;}
 if(command=="codex.toggle"){s.dock->setVisible(!s.dock->isVisible());if(s.dock->isVisible()&&!s.codex->isReady())s.codex->connectRuntime();return true;}
 if(command=="preview.accept"||command=="preview.discard"){cancelCodexTool();++s.calcSerial;s.calcCapture->stop();if(command=="preview.accept"&&s.previewContext&&*s.previewContext!=s.contextGeneration)throw std::runtime_error("Linked inputs changed after this proposal. Discard it and regenerate the proposal.");std::unique_ptr<WorkbookSession::PreparedTransaction> retired;if(command=="preview.accept"){std::string error;if(!s.session.acceptPrepared(retired,&error)&&!(error=="No prepared preview is active"&&s.session.accept(&error)))throw std::runtime_error(error);s.dirty=true;}else{retired=s.session.discardPrepared();s.model->refresh();s.session.discard();}s.workbookTools->reset();s.previewContext.reset();s.previewBar->hide();s.formula->setReadOnly(false);s.model->refresh();s.rebuildSheets();s.refreshFinancialFields();if(s.financialPane)s.financialPane->setRunState("idle",command=="preview.accept"?"Accepted — Undo restores the previous workbook":"Preview discarded; workbook retained");if(command=="preview.accept"&&s.model->onChanged)s.model->onChanged();if(retired){auto* disposal=QThread::create([retired=std::move(retired)]() mutable {retired.reset();});connect(disposal,&QThread::finished,disposal,&QObject::deleteLater);disposal->start();}s.timer->start();return true;}
 if(command=="copy"||command=="cut"){commitEditor();auto r=s.range();auto clip=command=="cut"?s.model->cutRange(r.top(),r.left(),r.bottom(),r.right()):s.model->copyRange(r.top(),r.left(),r.bottom(),r.right());if(clip.is_null())return false;auto presentation=clip.at("cells");if(clip.value("sparse",false)){int rows=0;const int columns=clip.at("columns");for(const auto& entry:clip.at("entries"))rows=std::max(rows,entry.at("row").get<int>()+1);if(static_cast<std::int64_t>(rows)*columns<=1000000&&columns<=16384){for(int row=0;row<rows;++row){Json line=Json::array();for(int col=0;col<columns;++col)line.push_back({{"input",""}});presentation.push_back(std::move(line));}for(const auto& entry:clip.at("entries"))presentation[entry.at("row").get<int>()][entry.at("col").get<int>()]=entry;}}QString text,html="<table>";for(const auto& row:presentation){QStringList cells;html+="<tr>";for(const auto&c:row){auto value=QString::fromStdString(c.value("input",std::string()));QString quoted=value;if(value.contains('\t')||value.contains('\n')||value.contains('"'))quoted='"'+QString(value).replace("\"","\"\"")+'"';cells<<quoted;html+="<td>"+value.toHtmlEscaped()+"</td>";}text+=cells.join('\t')+'\n';html+="</tr>";}auto* mime=new QMimeData;mime->setData("application/x-tigersheets-cells+json",QByteArray::fromStdString(clip.dump()));mime->setText(text);mime->setHtml(html+"</table>");const auto payload=mime->data("application/x-tigersheets-cells+json");{const QScopedValueRollback<bool> publishing(s.publishingClipboard,true);QApplication::clipboard()->setMimeData(mime);}const auto* published=QApplication::clipboard()->mimeData();const bool accepted=published&&published->data("application/x-tigersheets-cells+json")==payload;s.clipboardFallback=accepted?Json():clip;s.copySheet=s.model->sheetId();s.copyRange=r;s.refreshCopyOutline();s.saveState->setText("Select destination and press ENTER or choose Paste");if(!accepted)QMessageBox::warning(this,"TigerSheets","We could not copy the content to the Clipboard, it is in use by another application. You can still paste your content within this workbook but it will not be available in other applications.",QMessageBox::Ok);return true;}
 if(command=="paste"||command=="paste.special"){commitEditor();auto r=s.range();auto* mime=QApplication::clipboard()->mimeData();Json clip;if(!s.clipboardFallback.is_null())clip=s.clipboardFallback;else if(mime->hasFormat("application/x-tigersheets-cells+json"))clip=Json::parse(mime->data("application/x-tigersheets-cells+json").toStdString());else{Json rows=Json::array(),line=Json::array();QString cell;bool quote=false;auto text=mime->text();for(int i=0;i<text.size();++i){auto ch=text[i];if(ch=='"'){if(quote&&i+1<text.size()&&text[i+1]=='"'){cell+='"';++i;}else if(quote||cell.isEmpty())quote=!quote;else cell+=ch;}else if(!quote&&(ch=='\t'||ch=='\n'||ch=='\r')){line.push_back({{"input",cell.toStdString()}});cell.clear();if(ch!='\t'){rows.push_back(line);line=Json::array();if(ch=='\r'&&i+1<text.size()&&text[i+1]=='\n')++i;}}else cell+=ch;}if(quote)throw std::runtime_error("Unfinished quoted clipboard cell");if(!cell.isEmpty()||!line.empty()||rows.empty()){line.push_back({{"input",cell.toStdString()}});rows.push_back(line);}clip={{"sourceRow",r.top()},{"sourceCol",r.left()},{"cells",rows}};}auto mode=arguments.value("mode",std::string("all"));bool skipBlanks=arguments.value("skipBlanks",false);auto operation=arguments.value("operation",std::string("none"));if(command=="paste.special"&&!arguments.contains("mode")){const auto choice=choosePasteSpecial(this);if(!choice)return false;mode=choice->mode;skipBlanks=choice->skipBlanks;operation=choice->operation;if(choice->transpose)mode+=".transpose";}else if(arguments.value("transpose",false)&&mode!="transpose"&&!mode.ends_with(".transpose"))mode+=".transpose";if(operation!="none")throw std::runtime_error("This Paste Special operation is not supported.");int targetRows=r.height(),targetColumns=r.width();int sourceRows=clip.value("sparse",false)?clip.at("rows").get<int>():static_cast<int>(clip.at("cells").size()),sourceColumns=clip.value("sparse",false)?clip.at("columns").get<int>():0;for(const auto& line:clip.at("cells"))sourceColumns=std::max(sourceColumns,static_cast<int>(line.size()));if(mode=="transpose"||mode.ends_with(".transpose"))std::swap(sourceRows,sourceColumns);const bool repeats=sourceRows>0&&sourceColumns>0&&targetRows>=sourceRows&&targetColumns>=sourceColumns&&targetRows%sourceRows==0&&targetColumns%sourceColumns==0&&(clip.value("sparse",false)||static_cast<std::int64_t>(targetRows)*targetColumns<=1000000);if(!clip.value("cut",false)&&(targetRows>1||targetColumns>1)&&!repeats){if(QMessageBox::warning(this,"TigerSheets","The data you’re pasting isn’t the same size as your selection. Do you want to paste anyway?",QMessageBox::Ok|QMessageBox::Cancel,QMessageBox::Ok)!=QMessageBox::Ok)return false;targetRows=targetColumns=1;}const QScopedValueRollback<bool> copying(s.copyPasting,!clip.value("cut",false));if(!s.model->pasteRange(r.top(),r.left(),clip,mode,skipBlanks,targetRows,targetColumns))throw std::runtime_error(s.model->lastError().isEmpty()?"The clipboard cannot be pasted at this destination.":s.model->lastError().toStdString());if(clip.value("cut",false))QApplication::clipboard()->clear();return true;}
 if(command=="clear"){auto r=s.range();return s.model->apply(Json::array({{{"type","clear_range"},{"sheet",s.model->sheetId().toStdString()},{"row1",r.top()+1},{"col1",r.left()+1},{"row2",r.bottom()+1},{"col2",r.right()+1}}}));}
 if(command=="number.percent"){s.format({{"numberFormat","0%"}});return true;}
 if(command.startsWith("style.")){auto key=command.mid(6).toStdString();if(key=="fill"){showWorksheetFillPalette(*this,*s.model,*static_cast<WorksheetView*>(s.grid));}else if(key=="color"){showWorksheetFontPalette(*this,*s.model,*static_cast<WorksheetView*>(s.grid));}else{auto r=s.range();s.format({{key,!s.model->cellStyle(r.top(),r.left()).value(key,false)}});}return true;}
 if(command.startsWith("align.")){s.format({{"alignment",command.mid(6).toStdString()}});return true;}
 if(command=="sheet.add"){auto book=s.session.snapshot();int i=1;std::set<std::string> names;for(const auto&sh:book["sheets"])names.insert(sh["name"].get<std::string>());while(names.contains("Sheet"+std::to_string(i)))++i;auto id=QUuid::createUuid().toString().toStdString();bool ok=s.model->apply(Json::array({{{"type","add_sheet"},{"id",id},{"name","Sheet"+std::to_string(i)}}}));if(ok){s.switchSheet(QString::fromStdString(id));s.rebuildSheets();}return ok;}
 if(command=="sheet.rename"){bool ok=false;auto name=QInputDialog::getText(this,"Rename sheet","Name",QLineEdit::Normal,s.sheets->tabText(s.sheets->currentIndex()),&ok);if(!ok)return false;if(!s.model->apply(Json::array({{{"type","rename_sheet"},{"sheet",s.model->sheetId().toStdString()},{"name",name.toStdString()}}})))throw std::runtime_error(s.model->lastError().toStdString());s.error.clear();return true;}
 if(command=="sheet.delete"){
  const auto book=s.session.snapshot();const auto id=s.model->sheetId().toStdString();const auto& sheets=book.at("sheets");auto selected=std::find_if(sheets.begin(),sheets.end(),[&](const auto& sheet){return sheet.at("id")==id;});if(selected==sheets.end())return false;if(sheets.size()==1)throw std::runtime_error("Cannot delete the last worksheet");
  if(QMessageBox::question(this,"Delete sheet",QString("Delete the worksheet \"%1\" and its contents? Undo can restore it.").arg(QString::fromStdString(selected->at("name").get<std::string>())),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return false;
  const auto next=std::next(selected)==sheets.end()?std::prev(selected):std::next(selected);const auto nextId=QString::fromStdString(next->at("id").get<std::string>());if(!s.model->apply(Json::array({{{"type","delete_sheet"},{"sheet",id}}})))throw std::runtime_error(s.model->lastError().toStdString());s.model->setSheetId(nextId);s.grid->setCurrentIndex(s.model->index(0,0));s.rebuildSheets();s.error.clear();return true;
 }
 if(command=="autosum"){auto r=s.range();if(r.top()==0)return false;int start=r.top()-1;while(start>0&&!s.model->raw(start-1,r.left()).isEmpty())--start;const auto column=QString::fromStdString(columnName(r.left()+1));const auto formula="=SUM("+column+QString::number(start+1)+":"+column+QString::number(r.top())+")";auto* view=static_cast<WorksheetView*>(s.grid);if(!view->commitPendingEditor())return false;const auto target=s.model->index(r.top(),r.left());view->setCurrentIndex(target);view->scrollTo(target);view->edit(target);auto* editor=view->activeCellEditor();if(!editor)return false;editor->setText(formula);editor->setModified(true);editor->setSelection(5,formula.size()-6);editor->setFocus();s.name->setText("SUM");return view->setProposedReferenceRange(s.model->index(start,r.left()),s.model->index(r.top()-1,r.left()));}
 if(command=="function.insert"){if(s.formula->isReadOnly())throw std::runtime_error("Accept or discard the preview before inserting a function");QStringList names;auto catalog=CalculationEngine().functions();for(const auto&f:catalog["functions"])names<<QString::fromStdString(f.get<std::string>());bool ok=false;auto function=QInputDialog::getItem(this,"Insert function","Function",names,0,true,&ok);if(ok){s.formula->setText("="+function+"()");s.formula->setModified(true);s.formula->setFocus();s.formula->setCursorPosition(s.formula->text().size()-1);}return ok;}
 if(command=="name.define"){bool ok=false;auto name=QInputDialog::getText(this,"Define name","Name",QLineEdit::Normal,{},&ok);if(!ok||!QRegularExpression("^[A-Za-z_][A-Za-z0-9_.]*$").match(name).hasMatch())return false;auto r=s.range();auto names=s.session.snapshot().value("names",Json::object());auto sheet=s.sheets->tabText(s.sheets->currentIndex()).replace("'","''");names[name.toStdString()]=("'"+sheet+"'!$"+QString::fromStdString(columnName(r.left()+1))+"$"+QString::number(r.top()+1)+":$"+QString::fromStdString(columnName(r.right()+1))+"$"+QString::number(r.bottom()+1)).toStdString();return s.model->apply(Json::array({{{"type","set_metadata"},{"key","names"},{"value",names}}}));}
 if(command=="note.edit"){bool ok=false;auto r=s.range();auto note=QInputDialog::getMultiLineText(this,"Cell note","Note",QString::fromStdString(s.model->cellStyle(r.top(),r.left()).value("note",std::string())),&ok);if(ok)s.format({{"note",note.toStdString()}});return ok;}
 if(command=="view.gridlines")return s.model->apply(Json::array({{{"type","set_sheet_layout"},{"sheet",s.model->sheetId().toStdString()},{"showGridLines",!s.grid->showGrid()}}}));
 if(command=="view.formulas"){s.showFormulas=!s.showFormulas;s.model->setProperty("showFormulas",s.showFormulas);if(s.calculated.contains("contextGeneration")&&s.calculated.at("contextGeneration")==s.contextGeneration&&s.calculated.value("revision",std::uint64_t{})==s.bookView().at("revision").get<std::uint64_t>())s.model->setCalculated(s.calculated);updateSelectionStatus();s.grid->viewport()->update();return true;}
 if(command=="view.freeze"){
  if(s.frozen){static_cast<QVBoxLayout*>(centralWidget()->layout())->removeWidget(s.frozen);delete s.frozen;s.frozen=nullptr;s.grid->setProperty("worksheet.frozenBody",false);bool hidden=false;for(const auto& sheet:s.bookView().at("sheets"))if(sheet.at("id")==s.model->sheetId().toStdString()){for(const auto& row:sheet.value("rowLayout",Json::array()))if(row.at("index")==1)hidden=row.value("hidden",false);for(const auto& row:sheet.value("hiddenRows",Json::array()))if(row==1)hidden=true;}s.grid->setRowHidden(0,hidden);s.grid->horizontalHeader()->show();s.refreshCopyOutline();return true;}
  s.frozen=new WorksheetView(centralWidget());s.frozen->setObjectName("worksheet.frozen");s.frozen->setModel(s.model);
  static_cast<WorksheetView*>(s.frozen)->editorFactory=[this](QWidget* parent){state_->clearCopyOutline();return new QLineEdit(parent);};
  installWorksheetContextMenus(*s.frozen,*s.model,[this](const QString& action,const Json& args){auto& s=*state_;if(!s.frozen||!static_cast<WorksheetView*>(s.frozen)->commitPendingEditor()||!static_cast<WorksheetView*>(s.grid)->commitPendingEditor())return false;s.grid->selectionModel()->setCurrentIndex(s.frozen->currentIndex(),QItemSelectionModel::NoUpdate);s.grid->selectionModel()->select(s.frozen->selectionModel()->selection(),QItemSelectionModel::ClearAndSelect);return executeCommand(action.startsWith("paste.")&&action!="paste.special"?QString("paste.special"):action,args);});
  s.frozen->setFixedHeight(s.frozen->rowHeight(0)+s.frozen->horizontalHeader()->height()+2);s.frozen->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);s.frozen->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);s.frozen->setSelectionMode(QAbstractItemView::SingleSelection);for(int c=0;c<MaxColumns;++c)s.frozen->setColumnWidth(c,s.grid->columnWidth(c));s.frozen->verticalHeader()->setDefaultSectionSize(s.grid->verticalHeader()->defaultSectionSize());s.frozen->verticalHeader()->setFixedWidth(s.grid->verticalHeader()->width());auto* layout=static_cast<QVBoxLayout*>(centralWidget()->layout());layout->insertWidget(layout->indexOf(s.grid),s.frozen);s.grid->setProperty("worksheet.frozenBody",true);s.grid->horizontalHeader()->hide();s.grid->hideRow(0);
  connect(s.model,&QAbstractItemModel::dataChanged,s.frozen,[this]{if(state_->frozen){state_->grid->hideRow(0);state_->frozen->setFixedHeight(state_->frozen->rowHeight(0)+state_->frozen->horizontalHeader()->height()+2);state_->refreshCopyOutline();}});connect(s.grid->horizontalScrollBar(),&QScrollBar::valueChanged,s.frozen->horizontalScrollBar(),&QScrollBar::setValue);s.refreshCopyOutline();return true;
 }
 if(command=="sort.ascending"||command=="sort.descending"){commitEditor();auto r=s.range();const int first=r.top()+(arguments.value("header",false)?1:0);if(!s.model->sortRange(first,r.left(),r.bottom(),r.right(),command=="sort.ascending"))throw std::runtime_error(s.model->lastError().toStdString());return true;}
 if(command=="chart.edit"){
  if(s.session.hasPreview())throw std::runtime_error("Accept or discard the preview before editing a chart");commitEditor();const auto book=s.session.snapshot();auto charts=book.value("charts",Json::array());const auto id=arguments.value("id",std::string());auto found=std::find_if(charts.begin(),charts.end(),[&](const auto& chart){return chart.value("id",std::string())==id;});if(found==charts.end())throw std::runtime_error("The selected chart is no longer present");
  const auto edited=editChart(this,book,*found,static_cast<ChartEditAction>(arguments.value("section",0)));if(!edited)return false;if(s.session.snapshot().at("revision")!=book.at("revision"))throw std::runtime_error("Workbook changed while the chart editor was open; reopen the chart editor");*found=*edited;return s.model->apply(Json::array({{{"type","set_metadata"},{"key","charts"},{"value",charts}}}));
 }
 if(command=="chart.geometry"){
  if(s.session.hasPreview())throw std::runtime_error("Accept or discard the preview before moving a chart");auto charts=s.session.snapshot().value("charts",Json::array());const auto id=arguments.value("id",std::string());auto found=std::find_if(charts.begin(),charts.end(),[&](const auto& chart){return chart.value("id",std::string())==id;});if(found==charts.end())throw std::runtime_error("The moved chart is no longer present");auto anchor=found->value("anchor",Json::object());const auto old=s.grid->visualRect(s.model->index(anchor.value("row",12)-1,anchor.value("col",4)-1));if(old.x()+anchor.value("offsetX",0)!=arguments.at("beforeX").get<int>()||old.y()+anchor.value("offsetY",0)!=arguments.at("beforeY").get<int>())throw std::runtime_error("Chart placement changed during the gesture; try again");
  const int x=std::max(0,arguments.at("x").get<int>()),y=std::max(0,arguments.at("y").get<int>());const int row=s.grid->rowAt(y),col=s.grid->columnAt(x);if(row<0||col<0)throw std::runtime_error("Keep the chart anchor within the visible worksheet");anchor["row"]=row+1;anchor["col"]=col+1;anchor["offsetX"]=x-s.grid->columnViewportPosition(col);anchor["offsetY"]=y-s.grid->rowViewportPosition(row);anchor["width"]=arguments.at("width");anchor["height"]=arguments.at("height");(*found)["anchor"]=anchor;return s.model->apply(Json::array({{{"type","set_metadata"},{"key","charts"},{"value",charts}}}));
 }
 if(command=="chart.delete"){
  if(s.session.hasPreview())throw std::runtime_error("Accept or discard the preview before deleting a chart");auto charts=s.session.snapshot().value("charts",Json::array());const auto id=arguments.value("id",std::string());const auto count=charts.size();charts.erase(std::remove_if(charts.begin(),charts.end(),[&](const auto& chart){return chart.value("id",std::string())==id;}),charts.end());if(charts.size()==count)return false;return s.model->apply(Json::array({{{"type","set_metadata"},{"key","charts"},{"value",charts}}}));
 }
 if(command=="chart.clear")return s.model->apply(Json::array({{{"type","set_metadata"},{"key","charts"},{"value",Json::array()}}}));
 if(command=="chart.insert"){auto r=s.range();if(r.height()<2||r.width()<2)throw std::runtime_error("Select a table with headers and a data series");QDialog d(this);d.setWindowTitle("Insert chart");QFormLayout layout(&d);QLineEdit title("Forecast");QComboBox type;type.addItems({"line","column","bar","area","scatter","pie","doughnut"});layout.addRow("Title",&title);layout.addRow("Chart type",&type);QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout.addRow(&buttons);connect(&buttons,&QDialogButtonBox::accepted,&d,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&d,&QDialog::reject);if(d.exec()!=QDialog::Accepted)return false;auto charts=s.session.snapshot().value("charts",Json::array());charts.push_back({{"id",QUuid::createUuid().toString().toStdString()},{"sheet",s.model->sheetId().toStdString()},{"type",type.currentText().toStdString()},{"title",title.text().toStdString()},{"range",{{"row1",r.top()+1},{"col1",r.left()+1},{"row2",r.bottom()+1},{"col2",r.right()+1}}},{"anchor",{{"row",r.bottom()+3},{"col",r.left()+1},{"width",560},{"height",300}}}});return s.model->apply(Json::array({{{"type","set_metadata"},{"key","charts"},{"value",charts}}}));}
 if(command=="print.settings"){commitEditor();auto settings=s.session.snapshotView().value("settings",Json::object());auto sheets=settings.value("printSheets",Json::object());const auto sheet=s.model->sheetId().toStdString();Json chosen=arguments.value("settings",sheets.value(sheet,Json::object()));if(!arguments.contains("settings")){QDialog dialog(this);dialog.setWindowTitle("Print settings");dialog.setObjectName("print.settings.dialog");auto* form=new QFormLayout(&dialog);auto* area=new QLineEdit;area->setObjectName("print.area");area->setPlaceholderText("Automatic used area");if(chosen.contains("area")){const auto& a=chosen.at("area");area->setText(QString::fromStdString(columnName(a.at("col1")))+QString::number(a.at("row1").get<int>())+":"+QString::fromStdString(columnName(a.at("col2")))+QString::number(a.at("row2").get<int>()));}auto* repeats=new QLineEdit;repeats->setObjectName("print.repeatRows");repeats->setPlaceholderText("e.g. 1:2");if(chosen.contains("repeatHeaderRows"))repeats->setText(QString::number(chosen["repeatHeaderRows"]["row1"].get<int>())+":"+QString::number(chosen["repeatHeaderRows"]["row2"].get<int>()));auto* fit=new QCheckBox("Fit all columns to one page");fit->setChecked(chosen.value("fitWidth",true));auto* orientation=new QComboBox;orientation->addItems({"portrait","landscape"});orientation->setCurrentText(QString::fromStdString(chosen.value("orientation",std::string("portrait"))));form->addRow("Print area",area);form->addRow("Repeat header rows",repeats);form->addRow(fit);form->addRow("Orientation",orientation);auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form->addRow(buttons);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);if(dialog.exec()!=QDialog::Accepted)return false;chosen={{"fitWidth",fit->isChecked()},{"orientation",orientation->currentText().toStdString()}};if(!area->text().trimmed().isEmpty()){auto range=reportRange(s.model->sheetId(),area->text().trimmed());range.erase("sheet");chosen["area"]=std::move(range);}if(!repeats->text().trimmed().isEmpty()){const auto match=QRegularExpression("^([1-9][0-9]*)(?::([1-9][0-9]*))?$").match(repeats->text().trimmed());if(!match.hasMatch())throw std::runtime_error("Enter repeating rows such as 1:2");chosen["repeatHeaderRows"]={{"row1",match.captured(1).toInt()},{"row2",match.captured(2).isEmpty()?match.captured(1).toInt():match.captured(2).toInt()}};}}validatePrintSettings(chosen);sheets[sheet]=std::move(chosen);settings["printSheets"]=std::move(sheets);return s.model->apply(Json::array({{{"type","set_metadata"},{"key","settings"},{"value",settings}}}));}
 if(command=="print"||command=="export.pdf"){commitEditor();recalculateNow();if(s.calculated.value("status",std::string())!="ok")throw std::runtime_error("Calculate the workbook successfully before printing");QString path;if(command=="export.pdf"){path=QString::fromStdString(arguments.value("path",std::string()));if(path.isEmpty())path=QFileDialog::getSaveFileName(this,"Export PDF","Workbook.pdf","PDF (*.pdf)");if(path.isEmpty())return false;}return printWorksheet(this,*s.model,*s.grid,path,s.session.snapshotView(),s.calculated);}

 if(command.startsWith("vba."))return vbaCommand(this,s.session,*s.model,command);
 if(command.startsWith("pivot."))return reportingCommand(command,arguments);
 if(command.startsWith("analysis.")||command=="scenario.manage"){const auto context=calculationContext(arguments.value("useCachedLinks",false));return modelingCommand(this,s.session,*s.model,*s.grid,command,arguments,&context);}
 if(command=="fill.down"||command=="fill.right"){
  const auto ranges=state_->grid->selectionModel()->selection();if(ranges.isEmpty())return false;
  const auto range=ranges.front();auto clip=state_->model->copyRange(range.top(),range.left(),command=="fill.down"?range.top():range.bottom(),command=="fill.right"?range.left():range.right());
  Json ops=Json::array();
  for(int r=range.top();r<=range.bottom();++r)for(int c=range.left();c<=range.right();++c){int sr=command=="fill.down"?range.top():r,sc=command=="fill.right"?range.left():c;
   auto input=state_->model->raw(sr,sc).toStdString();if(!input.empty()&&input[0]=='=')input=shiftFormula(input,r-sr,c-sc);
   ops.push_back({{"type","set_cell"},{"sheet",state_->model->sheetId().toStdString()},{"row",r+1},{"col",c+1},{"input",input},{"style",state_->model->cellStyle(sr,sc)},{"replaceStyle",true}});
  }
  return state_->model->apply(ops);
 }
 return false;
 }catch(const std::exception&e){s.error=QString::fromUtf8(e.what());statusBar()->showMessage(s.error,10000);return false;}
}
void MainWindow::loadFinancialExample(){
 auto& s=*state_;resetWorkbookContext();s.session=WorkbookSession();s.package.reset();s.originalBytes.clear();s.path.clear();s.filename->setText("Book1.xlsx");if(s.recovery)s.recovery->seed(s.recoveryId,s.session.snapshot(),std::make_shared<const std::string>(),{});s.model->setSheetId({});auto sheet=s.model->sheetId().toStdString();Json ops=Json::array();
 auto put=[&](int row,int col,std::string input,Json style=Json::object()){ops.push_back({{"type","set_cell"},{"sheet",sheet},{"row",row},{"col",col},{"input",input},{"style",style}});};
 put(1,1,"FINANCIAL MODEL",{{"bold",true},{"fontSize",16},{"color","#245648"}});put(2,1,"Operating forecast & valuation",{{"color","#65756d"}});
 put(4,1,"ASSUMPTIONS",{{"bold",true},{"color","#245648"}});
 for(auto item:{std::tuple{5,"Revenue growth","0.12"},std::tuple{6,"Gross margin","0.65"},std::tuple{7,"Expense growth","0.06"},std::tuple{8,"Tax rate","0.25"},std::tuple{9,"Discount rate","0.10"},std::tuple{10,"Terminal growth","0.025"}}){put(std::get<0>(item),1,std::get<1>(item));put(std::get<0>(item),2,std::get<2>(item),{{"color","#286a9e"},{"numberFormat","0.0%"}});}
 for(int c=4;c<=9;++c){auto col=columnName(c),previous=columnName(c-1);put(4,c,std::to_string(2025+c-4)+(c==4?"A":"E"),{{"bold",true},{"fill","#245648"},{"color","#ffffff"},{"alignment","right"}});put(5,c,c==4?"1000000":"="+previous+"5*(1+$B$5)",{{"numberFormat","#,##0"},{"bold",true}});put(6,c,"="+col+"5*$B$6",{{"numberFormat","#,##0"}});put(7,c,c==4?"300000":"="+previous+"7*(1+$B$7)",{{"numberFormat","#,##0"}});put(8,c,"="+col+"6-"+col+"7",{{"numberFormat","#,##0"},{"bold",true},{"fill","#edf4ec"}});put(9,c,"="+col+"8*$B$8",{{"numberFormat","#,##0"}});put(10,c,"="+col+"8-"+col+"9",{{"numberFormat","#,##0"},{"bold",true}});}
 for(auto item:{std::pair{5,"Revenue"},std::pair{6,"Gross profit"},std::pair{7,"Operating expenses"},std::pair{8,"EBITDA"},std::pair{9,"Taxes"},std::pair{10,"Free cash flow"}})put(item.first,3,item.second,{{"bold",item.first==5||item.first==8||item.first==10}});
 put(12,3,"Enterprise value",{{"bold",true},{"color","#245648"}});put(12,4,"=NPV($B$9,E10:I10)+(I10*(1+$B$10)/($B$9-$B$10))/(1+$B$9)^5",{{"numberFormat","$#,##0"},{"fontSize",14},{"bold",true},{"fill","#dfece3"}});
 put(14,1,"MODEL GUIDE",{{"bold",true},{"color","#245648"}});put(15,1,"Blue cells are inputs");put(16,1,"Change assumptions");put(17,1,"to update the forecast.");put(19,1,"CHART DATA",{{"bold",true},{"color","#245648"}});put(20,1,"Year");put(20,2,"Revenue");put(20,3,"EBITDA");
 for(int i=0;i<5;++i){put(21+i,1,std::to_string(2026+i));put(21+i,2,"="+columnName(5+i)+"5",{{"numberFormat","#,##0"}});put(21+i,3,"="+columnName(5+i)+"8",{{"numberFormat","#,##0"}});}
 Json chart={{"id","forecast"},{"sheet",sheet},{"type","column"},{"title","Revenue & EBITDA forecast"},{"range",{{"row1",20},{"col1",1},{"row2",25},{"col2",3}}},{"anchor",{{"row",13},{"col",4},{"width",670},{"height",260}}}};
 ops.push_back({{"type","set_metadata"},{"key","charts"},{"value",Json::array({chart})}});s.model->apply(ops);s.grid->setColumnWidth(0,205);s.grid->setColumnWidth(1,120);s.grid->setColumnWidth(2,165);for(int i=3;i<9;++i)s.grid->setColumnWidth(i,120);s.grid->setCurrentIndex(s.model->index(4,1));recalculateNow();
}
bool MainWindow::eventFilter(QObject* object,QEvent* event){
 auto* widget=qobject_cast<QWidget*>(object);
 if(widget&&event->type()==QEvent::KeyRelease&&static_cast<QKeyEvent*>(event)->key()==Qt::Key_Shift)if(auto* modal=QApplication::activeModalWidget();modal&&(widget==modal||modal->isAncestorOf(widget)))for(auto* owner=modal->parent();owner;owner=owner->parent())if(owner==this){static_cast<WorksheetView*>(state_->grid)->finishSelectionExtension();break;}
 if(!widget||(widget!=this&&!isAncestorOf(widget)))return false;
 if(object==this&&event->type()==QEvent::Resize)state_->syncHeaderLayout(*this);
 if((event->type()==QEvent::MouseButtonPress&&object==state_->grid->viewport())||(event->type()==QEvent::FocusIn&&object==state_->grid))for(auto* chart:state_->charts)chart->setSelected(false);
 if(event->type()==QEvent::KeyPress){auto* key=static_cast<QKeyEvent*>(event);
  if((object==state_->grid||object==state_->frozen||(object==state_->formula&&!state_->formula->isModified()))&&key->key()==Qt::Key_Escape&&key->modifiers()==Qt::NoModifier){const bool copying=state_->copyRange.isValid();state_->clearCopyOutline();const auto* mime=QApplication::clipboard()->mimeData();if(mime&&mime->hasFormat("application/x-tigersheets-cells+json")){const auto clip=Json::parse(mime->data("application/x-tigersheets-cells+json").toStdString(),nullptr,false);if(clip.is_object()&&clip.contains("cut")&&clip["cut"].is_boolean()&&clip["cut"].get<bool>()){QApplication::clipboard()->clear();return true;}}if(copying)return true;}
  if(object==state_->composer&&(key->key()==Qt::Key_Return||key->key()==Qt::Key_Enter)&&!(key->modifiers()&Qt::ShiftModifier)){state_->codexSend->click();return true;}
  if(key->modifiers()==Qt::ControlModifier&&(key->key()==Qt::Key_PageUp||key->key()==Qt::Key_PageDown)){
   auto& s=*state_;auto* view=static_cast<WorksheetView*>(s.grid);if(object==s.grid||object==s.formula||object==view->activeCellEditor()){
    const int next=std::clamp(s.sheets->currentIndex()+(key->key()==Qt::Key_PageUp?-1:1),0,s.sheets->count()-1);if(next==s.sheets->currentIndex())return true;
    const bool reference=view->prepareReferenceSheetChange();if(object!=s.grid&&!reference)return false;s.sheets->setCurrentIndex(next);return true;
   }
  }
  if(object==state_->grid&&(key->modifiers()&Qt::ControlModifier)&&!(key->modifiers()&(Qt::AltModifier|Qt::MetaModifier))){
   auto& s=*state_;auto* navigationView=static_cast<WorksheetView*>(s.grid);const auto current=navigationView->navigationIndex(key->modifiers()&Qt::ShiftModifier);if(!current.isValid())return false;int row=current.row(),col=current.column();bool routed=true;
   if(key->modifiers()==(Qt::ControlModifier|Qt::ShiftModifier)&&(key->key()==Qt::Key_5||key->key()==Qt::Key_Percent)){executeCommand("number.percent");return true;}
   auto book=s.session.snapshot();
   if(key->key()==Qt::Key_Home){row=col=0;}
   else if(key->key()==Qt::Key_End){row=col=0;for(const auto& sheet:book["sheets"])if(sheet["id"]==s.model->sheetId().toStdString())for(const auto& cell:sheet["cells"]){row=std::max(row,cell["row"].get<int>()-1);col=std::max(col,cell["col"].get<int>()-1);}}
   else if(key->key()==Qt::Key_Up||key->key()==Qt::Key_Down||key->key()==Qt::Key_Left||key->key()==Qt::Key_Right){
    const bool vertical=key->key()==Qt::Key_Up||key->key()==Qt::Key_Down;const int direction=key->key()==Qt::Key_Up||key->key()==Qt::Key_Left?-1:1;const int position=vertical?row:col;std::set<int> occupied;
    for(const auto& sheet:book["sheets"])if(sheet["id"]==s.model->sheetId().toStdString())for(const auto& cell:sheet["cells"])if(!cell.value("input",std::string()).empty()&&(vertical?cell["col"].get<int>()-1==col:cell["row"].get<int>()-1==row))occupied.insert((vertical?cell["row"].get<int>():cell["col"].get<int>())-1);
    int next=position;if(occupied.contains(position)&&occupied.contains(position+direction)){while(occupied.contains(next+direction))next+=direction;}else if(direction>0){auto i=occupied.upper_bound(position);next=i==occupied.end()?(vertical?MaxRows:MaxColumns)-1:*i;}else{auto i=occupied.lower_bound(position);next=i==occupied.begin()?0:*std::prev(i);}if(vertical)row=next;else col=next;
   }else routed=false;
   if(routed){const auto destination=s.model->index(row,col);navigationView->navigateTo(destination,key->modifiers()&Qt::ShiftModifier);return true;}
  }

  if(key->key()==Qt::Key_F4&&key->modifiers()==Qt::NoModifier){auto* edit=qobject_cast<QLineEdit*>(object);if(edit&&!edit->isReadOnly()&&edit->isEnabled()&&edit->text().startsWith('=')&&(edit==state_->formula||state_->grid->isAncestorOf(edit))){auto* view=static_cast<WorksheetView*>(state_->grid);if(view->cyclePointedReference(edit))return true;const auto old=edit->text();const int cursor=edit->cursorPosition();if(edit->property("formula.cyclePointedRange").toBool()){const auto revised=QString::fromStdString(cycleReference(old.toStdString(),old.left(cursor).toUtf8().size()));if(revised!=old){edit->selectAll();edit->insert(revised);edit->setModified(true);}edit->setCursorPosition(cursor+revised.size()-old.size());return true;}const int start=edit->hasSelectedText()?edit->selectionStart():cursor;const auto result=cycleReferenceEdit(old.toStdString(),old.left(cursor).toUtf8().size(),old.left(start).toUtf8().size(),edit->selectedText().toUtf8().size());const auto revised=QString::fromStdString(result.formula);if(revised!=old){edit->selectAll();edit->insert(revised);edit->setModified(true);const auto before=QString::fromUtf8(result.formula.data(),result.selectionStart).size();const auto length=QString::fromUtf8(result.formula.data()+result.selectionStart,result.selectionLength).size();edit->setSelection(before,length);}return true;}}
  if(key->key()==Qt::Key_Escape&&state_->session.hasPreview()){executeCommand("preview.discard");return true;}if(key->modifiers()==Qt::AltModifier){int i=-1;switch(key->key()){case Qt::Key_H:i=0;break;case Qt::Key_N:i=1;break;case Qt::Key_P:i=2;break;case Qt::Key_M:i=3;break;case Qt::Key_A:i=4;break;case Qt::Key_R:i=5;break;case Qt::Key_W:i=6;break;case Qt::Key_L:i=7;break;}if(i>=0){state_->ribbon->setCurrentIndex(i);return true;}}}return QMainWindow::eventFilter(object,event);
}
void MainWindow::closeEvent(QCloseEvent* event){try{commitEditor();}catch(const std::exception& error){state_->error=QString::fromUtf8(error.what());statusBar()->showMessage(state_->error);event->ignore();return;}if(!state_->dirty){event->accept();return;}auto result=QMessageBox::question(this,"Save workbook?","Save your changes before closing?",QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel,QMessageBox::Save);if(result==QMessageBox::Cancel){event->ignore();return;}if(result==QMessageBox::Save&&(!executeCommand("save")||state_->dirty)){event->ignore();return;}if(state_->recovery){state_->recovery->erase(state_->recoveryId);try{state_->recovery->flush();}catch(const std::exception& e){statusBar()->showMessage(e.what());}}event->accept();}
}

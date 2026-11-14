#include "tigersheets/worksheet_validation_ui.hpp"
#include "tigersheets/worksheet_validation.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/references.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QScrollBar>
#include <QTableView>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>
namespace tigersheets {
namespace {
Json selectedRectangle(const QTableView& view){const auto selection=view.selectionModel()->selection();const auto active=view.currentIndex();const auto range=selection.isEmpty()?QItemSelectionRange(active,active):selection.front();return {{"row1",range.top()+1},{"col1",range.left()+1},{"row2",range.bottom()+1},{"col2",range.right()+1}};}
const Json* currentSheet(const WorksheetModel& model){for(const auto& sheet:model.workbookView().at("sheets"))if(sheet.at("id")==model.sheetId().toStdString())return &sheet;return nullptr;}
}
struct WorksheetValidationUi::State {
 QWidget* parent;WorksheetModel& model;QTableView& view;QTabBar& sheets;std::function<Json()> calculated;
 QToolButton* arrow;QMenu* menu;QPointer<QDialog> dialog;QPointer<QLineEdit> source;bool picking=false;
};
WorksheetValidationUi::WorksheetValidationUi(QWidget* parent,WorksheetModel& model,QTableView& view,QTabBar& sheets,std::function<Json()> calculated):QObject(parent),state_(std::make_unique<State>(State{parent,model,view,sheets,std::move(calculated),new QToolButton(view.viewport()),new QMenu(&view),{}, {},false})){
 auto& s=*state_;s.arrow->setObjectName("validation.dropdown");s.arrow->setAccessibleName("Open validation list");s.arrow->setText("▾");s.arrow->setFocusPolicy(Qt::NoFocus);s.arrow->hide();s.menu->setObjectName("validation.options");
 connect(s.arrow,&QToolButton::clicked,this,[this]{openOptions();});
 connect(view.selectionModel(),&QItemSelectionModel::currentChanged,this,[this]{state_->menu->hide();refresh();});connect(view.selectionModel(),&QItemSelectionModel::selectionChanged,this,[this]{pickSource();refresh();});
 connect(&model,&QAbstractItemModel::dataChanged,this,[this]{refresh();});connect(&model,&QAbstractItemModel::modelReset,this,[this]{refresh();});
 connect(view.horizontalScrollBar(),&QScrollBar::valueChanged,this,[this]{refresh();});connect(view.verticalScrollBar(),&QScrollBar::valueChanged,this,[this]{refresh();});
 connect(view.horizontalHeader(),&QHeaderView::sectionResized,this,[this]{refresh();});connect(view.verticalHeader(),&QHeaderView::sectionResized,this,[this]{refresh();});
 view.installEventFilter(this);view.viewport()->installEventFilter(this);
}
WorksheetValidationUi::~WorksheetValidationUi(){reset();}
void WorksheetValidationUi::reset(){auto& s=*state_;s.picking=false;s.menu->hide();s.arrow->hide();if(s.dialog){delete s.dialog.data();s.dialog.clear();}s.source.clear();}
void WorksheetValidationUi::refresh(){
 auto& s=*state_;const auto at=s.view.currentIndex();const auto* sheet=currentSheet(s.model);const auto* rule=sheet&&at.isValid()?worksheetListValidationAt(*sheet,at.row()+1,at.column()+1):nullptr;
 if(!rule||!(s.model.flags(at)&Qt::ItemIsEditable)){s.arrow->hide();return;}
 const auto cell=s.view.visualRect(at);if(!cell.intersects(s.view.viewport()->rect())){s.arrow->hide();return;}
 s.arrow->setGeometry(cell.right()-18,cell.top()+1,18,std::max(16,cell.height()-2));s.arrow->show();s.arrow->raise();
}
void WorksheetValidationUi::openOptions(){
 auto& s=*state_;const auto at=s.view.currentIndex();const auto* sheet=currentSheet(s.model);if(!sheet||!at.isValid())return;const auto* rule=worksheetListValidationAt(*sheet,at.row()+1,at.column()+1);if(!rule)return;
 s.menu->clear();try{
  const auto options=worksheetValidationOptions(s.model.workbookView(),s.calculated(),s.model.sheetId().toStdString(),*rule);const auto sheetId=s.model.sheetId();const auto revision=s.model.workbookView().at("revision");
  for(const auto& option:options){auto* action=s.menu->addAction(QString::fromStdString(option.at("label").get<std::string>()));const auto input=QString::fromStdString(option.at("input").get<std::string>());
   connect(action,&QAction::triggered,this,[this,at,sheetId,revision,input]{auto& s=*state_;if(s.view.currentIndex()==at&&s.model.sheetId()==sheetId&&s.model.workbookView().at("revision")==revision&&(s.model.flags(at)&Qt::ItemIsEditable))s.model.apply(Json::array({{{"type","set_cell"},{"sheet",sheetId.toStdString()},{"row",at.row()+1},{"col",at.column()+1},{"input",input.toStdString()}}}));});
  }
 }catch(const std::exception& error){auto* action=s.menu->addAction(QString::fromUtf8(error.what()));action->setEnabled(false);}
 if(s.menu->actions().isEmpty()){auto* action=s.menu->addAction("The source list is empty");action->setEnabled(false);}
 s.menu->popup(s.arrow->mapToGlobal(QPoint(0,s.arrow->height())));
}
void WorksheetValidationUi::pickSource(){
 auto& s=*state_;if(!s.picking||!s.dialog||!s.source)return;const auto* sheet=currentSheet(s.model);if(!sheet)return;const auto range=selectedRectangle(s.view);auto name=QString::fromStdString(sheet->at("name").get<std::string>());name.replace("'","''");
 s.source->setText("='"+name+"'!$"+QString::fromStdString(columnName(range.at("col1").get<int>()))+"$"+QString::number(range.at("row1").get<int>())+":$"+QString::fromStdString(columnName(range.at("col2").get<int>()))+"$"+QString::number(range.at("row2").get<int>()));
}
bool WorksheetValidationUi::edit(const Json& arguments){
 auto& s=*state_;if(!(s.model.flags(s.view.currentIndex())&Qt::ItemIsEditable))throw std::runtime_error("Accept or discard the preview before editing validation.");
 const auto sheetId=s.model.sheetId().toStdString();const auto range=selectedRectangle(s.view);
 if(arguments.contains("source")){const auto rule=makeWorksheetListValidation(s.model.workbookView(),sheetId,range,arguments.at("source").get<std::string>(),arguments.value("allowBlank",true));return s.model.apply(Json::array({{{"type","set_data_validation"},{"sheet",sheetId},{"validation",rule}}}));}
 if(s.dialog){s.dialog->show();s.dialog->raise();return true;}
 const auto revision=s.model.workbookView().at("revision");auto* dialog=new QDialog(s.parent);s.dialog=dialog;dialog->setObjectName("validation.dialog");dialog->setWindowTitle("Data Validation");dialog->setModal(false);
 auto* layout=new QVBoxLayout(dialog);auto* form=new QFormLayout;layout->addLayout(form);auto* allow=new QComboBox(dialog);allow->setObjectName("validation.allow");allow->addItems({"Any value","List"});form->addRow("Allow",allow);
 auto* source=new QLineEdit(dialog);s.source=source;source->setObjectName("validation.source");source->setAccessibleName("Validation source");source->setPlaceholderText("=Input!$B$3:$B$7");source->setMinimumWidth(270);source->installEventFilter(this);form->addRow("Source",source);
 auto* pick=new QPushButton("Select source range",dialog);pick->setObjectName("validation.pickSource");form->addRow(pick);auto* blank=new QCheckBox("Ignore blank",dialog);blank->setObjectName("validation.allowBlank");blank->setChecked(true);form->addRow(blank);
 auto* message=new QLabel(dialog);message->setObjectName("validation.error");message->setWordWrap(true);layout->addWidget(message);auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,dialog);layout->addWidget(buttons);
 const auto* sheet=currentSheet(s.model);const auto* existing=sheet?worksheetListValidationAt(*sheet,range.at("row1").get<int>(),range.at("col1").get<int>()):nullptr;
 if(existing){allow->setCurrentIndex(1);source->setText(QString::fromStdString(existing->at("formula1").get<std::string>()));blank->setChecked(existing->value("allowBlank",true));}
 auto update=[=]{const bool list=allow->currentText()=="List";source->setEnabled(list);pick->setEnabled(list);blank->setEnabled(list);};connect(allow,&QComboBox::currentTextChanged,dialog,[=]{update();});update();
 connect(pick,&QPushButton::clicked,dialog,[this,source]{state_->picking=true;source->setFocus();source->selectAll();});connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);
 connect(buttons,&QDialogButtonBox::accepted,dialog,[this,dialog,allow,source,blank,message,sheetId,range,revision]{
  auto& s=*state_;try{if(s.model.workbookView().at("revision")!=revision)throw std::runtime_error("The workbook changed while validation was open. Reopen Data Validation.");
   Json rule=nullptr;if(allow->currentText()=="List")rule=makeWorksheetListValidation(s.model.workbookView(),sheetId,range,source->text().toStdString(),blank->isChecked());
   if(!s.model.apply(Json::array({{{"type","set_data_validation"},{"sheet",sheetId},{"range",range},{"validation",rule}}})))throw std::runtime_error(s.model.lastError().toStdString());
   s.picking=false;for(int tab=0;tab<s.sheets.count();++tab)if(s.sheets.tabData(tab).toString().toStdString()==sheetId){s.sheets.setCurrentIndex(tab);break;}
   const auto target=s.model.index(range.at("row1").get<int>()-1,range.at("col1").get<int>()-1);s.view.setCurrentIndex(target);s.view.selectionModel()->select(QItemSelection(target,s.model.index(range.at("row2").get<int>()-1,range.at("col2").get<int>()-1)),QItemSelectionModel::ClearAndSelect);dialog->accept();s.view.setFocus();refresh();
  }catch(const std::exception& error){message->setText(QString::fromUtf8(error.what()));}
 });
 connect(dialog,&QDialog::finished,this,[this,dialog]{state_->picking=false;state_->source.clear();state_->dialog.clear();dialog->deleteLater();});dialog->show();return true;
}
bool WorksheetValidationUi::eventFilter(QObject* object,QEvent* event){
 auto& s=*state_;if(object==s.source&&event->type()==QEvent::FocusIn)s.picking=true;
 if((object==&s.view||object==s.view.viewport())&&event->type()==QEvent::KeyPress){auto* key=static_cast<QKeyEvent*>(event);if(key->key()==Qt::Key_Down&&key->modifiers()==Qt::AltModifier&&s.arrow->isVisible()){openOptions();return true;}}
 if(object==s.view.viewport()&&event->type()==QEvent::Resize)refresh();return QObject::eventFilter(object,event);
}
}

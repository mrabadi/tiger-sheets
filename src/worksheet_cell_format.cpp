#include "tigersheets/worksheet_cell_format.hpp"
#include "tigersheets/grid.hpp"
#include <QTableView>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
namespace tigersheets {
namespace {
Json normalizedStyle(const Json& requested){
 if(!requested.is_object())throw std::invalid_argument("Cell formatting must be an object");auto style=requested;const std::set<std::string> allowed{"numberFormat","alignment","fontFamily","fontSize","bold","italic","underline","fill","color"};
 for(const auto& [key,value]:requested.items()){
  if(!allowed.contains(key))throw std::invalid_argument("Unsupported cell format property: "+key);
  if(key=="bold"||key=="italic"||key=="underline"){if(!value.is_boolean())throw std::invalid_argument("Cell format "+key+" must be enabled or disabled");}
  else if(key=="fontSize"){if(!value.is_number()||!std::isfinite(value.get<double>())||value.get<double>()<1||value.get<double>()>400)throw std::invalid_argument("Font size must be between 1 and 400");}
  else{
   if(!value.is_string())throw std::invalid_argument("Cell format "+key+" must be text");const auto& text=value.get_ref<const std::string&>();if(text.size()>4096)throw std::invalid_argument("Cell format "+key+" is too long");
   if(key=="fontFamily"&&text.empty())throw std::invalid_argument("Choose a font family");
   if(key=="alignment"&&text!=""&&text!="general"&&text!="left"&&text!="center"&&text!="right")throw std::invalid_argument("Choose General, Left, Center or Right alignment");
   if(key=="fill"||key=="color"){const auto colorText=QString::fromStdString(text).trimmed();if(colorText.isEmpty())style[key]="";else{const QColor color(colorText);if(!color.isValid()||color.alpha()!=255)throw std::invalid_argument("Choose a valid opaque "+key+" color, for example #245648");style[key]=color.name().toStdString();}}
  }
 }
 return style;
}
QItemSelectionRange selectedRange(WorksheetModel& model,QTableView& view){
 if(view.model()!=&model)throw std::invalid_argument("The selection belongs to another worksheet");
 const auto ranges=view.selectionModel()->selection();if(ranges.size()>1)throw std::invalid_argument("Select one rectangular range to format cells");
 const auto current=view.currentIndex().isValid()?view.currentIndex():model.index(0,0);const auto range=ranges.empty()?QItemSelectionRange(current):ranges.front();
 if(!range.isValid()||static_cast<std::uint64_t>(range.height())*range.width()>100000)throw std::invalid_argument("Select a rectangular range of at most 100,000 cells to format");return range;
}
bool applyStyle(WorksheetModel& model,const QItemSelectionRange& range,const Json& style){
 const auto validated=normalizedStyle(style);if(validated.empty())return true;Json operations=Json::array();for(int row=range.top();row<=range.bottom();++row)for(int col=range.left();col<=range.right();++col)operations.push_back({{"type","set_style"},{"sheet",model.sheetId().toStdString()},{"row",row+1},{"col",col+1},{"style",validated}});return model.apply(operations);
}
}
bool formatWorksheetCells(QWidget* parent,WorksheetModel& model,QTableView& view,const Json& arguments){
 const auto range=selectedRange(model,view);if(arguments.contains("style"))return applyStyle(model,range,arguments.at("style"));
 const QModelIndex current=view.currentIndex().isValid()?view.currentIndex():QModelIndex(range.topLeft());const auto initial=model.cellStyle(current.row(),current.column());
 QDialog dialog(parent);dialog.setObjectName("formatCells.dialog");dialog.setAccessibleName("Format Cells");dialog.setWindowTitle("Format Cells");dialog.resize(420,330);auto* layout=new QVBoxLayout(&dialog);layout->setContentsMargins(10,10,10,10);layout->setSpacing(6);auto* tabs=new QTabWidget(&dialog);tabs->setObjectName("formatCells.tabs");tabs->setAccessibleName("Cell formatting categories");layout->addWidget(tabs);
 auto page=[&](const char* title){auto* widget=new QWidget(tabs);auto* form=new QFormLayout(widget);form->setContentsMargins(10,10,10,10);form->setSpacing(7);tabs->addTab(widget,QString::fromLatin1(title));return form;};
 auto identify=[](QWidget* widget,const char* key,const char* label){widget->setObjectName(QString("formatCells.")+QString::fromLatin1(key));widget->setAccessibleName(QString::fromLatin1(label));};
 auto* numberPage=page("Number");auto* number=new QComboBox(&dialog);identify(number,"numberFormat","Number format");number->setEditable(true);number->addItems({"General","0","0.00","#,##0","#,##0.00","$#,##0.00","$#,##0.00;($#,##0.00);-","0%","0.00%","0.00E+00","m/d/yyyy","yyyy-mm-dd","h:mm","@"});number->setEditText(QString::fromStdString(initial.value("numberFormat",std::string("General"))));numberPage->addRow("&Format code",number);auto* numberHelp=new QLabel("Choose a format or enter a custom Excel format code.",&dialog);numberHelp->setWordWrap(true);numberPage->addRow(numberHelp);
 auto* alignmentPage=page("Alignment");auto* alignment=new QComboBox(&dialog);identify(alignment,"alignment","Horizontal alignment");for(const auto& [label,value]:{std::pair{"General",""},std::pair{"Left","left"},std::pair{"Center","center"},std::pair{"Right","right"}})alignment->addItem(QString::fromLatin1(label),QString::fromLatin1(value));alignment->setCurrentIndex(std::max(0,alignment->findData(QString::fromStdString(initial.value("alignment",std::string())))));alignmentPage->addRow("&Horizontal",alignment);
 auto* fontPage=page("Font");auto* family=new QFontComboBox(&dialog);identify(family,"fontFamily","Font family");family->setCurrentFont(QFont(QString::fromStdString(initial.value("fontFamily",std::string("Calibri")))));fontPage->addRow("&Font",family);auto* size=new QSpinBox(&dialog);identify(size,"fontSize","Font size");size->setRange(1,400);size->setValue(static_cast<int>(initial.value("fontSize",11.0)));fontPage->addRow("&Size",size);
 auto flag=[&](const char* key,const char* title){auto* check=new QCheckBox(QString::fromLatin1(title),&dialog);identify(check,key,title);check->setChecked(initial.value(key,false));fontPage->addRow(check);return check;};auto* bold=flag("bold","Bold");auto* italic=flag("italic","Italic");auto* underline=flag("underline","Underline");
 auto colorField=[&](QFormLayout* form,const char* key,const char* label,const char* chooseLabel){auto* field=new QLineEdit(QString::fromStdString(initial.value(key,std::string())),&dialog);identify(field,key,label);field->setPlaceholderText(std::string(key)=="fill"?"No fill":"Automatic");auto* row=new QHBoxLayout;row->setSpacing(5);row->addWidget(field);auto* choose=new QPushButton("Choose…",&dialog);identify(choose,(std::string(key)+".choose").c_str(),chooseLabel);row->addWidget(choose);form->addRow(QString::fromLatin1(label),row);QObject::connect(choose,&QPushButton::clicked,&dialog,[&,field,label]{const auto color=QColorDialog::getColor(QColor(field->text()),&dialog,QString::fromLatin1(label));if(color.isValid())field->setText(color.name());});return field;};auto* color=colorField(fontPage,"color","Font color","Choose font color");auto* fillPage=page("Fill");auto* fill=colorField(fillPage,"fill","Fill color","Choose fill color");
 const Json original={{"numberFormat",number->currentText().toStdString()},{"alignment",alignment->currentData().toString().toStdString()},{"fontFamily",family->currentFont().family().toStdString()},{"fontSize",size->value()},{"bold",bold->isChecked()},{"italic",italic->isChecked()},{"underline",underline->isChecked()},{"color",color->text().toStdString()},{"fill",fill->text().toStdString()}};
 auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);identify(buttons,"buttons","Format Cells actions");layout->addWidget(buttons);QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);if(dialog.exec()!=QDialog::Accepted)return false;
 const Json configured={{"numberFormat",number->currentText().toStdString()},{"alignment",alignment->currentData().toString().toStdString()},{"fontFamily",family->currentFont().family().toStdString()},{"fontSize",size->value()},{"bold",bold->isChecked()},{"italic",italic->isChecked()},{"underline",underline->isChecked()},{"color",color->text().toStdString()},{"fill",fill->text().toStdString()}};Json changes=Json::object();for(const auto& [key,value]:configured.items())if(value!=original.at(key))changes[key]=value;return applyStyle(model,range,changes);
}
bool clearWorksheetFormats(WorksheetModel& model,QTableView& view){
 const auto range=selectedRange(model,view);Json operations=Json::array();for(int row=range.top();row<=range.bottom();++row)for(int col=range.left();col<=range.right();++col){const auto prior=model.cellStyle(row,col);Json style=Json::object();if(prior.contains("note"))style["note"]=prior.at("note");if(prior!=style)operations.push_back({{"type","set_style"},{"sheet",model.sheetId().toStdString()},{"row",row+1},{"col",col+1},{"style",std::move(style)},{"replace",true}});}return model.apply(operations);
}
}

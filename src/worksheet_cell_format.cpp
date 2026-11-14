#include "tigersheets/worksheet_cell_format.hpp"
#include "tigersheets/grid.hpp"
#include "tigersheets/formatting.hpp"
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
#include <QRegularExpression>
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
struct NumberOptions {
 QString category="custom";
 int decimals=2;
 bool grouping=false;
 bool parentheses=false;
 QString symbol="$";
};
NumberOptions numberOptions(const QString& code){
 NumberOptions options;
 if(code.isEmpty()||code.compare("General",Qt::CaseInsensitive)==0){options.category="general";return options;}
 if(code=="@"){options.category="text";return options;}
 if(dateNumberFormat(code.toStdString())){options.category="date";return options;}
 static const QRegularExpression percent("^0(?:\\.(0{1,30}))?%$");const auto percentage=percent.match(code);if(percentage.hasMatch()){options.category="percentage";options.decimals=percentage.captured(1).size();return options;}
 const auto sections=code.split(';');
 static const QRegularExpression numeric("^(\\$|€|£|¥|\"\\$\"|\"€\"|\"£\"|\"¥\")?(0|#,##0)(?:\\.(0{1,30}))?$");
 const auto match=numeric.match(sections.front());
 if(!match.hasMatch()||sections.size()>3||(sections.size()>1&&sections[1]!="("+sections[0]+")")||(sections.size()==3&&sections[2]!="-"))return options;
 options.category=sections.size()==3?"accounting":match.captured(1).isEmpty()?"number":"currency";options.decimals=match.captured(3).size();options.grouping=match.captured(2).startsWith('#');options.parentheses=sections.size()>1;if(!match.captured(1).isEmpty()){options.symbol=match.captured(1);options.symbol.remove('"');}else if(options.category=="accounting")options.symbol="";return options;
}
}
bool formatWorksheetCells(QWidget* parent,WorksheetModel& model,QTableView& view,const Json& arguments){
 const auto range=selectedRange(model,view);if(arguments.contains("style"))return applyStyle(model,range,arguments.at("style"));
 const QModelIndex current=view.currentIndex().isValid()?view.currentIndex():QModelIndex(range.topLeft());const auto initial=model.cellStyle(current.row(),current.column());
 QDialog dialog(parent);dialog.setObjectName("formatCells.dialog");dialog.setAccessibleName("Format Cells");dialog.setWindowTitle("Format Cells");dialog.resize(500,430);auto* layout=new QVBoxLayout(&dialog);layout->setContentsMargins(10,10,10,10);layout->setSpacing(6);auto* tabs=new QTabWidget(&dialog);tabs->setObjectName("formatCells.tabs");tabs->setAccessibleName("Cell formatting categories");layout->addWidget(tabs);
 auto page=[&](const char* title){auto* widget=new QWidget(tabs);auto* form=new QFormLayout(widget);form->setContentsMargins(10,10,10,10);form->setSpacing(7);tabs->addTab(widget,QString::fromLatin1(title));return form;};
 auto identify=[](QWidget* widget,const char* key,const char* label){widget->setObjectName(QString("formatCells.")+QString::fromLatin1(key));widget->setAccessibleName(QString::fromLatin1(label));};
 auto* numberPage=page("Number");
 auto* category=new QComboBox(&dialog);identify(category,"category","Number format category");for(const auto& [label,value]:{std::pair{"General","general"},std::pair{"Number","number"},std::pair{"Currency","currency"},std::pair{"Accounting","accounting"},std::pair{"Percentage","percentage"},std::pair{"Date","date"},std::pair{"Text","text"},std::pair{"Custom","custom"}})category->addItem(QString::fromLatin1(label),QString::fromLatin1(value));numberPage->addRow("&Category",category);
 auto* decimals=new QSpinBox(&dialog);identify(decimals,"decimalPlaces","Decimal places");decimals->setRange(0,30);numberPage->addRow("&Decimal places",decimals);
 auto* grouping=new QCheckBox("Use 1000 separator (,)",&dialog);identify(grouping,"thousandsSeparator","Use 1000 separator");numberPage->addRow(grouping);
 auto* symbol=new QComboBox(&dialog);identify(symbol,"currencySymbol","Currency symbol");symbol->addItem("None","");for(const auto* value:{"$","€","£","¥"})symbol->addItem(QString::fromUtf8(value),QString::fromUtf8(value));numberPage->addRow("&Symbol",symbol);
 auto* negatives=new QComboBox(&dialog);identify(negatives,"negativeStyle","Negative numbers");negatives->addItem("Minus sign","minus");negatives->addItem("Parentheses","parentheses");numberPage->addRow("&Negative numbers",negatives);
 auto* date=new QComboBox(&dialog);identify(date,"dateFormat","Date type");for(const auto* code:{"m/d/yyyy","mm/dd/yyyy","d/m/yyyy","dd/mm/yyyy","m/d/yy","d/m/yy","yyyy-mm-dd","d-mmm-yy","mmmm d, yyyy","dddd, mmmm d, yyyy"})date->addItem(QString::fromLatin1(code),QString::fromLatin1(code));numberPage->addRow("&Type",date);
 auto* number=new QComboBox(&dialog);identify(number,"numberFormat","Number format");number->setEditable(true);number->addItems({"General","0","0.00","#,##0","#,##0.00","$#,##0.00","$#,##0.00;($#,##0.00);-","0%","0.00%","0.00E+00","m/d/yyyy","yyyy-mm-dd","h:mm","@"});number->setEditText(QString::fromStdString(initial.value("numberFormat",std::string("General"))));numberPage->addRow("&Format code",number);
 auto* sample=new QLabel(&dialog);identify(sample,"sample","Sample");sample->setTextFormat(Qt::PlainText);sample->setTextInteractionFlags(Qt::TextSelectableByMouse);numberPage->addRow("Sample",sample);
 auto* numberHelp=new QLabel("Choose display options or enter a custom Excel format code. The underlying value is preserved.",&dialog);numberHelp->setWordWrap(true);numberPage->addRow(numberHelp);
 bool updatingNumber=false;
 auto updateSample=[&]{const auto numeric=model.numericValue(current.row(),current.column());if(!numeric){sample->setText(model.data(current).toString());return;}const auto settings=model.workbookView().value("settings",Json::object());const bool date1904=settings.value("date1904",false);double value=*numeric;const auto code=number->currentText().toStdString();if(!date1904&&value>0&&value<60&&dateNumberFormat(code))value+=1;sample->setText(QString::fromStdString(formattedValue(value,{{"numberFormat",code}},date1904)));};
 auto showNumberOptions=[&]{const auto kind=category->currentData().toString();const bool currency=kind=="currency"||kind=="accounting";auto show=[&](QWidget* field,bool visible){field->setVisible(visible);if(auto* label=numberPage->labelForField(field))label->setVisible(visible);};show(decimals,kind=="number"||currency||kind=="percentage");show(grouping,kind=="number");show(symbol,currency);show(negatives,kind=="number"||currency);show(date,kind=="date");negatives->setEnabled(kind!="accounting");};
 auto syncFromCode=[&]{if(updatingNumber)return;updatingNumber=true;const auto options=numberOptions(number->currentText());category->setCurrentIndex(category->findData(options.category));decimals->setValue(options.decimals);grouping->setChecked(options.grouping);symbol->setCurrentIndex(symbol->findData(options.symbol));negatives->setCurrentIndex(options.parentheses?1:0);if(options.category=="date"){int index=date->findData(number->currentText());if(index<0){date->addItem(number->currentText(),number->currentText());index=date->count()-1;}date->setCurrentIndex(index);}showNumberOptions();updateSample();updatingNumber=false;};
 auto syncFromOptions=[&]{if(updatingNumber)return;updatingNumber=true;const auto kind=category->currentData().toString();const bool currency=kind=="currency"||kind=="accounting";if(kind=="general")number->setEditText("General");else if(kind=="text")number->setEditText("@");else if(kind=="date")number->setEditText(date->currentData().toString());else if(kind=="percentage"){QString code="0";if(decimals->value())code+="."+QString(decimals->value(),'0');number->setEditText(code+"%");}else if(kind=="number"||currency){if(kind=="accounting")negatives->setCurrentIndex(negatives->findData("parentheses"));QString code=currency?symbol->currentData().toString():QString();code+=currency||grouping->isChecked()?"#,##0":"0";if(decimals->value())code+="."+QString(decimals->value(),'0');if(negatives->currentData()=="parentheses")code+=";("+code+")";if(kind=="accounting")code+=";-";number->setEditText(code);}showNumberOptions();updateSample();updatingNumber=false;};
 syncFromCode();
 if(arguments.contains("category")&&arguments.at("category").is_string()){const auto requested=QString::fromStdString(arguments.at("category").get<std::string>()).toLower();const int index=category->findData(requested);if(index>=0){category->setCurrentIndex(index);showNumberOptions();}}
 QObject::connect(number,&QComboBox::currentTextChanged,&dialog,syncFromCode);QObject::connect(category,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,syncFromOptions);QObject::connect(decimals,qOverload<int>(&QSpinBox::valueChanged),&dialog,syncFromOptions);QObject::connect(grouping,&QCheckBox::toggled,&dialog,syncFromOptions);QObject::connect(symbol,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,syncFromOptions);QObject::connect(negatives,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,syncFromOptions);QObject::connect(date,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,syncFromOptions);
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

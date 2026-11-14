#include "tigersheets/modeling_ui.hpp"
#include "tigersheets/analysis.hpp"
#include "tigersheets/calculation.hpp"
#include "tigersheets/references.hpp"
#include "tigersheets/solver.hpp"
#include "tigersheets/vba.hpp"
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QUuid>
#include <map>
#include <set>
#include <tuple>
#include <QLineEdit>
#include <QMessageBox>
#include <QRegularExpression>
#include <QTableView>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace tigersheets {
namespace {
struct Address {std::string sheet;int row{},col{};};
QString label(const Address& address){return QString::fromStdString(columnName(address.col))+QString::number(address.row);}
Address parseAddress(const QString& raw,const Json& book,const std::string& active){
 static const QRegularExpression pattern(QStringLiteral("^(?:(?:'((?:[^']|'')+)'|([^'!]+))!)?\\$?([A-Za-z]{1,3})\\$?([1-9][0-9]*)$"));
 const auto match=pattern.match(raw.trimmed());if(!match.hasMatch())throw std::runtime_error("Enter a valid cell reference such as A1 or 'Model'!B2.");
 auto sheet=active;auto requested=match.captured(1).isEmpty()?match.captured(2):match.captured(1).replace("''","'");
 if(!requested.isEmpty()){bool found=false;for(const auto& entry:book.at("sheets"))if(QString::fromStdString(entry.at("name").get<std::string>()).compare(requested,Qt::CaseInsensitive)==0||entry.at("id")==requested.toStdString()){sheet=entry.at("id").get<std::string>();found=true;break;}if(!found)throw std::runtime_error("The referenced worksheet does not exist.");}
 bool okay=false;const auto row=match.captured(4).toInt(&okay);const auto col=columnNumber(match.captured(3).toStdString());if(!okay||row<1||row>MaxRows||col<1||col>MaxColumns)throw std::runtime_error("The cell reference is outside the worksheet.");
 return {sheet,row,col};
}
Json& inputCell(Json& book,const Address& address){for(auto& sheet:book.at("sheets"))if(sheet.at("id")==address.sheet){for(auto& cell:sheet.at("cells"))if(cell.at("row")==address.row&&cell.at("col")==address.col)return cell;sheet["cells"].push_back({{"row",address.row},{"col",address.col},{"input",""}});return sheet["cells"].back();}throw std::runtime_error("The worksheet is unavailable.");}
Json calculation(CalculationEngine& engine,const Json& book){
 const auto vba=book.value("vba",Json::object());if(!vba.is_object()||!vba.value("enabled",false))return engine.calculate(book);
 std::string source;for(const auto& module:vba.value("modules",Json::array()))if(module.is_object())source+=module.value("source",std::string())+"\n";
 if(source.empty())return engine.calculate(book);
 return engine.calculate(book,[&](const std::string& name,const Json& args){return evaluateVbaFunction(book,source,name,args);});
}
double numberAt(const Json& calculated,const Address& address){for(const auto& cell:calculated.at("values"))if(cell.at("sheet")==address.sheet&&cell.at("row")==address.row&&cell.at("col")==address.col){if(cell.at("type")!="number"||!cell.at("value").is_number())throw std::runtime_error("The output cell must calculate a number without a formula error.");const auto number=cell.at("value").get<double>();if(!std::isfinite(number))throw std::runtime_error("The model returned a non-finite number.");return number;}throw std::runtime_error("The output cell was not calculated.");}
Json setInput(const Address& cell,const std::string& input){return {{"type","set_cell"},{"sheet",cell.sheet},{"row",cell.row},{"col",cell.col},{"input",input}};}
bool commit(WorkbookSession& session,WorksheetModel& model,const Json& before,const Json& operations,const CalculationContext* context){if(context&&!context->current())throw std::runtime_error("The calculation context changed while analysis was running. Run it again.");if(session.snapshot().at("revision")!=before.at("revision"))throw std::runtime_error("The workbook changed while the analysis was running. Run it again.");if(!model.apply(operations))throw std::runtime_error("The result could not be applied to the workbook.");return true;}
bool confirm(QWidget* parent,const QString& title,const QString& description,bool interactive,const Json& args){if(!interactive)return args.value("accept",true);QMessageBox box(QMessageBox::Question,title,description,QMessageBox::Yes|QMessageBox::No,parent);box.setTextFormat(Qt::PlainText);box.setDefaultButton(QMessageBox::Yes);return box.exec()==QMessageBox::Yes;}
QLineEdit* line(QFormLayout* form,const QString& title,const QString& value,const char* name){auto* field=new QLineEdit(value);field->setObjectName(QString::fromLatin1(name));form->addRow(title,field);return field;}
QDoubleSpinBox* decimal(QFormLayout* form,const QString& title,double value,const char* name){auto* field=new QDoubleSpinBox;field->setObjectName(QString::fromLatin1(name));field->setDecimals(10);field->setRange(-1e100,1e100);field->setValue(value);form->addRow(title,field);return field;}
void buttons(QDialog& dialog,QVBoxLayout* layout){auto* box=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);QObject::connect(box,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);QObject::connect(box,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);layout->addWidget(box);}
QString currentCell(QTableView& view){const auto index=view.currentIndex();return QString::fromStdString(columnName(index.isValid()?index.column()+1:1))+QString::number(index.isValid()?index.row()+1:1);}
QString selectedRange(QTableView& view){const auto selection=view.selectionModel()?view.selectionModel()->selection():QItemSelection();if(selection.isEmpty())return currentCell(view);const auto range=selection.front();const auto first=QString::fromStdString(columnName(range.left()+1))+QString::number(range.top()+1),last=QString::fromStdString(columnName(range.right()+1))+QString::number(range.bottom()+1);return first==last?first:first+":"+last;}
std::vector<Address> rangeCells(const QString& text,const Json& book,const std::string& active,std::size_t maximum){
 QStringList ranges;QString part;bool quoted=false;
 for(int i=0;i<text.size();++i){const auto c=text[i];if(c=='\''){if(quoted&&i+1<text.size()&&text[i+1]=='\''){part+=c;part+=text[++i];continue;}quoted=!quoted;}if(c==','&&!quoted){ranges.push_back(part);part.clear();}else part+=c;}ranges.push_back(part);
 std::vector<Address> cells;
 for(const auto& range:ranges){const auto endpoints=range.trimmed().split(':');if(endpoints.isEmpty()||endpoints.size()>2)throw std::runtime_error("Enter cell references or rectangular ranges.");const auto first=parseAddress(endpoints[0],book,active),last=endpoints.size()==2?parseAddress(endpoints[1],book,first.sheet):first;if(first.sheet!=last.sheet||last.row<first.row||last.col<first.col)throw std::runtime_error("The cell range is reversed or crosses worksheets.");const auto size=static_cast<std::size_t>(last.row-first.row+1)*static_cast<std::size_t>(last.col-first.col+1);if(cells.size()+size>maximum)throw std::runtime_error("Too many cells are selected for this analysis.");for(int row=first.row;row<=last.row;++row)for(int col=first.col;col<=last.col;++col)cells.push_back({first.sheet,row,col});}
 return cells;
}
QString reference(const Address& address,const Json& book,const std::string& active){if(address.sheet==active)return label(address);for(const auto& sheet:book.at("sheets"))if(sheet.at("id")==address.sheet)return "'"+QString::fromStdString(sheet.at("name").get<std::string>()).replace("'","''")+"'!"+label(address);throw std::runtime_error("The referenced worksheet is unavailable.");}
Json coordinate(const Address& address){return {{"sheet",address.sheet},{"row",address.row},{"col",address.col}};}
QComboBox* choices(QFormLayout* form,const QString& title,const char* name,const std::vector<std::pair<QString,QString>>& items){auto* field=new QComboBox;field->setObjectName(name);for(const auto& [label,value]:items)field->addItem(label,value);form->addRow(title,field);return field;}
Json solverDialog(QWidget* parent,const Json& book,const std::string& active,QTableView& view){
 QDialog dialog(parent);dialog.setObjectName("modeling.solver");dialog.setWindowTitle("Solver");dialog.resize(740,680);auto* layout=new QVBoxLayout(&dialog);auto* form=new QFormLayout;layout->addLayout(form);
 auto* objective=line(form,"Objective cell",currentCell(view),"solver.objective");auto* goal=choices(form,"Goal","solver.goal",{{"Minimize","min"},{"Maximize","max"},{"Set to value","value"}});auto* target=decimal(form,"Target value",0,"solver.target");target->setEnabled(false);QObject::connect(goal,qOverload<int>(&QComboBox::currentIndexChanged),target,[goal,target]{target->setEnabled(goal->currentData()=="value");});
 auto* method=choices(form,"Method","solver.method",{{"Linear / integer (HiGHS)","linear"},{"Continuous nonlinear (COBYLA)","nonlinear"},{"Evolutionary / mixed integer (IHS)","evolutionary"}});
 auto* range=line(form,"Changing cell range",selectedRange(view),"solver.range");
 auto* variables=new QTableWidget(0,4,&dialog);variables->setObjectName("solver.variables");variables->setHorizontalHeaderLabels({"Cell","Lower bound","Upper bound","Type"});variables->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);layout->addWidget(variables);
 const auto fill=[&]{const auto cells=rangeCells(range->text(),book,active,200);variables->setRowCount(0);for(const auto& cell:cells){const int row=variables->rowCount();variables->insertRow(row);variables->setItem(row,0,new QTableWidgetItem(reference(cell,book,active)));variables->setItem(row,1,new QTableWidgetItem());variables->setItem(row,2,new QTableWidgetItem());auto* type=new QComboBox;type->addItems({"Continuous","Integer","Binary"});variables->setCellWidget(row,3,type);}};
 fill();auto* update=new QPushButton("Update changing cells from range",&dialog);update->setObjectName("solver.updateRange");QObject::connect(update,&QPushButton::clicked,&dialog,[&]{try{fill();}catch(const std::exception& error){QMessageBox::warning(&dialog,"Changing cells",QString::fromUtf8(error.what()));}});layout->addWidget(update);
 auto* hint=new QLabel("Enter finite lower and upper bounds for nonlinear or evolutionary search. Integer and binary types are supported by linear and evolutionary methods.",&dialog);hint->setWordWrap(true);layout->addWidget(hint);
 auto* constraints=new QTableWidget(0,3,&dialog);constraints->setObjectName("solver.constraints");constraints->setHorizontalHeaderLabels({"Constraint cell","Relation","Value"});constraints->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);layout->addWidget(constraints);
 auto* constraintButtons=new QHBoxLayout;auto* add=new QPushButton("Add constraint",&dialog);add->setObjectName("solver.addConstraint");auto* remove=new QPushButton("Remove constraint",&dialog);constraintButtons->addWidget(add);constraintButtons->addWidget(remove);layout->addLayout(constraintButtons);
 QObject::connect(add,&QPushButton::clicked,&dialog,[&]{const int row=constraints->rowCount();if(row>=1000)return;constraints->insertRow(row);constraints->setItem(row,0,new QTableWidgetItem(currentCell(view)));auto* relation=new QComboBox;relation->addItems({"<=",">=","="});constraints->setCellWidget(row,1,relation);constraints->setItem(row,2,new QTableWidgetItem("0"));});QObject::connect(remove,&QPushButton::clicked,&dialog,[&]{const int row=constraints->currentRow();if(row>=0)constraints->removeRow(row);});
 auto* options=new QFormLayout;layout->addLayout(options);auto* evaluations=new QSpinBox(&dialog);evaluations->setObjectName("solver.maxEvaluations");evaluations->setRange(10,1000000);evaluations->setValue(10000);options->addRow("Maximum evaluations",evaluations);auto* tolerance=decimal(options,"Tolerance",1e-7,"solver.tolerance");tolerance->setRange(1e-10,1);
 buttons(dialog,layout);if(dialog.exec()!=QDialog::Accepted)return nullptr;
 Json spec={{"objective",coordinate(parseAddress(objective->text(),book,active))},{"goal",goal->currentData().toString().toStdString()},{"method",method->currentData().toString().toStdString()},{"variables",Json::array()},{"constraints",Json::array()},{"maxEvaluations",evaluations->value()},{"tolerance",tolerance->value()}};
 if(goal->currentData()=="value")spec["target"]=target->value();
 for(int row=0;row<variables->rowCount();++row){auto variable=coordinate(parseAddress(variables->item(row,0)->text(),book,active));for(int column=1;column<=2;++column){const auto value=variables->item(row,column)->text().trimmed();if(!value.isEmpty()){bool okay=false;const auto number=value.toDouble(&okay);if(!okay||!std::isfinite(number))throw std::runtime_error("Variable bounds must be finite numbers.");variable[column==1?"lower":"upper"]=number;}}const int type=static_cast<QComboBox*>(variables->cellWidget(row,3))->currentIndex();if(type==1)variable["integer"]=true;if(type==2)variable["binary"]=true;spec["variables"].push_back(variable);}
 for(int row=0;row<constraints->rowCount();++row){auto constraint=coordinate(parseAddress(constraints->item(row,0)->text(),book,active));constraint["relation"]=static_cast<QComboBox*>(constraints->cellWidget(row,1))->currentText().toStdString();bool okay=false;const auto value=constraints->item(row,2)->text().toDouble(&okay);if(!okay||!std::isfinite(value))throw std::runtime_error("Constraint values must be finite numbers.");constraint["value"]=value;spec["constraints"].push_back(constraint);}
 return spec;
}
bool optimize(QWidget* parent,WorkbookSession& session,WorksheetModel& model,QTableView& view,Json args,bool interactive,const CalculationContext* context){
 const auto before=session.snapshot();if(interactive){args=solverDialog(parent,before,model.sheetId().toStdString(),view);if(args.is_null())return false;}
 const auto result=solveWorkbook(before,args,context);if(!result.value("feasibility",false))throw std::runtime_error(result.value("message",std::string("Solver did not find a feasible result ("+result.value("status",std::string("unknown"))+"). No cells were changed.")));
 auto description=QString("Objective: %1\nStatus: %2\nChanging cells: %3\n\nApply these values as one undoable edit?").arg(QString::number(result.at("objective").get<double>(),'g',15),QString::fromStdString(result.at("status").get<std::string>()),QString::number(result.at("changingCells").size()));
 int shown=0;for(const auto& cell:result.at("changingCells")){if(shown++>=20)break;const Address address{cell.at("sheet").get<std::string>(),cell.at("row").get<int>(),cell.at("col").get<int>()};description+="\n"+reference(address,before,model.sheetId().toStdString())+" = "+QString::fromStdString(cell.at("input").get<std::string>());}
 if(!confirm(parent,"Solver result",description,interactive,args))return false;
 Json operations=Json::array();for(const auto& cell:result.at("changingCells")){auto operation=cell;operation["type"]="set_cell";operations.push_back(operation);}return commit(session,model,before,operations,context);
}

bool scenarios(QWidget* parent,WorkbookSession& session,WorksheetModel& model,QTableView& view,Json args,bool interactive,const CalculationContext* context){
 const auto before=session.snapshot();auto saved=before.value("scenarios",Json::array());if(!saved.is_array())throw std::runtime_error("Saved scenario metadata is invalid.");
 if(interactive){QDialog dialog(parent);dialog.setObjectName("modeling.scenarios");dialog.setWindowTitle("Scenario Manager");auto* layout=new QVBoxLayout(&dialog);auto* form=new QFormLayout;layout->addLayout(form);auto* action=choices(form,"Action","scenario.action",{{"Save current inputs","save"},{"Apply saved inputs","apply"},{"Delete saved scenario","delete"}});auto* name=new QComboBox(&dialog);name->setObjectName("scenario.name");name->setEditable(true);for(const auto& entry:saved)name->addItem(QString::fromStdString(entry.at("name").get<std::string>()));form->addRow("Scenario name",name);auto* range=line(form,"Input cells",selectedRange(view),"scenario.range");QObject::connect(action,qOverload<int>(&QComboBox::currentIndexChanged),range,[action,range]{range->setEnabled(action->currentData()=="save");});auto* note=new QLabel("Save captures constant input values. Applying a scenario restores its saved values as one undoable edit; formulas stay in place.",&dialog);note->setWordWrap(true);layout->addWidget(note);buttons(dialog,layout);if(dialog.exec()!=QDialog::Accepted)return false;args={{"action",action->currentData().toString().toStdString()},{"name",name->currentText().toStdString()},{"range",range->text().toStdString()}};}
 const auto name=QString::fromStdString(args.at("name").get<std::string>()).trimmed();if(name.isEmpty()||name.size()>64||name.contains('\n')||name.contains('\r'))throw std::runtime_error("Enter a scenario name containing 1–64 characters.");
 const auto action=args.value("action",std::string("save"));std::size_t index=saved.size();for(std::size_t i=0;i<saved.size();++i)if(QString::fromStdString(saved[i].at("name").get<std::string>()).compare(name,Qt::CaseInsensitive)==0){index=i;break;}
 if(action=="save"){
  if(!interactive&&!args.value("accept",true))return false;
  if(index!=saved.size()&&!args.value("replace",false)){if(!interactive)throw std::runtime_error("A scenario with this name already exists. Choose another name or explicitly replace it.");if(!confirm(parent,"Replace scenario",QString("Replace the saved inputs in %1?").arg(name),true,args))return false;}
  const auto cells=rangeCells(QString::fromStdString(args.value("range",selectedRange(view).toStdString())),before,model.sheetId().toStdString(),1000);Json values=Json::array();auto source=before;
  for(const auto& cell:cells){const auto input=inputCell(source,cell).value("input",std::string());if(!input.empty()&&input.front()=='=')throw std::runtime_error("Scenario input cells must contain constants rather than formulas.");auto value=coordinate(cell);value["input"]=input;values.push_back(value);}
  const Json scenario={{"name",name.toStdString()},{"cells",values}};if(index==saved.size())saved.push_back(scenario);else saved[index]=scenario;
  return commit(session,model,before,Json::array({{{"type","set_metadata"},{"key","scenarios"},{"value",saved}}}),context);
 }
 if(index==saved.size())throw std::runtime_error("The selected scenario was not found.");
 if(action=="delete"){if(!confirm(parent,"Delete scenario",QString("Delete the saved scenario %1? Workbook cells will remain as they are.").arg(name),interactive,args))return false;saved.erase(saved.begin()+static_cast<Json::difference_type>(index));return commit(session,model,before,Json::array({{{"type","set_metadata"},{"key","scenarios"},{"value",saved}}}),context);}
 if(action!="apply")throw std::runtime_error("Choose a valid scenario action.");
 Json operations=Json::array();auto current=before;QString description=QString("Apply saved inputs from %1?\n").arg(name);int shown=0;
 for(const auto& value:saved[index].at("cells")){const Address cell{value.at("sheet").get<std::string>(),value.at("row").get<int>(),value.at("col").get<int>()};const auto input=value.at("input").get<std::string>();if(!input.empty()&&input.front()=='=')throw std::runtime_error("Saved scenario contains a formula instead of a constant.");const auto destination=inputCell(current,cell).value("input",std::string());if(!destination.empty()&&destination.front()=='=')throw std::runtime_error("Scenario application would replace a current formula. Choose constant input cells.");operations.push_back(setInput(cell,input));if(shown++<20)description+="\n"+reference(cell,before,model.sheetId().toStdString())+" = "+QString::fromStdString(input).left(100);}
 if(operations.empty())throw std::runtime_error("The saved scenario contains no input cells.");
 if(!confirm(parent,"Apply scenario",description,interactive,args))return false;
 return commit(session,model,before,operations,context);
}

struct Rectangle {Address first,last;};
Rectangle rectangle(const QString& range,const Json& book,const std::string& active,std::size_t maximum){const auto cells=rangeCells(range,book,active,maximum);if(cells.empty())throw std::runtime_error("Select a rectangular cell range.");const auto& first=cells.front();const auto& last=cells.back();const auto width=last.col-first.col+1;if(first.sheet!=last.sheet||last.row<first.row||width<1||cells.size()!=static_cast<std::size_t>(last.row-first.row+1)*static_cast<std::size_t>(width))throw std::runtime_error("Select one contiguous rectangular range.");for(std::size_t i=0;i<cells.size();++i)if(cells[i].sheet!=first.sheet||cells[i].row!=first.row+static_cast<int>(i/static_cast<std::size_t>(width))||cells[i].col!=first.col+static_cast<int>(i%static_cast<std::size_t>(width)))throw std::runtime_error("Select one contiguous rectangular range.");return {first,last};}
bool within(const Address& cell,const Rectangle& range){return cell.sheet==range.first.sheet&&cell.row>=range.first.row&&cell.row<=range.last.row&&cell.col>=range.first.col&&cell.col<=range.last.col;}
std::string literal(const Json& value){if(value.is_null())return "";if(value.is_string())return "'"+value.get<std::string>();if(value.is_boolean())return value.get<bool>()?"TRUE":"FALSE";return value.dump();}
struct PivotSource {Rectangle range;std::vector<std::string> headers;Json records=Json::array();};
PivotSource pivotSource(const Json& book,const QString& range,const std::string& active,const CalculationContext* context){
 PivotSource source;source.range=rectangle(range,book,active,100000);if(source.range.last.row<=source.range.first.row||source.range.last.col<=source.range.first.col)throw std::runtime_error("Select at least two columns and a header row followed by data.");
 const auto calculated=context->calculate(context->snapshot);std::map<std::pair<int,int>,Json> values;
 for(const auto& cell:calculated.at("values")){const Address address{cell.at("sheet").get<std::string>(),cell.at("row").get<int>(),cell.at("col").get<int>()};if(!within(address,source.range))continue;if(cell.at("type")=="error")throw std::runtime_error("Resolve formula errors in the source range before creating a pivot summary.");values[{address.row,address.col}]=cell.at("value");}
 const auto value=[&](int row,int col){const auto found=values.find({row,col});return found==values.end()?Json(""):found->second;};std::set<QString> used;
 for(int col=source.range.first.col;col<=source.range.last.col;++col){const auto entry=value(source.range.first.row,col);const auto header=QString::fromStdString(entry.is_string()?entry.get<std::string>():entry.is_null()?std::string():entry.dump()).trimmed();if(header.isEmpty()||!used.insert(header.toCaseFolded()).second)throw std::runtime_error("Source headers must be nonempty and unique.");source.headers.push_back(header.toStdString());}
 for(int row=source.range.first.row+1;row<=source.range.last.row;++row){Json record=Json::object();bool any=false;for(int col=source.range.first.col;col<=source.range.last.col;++col){const auto entry=value(row,col);record[source.headers[static_cast<std::size_t>(col-source.range.first.col)]]=entry;any=any||(!entry.is_null()&&entry!="");}if(any)source.records.push_back(record);}
 return source;
}
bool pivot(QWidget* parent,WorkbookSession& session,WorksheetModel& model,QTableView& view,Json args,bool interactive,const CalculationContext* context){
 const auto before=session.snapshot();const auto active=model.sheetId().toStdString();
 if(interactive){QDialog dialog(parent);dialog.setObjectName("modeling.pivot");dialog.setWindowTitle("Pivot Summary");auto* layout=new QVBoxLayout(&dialog);auto* form=new QFormLayout;layout->addLayout(form);auto* range=line(form,"Source range (first row is headers)",selectedRange(view),"pivot.range");auto* rowField=new QComboBox(&dialog);rowField->setObjectName("pivot.rowField");form->addRow("Group rows by",rowField);auto* valueField=new QComboBox(&dialog);valueField->setObjectName("pivot.valueField");form->addRow("Value field",valueField);auto* aggregate=choices(form,"Aggregate","pivot.aggregate",{{"Sum","sum"},{"Count nonempty values","count"},{"Average","average"},{"Minimum","min"},{"Maximum","max"}});auto* output=line(form,"Output cell","E1","pivot.output");auto* reload=new QPushButton("Read source field names",&dialog);reload->setObjectName("pivot.readFields");layout->addWidget(reload);const auto load=[&]{const auto data=pivotSource(before,range->text(),active,context);rowField->clear();valueField->clear();for(const auto& header:data.headers){rowField->addItem(QString::fromStdString(header));valueField->addItem(QString::fromStdString(header));}valueField->setCurrentIndex(1);};try{load();}catch(const std::exception&){}QObject::connect(reload,&QPushButton::clicked,&dialog,[&]{try{load();}catch(const std::exception& error){QMessageBox::warning(&dialog,"Source range",QString::fromUtf8(error.what()));}});auto* note=new QLabel("Creates a static grouped summary. Excel opens the result as ordinary cells; use this builder again to refresh the summary.",&dialog);note->setWordWrap(true);layout->addWidget(note);buttons(dialog,layout);if(dialog.exec()!=QDialog::Accepted)return false;args={{"range",range->text().toStdString()},{"rowField",rowField->currentText().toStdString()},{"valueField",valueField->currentText().toStdString()},{"aggregate",aggregate->currentData().toString().toStdString()},{"output",output->text().toStdString()}};}
 const auto range=QString::fromStdString(args.value("range",selectedRange(view).toStdString()));const auto source=pivotSource(before,range,active,context);const auto rowField=args.at("rowField").get<std::string>(),valueField=args.at("valueField").get<std::string>();
 if(std::find(source.headers.begin(),source.headers.end(),rowField)==source.headers.end()||std::find(source.headers.begin(),source.headers.end(),valueField)==source.headers.end())throw std::runtime_error("Choose row and value fields from the source headers.");
 const auto aggregate=args.value("aggregate",std::string("sum"));const auto summary=pivotRows(source.records,{rowField},valueField,aggregate);const auto output=parseAddress(QString::fromStdString(args.at("output").get<std::string>()),before,active);const auto lastRow=output.row+static_cast<int>(summary.at("rows").size())+1;const Rectangle destination{output,{output.sheet,lastRow,output.col+1}};
 if(lastRow>MaxRows||output.col+1>MaxColumns)throw std::runtime_error("The summary does not fit at this destination.");
 if(output.sheet==source.range.first.sheet&&destination.first.row<=source.range.last.row&&destination.last.row>=source.range.first.row&&destination.first.col<=source.range.last.col&&destination.last.col>=source.range.first.col)throw std::runtime_error("The pivot destination overlaps its source records.");
 Json operations=Json::array();const auto write=[&](int row,int col,const Json& value,bool bold=false){auto operation=setInput({output.sheet,row,col},literal(value));if(bold)operation["style"]={{"bold",true}};operations.push_back(operation);};
 write(output.row,output.col,rowField,true);write(output.row,output.col+1,aggregate+" of "+valueField,true);int row=output.row+1;
 for(const auto& group:summary.at("rows")){const auto key=group.at("keys")[0];write(row,output.col,key.is_null()||key==""?Json("(blank)"):key);write(row,output.col+1,group.at("value"));++row;}write(row,output.col,"Grand Total",true);write(row,output.col+1,summary.at("total"),true);
 auto definitions=before.value("pivots",Json::array());if(!definitions.is_array())throw std::runtime_error("Saved pivot metadata is invalid.");definitions.push_back({{"id",QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString()},{"source",{{"sheet",source.range.first.sheet},{"row1",source.range.first.row},{"col1",source.range.first.col},{"row2",source.range.last.row},{"col2",source.range.last.col}}},{"rowField",rowField},{"valueField",valueField},{"aggregate",aggregate},{"output",coordinate(output)},{"staticSummary",true}});operations.push_back({{"type","set_metadata"},{"key","pivots"},{"value",definitions}});
 if(!confirm(parent,"Create pivot summary",QString("Write %1 grouped rows and a grand total at %2:%3?\nThe source records stay in place.").arg(QString::number(summary.at("rows").size()),reference(output,before,active),label(destination.last)),interactive,args))return false;
 return commit(session,model,before,operations,context);
}

Json numericList(const QString& text){Json values=Json::array();for(const auto& token:text.split(QRegularExpression("[,;\\s]+"),Qt::SkipEmptyParts)){bool okay=false;const auto value=token.toDouble(&okay);if(!okay||!std::isfinite(value))throw std::runtime_error("Enter finite numeric values separated by commas.");values.push_back(value);}return values;}
std::vector<double> numericValues(const Json& values){if(!values.is_array()||values.empty()||values.size()>1000)throw std::runtime_error("Enter between 1 and 1000 values for each input.");std::vector<double> numbers;for(const auto& entry:values){if(!entry.is_number()||!std::isfinite(entry.get<double>()))throw std::runtime_error("Sensitivity input values must be finite numbers.");numbers.push_back(entry.get<double>());}return numbers;}
bool sensitivity(QWidget* parent,WorkbookSession& session,WorksheetModel& model,QTableView& view,Json args,bool interactive,const CalculationContext* context){
 const auto before=session.snapshot();const auto active=model.sheetId().toStdString();
 if(interactive){QDialog dialog(parent);dialog.setObjectName("modeling.sensitivity");dialog.setWindowTitle("Two-Variable Sensitivity Table");auto* layout=new QVBoxLayout(&dialog);auto* form=new QFormLayout;layout->addLayout(form);auto* output=line(form,"Result formula cell",currentCell(view),"table.output");auto* rowInput=line(form,"Row input cell","B2","table.rowInput");auto* columnInput=line(form,"Column input cell","B3","table.columnInput");auto* rows=line(form,"Values across top row","10, 20, 30","table.rowValues");auto* columns=line(form,"Values down first column","1, 2, 3","table.colValues");auto* destination=line(form,"Output grid starts at","E10","table.destination");auto* note=new QLabel("Each combination recalculates an isolated model. Input cells remain unchanged. The inserted values are snapshots; run the table again after changing the model.",&dialog);note->setWordWrap(true);layout->addWidget(note);buttons(dialog,layout);if(dialog.exec()!=QDialog::Accepted)return false;args={{"output",output->text().toStdString()},{"rowInput",rowInput->text().toStdString()},{"columnInput",columnInput->text().toStdString()},{"rowValues",numericList(rows->text())},{"colValues",numericList(columns->text())},{"destination",destination->text().toStdString()}};}
 const auto rowValues=numericValues(args.at("rowValues")),columnValues=numericValues(args.at("colValues"));if(rowValues.size()*columnValues.size()>10000)throw std::runtime_error("A sensitivity table supports at most 10000 input combinations.");
 const auto rowInput=parseAddress(QString::fromStdString(args.at("rowInput").get<std::string>()),before,active),columnInput=parseAddress(QString::fromStdString(args.at("columnInput").get<std::string>()),before,active),output=parseAddress(QString::fromStdString(args.at("output").get<std::string>()),before,active),destination=parseAddress(QString::fromStdString(args.at("destination").get<std::string>()),before,active);
 if(std::tie(rowInput.sheet,rowInput.row,rowInput.col)==std::tie(columnInput.sheet,columnInput.row,columnInput.col))throw std::runtime_error("Row and column inputs must be different cells.");
 auto source=before;for(const auto& cell:{rowInput,columnInput}){const auto input=QString::fromStdString(inputCell(source,cell).value("input",std::string()));bool okay=false;input.toDouble(&okay);if(!input.isEmpty()&&!okay)throw std::runtime_error("Sensitivity input cells must contain numeric constants.");}
 const Rectangle grid{destination,{destination.sheet,destination.row+static_cast<int>(columnValues.size()),destination.col+static_cast<int>(rowValues.size())}};
 if(grid.last.row>MaxRows||grid.last.col>MaxColumns)throw std::runtime_error("The sensitivity grid does not fit at this destination.");
 if(within(rowInput,grid)||within(columnInput,grid)||within(output,grid))throw std::runtime_error("The sensitivity destination overlaps a model input or its output formula.");
 const auto values=sensitivityTable(rowValues,columnValues,[&](double row,double column){auto candidate=context->snapshot;inputCell(candidate,rowInput)["input"]=Json(row).dump();inputCell(candidate,columnInput)["input"]=Json(column).dump();return numberAt(context->calculate(candidate),output);});
 Json operations=Json::array();auto corner=setInput(destination,"="+reference(output,before,destination.sheet).toStdString());corner["style"]={{"bold",true}};operations.push_back(corner);
 for(std::size_t row=0;row<rowValues.size();++row){auto header=setInput({destination.sheet,destination.row,destination.col+1+static_cast<int>(row)},Json(rowValues[row]).dump());header["style"]={{"bold",true}};operations.push_back(header);}
 for(std::size_t column=0;column<columnValues.size();++column){auto header=setInput({destination.sheet,destination.row+1+static_cast<int>(column),destination.col},Json(columnValues[column]).dump());header["style"]={{"bold",true}};operations.push_back(header);for(std::size_t row=0;row<rowValues.size();++row)operations.push_back(setInput({destination.sheet,destination.row+1+static_cast<int>(column),destination.col+1+static_cast<int>(row)},Json(values[row][column]).dump()));}
 if(!confirm(parent,"Create sensitivity table",QString("Write %1 combinations at %2:%3?\nModel inputs will remain unchanged.").arg(QString::number(rowValues.size()*columnValues.size()),reference(destination,before,active),label(grid.last)),interactive,args))return false;
 return commit(session,model,before,operations,context);
}

bool seek(QWidget* parent,WorkbookSession& session,WorksheetModel& model,QTableView& view,Json args,bool interactive,const CalculationContext* context){
 if(interactive){QDialog dialog(parent);dialog.setObjectName("modeling.goalSeek");dialog.setWindowTitle("Goal Seek");auto* layout=new QVBoxLayout(&dialog);auto* form=new QFormLayout;layout->addLayout(form);auto* output=line(form,"Set cell",currentCell(view),"goalSeek.output");auto* target=decimal(form,"To value",0,"goalSeek.target");auto* changing=line(form,"By changing cell","A1","goalSeek.changing");buttons(dialog,layout);if(dialog.exec()!=QDialog::Accepted)return false;args={{"output",output->text().toStdString()},{"target",target->value()},{"changing",changing->text().toStdString()}};}
 const auto before=session.snapshot();const auto sheet=model.sheetId().toStdString();const auto output=parseAddress(QString::fromStdString(args.at("output").get<std::string>()),before,sheet),changing=parseAddress(QString::fromStdString(args.at("changing").get<std::string>()),before,sheet);
 if(!args.at("target").is_number())throw std::runtime_error("Goal Seek target must be numeric.");
 const auto target=args.at("target").get<double>();if(!std::isfinite(target))throw std::runtime_error("Goal Seek target must be finite.");
 auto source=before;const auto raw=QString::fromStdString(inputCell(source,changing).at("input").get<std::string>());bool okay=false;double guess=raw.isEmpty()?0:raw.toDouble(&okay);if(!raw.isEmpty()&&!okay)throw std::runtime_error("The changing cell must contain a numeric constant.");
 const auto result=goalSeek([&](double value){auto candidate=context->snapshot;inputCell(candidate,changing)["input"]=Json(value).dump();return numberAt(context->calculate(candidate),output);},target,guess,args.value("tolerance",1e-9),args.value("iterations",100));
 if(result.status!="converged")throw std::runtime_error("Goal Seek did not converge ("+result.status+"). No cells were changed.");
 if(!confirm(parent,"Goal Seek result",QString("Set %1 to %2?\n%3 will calculate %4.").arg(label(changing),QString::number(result.value,'g',15),label(output),QString::number(result.result,'g',15)),interactive,args))return false;
 return commit(session,model,before,Json::array({setInput(changing,Json(result.value).dump())}),context);
}
}
bool modelingCommand(QWidget* parent,WorkbookSession& session,WorksheetModel& model,QTableView& view,const QString& command,const Json& args,const CalculationContext* context){
 const bool recognized=command=="analysis.goalSeek"||command=="analysis.solver"||command=="scenario.manage"||command=="pivot.create"||command=="analysis.table";
 if(!recognized)return false;
 if(!args.is_null()&&!args.is_object())throw std::runtime_error("Modeling command arguments must be an object.");
 CalculationEngine engine;CalculationContext fallback;fallback.snapshot=session.snapshot();fallback.evaluator=[&](const Json& candidate){return calculation(engine,candidate);};if(!context)context=&fallback;
 if(!context->current()||context->snapshot.value("revision",Json())!=session.snapshot().at("revision"))throw std::runtime_error("The calculation context is stale. Run the analysis again.");
 const bool interactive=args.is_null()||args.empty();
 if(command=="analysis.goalSeek")return seek(parent,session,model,view,args,interactive,context);
 if(command=="analysis.solver")return optimize(parent,session,model,view,args,interactive,context);
 if(command=="scenario.manage")return scenarios(parent,session,model,view,args,interactive,context);
 if(command=="pivot.create")return pivot(parent,session,model,view,args,interactive,context);
 if(command=="analysis.table")return sensitivity(parent,session,model,view,args,interactive,context);
 return false;
}
}

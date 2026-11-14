#include "tigersheets/formula_editor.hpp"
#include <QListWidget>
#include <QKeyEvent>
#include <QLabel>
#include <QMap>
#include <QGuiApplication>
#include <QScreen>
#include <QStyle>
#include <QRegularExpression>
#include <QSet>

namespace tigersheets {
namespace {
QRect availableScreenGeometry(const QWidget* editor) {
    for(auto* screen:{editor->screen(),QGuiApplication::primaryScreen()})
        if(screen&&screen->availableGeometry().isValid())return screen->availableGeometry();
    return {0,0,1024,768};
}
int compactPopupWidth(const QListWidget* popup,const QRect& available) {
    int desired=280;
    const int padding=24+2*popup->frameWidth()+popup->style()->pixelMetric(QStyle::PM_ScrollBarExtent,nullptr,popup);
    for(int i=0;i<popup->count();++i)desired=qMax(desired,popup->fontMetrics().horizontalAdvance(popup->item(i)->text())+padding);
    return qMin(available.width(),qBound(280,desired,520));
}
QPoint boundedOrigin(const QRect& available,const QSize& size,const QPoint& preferred) {
    return {qBound(available.left(),preferred.x(),qMax(available.left(),available.right()-size.width()+1)),
            qBound(available.top(),preferred.y(),qMax(available.top(),available.bottom()-size.height()+1))};
}
struct CompletionSpan {int start=-1,end=-1;QString prefix;};
struct CallContext {QString name;int argument=-1;};
struct FunctionHelp {QStringList parameters;QString description;};
enum CompletionKind {BuiltinFunction,DefinedName,WorkbookFunction};
bool identifier(QChar c){return c.isLetterOrNumber()||c=='_'||c=='.'||c=='\\';}
bool cellReferenceIdentifier(const QString& name) {
    static const QRegularExpression expression("^\\$?([A-Za-z]{1,3})\\$?([1-9][0-9]*)$");
    const auto match=expression.match(name);if(!match.hasMatch())return false;
    int column=0;for(const auto c:match.captured(1).toUpper())column=column*26+c.unicode()-'A'+1;
    bool valid=false;const auto row=match.captured(2).toULongLong(&valid);
    return valid&&row<=1048576&&column<=16384;
}
bool validWorkbookIdentifier(const QString& name) {
    if(name.isEmpty()||name.size()>255||(!name[0].isLetter()&&name[0]!='_'&&name[0]!='\\'))return false;
    for(int i=1;i<name.size();++i)if(!name[i].isLetterOrNumber()&&name[i]!='_'&&name[i]!='.')return false;
    if(name.compare("R",Qt::CaseInsensitive)==0||name.compare("C",Qt::CaseInsensitive)==0)return false;
    if(cellReferenceIdentifier(name))return false;
    return !QRegularExpression("^R[1-9][0-9]*C[1-9][0-9]*$",QRegularExpression::CaseInsensitiveOption).match(name).hasMatch();
}
QString definedNameHelp(const QString& name) {
    return name.toHtmlEscaped()+"<br>Defined name. Inserts a workbook identifier without parentheses.";
}
QString workbookFunctionHelp(const QString& name) {
    return name.toHtmlEscaped()+"(…)<br>Workbook function. Provided by this workbook. Parameter details are not included in this completion list.";
}
CompletionSpan completionSpan(const QString& formula,int cursor) {
    if(!formula.startsWith('=')||cursor<1)return {};
    bool quoted=false,sheet=false;int brackets=0;
    for(int i=1;i<cursor;++i) {
        const auto c=formula[i];
        if(quoted){if(c=='"'){if(i+1<cursor&&formula[i+1]=='"')++i;else quoted=false;}continue;}
        if(sheet){if(c=='\''){if(i+1<cursor&&formula[i+1]=='\'')++i;else sheet=false;}continue;}
        if(c=='"'){quoted=true;continue;}if(c=='\''){sheet=true;continue;}
        if(c=='[')++brackets;else if(c==']'&&brackets)--brackets;
    }
    if(quoted||sheet||brackets)return {};
    int refStart=cursor,refEnd=cursor;
    auto referenceCharacter=[](QChar c){return c.isLetterOrNumber()||c=='$';};
    while(refStart>1&&referenceCharacter(formula[refStart-1]))--refStart;
    while(refEnd<formula.size()&&referenceCharacter(formula[refEnd]))++refEnd;
    if((refStart==1||!identifier(formula[refStart-1]))&&cellReferenceIdentifier(formula.mid(refStart,refEnd-refStart)))return {};
    int start=cursor,end=cursor;
    while(start>1&&identifier(formula[start-1]))--start;
    while(end<formula.size()&&identifier(formula[end]))++end;
    if(start==cursor||(!formula[start].isLetter()&&formula[start]!='_'&&formula[start]!='\\'))return {};
    if(start>0&&(formula[start-1]=='!'||formula[start-1]=='$'||formula[start-1]==':'))return {};
    if(end<formula.size()&&(formula[end]==':'||formula[end]=='!'||formula[end]=='['))return {};
    const auto token=formula.mid(start,end-start);
    if(cellReferenceIdentifier(token))return {};
    return {start,end,formula.mid(start,cursor-start)};
}
CallContext callContext(const QString& formula,int cursor) {
    struct Frame {QString name;int argument=0,arrayDepth=0;};
    QList<Frame> stack;bool quoted=false,sheet=false;int brackets=0,arrays=0;
    if(!formula.startsWith('='))return {};
    for(int i=1;i<cursor&&i<formula.size();++i) {
        const auto c=formula[i];
        if(quoted){if(c=='"'){if(i+1<cursor&&formula[i+1]=='"')++i;else quoted=false;}continue;}
        if(sheet){if(c=='\''){if(i+1<cursor&&formula[i+1]=='\'')++i;else sheet=false;}continue;}
        if(c=='"'){quoted=true;continue;}if(c=='\''){sheet=true;continue;}
        if(c=='['){++brackets;continue;}if(c==']'&&brackets){--brackets;continue;}if(brackets)continue;
        if(c=='{'){++arrays;continue;}if(c=='}'){arrays=qMax(arrays-1,0);continue;}
        if(c=='(') {
            int end=i;while(end>0&&formula[end-1].isSpace())--end;
            int start=end;while(start>0&&identifier(formula[start-1]))--start;
            auto name=formula.mid(start,end-start).toUpper();
            if(name.isEmpty()||(!name.front().isLetter()&&name.front()!='_'&&name.front()!='\\'))name.clear();
            stack.push_back({name,0,arrays});
        } else if(c==')') {if(!stack.isEmpty())stack.removeLast();}
        else if((c==','||c==';')&&!stack.isEmpty()&&arrays==stack.last().arrayDepth)++stack.last().argument;
    }
    for(auto i=stack.crbegin();i!=stack.crend();++i)if(!i->name.isEmpty())return {i->name,i->argument};
    return {};
}
const QMap<QString,FunctionHelp>& commonHelp() {
    static const QMap<QString,FunctionHelp> data={
        {"SUM",{{"number1","[number2]","…"},"Adds the supplied numbers and ranges."}},
        {"AVERAGE",{{"number1","[number2]","…"},"Returns the arithmetic mean of numeric values."}},
        {"SUMIF",{{"range","criteria","[sum_range]"},"Adds values whose corresponding cells meet one condition."}},
        {"SUMIFS",{{"sum_range","criteria_range1","criteria1","[criteria_range2]","[criteria2]","…"},"Adds values that meet every supplied condition."}},
        {"AVERAGEIF",{{"range","criteria","[average_range]"},"Averages values that meet one condition."}},
        {"AVERAGEIFS",{{"average_range","criteria_range1","criteria1","[criteria_range2]","[criteria2]","…"},"Averages values that meet every supplied condition."}},
        {"COUNT",{{"value1","[value2]","…"},"Counts numeric values."}},
        {"COUNTA",{{"value1","[value2]","…"},"Counts nonempty values."}},
        {"COUNTIF",{{"range","criteria"},"Counts cells that meet a condition."}},
        {"COUNTIFS",{{"criteria_range1","criteria1","[criteria_range2]","[criteria2]","…"},"Counts cells that meet all supplied conditions."}},
        {"IF",{{"logical_test","value_if_true","[value_if_false]"},"Chooses a result according to a logical test."}},
        {"IFERROR",{{"value","value_if_error"},"Returns an alternative result when a value is an error."}},
        {"IFNA",{{"value","value_if_na"},"Returns an alternative result for a missing-match error."}},
        {"IFS",{{"logical_test1","value_if_true1","…"},"Returns the result of the first true condition."}},
        {"NOT",{{"logical"},"Reverses a logical value."}},
        {"AND",{{"logical1","[logical2]","…"},"Tests whether every supplied condition is true."}},
        {"OR",{{"logical1","[logical2]","…"},"Tests whether any supplied condition is true."}},
        {"XLOOKUP",{{"lookup_value","lookup_array","return_array","[if_not_found]","[match_mode]","[search_mode]"},"Finds a match and returns the corresponding value from another range."}},
        {"XMATCH",{{"lookup_value","lookup_array","[match_mode]","[search_mode]"},"Returns the position of a matching value."}},
        {"INDEX",{{"array","row_num","[column_num]"},"Returns a value at a row and column. This hint shows the array form."}},
        {"MATCH",{{"lookup_value","lookup_array","[match_type]"},"Returns the position of a match in a range."}},
        {"VLOOKUP",{{"lookup_value","table_array","col_index_num","[range_lookup]"},"Searches the first column and returns a value from another column."}},
        {"HLOOKUP",{{"lookup_value","table_array","row_index_num","[range_lookup]"},"Searches the first row and returns a value from another row."}},
        {"NPV",{{"rate","value1","[value2]","…"},"Discounts periodic future cash flows. Add a time-zero cash flow separately."}},
        {"XNPV",{{"rate","values","dates"},"Discounts cash flows on their individual dates."}},
        {"IRR",{{"values","[guess]"},"Estimates the internal rate of return for periodic cash flows."}},
        {"XIRR",{{"values","dates","[guess]"},"Estimates the annual return for cash flows on individual dates."}},
        {"MIRR",{{"values","finance_rate","reinvest_rate"},"Calculates a modified return using financing and reinvestment rates."}},
        {"PV",{{"rate","nper","pmt","[fv]","[type]"},"Calculates present value for periodic payments and a fixed rate."}},
        {"FV",{{"rate","nper","pmt","[pv]","[type]"},"Calculates future value for periodic payments and a fixed rate."}},
        {"PMT",{{"rate","nper","pv","[fv]","[type]"},"Calculates a constant periodic payment."}},
        {"RATE",{{"nper","pmt","pv","[fv]","[type]","[guess]"},"Estimates the interest rate per payment period."}},
        {"NPER",{{"rate","pmt","pv","[fv]","[type]"},"Calculates the number of payment periods."}},
        {"IPMT",{{"rate","per","nper","pv","[fv]","[type]"},"Returns the interest portion of a payment."}},
        {"PPMT",{{"rate","per","nper","pv","[fv]","[type]"},"Returns the principal portion of a payment."}},
        {"DATE",{{"year","month","day"},"Builds a date serial from a year, month and day."}},
        {"YEAR",{{"serial_number"},"Returns the year of a date serial."}},
        {"MONTH",{{"serial_number"},"Returns the month of a date serial."}},
        {"DAY",{{"serial_number"},"Returns the day of a date serial."}},
        {"EOMONTH",{{"start_date","months"},"Returns the month-end date a number of months away."}},
        {"EDATE",{{"start_date","months"},"Shifts a date by a number of months."}},
        {"YEARFRAC",{{"start_date","end_date","[basis]"},"Measures the fraction of a year between two dates."}},
        {"ROUND",{{"number","num_digits"},"Rounds a number to a chosen precision."}},
        {"ROUNDUP",{{"number","num_digits"},"Rounds a number away from zero."}},
        {"ROUNDDOWN",{{"number","num_digits"},"Rounds a number toward zero."}},
        {"INT",{{"number"},"Rounds a number down to the nearest integer."}},
        {"MOD",{{"number","divisor"},"Returns the remainder after division."}},
        {"POWER",{{"number","power"},"Raises a number to a power."}},
        {"SQRT",{{"number"},"Returns the nonnegative square root."}},
        {"PRODUCT",{{"number1","[number2]","…"},"Multiplies the supplied numeric values."}},
        {"SUMPRODUCT",{{"array1","[array2]","…"},"Sums the products of corresponding array entries."}},
        {"SUBTOTAL",{{"function_num","ref1","[ref2]","…"},"Applies a selected summary operation to references."}},
        {"MIN",{{"number1","[number2]","…"},"Returns the smallest numeric value."}},
        {"MAX",{{"number1","[number2]","…"},"Returns the largest numeric value."}},
        {"ABS",{{"number"},"Returns the magnitude of a number."}},
        {"LET",{{"name1","name_value1","calculation_or_name2","[name_value2]","…"},"Assigns local names for use in a final calculation."}},
        {"LAMBDA",{{"[parameter1]","…","calculation"},"Defines a reusable calculation with optional parameters and a final expression."}},
        {"FILTER",{{"array","include","[if_empty]"},"Returns entries selected by a Boolean array."}},
        {"SORT",{{"array","[sort_index]","[sort_order]","[by_col]"},"Returns an array ordered by a selected row or column."}},
        {"SORTBY",{{"array","by_array1","[sort_order1]","…"},"Orders an array by corresponding values in other arrays."}},
        {"UNIQUE",{{"array","[by_col]","[exactly_once]"},"Returns distinct entries or entries occurring exactly once."}},
        {"SEQUENCE",{{"rows","[columns]","[start]","[step]"},"Creates an array of numbers with a chosen start and step."}},
        {"CHOOSE",{{"index_num","value1","[value2]","…"},"Selects a value using its position in a list."}},
        {"ROWS",{{"array"},"Counts the rows in a reference or array."}},
        {"COLUMNS",{{"array"},"Counts the columns in a reference or array."}},
        {"TRANSPOSE",{{"array"},"Exchanges the rows and columns of an array."}},
        {"TODAY",{{},"Returns the current date."}},
        {"NOW",{{},"Returns the current date and time."}},
        {"TIME",{{"hour","minute","second"},"Builds a time serial from hours, minutes and seconds."}},
        {"WEEKDAY",{{"serial_number","[return_type]"},"Returns the weekday number using a selected numbering scheme."}},
        {"DATEVALUE",{{"date_text"},"Converts a recognized text date to a date serial."}},
        {"WORKDAY",{{"start_date","days","[holidays]"},"Moves a date by working days, excluding weekends and optional holidays."}},
        {"NETWORKDAYS",{{"start_date","end_date","[holidays]"},"Counts working days between two dates."}},
        {"DAYS",{{"end_date","start_date"},"Counts days from a start date to an end date."}},
        {"TEXT",{{"value","format_text"},"Formats a value as text using a number-format pattern."}},
        {"VALUE",{{"text"},"Converts recognized numeric text to a number."}},
        {"LEN",{{"text"},"Counts the characters in text."}},
        {"LEFT",{{"text","[num_chars]"},"Returns characters from the beginning of text."}},
        {"RIGHT",{{"text","[num_chars]"},"Returns characters from the end of text."}},
        {"MID",{{"text","start_num","num_chars"},"Returns characters starting at a selected position."}},
        {"TRIM",{{"text"},"Removes extra spaces from text."}},
        {"UPPER",{{"text"},"Converts text to uppercase."}},
        {"LOWER",{{"text"},"Converts text to lowercase."}},
        {"SUBSTITUTE",{{"text","old_text","new_text","[instance_num]"},"Replaces matching text, optionally choosing one occurrence."}},
        {"REPLACE",{{"old_text","start_num","num_chars","new_text"},"Replaces text at a selected character position."}},
        {"FIND",{{"find_text","within_text","[start_num]"},"Finds a case-sensitive text match and returns its position."}},
        {"SEARCH",{{"find_text","within_text","[start_num]"},"Finds a text match without requiring matching case."}},
        {"CONCAT",{{"text1","[text2]","…"},"Combines text values and ranges."}},
        {"CONCATENATE",{{"text1","[text2]","…"},"Combines supplied text values."}},
        {"EXACT",{{"text1","text2"},"Tests whether two text values match with the same case."}},
        {"TEXTJOIN",{{"delimiter","ignore_empty","text1","[text2]","…"},"Joins text values with a delimiter."}},
        {"AGGREGATE",{{"function_num","options","ref1_or_array","[ref2_or_k]","…"},"Applies a selected aggregate while controlling ignored errors and rows."}}
    };
    return data;
}
QString renderHelp(const QString& name,int argument,const QString& category) {
    auto i=commonHelp().constFind(name);
    if(i==commonHelp().cend())return name.toHtmlEscaped()+"(…)<br>"+category.toHtmlEscaped()+" function. Argument details are not included in this local catalog.";
    QStringList parameters;
    for(int n=0;n<i->parameters.size();++n) {
        auto value=i->parameters[n].toHtmlEscaped();
        if(n==argument||(argument>=i->parameters.size()&&n==i->parameters.size()-1&&i->parameters[n]==QStringLiteral("…")))value="<b>"+value+"</b>";
        parameters<<value;
    }
    return name.toHtmlEscaped()+"("+parameters.join(", ")+")<br>"+i->description.toHtmlEscaped();
}
}
struct FormulaLineEdit::State {
    QStringList functions;
    QListWidget* popup{};
    CompletionSpan span;
    QMap<QString,QString> categories;
    QLabel* help{};
    CallContext call;
    bool dismissed=false;
    QString dismissedText;
    QString completionNotice;
    QMap<QString,QString> workbookNames;
    QMap<QString,QString> workbookFunctions;
    QSet<QString> builtinNames;
};

FormulaLineEdit::FormulaLineEdit(QWidget* parent):QLineEdit(parent),state_(std::make_unique<State>()) {
    state_->popup=new QListWidget(this);
    state_->popup->setWindowFlags(Qt::ToolTip);
    state_->popup->setAttribute(Qt::WA_ShowWithoutActivating);
    state_->popup->setFocusPolicy(Qt::NoFocus);
    state_->popup->setTextElideMode(Qt::ElideRight);
    state_->popup->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    state_->popup->setObjectName("formula.completions");
    state_->popup->setAccessibleName("Formula suggestions");
    state_->popup->setAccessibleDescription("Use Up and Down to select an entry. Tab inserts it. Escape closes suggestions.");
    state_->help=new QLabel(this,Qt::ToolTip);
    state_->help->setObjectName("formula.argumentHelp");
    state_->help->setAccessibleName("Formula help");
    state_->help->setAttribute(Qt::WA_ShowWithoutActivating);
    state_->help->setTextFormat(Qt::RichText);
    state_->help->setWordWrap(true);state_->help->setMargin(8);
    state_->help->setFrameStyle(QFrame::StyledPanel);
    connect(state_->popup,&QListWidget::currentItemChanged,this,[this]{updateHelpPresentation();});
    connect(state_->popup,&QListWidget::itemDoubleClicked,this,[this]{acceptCompletion();});
    connect(this,&QLineEdit::textChanged,this,[this]{refreshFormulaHelp();});
    connect(this,&QLineEdit::cursorPositionChanged,this,[this]{refreshFormulaHelp();});
}
FormulaLineEdit::~FormulaLineEdit()=default;
void FormulaLineEdit::setFunctionCatalog(const Json& frozen,const Json& available) {
    QSet<QString> implemented;
    if(available.is_object()&&available.contains("functions")&&available["functions"].is_array())
        for(const auto& name:available["functions"])if(name.is_string())implemented.insert(QString::fromStdString(name.get<std::string>()).toUpper());
    state_->functions.clear();state_->categories.clear();state_->builtinNames=implemented;
    if(frozen.is_object()&&frozen.contains("entries")&&frozen["entries"].is_array())
        for(const auto& entry:frozen["entries"])
            if(entry.is_object()&&entry.contains("name")&&entry["name"].is_string()) {
                auto name=QString::fromStdString(entry["name"].get<std::string>()).toUpper();
                state_->builtinNames.insert(name);
                if(implemented.contains(name)) {
                    state_->functions<<name;
                    if(entry.contains("category")&&entry["category"].is_string())state_->categories[name]=QString::fromStdString(entry["category"].get<std::string>());
                }
            }
    state_->functions.removeDuplicates();state_->functions.sort();refreshFormulaHelp();
}
void FormulaLineEdit::setWorkbookCompletions(const QStringList& names,const QStringList& functions) {
    state_->workbookNames.clear();state_->workbookFunctions.clear();
    for(const auto& name:names)if(validWorkbookIdentifier(name)&&!state_->workbookNames.contains(name.toUpper()))state_->workbookNames.insert(name.toUpper(),name);
    for(const auto& name:functions)if(validWorkbookIdentifier(name)&&!state_->workbookFunctions.contains(name.toUpper())) {
        state_->workbookFunctions.insert(name.toUpper(),name);state_->workbookNames.remove(name.toUpper());
    }
    refreshFormulaHelp();
}
QListWidget* FormulaLineEdit::completionPopup()const{return state_->popup;}
QString FormulaLineEdit::argumentHelpText()const{return state_->help->text();}
QString FormulaLineEdit::currentFunction()const{return state_->call.name;}
int FormulaLineEdit::currentArgument()const{return state_->call.argument;}
bool FormulaLineEdit::handleFormulaKey(QKeyEvent* event){
    if(event->key()==Qt::Key_Escape&&event->modifiers()==Qt::NoModifier&&
       (state_->popup->isVisible()||state_->help->isVisible())) {
        state_->dismissed=true;state_->dismissedText=text();
        state_->popup->hide();state_->help->clear();state_->help->hide();
        event->accept();return true;
    }
    if(event->key()==Qt::Key_Down&&event->modifiers()==Qt::AltModifier&&text().startsWith('=')) {
        state_->dismissed=state_->popup->isVisible();state_->dismissedText=text();
        refreshFormulaHelp();event->accept();return true;
    }
    if(state_->popup->isVisible()&&event->modifiers()==Qt::NoModifier) {
        const int current=state_->popup->currentRow(),last=state_->popup->count()-1;
        int next=-1;
        switch(event->key()) {
        case Qt::Key_Down:next=qMin(current+1,last);break;
        case Qt::Key_Up:next=qMax(current-1,0);break;
        case Qt::Key_Home:next=0;break;
        case Qt::Key_End:next=last;break;
        case Qt::Key_PageDown:next=qMin(current+8,last);break;
        case Qt::Key_PageUp:next=qMax(current-8,0);break;
        default:break;
        }
        if(next>=0){state_->popup->setCurrentRow(next);event->accept();return true;}
    }
    if(event->key()==Qt::Key_Tab&&event->modifiers()==Qt::NoModifier&&state_->popup->isVisible()) {
        if(acceptCompletion()){event->accept();return true;}
    }
    return false;
}
bool FormulaLineEdit::acceptCompletion(){
    if(isReadOnly()||!isEnabled()||!hasFocus()||!state_->popup->isVisible()||!state_->popup->currentItem())return false;
    const auto name=state_->popup->currentItem()->data(Qt::UserRole).toString();
    const bool function=state_->popup->currentItem()->data(Qt::UserRole+1).toInt()!=DefinedName;
    const auto span=state_->span;
    int paren=span.end;while(paren<text().size()&&text()[paren].isSpace())++paren;
    const bool existing=paren<text().size()&&text()[paren]=='(';
    setSelection(span.start,span.end-span.start);insert(name+(!function||existing?"":"("));
    if(function&&existing)setCursorPosition(span.start+name.size()+paren-span.end+1);
    setModified(true);
    state_->popup->hide();updateHelpPresentation();return true;
}
bool FormulaLineEdit::event(QEvent* event){
    if(event->type()==QEvent::KeyPress&&handleFormulaKey(static_cast<QKeyEvent*>(event)))return true;
    const auto type=event->type();
    if(type==QEvent::FocusOut||type==QEvent::Hide) {
        state_->popup->hide();state_->help->clear();state_->help->hide();
    }
    const bool handled=QLineEdit::event(event);
    if(type==QEvent::FocusIn||type==QEvent::ReadOnlyChange||type==QEvent::EnabledChange)refreshFormulaHelp();
    return handled;
}
void FormulaLineEdit::refreshFormulaHelp() {
    auto* popup=state_->popup;popup->clear();
    state_->call=callContext(text(),cursorPosition());
    if(!state_->functions.contains(state_->call.name)) {
        const auto function=state_->workbookFunctions.constFind(state_->call.name);
        if(state_->builtinNames.contains(state_->call.name)||function==state_->workbookFunctions.cend())state_->call={};else state_->call.name=function.value();
    }
    if(state_->dismissed&&text()!=state_->dismissedText)state_->dismissed=false;
    state_->span=completionSpan(text(),cursorPosition());
    if(state_->dismissed){popup->hide();state_->help->clear();state_->help->hide();return;}
    if(!hasFocus()||isReadOnly()||!isEnabled()||state_->span.start<0){popup->hide();updateHelpPresentation();return;}
    const auto prefix=state_->span.prefix;
    QStringList matches;state_->completionNotice.clear();
    for(const auto& name:state_->functions)if(name.startsWith(prefix,Qt::CaseInsensitive))matches<<name;
    for(const auto& name:state_->workbookNames)if(!state_->builtinNames.contains(name.toUpper())&&name.startsWith(prefix,Qt::CaseInsensitive))matches<<name;
    for(const auto& name:state_->workbookFunctions)if(!state_->builtinNames.contains(name.toUpper())&&name.startsWith(prefix,Qt::CaseInsensitive))matches<<name;
    matches.sort(Qt::CaseInsensitive);
    if(matches.isEmpty()&&prefix.compare("AVG",Qt::CaseInsensitive)==0) {
        for(const auto& name:state_->functions)if(name.startsWith("AVERAGE"))matches<<name;
        if(!matches.isEmpty())state_->completionNotice="AVG is not an Excel function. Choose an available canonical AVERAGE function.";
    }
    for(const auto& name:matches) {
        const auto kind=state_->functions.contains(name.toUpper())?BuiltinFunction:state_->workbookFunctions.contains(name.toUpper())?WorkbookFunction:DefinedName;
        auto caption=name;if(kind==DefinedName)caption+=" — Defined name";else if(kind==WorkbookFunction)caption+=" — Workbook function";
        if(!state_->completionNotice.isEmpty())caption+=" — canonical suggestion for AVG";
        auto* item=new QListWidgetItem(caption,popup);item->setData(Qt::UserRole,name);
        item->setData(Qt::UserRole+1,kind);
        auto tip=kind==DefinedName?definedNameHelp(name):kind==WorkbookFunction?workbookFunctionHelp(name):renderHelp(name,-1,state_->categories.value(name));
        if(!state_->completionNotice.isEmpty())tip+="<br>"+state_->completionNotice.toHtmlEscaped();
        item->setToolTip(tip);item->setData(Qt::AccessibleTextRole,caption);
    }
    if(!popup->count()){popup->hide();updateHelpPresentation();return;}
    const auto available=availableScreenGeometry(this);
    const int rowHeight=qMax(popup->fontMetrics().height()+4,popup->sizeHintForRow(0));
    popup->setCurrentRow(0);popup->resize(compactPopupWidth(popup,available),qMin(available.height(),qMin(8,popup->count())*rowHeight+2*popup->frameWidth()));
    popup->move(boundedOrigin(available,popup->size(),mapToGlobal(QPoint(0,height()+4))));if(!popup->isVisible())popup->show();
    updateHelpPresentation();
}
void FormulaLineEdit::updateHelpPresentation() {
    QString name=state_->call.name;int argument=state_->call.argument;
    auto kind=!state_->functions.contains(name.toUpper())&&state_->workbookFunctions.contains(name.toUpper())?WorkbookFunction:BuiltinFunction;
    if(state_->popup->isVisible()&&state_->popup->currentItem()) {
        name=state_->popup->currentItem()->data(Qt::UserRole).toString();argument=-1;
        kind=static_cast<CompletionKind>(state_->popup->currentItem()->data(Qt::UserRole+1).toInt());
    }
    if(state_->dismissed||!hasFocus()||isReadOnly()||!isEnabled()||name.isEmpty()){state_->help->clear();state_->help->hide();return;}
    auto rendered=kind==DefinedName?definedNameHelp(name):kind==WorkbookFunction?workbookFunctionHelp(name):renderHelp(name,argument,state_->categories.value(name));
    if(state_->popup->isVisible()&&!state_->completionNotice.isEmpty())rendered+="<br>"+state_->completionNotice.toHtmlEscaped();
    state_->help->setText(rendered);
    state_->help->setAccessibleDescription(name+" argument "+QString::number(argument+1));
    const auto available=availableScreenGeometry(this);
    const int helpWidth=qMin(available.width(),qBound(320,state_->help->sizeHint().width(),680));
    state_->help->setFixedWidth(helpWidth);
    const int helpHeight=state_->help->heightForWidth(helpWidth);
    state_->help->resize(helpWidth,qBound(1,helpHeight>0?helpHeight:state_->help->sizeHint().height(),available.height()));
    state_->help->move(boundedOrigin(available,state_->help->size(),mapToGlobal(QPoint(0,height()+4))));
    if(!state_->help->isVisible())state_->help->show();
    positionHelpWindows();
}
void FormulaLineEdit::positionHelpWindows() {
    auto* popup=state_->popup;auto* help=state_->help;
    const bool listVisible=popup->isVisible();bool helpVisible=help->isVisible();
    if(!listVisible&&!helpVisible)return;
    const auto available=availableScreenGeometry(this);
    const QSize listFrame(qMax(0,popup->frameGeometry().width()-popup->width()),qMax(0,popup->frameGeometry().height()-popup->height()));
    const QSize helpFrame(qMax(0,help->frameGeometry().width()-help->width()),qMax(0,help->frameGeometry().height()-help->height()));
    // On a screen too small for two windows, retain the selectable list.
    if(listVisible&&helpVisible&&available.height()<3){help->hide();helpVisible=false;}
    const int gap=listVisible&&helpVisible?qMin(4,available.height()-2):0;
    int listHeight=listVisible?qMin(popup->frameGeometry().height(),available.height()):0;
    int helpHeight=helpVisible?qMin(help->frameGeometry().height(),available.height()):0;
    if(listVisible&&helpVisible&&listHeight+gap+helpHeight>available.height()) {
        listHeight=qMax(1,qMin(listHeight,available.height()/2));
        helpHeight=qMax(1,available.height()-listHeight-gap);
    }
    if(listVisible)popup->resize(qMax(1,qMin(popup->width(),available.width()-listFrame.width())),qMax(1,listHeight-listFrame.height()));
    if(helpVisible) {
        help->setFixedWidth(qMax(1,qMin(help->width(),available.width()-helpFrame.width())));
        help->resize(help->width(),qMax(1,helpHeight-helpFrame.height()));
    }
    const int blockWidth=qMax(listVisible?popup->frameGeometry().width():0,helpVisible?help->frameGeometry().width():0);
    const int blockHeight=listHeight+gap+helpHeight;
    const auto editorTop=mapToGlobal(QPoint(0,0));
    int top=editorTop.y()+height()+4;
    if(top+blockHeight>available.bottom()+1&&editorTop.y()-4-blockHeight>=available.top())top=editorTop.y()-4-blockHeight;
    const auto origin=boundedOrigin(available,{blockWidth,blockHeight},{editorTop.x(),top});
    if(listVisible)popup->move(origin);
    if(helpVisible)help->move(origin+QPoint(0,listVisible?listHeight+gap:0));
}
} // namespace tigersheets

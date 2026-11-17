#include "tigersheets/formula_editor.hpp"
#include "tigersheets/formula_reference_presentation.hpp"
#include "tigersheets/font_presentation.hpp"
#include <QListWidget>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTextBoundaryFinder>
#include <QTimer>
#include <QLabel>
#include <QMap>
#include <QGuiApplication>
#include <QApplication>
#include <QScreen>
#include <QStyle>
#include <QRegularExpression>
#include <QSet>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QStyleOptionFrame>
#include <QStyledItemDelegate>
#include <QTextLayout>
#include <cmath>
#include <algorithm>

namespace tigersheets {
namespace {
QRect availableScreenGeometry(const QWidget* editor) {
    for(auto* screen:{editor->screen(),QGuiApplication::primaryScreen()})
        if(screen&&screen->availableGeometry().isValid())return screen->availableGeometry();
    return {0,0,1024,768};
}
int compactPopupWidth(const QListWidget* popup,const QRect& available) {
    const auto prefix=popup->property("formula.completionPrefix").toString();
    bool hasPrefix=false;for(int i=0;i<popup->count();++i)hasPrefix|=popup->item(i)->data(Qt::UserRole).toString().startsWith(prefix,Qt::CaseInsensitive);
    const int padding=popup->iconSize().width()+20+2*popup->frameWidth()+(popup->count()>12?popup->style()->pixelMetric(QStyle::PM_ScrollBarExtent,nullptr,popup):0);
    int desired=80;
    for(int i=0;i<popup->count();++i)if(!hasPrefix||popup->item(i)->data(Qt::UserRole).toString().startsWith(prefix,Qt::CaseInsensitive))desired=qMax(desired,popup->fontMetrics().horizontalAdvance(popup->item(i)->text())+padding);
    return qMin(available.width(),qMin(desired,520));
}
bool softWrapped(const QLineEdit& editor){return editor.property("formula.softWrap").toBool()&&editor.text().startsWith('=')&&editor.echoMode()==QLineEdit::Normal;}
std::unique_ptr<QTextLayout> wrappedLayout(const QLineEdit& editor,bool colored){
    auto layout=std::make_unique<QTextLayout>(editor.text(),editor.font());QTextOption option;option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);option.setTextDirection(editor.layoutDirection());layout->setTextOption(option);
    if(colored){const auto utf8=editor.text().toUtf8();const std::string_view formula(utf8.constData(),static_cast<std::size_t>(utf8.size()));const auto references=formulaReferences(formula);const auto colors=formulaReferenceColorIndexes(formula,references);QList<QTextLayout::FormatRange> formats;for(std::size_t i=0;i<references.size();++i){const auto& reference=references[i];if(reference.external)continue;QTextLayout::FormatRange range;range.start=QString::fromUtf8(utf8.constData(),reference.start).size();range.length=QString::fromUtf8(utf8.constData()+reference.start,reference.length).size();range.format.setForeground(formulaReferenceColor(colors[i]));formats.push_back(range);}layout->setFormats(formats);}
    const int pitch=editor.property("formula.linePitch").toInt();layout->beginLayout();for(int row=0;;++row){auto line=layout->createLine();if(!line.isValid())break;line.setLineWidth(std::max(1,editor.width()-4));line.setPosition(QPointF(0,row*pitch));}layout->endLayout();return layout;
}
QPointF wrappedOrigin(const QLineEdit& editor,const QTextLayout& layout){const int pitch=std::max(1,editor.property("formula.linePitch").toInt()),visible=std::max(1,editor.height()/pitch);const auto caret=layout.lineForTextPosition(editor.cursorPosition());const int first=caret.isValid()?std::max(0,caret.lineNumber()-visible+1):0;return QPointF(2,(pitch-editor.fontMetrics().height())/2-first*pitch);}
int wrappedCursorAt(const QLineEdit& editor,const QPointF& point){const auto layout=wrappedLayout(editor,false);if(!layout->lineCount())return 0;const auto local=point-wrappedOrigin(editor,*layout);const int row=std::clamp(int(std::floor(local.y()/std::max(1,editor.property("formula.linePitch").toInt()))),0,layout->lineCount()-1);return layout->lineAt(row).xToCursor(local.x());}
QPoint boundedOrigin(const QRect& available,const QSize& size,const QPoint& preferred) {
    return {qBound(available.left(),preferred.x(),qMax(available.left(),available.right()-size.width()+1)),
            qBound(available.top(),preferred.y(),qMax(available.top(),available.bottom()-size.height()+1))};
}
struct CompletionSpan {int start=-1,end=-1;QString prefix;};
struct CallContext {QString name;int argument=-1;};
struct FunctionHelp {QStringList parameters;QString description;};
enum CompletionKind {BuiltinFunction,DefinedName,WorkbookFunction};
QIcon functionSuggestionIcon() {
    static const auto icon=[] {
        QPixmap pixels(16,16);pixels.fill(Qt::transparent);QPainter painter(&pixels);painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor("#707070"),1));painter.drawEllipse(QRectF(1,1,14,14));painter.translate(2,2);painter.scale(.75,.75);
        painter.setPen(QPen(QColor("#606060"),1.4,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        QPainterPath function;function.moveTo(9,2.5);function.cubicTo(6,1,5.5,4.5,5,8);function.lineTo(4.2,12);function.cubicTo(3.7,14.4,2.4,14.8,1.5,13.6);painter.drawPath(function);painter.drawLine(QPointF(3.2,6),QPointF(8,6));
        QPainterPath variable;variable.moveTo(9,7.5);variable.cubicTo(10.5,6.8,11.1,8,11.8,10);variable.cubicTo(12.5,12,13,12.5,14.4,11.6);variable.moveTo(14,7.3);variable.cubicTo(12.5,7.4,11.5,11.7,9.3,12.1);painter.drawPath(variable);
        painter.end();return QIcon(pixels);
    }();
    return icon;
}
class FunctionSuggestionDelegate final:public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter,const QStyleOptionViewItem& option,const QModelIndex& index)const override {
        QStyledItemDelegate::paint(painter,option,index);
        QStyleOptionViewItem item(option);initStyleOption(&item,index);
        if(!(item.state&QStyle::State_Selected)||item.icon.isNull())return;
        const auto* style=item.widget?item.widget->style():QApplication::style();const auto decoration=style->subElementRect(QStyle::SE_ItemViewItemDecoration,&item,item.widget);
        if(!decoration.isValid())return;
        auto column=item.rect;column.setRight(decoration.right()+2);painter->save();painter->fillRect(column,Qt::white);item.icon.paint(painter,decoration,Qt::AlignCenter,QIcon::Normal);painter->restore();
    }
};
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
        {"SUM",{{"number1","[number2]","…"},"Adds all the numbers in a range of cells"}},
        {"AVERAGE",{{"number1","[number2]","…"},"Returns the arithmetic mean of numeric values."}},
        {"F.DIST",{{"x","deg_freedom1","deg_freedom2","cumulative"},"Returns the (left-tailed) F probability distribution (degree of diversity) for two data sets."}},
        {"FIELDVALUE",{{"value","field_name"},"Ordinary cells return #FIELD!. Linked data types are not supported."}},
        {"SUMIF",{{"range","criteria","[sum_range]"},"Adds values whose corresponding cells meet one condition."}},
        {"SUMIFS",{{"sum_range","criteria_range1","criteria1","[criteria_range2]","[criteria2]","…"},"Adds values that meet every supplied condition."}},
        {"AVERAGEIF",{{"range","criteria","[average_range]"},"Averages values that meet one condition."}},
        {"AVERAGEIFS",{{"average_range","criteria_range1","criteria1","[criteria_range2]","[criteria2]","…"},"Averages values that meet every supplied condition."}},
        {"COUNT",{{"value1","[value2]","…"},"Counts numeric values."}},
        {"COUNTA",{{"value1","[value2]","…"},"Counts nonempty values."}},
        {"COUNTIF",{{"range","criteria"},"Counts cells that meet a condition."}},
        {"COUNTIFS",{{"criteria_range1","criteria1","[criteria_range2]","[criteria2]","…"},"Counts cells that meet all supplied conditions."}},
        {"IF",{{"logical_test","[value_if_true]","[value_if_false]"},"Chooses a result according to a logical test."}},
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
        {"ABS",{{"number"},"Returns the absolute value of a number, a number without its sign"}},
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
QString renderDescription(const QString& name,const QString& category) {
    const auto item=commonHelp().constFind(name);
    return item==commonHelp().cend()?category+" function.":item->description;
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
    int wrappedAnchor=-1;
    QTimer* wrappedCaret{};
    bool caretVisible=true;
    QPoint visibleAssistanceOrigin;
    int visibleAssistanceHeight=0;
};

FormulaLineEdit::FormulaLineEdit(QWidget* parent):QLineEdit(parent),state_(std::make_unique<State>()) {
    setProperty("formula.supportsSoftWrap",true);
    state_->popup=new QListWidget(this);
    state_->popup->setWindowFlags(Qt::ToolTip);
    state_->popup->setAttribute(Qt::WA_ShowWithoutActivating);
    state_->popup->setFocusPolicy(Qt::NoFocus);
    state_->popup->setIconSize(QSize(16,16));state_->popup->setItemDelegate(new FunctionSuggestionDelegate(state_->popup));
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
    state_->wrappedCaret=new QTimer(this);connect(state_->wrappedCaret,&QTimer::timeout,this,[this]{if(!hasFocus()||!softWrapped(*this)){state_->wrappedCaret->stop();return;}state_->caretVisible=!state_->caretVisible;update();});
    connect(state_->popup,&QListWidget::currentItemChanged,this,[this]{updateHelpPresentation();});
    connect(state_->popup,&QListWidget::itemDoubleClicked,this,[this]{acceptCompletion();});
    connect(this,&QLineEdit::textChanged,this,[this]{refreshFormulaHelp();resetWrappedCaret();});
    connect(this,&QLineEdit::cursorPositionChanged,this,[this]{refreshFormulaHelp();resetWrappedCaret();});
}
FormulaLineEdit::~FormulaLineEdit()=default;
void FormulaLineEdit::resetWrappedCaret(){state_->caretVisible=true;if(!state_->wrappedCaret)return;state_->wrappedCaret->stop();if(hasFocus()&&!isReadOnly()&&softWrapped(*this)&&QApplication::cursorFlashTime()>0)state_->wrappedCaret->start(std::max(1,QApplication::cursorFlashTime()/2));update();}
void FormulaLineEdit::paintEvent(QPaintEvent* event){
    QLineEdit::paintEvent(event);
    if(softWrapped(*this)){
        const auto layout=wrappedLayout(*this,hasFocus()&&!isReadOnly()&&isEnabled());QStyleOptionFrame frame;initStyleOption(&frame);const auto contents=style()->subElementRect(QStyle::SE_LineEditContents,&frame,this).marginsRemoved(textMargins());QPainter painter(this);painter.setClipRect(contents);painter.fillRect(contents,palette().brush(QPalette::Base));painter.setPen(palette().color(QPalette::Text));QList<QTextLayout::FormatRange> selections;if(hasSelectedText()){QTextLayout::FormatRange selection;selection.start=selectionStart();selection.length=selectedText().size();selection.format.setBackground(palette().brush(QPalette::Highlight));selection.format.setForeground(palette().brush(QPalette::HighlightedText));selections.push_back(selection);}const auto origin=wrappedOrigin(*this,*layout);layout->draw(&painter,origin,selections);if(hasFocus()&&!isReadOnly()&&!hasSelectedText()&&state_->caretVisible)layout->drawCursor(&painter,origin,cursorPosition(),1);return;
    }
    if(!hasFocus()||isReadOnly()||!isEnabled()||echoMode()!=Normal||!text().startsWith('='))return;
    const auto utf8=text().toUtf8();const std::string_view formula(utf8.constData(),static_cast<std::size_t>(utf8.size()));const auto references=formulaReferences(formula);if(references.empty())return;
    const auto colors=formulaReferenceColorIndexes(formula,references);
    QTextLayout layout(text(),font());QTextOption option;option.setWrapMode(QTextOption::NoWrap);option.setTextDirection(layoutDirection());layout.setTextOption(option);
    QList<QTextLayout::FormatRange> formats;for(std::size_t i=0;i<references.size();++i){const auto& reference=references[i];if(reference.external)continue;QTextLayout::FormatRange range;range.start=QString::fromUtf8(utf8.constData(),reference.start).size();range.length=QString::fromUtf8(utf8.constData()+reference.start,reference.length).size();range.format.setForeground(formulaReferenceColor(colors[i]));formats.push_back(range);}layout.setFormats(formats);layout.beginLayout();auto line=layout.createLine();line.setLineWidth(100000000);layout.endLayout();if(!line.isValid())return;
    // Keep QLineEdit's native frame, selection, scrolling and blinking caret.
    // Only repaint reference glyphs, using its current caret as the text origin.
    const auto caret=cursorRect();const qreal cursorX=caret.left()+5;const QPointF origin(cursorX-line.cursorToX(cursorPosition()),caret.top());QRegion colored;
    for(const auto& range:formats){const auto x1=origin.x()+line.cursorToX(range.start),x2=origin.x()+line.cursorToX(range.start+range.length);colored+=QRect(static_cast<int>(std::floor(std::min(x1,x2))),caret.top(),static_cast<int>(std::ceil(std::abs(x2-x1)))+1,caret.height());}
    QStyleOptionFrame frame;initStyleOption(&frame);colored&=style()->subElementRect(QStyle::SE_LineEditContents,&frame,this).marginsRemoved(textMargins());
    if(hasSelectedText()){const auto x1=origin.x()+line.cursorToX(selectionStart()),x2=origin.x()+line.cursorToX(selectionStart()+selectedText().size());colored-=QRect(static_cast<int>(std::floor(std::min(x1,x2))),caret.top(),static_cast<int>(std::ceil(std::abs(x2-x1)))+1,caret.height());}
    colored-=QRect(static_cast<int>(cursorX),caret.top(),1,caret.height());if(colored.isEmpty())return;
    QPainter painter(this);painter.setClipRegion(colored);painter.fillRect(rect(),palette().brush(QPalette::Base));painter.setPen(palette().color(QPalette::Text));layout.draw(&painter,origin);
}
void FormulaLineEdit::mousePressEvent(QMouseEvent* event){if(!softWrapped(*this)||event->button()!=Qt::LeftButton){QLineEdit::mousePressEvent(event);return;}const int cursor=wrappedCursorAt(*this,event->position());if(event->modifiers()&Qt::ShiftModifier){state_->wrappedAnchor=hasSelectedText()?(cursorPosition()==selectionStart()?selectionStart()+selectedText().size():selectionStart()):cursorPosition();setSelection(state_->wrappedAnchor,cursor-state_->wrappedAnchor);}else{state_->wrappedAnchor=cursor;setCursorPosition(cursor);}setFocus(Qt::MouseFocusReason);event->accept();update();}
void FormulaLineEdit::mouseMoveEvent(QMouseEvent* event){if(!softWrapped(*this)||!(event->buttons()&Qt::LeftButton)||state_->wrappedAnchor<0){QLineEdit::mouseMoveEvent(event);return;}setSelection(state_->wrappedAnchor,wrappedCursorAt(*this,event->position())-state_->wrappedAnchor);event->accept();update();}
void FormulaLineEdit::mouseReleaseEvent(QMouseEvent* event){if(softWrapped(*this)&&event->button()==Qt::LeftButton&&state_->wrappedAnchor>=0){state_->wrappedAnchor=-1;event->accept();return;}QLineEdit::mouseReleaseEvent(event);}
void FormulaLineEdit::mouseDoubleClickEvent(QMouseEvent* event){if(!softWrapped(*this)||event->button()!=Qt::LeftButton){QLineEdit::mouseDoubleClickEvent(event);return;}const int cursor=wrappedCursorAt(*this,event->position());QTextBoundaryFinder words(QTextBoundaryFinder::Word,text());words.setPosition(cursor);const int start=words.isAtBoundary()?cursor:std::max(0,int(words.toPreviousBoundary()));words.setPosition(cursor);const int end=words.toNextBoundary();setSelection(start,std::max(0,(end<0?int(text().size()):end)-start));state_->wrappedAnchor=start;event->accept();update();}
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
QString FormulaLineEdit::argumentHelpText()const{return state_->help->text().isEmpty()?QString():state_->help->property("formula.fullHelpText").toString();}
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
        if(state_->wrappedCaret)state_->wrappedCaret->stop();
    }
    const bool handled=QLineEdit::event(event);
    if(type==QEvent::FocusIn||type==QEvent::ReadOnlyChange||type==QEvent::EnabledChange)refreshFormulaHelp();
    if(type==QEvent::Move||type==QEvent::Resize||type==QEvent::Show||type==QEvent::FocusIn){if(isVisible()&&!visibleRegion().isEmpty()){state_->visibleAssistanceOrigin=mapTo(window(),QPoint());state_->visibleAssistanceHeight=height();}positionHelpWindows();}
    if(type==QEvent::FocusIn||type==QEvent::Resize||type==QEvent::DynamicPropertyChange)resetWrappedCaret();
    return handled;
}
void FormulaLineEdit::refreshFormulaHelp() {
    if(isVisible()&&!visibleRegion().isEmpty()){state_->visibleAssistanceOrigin=mapTo(window(),QPoint());state_->visibleAssistanceHeight=height();}
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
    for(const auto& name:state_->functions)if(name.contains(prefix,Qt::CaseInsensitive))matches<<name;
    for(const auto& name:state_->workbookNames)if(!state_->builtinNames.contains(name.toUpper())&&name.contains(prefix,Qt::CaseInsensitive))matches<<name;
    for(const auto& name:state_->workbookFunctions)if(!state_->builtinNames.contains(name.toUpper())&&name.contains(prefix,Qt::CaseInsensitive))matches<<name;
    std::sort(matches.begin(),matches.end(),[&](const QString& left,const QString& right){
        const bool leftPrefix=left.startsWith(prefix,Qt::CaseInsensitive),rightPrefix=right.startsWith(prefix,Qt::CaseInsensitive);
        if(leftPrefix!=rightPrefix)return leftPrefix;
        const bool leftCompatibility=state_->categories.value(left).compare("Compatibility",Qt::CaseInsensitive)==0,rightCompatibility=state_->categories.value(right).compare("Compatibility",Qt::CaseInsensitive)==0;
        return leftCompatibility!=rightCompatibility?!leftCompatibility:left.compare(right,Qt::CaseInsensitive)<0;
    });
    if(prefix.compare("AVG",Qt::CaseInsensitive)==0&&std::none_of(matches.begin(),matches.end(),[&](const QString& name){return name.startsWith(prefix,Qt::CaseInsensitive);})) {
        QStringList canonical;for(const auto& name:state_->functions)if(name.startsWith("AVERAGE"))canonical<<name;
        if(!canonical.isEmpty()){matches=canonical;state_->completionNotice="AVG is not an Excel function. Choose an available canonical AVERAGE function.";}
    }
    QFont completionFont=worksheetPresentationFont(QFont("Calibri"));completionFont.setPixelSize(12);popup->setFont(completionFont);
    const int rowHeight=qMax(17,popup->fontMetrics().height());popup->setProperty("formula.completionPrefix",prefix);
    for(const auto& name:matches) {
        const auto kind=state_->functions.contains(name.toUpper())?BuiltinFunction:state_->workbookFunctions.contains(name.toUpper())?WorkbookFunction:DefinedName;
        auto caption=name;if(kind==DefinedName)caption+=" — Defined name";else if(kind==WorkbookFunction)caption+=" — Workbook function";
        if(!state_->completionNotice.isEmpty())caption+=" — canonical suggestion for AVG";
        auto* item=new QListWidgetItem(caption,popup);if(kind!=DefinedName)item->setIcon(functionSuggestionIcon());item->setData(Qt::UserRole,name);item->setSizeHint(QSize(0,rowHeight));
        item->setData(Qt::UserRole+1,kind);
        auto tip=kind==DefinedName?definedNameHelp(name):kind==WorkbookFunction?workbookFunctionHelp(name):renderHelp(name,-1,state_->categories.value(name));
        if(!state_->completionNotice.isEmpty())tip+="<br>"+state_->completionNotice.toHtmlEscaped();
        item->setToolTip(tip);item->setData(Qt::AccessibleTextRole,caption);
    }
    if(!popup->count()){popup->hide();updateHelpPresentation();return;}
    const auto available=availableScreenGeometry(this);
    const auto exact=std::find_if(matches.begin(),matches.end(),[&](const QString& name){return name.compare(prefix,Qt::CaseInsensitive)==0;});
    popup->setCurrentRow(exact==matches.end()?0:static_cast<int>(std::distance(matches.begin(),exact)));popup->resize(compactPopupWidth(popup,available),qMin(available.height(),qMin(12,popup->count())*rowHeight+2*popup->frameWidth()));
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
    const bool completion=state_->popup->isVisible();
    const bool compact=!completion&&kind==BuiltinFunction&&commonHelp().contains(name);
    state_->help->setProperty("formula.compactCall",compact);state_->help->setProperty("formula.fullHelpText",rendered);state_->help->setWordWrap(!compact&&!completion);state_->help->setMargin(compact||completion?1:8);
    if(compact||completion){QFont font=completion?state_->popup->font():QFont("Calibri");font.setPixelSize(12);state_->help->setFont(font);state_->help->setStyleSheet("QLabel {background:white; color:#303030; border:1px solid #b8b8b8; padding:0px;}");}else{state_->help->setFont(QApplication::font());state_->help->setStyleSheet(QString());}
    auto visible=compact?rendered.section("<br>",0,0).replace(QChar(0x2026),"..."):rendered;
    if(completion){visible=kind==BuiltinFunction?renderDescription(name,state_->categories.value(name)).toHtmlEscaped():rendered.section("<br>",1);if(!state_->completionNotice.isEmpty())visible+="<br>"+state_->completionNotice.toHtmlEscaped();}
    state_->help->setText(visible);
    state_->help->setAccessibleDescription(name+" argument "+QString::number(argument+1));
    const auto available=availableScreenGeometry(this);
    state_->help->setMinimumWidth(0);state_->help->setMaximumWidth(QWIDGETSIZE_MAX);
    const int helpWidth=qMin(available.width(),compact||completion?state_->help->sizeHint().width():qBound(320,state_->help->sizeHint().width(),680));
    state_->help->setFixedWidth(helpWidth);
    const int helpHeight=compact?state_->help->fontMetrics().height()+4:completion?state_->help->sizeHint().height():state_->help->heightForWidth(helpWidth);
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
    if(isVisible()&&!visibleRegion().isEmpty()){state_->visibleAssistanceOrigin=mapTo(window(),QPoint());state_->visibleAssistanceHeight=height();}
    const auto assistanceBase=state_->visibleAssistanceHeight>0?window()->mapToGlobal(state_->visibleAssistanceOrigin):mapToGlobal(QPoint());
    const int assistanceHeight=state_->visibleAssistanceHeight>0?state_->visibleAssistanceHeight:height();
    const QSize listFrame(qMax(0,popup->frameGeometry().width()-popup->width()),qMax(0,popup->frameGeometry().height()-popup->height()));
    const QSize helpFrame(qMax(0,help->frameGeometry().width()-help->width()),qMax(0,help->frameGeometry().height()-help->height()));
    if(listVisible) {
        popup->resize(qMax(1,qMin(popup->width(),available.width()-listFrame.width())),qMax(1,qMin(popup->height(),available.height()-listFrame.height())));
        const int gap=2;
        int tokenOffset=0;
        if(state_->span.start>=0){
            if(softWrapped(*this)){const auto layout=wrappedLayout(*this,false);const auto line=layout->lineForTextPosition(state_->span.start);if(line.isValid())tokenOffset=qRound(wrappedOrigin(*this,*layout).x()+line.cursorToX(state_->span.start));}
            else{QTextLayout layout(text(),font());layout.beginLayout();auto line=layout.createLine();line.setLineWidth(100000000);layout.endLayout();if(line.isValid())tokenOffset=qRound(cursorRect().left()+5-line.cursorToX(cursorPosition())+line.cursorToX(state_->span.start));}
        }
        const auto editorTop=assistanceBase+QPoint(tokenOffset,0);int top=editorTop.y()+assistanceHeight+4;
        const auto listSize=popup->frameGeometry().size();
        if(top+listSize.height()>available.bottom()+1&&editorTop.y()-4-listSize.height()>=available.top())top=editorTop.y()-4-listSize.height();
        const auto origin=boundedOrigin(available,listSize,QPoint(editorTop.x(),top));popup->move(origin);
        const int remaining=available.right()-popup->frameGeometry().right()-gap-helpFrame.width();
        if(helpVisible&&remaining<1){help->hide();helpVisible=false;}
        if(helpVisible){help->setFixedWidth(qMax(1,qMin(help->width(),remaining)));help->resize(help->width(),qMax(1,qMin(help->height(),available.height()-helpFrame.height())));}
        if(helpVisible)help->move(origin+QPoint(popup->frameGeometry().width()+gap,0));
        return;
    }
    help->setFixedWidth(qMax(1,qMin(help->width(),available.width()-helpFrame.width())));
    help->resize(help->width(),qMax(1,qMin(help->height(),available.height()-helpFrame.height())));
    const auto block=help->frameGeometry().size();
    const auto editorTop=assistanceBase;
    const bool compact=help->property("formula.compactCall").toBool();const int editorGap=compact?1:4;
    int top=editorTop.y()+assistanceHeight+editorGap;
    if(top+block.height()>available.bottom()+1&&editorTop.y()-editorGap-block.height()>=available.top())top=editorTop.y()-editorGap-block.height();
    const auto clientOffset=compact?help->mapToGlobal(QPoint())-help->frameGeometry().topLeft():QPoint();
    help->move(boundedOrigin(available,block,QPoint(editorTop.x()+(compact?8:0),top)-clientOffset));
}
} // namespace tigersheets

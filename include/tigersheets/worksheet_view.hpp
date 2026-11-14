#pragma once
#include <QTableView>
#include <QPointer>
#include <QPersistentModelIndex>
#include <functional>
#include <vector>
class QLineEdit;
class QKeyEvent;
class QTimer;
namespace tigersheets {
class WorksheetView : public QTableView {
public:
 explicit WorksheetView(QWidget* parent=nullptr);
 ~WorksheetView() override;
 void setModel(QAbstractItemModel* model) override;
 void setFormulaBar(QLineEdit* editor);
 void setCurrentSheetName(const QString& name);
 QLineEdit* activeCellEditor() const;
 bool commitPendingEditor(bool force=false);
 void cancelEntry();
 void setCopyRange(const QString& sheet,const QItemSelectionRange& range);
 void clearCopyRange();
 bool handleEditorKey(QLineEdit* editor,QKeyEvent* key);
 QLineEdit* createCellEditor(QWidget* parent,const QModelIndex& index);
 std::function<QLineEdit*(QWidget*)> editorFactory;
 std::function<QString(QModelIndex,QModelIndex)> pointedReference;
 std::function<bool(QLineEdit*,QKeyEvent*)> beforeEditorKey;
protected:
 void currentChanged(const QModelIndex& current,const QModelIndex& previous) override;
 void selectionChanged(const QItemSelection& selected,const QItemSelection& deselected) override;
 void resizeEvent(QResizeEvent* event) override;
 void scrollContentsBy(int dx,int dy) override;
 void paintEvent(QPaintEvent* event) override;
 void keyPressEvent(QKeyEvent* event) override;
 bool eventFilter(QObject* object,QEvent* event) override;
 void mousePressEvent(QMouseEvent* event) override;
 void mouseMoveEvent(QMouseEvent* event) override;
 void mouseReleaseEvent(QMouseEvent* event) override;
private:
 QString currentSheetName_;
 QWidget* copyBorder_=nullptr;
 QString copySheet_;
 QItemSelectionRange copyRange_;
 void updateCopyBorder();
 QString layoutSheet_,mergedDefinition_;
 QMetaObject::Connection layoutChanged_;
 std::vector<int> layoutRows_,layoutColumns_;
 void synchronizeLayout();
 QPointer<QLineEdit> formulaBar_,cellEditor_;
 QPersistentModelIndex editIndex_;
 QPointer<QLineEdit> referenceEditor_;
 QModelIndex referenceAnchor_,referenceCursor_;
 int referenceStart_=-1,referenceLength_=0;
 bool picking_=false,insertingReference_=false,pointMode_=true;
 QPersistentModelIndex fillSource_,fillTarget_;
 QTimer* fillScrollTimer_=nullptr;
 QPoint fillPosition_;
 bool filling_=false;
 QRect fillHandleRect() const;
 void cancelFill();
 void updateFillTarget(const QPoint& position);
 void scrollFillViewport();
 void finishFill();
 void persistAutoFitColumn(int column);
 QLineEdit* referenceCandidate() const;
 bool canPoint(QLineEdit* editor) const;
 void insertPointedRange(const QModelIndex& end);
};
}

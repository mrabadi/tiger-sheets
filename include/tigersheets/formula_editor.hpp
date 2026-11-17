#pragma once

#include <QLineEdit>
#include <QStringList>
#include <nlohmann/json.hpp>
#include <memory>

class QKeyEvent;
class QMouseEvent;
class QListWidget;

namespace tigersheets {
using Json=nlohmann::json;

// Uses ordinary QLineEdit signals and editing semantics; no workbook mutation.
class FormulaLineEdit : public QLineEdit {
public:
    explicit FormulaLineEdit(QWidget* parent=nullptr);
    ~FormulaLineEdit() override;
    void setFunctionCatalog(const Json& frozenCatalog,const Json& engineAvailability);
    void setWorkbookCompletions(const QStringList& names,const QStringList& functions={});
    QListWidget* completionPopup() const;
    QString argumentHelpText() const;
    QString currentFunction() const;
    // Zero-based argument position, or -1 outside a registered function call.
    int currentArgument() const;
    // A cell delegate must call this before its own Tab/commit handling.
    bool handleFormulaKey(QKeyEvent* event);
    bool acceptCompletion();
protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
private:
    struct State;
    std::unique_ptr<State> state_;
    void refreshFormulaHelp();
    void updateHelpPresentation();
    void positionHelpWindows();
    void resetWrappedCaret();
};
} // namespace tigersheets

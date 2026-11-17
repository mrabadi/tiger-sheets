#pragma once
#include "tigersheets/session.hpp"
#include <QWidget>
#include <QMargins>
#include <QStringList>
#include <vector>
#include <utility>
class QLabel;class QSpinBox;class QDoubleSpinBox;class QLineEdit;class QPlainTextEdit;class QScrollArea;class QToolButton;class QListWidget;
namespace tigersheets::testing {
// Read-only adapter for the actual supplied PDF. The observed hover tool and
// local-text Find autocomplete are adapted, rather than full Acrobat/Word UI.
class NativeDocumentContext:public QWidget {
public:
 explicit NativeDocumentContext(QWidget* parent=nullptr);
 void openPdf(const QString& path);
 Json observation() const;
private:
 QString path_,sha_,lastQuery_;QStringList pages_;std::vector<std::pair<int,int>> matches_;int matchIndex_=-1;int pageCount_=0;
 QSpinBox* page_;QDoubleSpinBox* zoom_;QLineEdit* find_;QLabel* surface_;QLabel* status_;QPlainTextEdit* text_;QScrollArea* scroll_;
 QToolButton* editPdf_;
 QListWidget* suggestions_;
 QWidget* controls_;
 bool presentationView_=false;
 double readerZoom_=100;
 QMargins readerMargins_;
 int readerSpacing_=0,readerFrame_=0;
 void setPresentationView(bool enabled);
 void resizeEvent(QResizeEvent* event) override;
 bool eventFilter(QObject* watched,QEvent* event) override;
 void updateFindSuggestions();
 void render();
 void findNext();
};
}

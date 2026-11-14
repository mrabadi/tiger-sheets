#pragma once
#include <QWidget>
#include <nlohmann/json.hpp>
class QComboBox;
namespace tigersheets {
// The three always-visible Codex choices shared with the OwlDocs suite design.
class CodexControls final : public QWidget {
public:
 explicit CodexControls(QWidget* parent=nullptr);
 void setCatalog(const nlohmann::json& models);
 void setConnected(bool connected);
 QString model() const;
 QString reasoningEffort() const;
 QString serviceTier() const;
private:
 void refreshCapabilities();
 nlohmann::json catalog_=nlohmann::json::array();
 bool connected_=false;
 QComboBox* models_;
 QComboBox* efforts_;
 QComboBox* tiers_;
};
}

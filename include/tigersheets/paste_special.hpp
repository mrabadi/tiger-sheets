#pragma once
#include <optional>
#include <string>
class QWidget;
namespace tigersheets {
struct PasteSpecialOptions {
 std::string mode="all";
 bool skipBlanks=false;
 bool transpose=false;
 std::string operation="none";
};
std::optional<PasteSpecialOptions> choosePasteSpecial(QWidget* parent);
}

#pragma once

#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>

namespace tigersheets {

// This font is a presentation value. Workbook declarations remain unchanged.
inline QFont worksheetPresentationFont(QFont declared) {
    if (declared.family().compare(QStringLiteral("Calibri"), Qt::CaseInsensitive) != 0)
        return declared;
    static const bool calibriInstalled = QFontDatabase::families().contains(QStringLiteral("Calibri"), Qt::CaseInsensitive);
    if (!calibriInstalled) {
        static const auto preferred = QFontInfo(QFont(QStringLiteral("Helvetica"))).family();
        if (!preferred.isEmpty())
            declared.setFamily(preferred);
    }
    return declared;
}

}  // namespace tigersheets

#pragma once

#include <QString>
#include <QStringList>
#include <QTextStream>
#include <QVector>
#include <cmath>

// Strict unquoted numeric CSV. This reader does not interpret coordinates,
// units, simulation time, or protocol fields. Schema labels include units.
namespace NumericCsv {
struct Row {
    Row() : sourceLine(0) {}
    Row(int line, const QStringList& values) : sourceLine(line), fields(values) {}
    int sourceLine;
    QStringList fields;
};
struct Error {
    Error() : sourceLine(0), column(0) {}
    Error(int line, int col, const QString& name, const QString& why)
        : sourceLine(line), column(col), field(name), reason(why) {}
    int sourceLine;
    int column; // One-based; zero means a whole-line error.
    QString field;
    QString reason;
};
struct Result {
    QVector<Row> rows;
    QVector<Error> errors;
    int blankLines = 0;
    int headerLine = 0;
    int physicalLines = 0;
    bool ok() const { return errors.isEmpty(); }
};

inline Result read(QString text, const QStringList& schema,
                   const QVector<int>& integerColumns = QVector<int>())
{
    Result result;
    if (schema.isEmpty()) {
        result.errors.push_back({0, 0, QString(), QStringLiteral("empty schema")});
        return result;
    }
    for (int column : integerColumns) {
        if (column < 0 || column >= schema.size()) {
            result.errors.push_back({0, 0, QString(), QStringLiteral("invalid integer column")});
            return result;
        }
    }
    if (text.startsWith(QChar(0xfeff))) text.remove(0, 1);
    QTextStream stream(&text, QIODevice::ReadOnly);
    QVector<Row> pending;
    bool firstNonBlank = true;
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        ++result.physicalLines;
        if (line.trimmed().isEmpty()) {
            ++result.blankLines;
            continue;
        }
        QStringList fields = line.split(QLatin1Char(','), QString::KeepEmptyParts);
        for (QString& field : fields) field = field.trimmed();
        if (firstNonBlank && fields == schema) {
            result.headerLine = result.physicalLines;
            firstNonBlank = false;
            continue;
        }
        firstNonBlank = false;
        if (fields.size() != schema.size()) {
            result.errors.push_back({result.physicalLines, 0, QString(),
                QStringLiteral("expected %1 columns, got %2").arg(schema.size()).arg(fields.size())});
            continue;
        }
        bool valid = true;
        for (int column = 0; column < fields.size(); ++column) {
            bool converted = false;
            const double value = fields[column].toDouble(&converted);
            QString reason;
            if (!converted || !std::isfinite(value)) {
                reason = QStringLiteral("expected finite number");
            } else if (integerColumns.contains(column)) {
                fields[column].toInt(&converted);
                if (!converted) reason = QStringLiteral("expected 32-bit integer");
            }
            if (!reason.isEmpty()) {
                valid = false;
                result.errors.push_back({result.physicalLines, column + 1, schema[column], reason});
            }
        }
        if (valid) pending.push_back({result.physicalLines, fields});
    }
    if (result.errors.isEmpty() && pending.isEmpty()) {
        result.errors.push_back({0, 0, QString(), QStringLiteral("no numeric data rows")});
    }
    // A malformed file must never turn into a shorter, apparently valid replay.
    if (result.errors.isEmpty()) result.rows.swap(pending);
    return result;
}
}

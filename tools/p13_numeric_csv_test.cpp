#include "../Shared/NumericCsvReader.h"
#include "../DataDrivenTestQT/ReplayCsvSchema.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>
#include <stdexcept>

static int checks = 0;
static void check(bool condition, const char* message)
{
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    try {
        const QStringList schema = {"Time(ms)", "Value(m)", "Flag"};
        const QVector<int> integers = {2};
        const auto parse = [&](const QString& text) { return NumericCsv::read(text, schema, integers); };
        const auto normal = parse("Time(ms),Value(m),Flag\r\n20,1,0\r\n30,2,1\r\n\r\n");
        check(normal.ok() && normal.rows.size() == 2, "header and CRLF");
        check(normal.headerLine == 1 && normal.blankLines == 1, "blank/header counts");
        check(normal.rows.first().sourceLine == 2 && normal.rows.last().sourceLine == 3, "physical source lines");
        check(normal.rows.first().fields[0] == "20" && normal.rows.last().fields[0] == "30", "source time retained");
        const auto headerless = parse("20,1,0\n30,2,1");
        check(headerless.ok() && headerless.rows.size() == 2 && headerless.headerLine == 0, "no discarded first numeric line");
        const auto bom = parse(QString(QChar(0xfeff)) + "\nTime(ms),Value(m),Flag\n20,1,0\n");
        check(bom.ok() && bom.headerLine == 2 && bom.rows.first().sourceLine == 3, "UTF8 BOM and leading blank");
        check(parse(" Time(ms) , Value(m) , Flag \n 20 , 1 , 0 \n").ok(), "whitespace normalization");
        for (const QString& bad : QStringList{"x", "NaN", "Inf", "-inf", "1e9999", "", "1m", "\"1\""}) {
            const auto result = parse("Time(ms),Value(m),Flag\n20,1,0\n30," + bad + ",1\n40,2,1");
            check(!result.ok() && result.rows.isEmpty(), "reject malformed whole file; no partial replay");
            check(result.errors.first().sourceLine == 3 && result.errors.first().column == 2 &&
                  result.errors.first().field == "Value(m)", "precise numeric error location");
        }
        for (const QString& bad : QStringList{"0.5", "2147483648", "1e0"})
            check(!parse("20,1," + bad).ok(), "integer validation before downstream toInt");
        check(!parse("Time(s),Value(m),Flag\n20,1,0").ok(), "bad header units");
        check(!parse("Time(ms),Value(m),Flag\n20,1,0\nTime(ms),Value(m),Flag").ok(), "repeated header is not ignored");
        check(!parse("20,1\n").ok(), "missing column");
        check(!parse("20,1,0,7\n").ok(), "extra column");
        check(!parse("Time(ms),Value(m),Flag\n").ok(), "header-only file");
        check(!parse("\n \n").ok(), "blank-only file");
        check(!NumericCsv::read("1", QStringList()).ok(), "empty schema");
        check(!NumericCsv::read("20,1,0", schema, QVector<int>{3}).ok(), "invalid schema column");
        check(parse("20,1e-6,0").ok(), "scientific notation finite double");
        // Audit the original bytes using the same parser without starting the UI,
        // loading models, sending packets, or deriving scene/query geometry.
        if (application.arguments().size() == 3) {
            const QString path = application.arguments()[1];
            QFile input(path);
            check(input.open(QIODevice::ReadOnly), "open original input");
            const QByteArray bytes = input.readAll();
            const auto result = NumericCsv::read(QString::fromUtf8(bytes), replayCsvSchema(), {17, 28, 29});
            QJsonArray errors, lineage;
            for (const auto& error : result.errors)
                errors.append(QJsonObject{{"line", error.sourceLine}, {"column", error.column},
                                         {"field", error.field}, {"reason", error.reason}});
            double first = 0, last = 0, minStep = 0, maxStep = 0;
            int nonIncreasing = 0;
            for (int i = 0; i < result.rows.size(); ++i) {
                const auto& row = result.rows[i];
                const double value = row.fields[0].toDouble();
                if (i == 0) first = value;
                else {
                    const double step = value - last;
                    if (i == 1 || step < minStep) minStep = step;
                    if (i == 1 || step > maxStep) maxStep = step;
                    if (step <= 0) ++nonIncreasing;
                }
                last = value;
                lineage.append(QJsonObject{{"rowIndex", i}, {"sourceLine", row.sourceLine}, {"sourceTimeMs", value}});
            }
            const QJsonObject audit{
                {"scope", "file integrity and numeric syntax only; NOT a consumption or atmosphere coverage audit"},
                {"path", QFileInfo(input).absoluteFilePath()}, {"sha256", QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex())},
                {"bytes", bytes.size()}, {"columns", replayCsvSchema().size()},
                {"header", QJsonArray::fromStringList(replayCsvSchema())},
                {"physicalLines", result.physicalLines}, {"headerLine", result.headerLine}, {"blankLines", result.blankLines},
                {"validRows", result.rows.size()}, {"errors", errors},
                {"sourceTime", QJsonObject{{"firstMs", first}, {"lastMs", last}, {"minStepMs", minStep},
                                          {"maxStepMs", maxStep}, {"nonIncreasingSteps", nonIncreasing}}},
                {"sourceLineage", lineage}, {"simulationAndVideoTimeVerified", false}};
            QFile output(application.arguments()[2]);
            check(output.open(QIODevice::WriteOnly), "open audit output");
            const QByteArray json = QJsonDocument(audit).toJson();
            check(output.write(json) == json.size(), "write audit output");
            output.close();
            check(result.ok(), "original input numeric syntax");
            std::cout << "original rows=" << result.rows.size() << " bytes=" << bytes.size()
                      << " sourceTimeMs=" << first << ".." << last << "\n";
        }
        std::cout << "PASS checks=" << checks << "\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL after checks=" << checks << ": " << error.what() << "\n";
        return 1;
    }
}

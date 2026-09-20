#include "../HwaSim_IR/HwaSim_IR/IR/IRModtranRadianceLut.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> result;
    std::istringstream stream(line);
    std::string item;
    while (std::getline(stream, item, ',')) result.push_back(item);
    return result;
}

double number(const std::vector<std::string>& fields,
              const std::map<std::string, size_t>& columns,
              const char* name) {
    const auto it = columns.find(name);
    if (it == columns.end() || it->second >= fields.size())
        return std::numeric_limits<double>::quiet_NaN();
    return std::stod(fields[it->second]);
}

std::string text(const std::vector<std::string>& fields,
                 const std::map<std::string, size_t>& columns,
                 const char* name) {
    const auto it = columns.find(name);
    return it == columns.end() || it->second >= fields.size()
        ? std::string() : fields[it->second];
}

IRBand bandFromText(const std::string& value) {
    if (value == "SWIR") return IRBand::ShortWaveInfrared;
    if (value == "MWIR") return IRBand::MidWaveInfrared;
    return IRBand::NearInfrared;
}

bool fiveComponents(const IRModtranRadianceResult& value) {
    return value.valid && std::isfinite(value.tauUp) &&
        value.tauUp >= 0.0 && value.tauUp <= 1.0 &&
        std::isfinite(value.pathThermalWm2SrUm) && value.pathThermalWm2SrUm >= 0.0 &&
        std::isfinite(value.directSolarIrradianceWm2Um) && value.directSolarIrradianceWm2Um > 0.0 &&
        std::isfinite(value.downwardSkyDiffuseIrradianceWm2Um) && value.downwardSkyDiffuseIrradianceWm2Um > 0.0 &&
        std::isfinite(value.pathScatteringRadianceWm2SrUm) && value.pathScatteringRadianceWm2SrUm >= 0.0;
}

IRModtranRadianceQuery makeQuery(IRBand band, double observer, double target,
                                 double range, double visibility, double sza) {
    IRModtranRadianceQuery query;
    query.band = band;
    query.atmosphereModel = "Mid-Latitude Summer";
    query.aerosolModel = "Rural";
    query.observerAltKm = observer;
    query.targetAltKm = target;
    query.rangeKm = range;
    query.visibilityKm = visibility;
    query.solarZenithDeg = sza;
    return query;
}
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: p13_track_query <formal_lut.csv> <input_query_manifest.csv>\n";
        return 2;
    }
    IRModtranRadianceLut lut;
    if (!lut.load(argv[1])) {
        std::cerr << "FAIL formal_lut_load\n";
        return 1;
    }
    std::ifstream input(argv[2]);
    std::string line;
    if (!std::getline(input, line)) return 2;
    const std::vector<std::string> header = split(line);
    std::map<std::string, size_t> columns;
    for (size_t index = 0; index < header.size(); ++index) columns[header[index]] = index;
    size_t total = 0;
    size_t valid = 0;
    size_t failures = 0;
    std::map<std::string, size_t> modes;
    std::map<std::string, size_t> reasons;
    double minTau = std::numeric_limits<double>::infinity();
    double maxTau = 0.0;
    std::string firstFailure;
    while (std::getline(input, line)) {
        if (line.empty()) continue;
        const std::vector<std::string> fields = split(line);
        const IRBand band = bandFromText(text(fields, columns, "band"));
        IRModtranRadianceQuery query = makeQuery(
            band, number(fields, columns, "observerAltKm"),
            number(fields, columns, "targetAltKm"),
            number(fields, columns, "productionPreflightLosKm"),
            number(fields, columns, "visibilityKm"),
            number(fields, columns, "solarZenithDeg"));
        const double humidity = number(fields, columns, "relativeHumidityPercent");
        const IRModtranRadianceResult result = lut.queryRelativeHumidity(query, humidity);
        ++total;
        ++modes[result.interpolationMode];
        ++reasons[result.fallbackReason];
        if (fiveComponents(result)) {
            ++valid;
            minTau = std::min(minTau, result.tauUp);
            maxTau = std::max(maxTau, result.tauUp);
        } else {
            ++failures;
            if (firstFailure.empty()) {
                std::ostringstream detail;
                detail << "row=" << text(fields, columns, "sourceLine")
                       << " band=" << text(fields, columns, "band")
                       << " reason=" << result.fallbackReason
                       << " axis=" << result.fallbackAxis
                       << " value=" << result.fallbackQuery
                       << " bounds=" << result.fallbackMin << ".." << result.fallbackMax;
                firstFailure = detail.str();
            }
        }
    }

    size_t vertexFailures = 0;
    const IRBand bands[] = {IRBand::ShortWaveInfrared, IRBand::MidWaveInfrared};
    const double rhs[] = {30.0, 60.0, 85.0};
    const double observers[] = {10.95, 12.0};
    const double targets[] = {9.7, 10.05};
    const double ranges[] = {2.4, 5.0, 10.0, 20.0, 23.0, 35.0, 50.0};
    const double szas[] = {20.0, 45.0};
    for (IRBand band : bands) for (double rh : rhs)
        for (double observer : observers) for (double target : targets)
            for (double range : ranges) for (double sza : szas) {
                const IRModtranRadianceResult result = lut.queryRelativeHumidity(
                    makeQuery(band, observer, target, range, 6.0, sza), rh);
                if (!fiveComponents(result) || result.interpolationMode.find("exact_profile+exact_match") == std::string::npos)
                    ++vertexFailures;
            }
    size_t boundaryFailures = 0;
    for (IRBand band : bands) {
        IRModtranRadianceResult below = lut.queryRelativeHumidity(
            makeQuery(band, 11.5, 9.9, 2.398, 6.0, 40.0), 76.18);
        IRModtranRadianceResult above = lut.queryRelativeHumidity(
            makeQuery(band, 11.5, 9.9, 50.002, 6.0, 40.0), 76.18);
        if (below.valid || above.valid) ++boundaryFailures;
    }

    std::cout << std::setprecision(12)
              << "SUMMARY loadedEntries=" << lut.entryCount()
              << " queryRows=" << total << " valid=" << valid
              << " failures=" << failures << " vertexFailures=" << vertexFailures
              << " boundaryFailures=" << boundaryFailures
              << " tauMin=" << minTau << " tauMax=" << maxTau << '\n';
    for (const auto& item : modes)
        std::cout << "MODE " << item.first << '=' << item.second << '\n';
    for (const auto& item : reasons)
        std::cout << "REASON " << item.first << '=' << item.second << '\n';
    if (!firstFailure.empty()) std::cout << "FIRST_FAILURE " << firstFailure << '\n';
    return failures == 0 && vertexFailures == 0 && boundaryFailures == 0 ? 0 : 1;
}

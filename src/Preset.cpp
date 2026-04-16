#include "aimp/Preset.h"

#include <charconv>
#include <cstdlib>
#include <fstream>
#include <string>
#include <unordered_map>

namespace aimp {

namespace {

bool ParseUInt(const std::string& value, std::uint32_t& output) {
    const char* begin = value.data();
    const char* end = value.data() + value.size();
    const auto result = std::from_chars(begin, end, output);
    return result.ec == std::errc {} && result.ptr == end;
}

bool ParseDouble(const std::string& value, double& output) {
    char* parseEnd = nullptr;
    output = std::strtod(value.c_str(), &parseEnd);
    return parseEnd != value.c_str() && *parseEnd == '\0';
}

bool ParseBool(const std::string& value, bool& output) {
    if (value == "1" || value == "true") {
        output = true;
        return true;
    }
    if (value == "0" || value == "false") {
        output = false;
        return true;
    }
    return false;
}

std::string FilterToString(PageFilter filter) {
    switch (filter) {
        case PageFilter::All: return "all";
        case PageFilter::EvenOnly: return "even";
        case PageFilter::OddOnly: return "odd";
        default: return "all";
    }
}

PageFilter StringToFilter(const std::string& value) {
    if (value == "even") {
        return PageFilter::EvenOnly;
    }
    if (value == "odd") {
        return PageFilter::OddOnly;
    }
    return PageFilter::All;
}

} // namespace

bool SavePreset(const PlannerPreset& preset, const std::string& path, std::string& errorMessage) {
    std::ofstream out(path);
    if (!out) {
        errorMessage = "Could not open preset file for writing";
        return false;
    }

    out << "sheetWidth=" << preset.sheetSize.widthPoints << '\n';
    out << "sheetHeight=" << preset.sheetSize.heightPoints << '\n';
    out << "columns=" << preset.columns << '\n';
    out << "rows=" << preset.rows << '\n';
    out << "repeatX=" << preset.repeatX << '\n';
    out << "repeatY=" << preset.repeatY << '\n';
    out << "stepX=" << preset.stepX << '\n';
    out << "stepY=" << preset.stepY << '\n';
    out << "slotWidth=" << preset.slotWidth << '\n';
    out << "slotHeight=" << preset.slotHeight << '\n';
    out << "reverse=" << (preset.buildOptions.reverseOrder ? 1 : 0) << '\n';
    out << "filter=" << FilterToString(preset.buildOptions.filter) << '\n';
    out << "padMultiple=" << preset.buildOptions.padToMultiple << '\n';
    out << "pdfSheetNumber=" << (preset.pdfOptions.includeSheetNumber ? 1 : 0) << '\n';
    out << "pdfHeader=" << preset.pdfOptions.headerText << '\n';
    out << "pdfFooter=" << preset.pdfOptions.footerText << '\n';
    out << "pdfIncludeBates=" << (preset.pdfOptions.includeBates ? 1 : 0) << '\n';
    out << "pdfBatesPrefix=" << preset.pdfOptions.batesPrefix << '\n';
    out << "pdfBatesStart=" << preset.pdfOptions.batesStart << '\n';

    if (!out.good()) {
        errorMessage = "Failed while writing preset file";
        return false;
    }
    return true;
}

bool LoadPreset(const std::string& path, PlannerPreset& preset, std::string& errorMessage) {
    std::ifstream in(path);
    if (!in) {
        errorMessage = "Could not open preset file for reading";
        return false;
    }

    std::unordered_map<std::string, std::string> values;
    std::string line;
    while (std::getline(in, line)) {
        const auto pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        values[line.substr(0, pos)] = line.substr(pos + 1);
    }

    ParseDouble(values["sheetWidth"], preset.sheetSize.widthPoints);
    ParseDouble(values["sheetHeight"], preset.sheetSize.heightPoints);
    ParseUInt(values["columns"], preset.columns);
    ParseUInt(values["rows"], preset.rows);
    ParseUInt(values["repeatX"], preset.repeatX);
    ParseUInt(values["repeatY"], preset.repeatY);
    ParseDouble(values["stepX"], preset.stepX);
    ParseDouble(values["stepY"], preset.stepY);
    ParseDouble(values["slotWidth"], preset.slotWidth);
    ParseDouble(values["slotHeight"], preset.slotHeight);
    ParseUInt(values["padMultiple"], preset.buildOptions.padToMultiple);
    preset.buildOptions.filter = StringToFilter(values["filter"]);
    ParseBool(values["reverse"], preset.buildOptions.reverseOrder);
    ParseBool(values["pdfSheetNumber"], preset.pdfOptions.includeSheetNumber);
    preset.pdfOptions.headerText = values["pdfHeader"];
    preset.pdfOptions.footerText = values["pdfFooter"];
    ParseBool(values["pdfIncludeBates"], preset.pdfOptions.includeBates);
    preset.pdfOptions.batesPrefix = values["pdfBatesPrefix"];
    ParseUInt(values["pdfBatesStart"], preset.pdfOptions.batesStart);

    return true;
}

} // namespace aimp

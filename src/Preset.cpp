#include "aimp/Preset.h"

#include <charconv>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

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

std::string TrimAscii(const std::string& value) {
    std::size_t first = 0;
    while (first < value.size() && (value[first] == ' ' || value[first] == '\t' || value[first] == '\r')) {
        ++first;
    }
    std::size_t last = value.size();
    while (last > first && (value[last - 1] == ' ' || value[last - 1] == '\t' || value[last - 1] == '\r')) {
        --last;
    }
    return value.substr(first, last - first);
}

bool RequireKey(const std::unordered_map<std::string, std::string>& values,
                const std::string& key,
                std::string& outValue) {
    const auto it = values.find(key);
    if (it == values.end()) {
        return false;
    }
    outValue = it->second;
    return true;
}

std::string SerializePageSequence(const std::vector<std::uint32_t>& sequence) {
    std::ostringstream out;
    for (std::size_t i = 0; i < sequence.size(); ++i) {
        if (i > 0) {
            out << ',';
        }
        if (sequence[i] == kBlankPageIndex) {
            out << '0';
        } else {
            out << (sequence[i] + 1u);
        }
    }
    return out.str();
}

bool ParsePageSequence(const std::string& csv, std::vector<std::uint32_t>& sequence) {
    sequence.clear();
    if (csv.empty()) {
        return true;
    }

    std::stringstream stream(csv);
    std::string token;
    while (std::getline(stream, token, ',')) {
        if (token.empty()) {
            return false;
        }
        std::uint32_t value = 0;
        if (!ParseUInt(token, value)) {
            return false;
        }
        if (value == 0) {
            sequence.push_back(kBlankPageIndex);
        } else {
            sequence.push_back(value - 1u);
        }
    }
    return true;
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
    out << "tileOverlap=" << preset.tileOverlap << '\n';
    out << "repeatX=" << preset.repeatX << '\n';
    out << "repeatY=" << preset.repeatY << '\n';
    out << "stepX=" << preset.stepX << '\n';
    out << "stepY=" << preset.stepY << '\n';
    out << "slotWidth=" << preset.slotWidth << '\n';
    out << "slotHeight=" << preset.slotHeight << '\n';
    out << "reverse=" << (preset.buildOptions.reverseOrder ? 1 : 0) << '\n';
    out << "filter=" << FilterToString(preset.buildOptions.filter) << '\n';
    out << "padMultiple=" << preset.buildOptions.padToMultiple << '\n';
    out << "bookletSignatureSize=" << preset.buildOptions.bookletSignatureSize << '\n';
    out << "explicitPageSequence=" << SerializePageSequence(preset.buildOptions.explicitPageSequence) << '\n';
    out << "manualSequence=" << SerializePageSequence(preset.manualSequence) << '\n';
    out << "scaleToFit=" << (preset.buildOptions.scaleToFit ? 1 : 0) << '\n';
    out << "autoRotateToFit=" << (preset.buildOptions.autoRotateToFit ? 1 : 0) << '\n';
    out << "sourcePageWidth=" << preset.buildOptions.sourcePageWidthPoints << '\n';
    out << "sourcePageHeight=" << preset.buildOptions.sourcePageHeightPoints << '\n';
    out << "pdfSheetNumber=" << (preset.pdfOptions.includeSheetNumber ? 1 : 0) << '\n';
    out << "pdfHeader=" << preset.pdfOptions.headerText << '\n';
    out << "pdfFooter=" << preset.pdfOptions.footerText << '\n';
    out << "pdfIncludeBates=" << (preset.pdfOptions.includeBates ? 1 : 0) << '\n';
    out << "pdfBatesPrefix=" << preset.pdfOptions.batesPrefix << '\n';
    out << "pdfBatesStart=" << preset.pdfOptions.batesStart << '\n';
    out << "pdfDrawSheetBorder=" << (preset.pdfOptions.drawSheetBorder ? 1 : 0) << '\n';
    out << "pdfDrawSlotOutlines=" << (preset.pdfOptions.drawSlotOutlines ? 1 : 0) << '\n';
    out << "pdfDrawSlotLabels=" << (preset.pdfOptions.drawSlotLabels ? 1 : 0) << '\n';
    out << "pdfDrawCenterMarks=" << (preset.pdfOptions.drawCenterMarks ? 1 : 0) << '\n';
    out << "pdfDrawTrimMarks=" << (preset.pdfOptions.drawTrimMarks ? 1 : 0) << '\n';
    out << "pdfTrimMarkLength=" << preset.pdfOptions.trimMarkLengthPoints << '\n';
    out << "pdfTrimMarkOffset=" << preset.pdfOptions.trimMarkOffsetPoints << '\n';
    out << "pdfDrawBleedBox=" << (preset.pdfOptions.drawBleedBox ? 1 : 0) << '\n';
    out << "pdfBleedPoints=" << preset.pdfOptions.bleedPoints << '\n';

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
        values[TrimAscii(line.substr(0, pos))] = TrimAscii(line.substr(pos + 1));
    }

    std::string raw;
    auto fail = [&](const std::string& key) {
        errorMessage = "Missing or invalid preset key: " + key;
        return false;
    };

    if (!RequireKey(values, "sheetWidth", raw) || !ParseDouble(raw, preset.sheetSize.widthPoints)) return fail("sheetWidth");
    if (!RequireKey(values, "sheetHeight", raw) || !ParseDouble(raw, preset.sheetSize.heightPoints)) return fail("sheetHeight");
    if (!RequireKey(values, "columns", raw) || !ParseUInt(raw, preset.columns)) return fail("columns");
    if (!RequireKey(values, "rows", raw) || !ParseUInt(raw, preset.rows)) return fail("rows");
    if (RequireKey(values, "tileOverlap", raw)) {
        if (!ParseDouble(raw, preset.tileOverlap)) return fail("tileOverlap");
    } else {
        preset.tileOverlap = 0.0;
    }
    if (!RequireKey(values, "repeatX", raw) || !ParseUInt(raw, preset.repeatX)) return fail("repeatX");
    if (!RequireKey(values, "repeatY", raw) || !ParseUInt(raw, preset.repeatY)) return fail("repeatY");
    if (!RequireKey(values, "stepX", raw) || !ParseDouble(raw, preset.stepX)) return fail("stepX");
    if (!RequireKey(values, "stepY", raw) || !ParseDouble(raw, preset.stepY)) return fail("stepY");
    if (!RequireKey(values, "slotWidth", raw) || !ParseDouble(raw, preset.slotWidth)) return fail("slotWidth");
    if (!RequireKey(values, "slotHeight", raw) || !ParseDouble(raw, preset.slotHeight)) return fail("slotHeight");
    if (!RequireKey(values, "padMultiple", raw) || !ParseUInt(raw, preset.buildOptions.padToMultiple)) return fail("padMultiple");
    if (RequireKey(values, "bookletSignatureSize", raw)) {
        if (!ParseUInt(raw, preset.buildOptions.bookletSignatureSize)) return fail("bookletSignatureSize");
    } else {
        preset.buildOptions.bookletSignatureSize = 0;
    }
    if (RequireKey(values, "explicitPageSequence", raw)) {
        if (!ParsePageSequence(raw, preset.buildOptions.explicitPageSequence)) return fail("explicitPageSequence");
    } else {
        preset.buildOptions.explicitPageSequence.clear();
    }
    if (RequireKey(values, "manualSequence", raw)) {
        if (!ParsePageSequence(raw, preset.manualSequence)) return fail("manualSequence");
    } else {
        preset.manualSequence.clear();
    }
    if (RequireKey(values, "scaleToFit", raw)) {
        if (!ParseBool(raw, preset.buildOptions.scaleToFit)) return fail("scaleToFit");
    } else {
        preset.buildOptions.scaleToFit = false;
    }
    if (RequireKey(values, "autoRotateToFit", raw)) {
        if (!ParseBool(raw, preset.buildOptions.autoRotateToFit)) return fail("autoRotateToFit");
    } else {
        preset.buildOptions.autoRotateToFit = false;
    }
    if (RequireKey(values, "sourcePageWidth", raw)) {
        if (!ParseDouble(raw, preset.buildOptions.sourcePageWidthPoints)) return fail("sourcePageWidth");
    } else {
        preset.buildOptions.sourcePageWidthPoints = 0.0;
    }
    if (RequireKey(values, "sourcePageHeight", raw)) {
        if (!ParseDouble(raw, preset.buildOptions.sourcePageHeightPoints)) return fail("sourcePageHeight");
    } else {
        preset.buildOptions.sourcePageHeightPoints = 0.0;
    }
    if (!RequireKey(values, "filter", raw)) return fail("filter");
    preset.buildOptions.filter = StringToFilter(raw);
    if (!RequireKey(values, "reverse", raw) || !ParseBool(raw, preset.buildOptions.reverseOrder)) return fail("reverse");
    if (!RequireKey(values, "pdfSheetNumber", raw) || !ParseBool(raw, preset.pdfOptions.includeSheetNumber)) return fail("pdfSheetNumber");
    if (!RequireKey(values, "pdfHeader", preset.pdfOptions.headerText)) return fail("pdfHeader");
    if (!RequireKey(values, "pdfFooter", preset.pdfOptions.footerText)) return fail("pdfFooter");
    if (!RequireKey(values, "pdfIncludeBates", raw) || !ParseBool(raw, preset.pdfOptions.includeBates)) return fail("pdfIncludeBates");
    if (!RequireKey(values, "pdfBatesPrefix", preset.pdfOptions.batesPrefix)) return fail("pdfBatesPrefix");
    if (!RequireKey(values, "pdfBatesStart", raw) || !ParseUInt(raw, preset.pdfOptions.batesStart)) return fail("pdfBatesStart");
    if (RequireKey(values, "pdfDrawSheetBorder", raw)) {
        if (!ParseBool(raw, preset.pdfOptions.drawSheetBorder)) return fail("pdfDrawSheetBorder");
    }
    if (RequireKey(values, "pdfDrawSlotOutlines", raw)) {
        if (!ParseBool(raw, preset.pdfOptions.drawSlotOutlines)) return fail("pdfDrawSlotOutlines");
    }
    if (RequireKey(values, "pdfDrawSlotLabels", raw)) {
        if (!ParseBool(raw, preset.pdfOptions.drawSlotLabels)) return fail("pdfDrawSlotLabels");
    }
    if (RequireKey(values, "pdfDrawCenterMarks", raw)) {
        if (!ParseBool(raw, preset.pdfOptions.drawCenterMarks)) return fail("pdfDrawCenterMarks");
    }
    if (RequireKey(values, "pdfDrawTrimMarks", raw)) {
        if (!ParseBool(raw, preset.pdfOptions.drawTrimMarks)) return fail("pdfDrawTrimMarks");
    }
    if (RequireKey(values, "pdfTrimMarkLength", raw)) {
        if (!ParseDouble(raw, preset.pdfOptions.trimMarkLengthPoints)) return fail("pdfTrimMarkLength");
    }
    if (RequireKey(values, "pdfTrimMarkOffset", raw)) {
        if (!ParseDouble(raw, preset.pdfOptions.trimMarkOffsetPoints)) return fail("pdfTrimMarkOffset");
    }
    if (RequireKey(values, "pdfDrawBleedBox", raw)) {
        if (!ParseBool(raw, preset.pdfOptions.drawBleedBox)) return fail("pdfDrawBleedBox");
    }
    if (RequireKey(values, "pdfBleedPoints", raw)) {
        if (!ParseDouble(raw, preset.pdfOptions.bleedPoints)) return fail("pdfBleedPoints");
    }

    return true;
}

} // namespace aimp

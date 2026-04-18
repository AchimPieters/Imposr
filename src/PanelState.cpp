#include "aimp/PanelState.h"

#include <cctype>
#include <cstdlib>
#include <cmath>

namespace aimp {
namespace {

std::size_t SkipWs(const std::string& text, std::size_t pos) {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])) != 0) {
        ++pos;
    }
    return pos;
}

bool FindQuotedKey(const std::string& text, const std::string& key, std::size_t& outKeyPos) {
    bool inString = false;
    bool escaped = false;
    std::size_t tokenStart = std::string::npos;
    std::string current;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char ch = text[i];
        if (!inString) {
            if (ch == '"') {
                inString = true;
                escaped = false;
                tokenStart = i;
                current.clear();
            }
            continue;
        }
        if (escaped) {
            current.push_back(ch);
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') {
            if (current == key) {
                outKeyPos = tokenStart;
                return true;
            }
            inString = false;
            tokenStart = std::string::npos;
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    return false;
}

bool ParseStringAt(const std::string& text, std::size_t startQuote, std::string& outValue, std::size_t& outNextPos) {
    if (startQuote >= text.size() || text[startQuote] != '"') {
        return false;
    }
    std::string value;
    bool escaped = false;
    for (std::size_t i = startQuote + 1; i < text.size(); ++i) {
        const char ch = text[i];
        if (escaped) {
            switch (ch) {
                case '"': value.push_back('"'); break;
                case '\\': value.push_back('\\'); break;
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                default: value.push_back(ch); break;
            }
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') {
            outValue = value;
            outNextPos = i + 1;
            return true;
        }
        value.push_back(ch);
    }
    return false;
}

bool FindObjectScope(const std::string& json,
                     const std::string& key,
                     std::size_t& outOpenBrace,
                     std::size_t& outCloseBrace) {
    std::size_t keyPos = 0;
    if (!FindQuotedKey(json, key, keyPos)) {
        return false;
    }
    const std::size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) {
        return false;
    }
    const std::size_t valuePos = SkipWs(json, colonPos + 1);
    if (valuePos >= json.size() || json[valuePos] != '{') {
        return false;
    }
    std::size_t depth = 0;
    bool inString = false;
    bool escaped = false;
    for (std::size_t i = valuePos; i < json.size(); ++i) {
        const char ch = json[i];
        if (inString) {
            if (escaped) {
                escaped = false;
                continue;
            }
            if (ch == '\\') {
                escaped = true;
                continue;
            }
            if (ch == '"') {
                inString = false;
            }
            continue;
        }
        if (ch == '"') {
            inString = true;
            continue;
        }
        if (ch == '{') {
            ++depth;
        } else if (ch == '}') {
            if (depth == 0) {
                return false;
            }
            --depth;
            if (depth == 0) {
                outOpenBrace = valuePos;
                outCloseBrace = i;
                return true;
            }
        }
    }
    return false;
}

bool FindScopedValue(const std::string& jsonScope, const std::string& key, std::size_t& outValuePos) {
    std::size_t keyPos = 0;
    if (!FindQuotedKey(jsonScope, key, keyPos)) {
        return false;
    }
    const std::size_t colonPos = jsonScope.find(':', keyPos);
    if (colonPos == std::string::npos) {
        return false;
    }
    outValuePos = SkipWs(jsonScope, colonPos + 1);
    return outValuePos < jsonScope.size();
}

bool TryReadScopedString(const std::string& jsonScope, const std::string& key, std::string& outValue) {
    std::size_t valuePos = 0;
    if (!FindScopedValue(jsonScope, key, valuePos)) {
        return false;
    }
    std::size_t nextPos = 0;
    return ParseStringAt(jsonScope, valuePos, outValue, nextPos);
}

bool TryReadScopedBool(const std::string& jsonScope, const std::string& key, bool& outValue) {
    std::size_t valuePos = 0;
    if (!FindScopedValue(jsonScope, key, valuePos)) {
        return false;
    }
    if (jsonScope.compare(valuePos, 4, "true") == 0) {
        outValue = true;
        return true;
    }
    if (jsonScope.compare(valuePos, 5, "false") == 0) {
        outValue = false;
        return true;
    }
    return false;
}

bool TryReadScopedDouble(const std::string& jsonScope, const std::string& key, double& outValue) {
    std::size_t valuePos = 0;
    if (!FindScopedValue(jsonScope, key, valuePos)) {
        return false;
    }
    char* end = nullptr;
    const double value = std::strtod(jsonScope.c_str() + valuePos, &end);
    if (end == jsonScope.c_str() + valuePos) {
        return false;
    }
    const std::size_t endOffset = static_cast<std::size_t>(end - jsonScope.c_str());
    const std::size_t next = SkipWs(jsonScope, endOffset);
    if (next < jsonScope.size() && jsonScope[next] != ',' && jsonScope[next] != '}') {
        return false;
    }
    outValue = value;
    return true;
}

bool TryReadScopedUInt(const std::string& jsonScope, const std::string& key, std::uint32_t& outValue) {
    double raw = 0.0;
    if (!TryReadScopedDouble(jsonScope, key, raw) || raw < 0.0) {
        return false;
    }
    if (std::fabs(raw - std::floor(raw)) > 1e-9) {
        return false;
    }
    outValue = static_cast<std::uint32_t>(raw);
    return true;
}

void NormalizePanelPreset(PlannerPreset& preset, PanelStateApplyResult& outResult) {
    if (preset.columns == 0) {
        preset.columns = 1;
        outResult.warnings.emplace_back("columns was 0; normalized to 1");
    }
    if (preset.rows == 0) {
        preset.rows = 1;
        outResult.warnings.emplace_back("rows was 0; normalized to 1");
    }
    if (preset.sheetSize.widthPoints <= 0.0) {
        preset.sheetSize.widthPoints = 1190.55;
        outResult.warnings.emplace_back("sheet width was <= 0; normalized to 1190.55pt");
    }
    if (preset.sheetSize.heightPoints <= 0.0) {
        preset.sheetSize.heightPoints = 841.89;
        outResult.warnings.emplace_back("sheet height was <= 0; normalized to 841.89pt");
    }
    if (preset.pdfOptions.trimMarkLengthPoints < 0.0) {
        preset.pdfOptions.trimMarkLengthPoints = 0.0;
        outResult.warnings.emplace_back("trim mark length was negative; normalized to 0");
    }
    if (preset.pdfOptions.trimMarkOffsetPoints < 0.0) {
        preset.pdfOptions.trimMarkOffsetPoints = 0.0;
        outResult.warnings.emplace_back("trim mark offset was negative; normalized to 0");
    }
    if (preset.pdfOptions.bleedPoints < 0.0) {
        preset.pdfOptions.bleedPoints = 0.0;
        outResult.warnings.emplace_back("bleed points was negative; normalized to 0");
    }
    if (preset.outputStem.empty()) {
        preset.outputStem = "acrobat-imposition-run";
        outResult.warnings.emplace_back("outputStem was empty; normalized to acrobat-imposition-run");
    }
}

} // namespace

const char* PanelStateFilterName(PageFilter filter) {
    switch (filter) {
        case PageFilter::EvenOnly: return "even";
        case PageFilter::OddOnly: return "odd";
        case PageFilter::All:
        default: return "all";
    }
}

PageFilter ParsePanelStateFilter(const std::string& value) {
    if (value == "even") {
        return PageFilter::EvenOnly;
    }
    if (value == "odd") {
        return PageFilter::OddOnly;
    }
    return PageFilter::All;
}

bool ApplyPanelStateJsonToPreset(const std::string& json,
                                 PlannerPreset& preset,
                                 PanelStateApplyResult& outResult) {
    outResult = {};

    std::size_t sheetOpen = 0;
    std::size_t sheetClose = 0;
    std::size_t presetOpen = 0;
    std::size_t presetClose = 0;
    if (!FindObjectScope(json, "sheet", sheetOpen, sheetClose) ||
        !FindObjectScope(json, "preset", presetOpen, presetClose)) {
        return false;
    }
    const std::string sheetScope = json.substr(sheetOpen, sheetClose - sheetOpen + 1);
    const std::string presetScope = json.substr(presetOpen, presetClose - presetOpen + 1);

    std::string mode;
    if (TryReadScopedString(json, "mode", mode)) {
        if (mode == "two-up") {
            preset.columns = 2;
            preset.rows = 1;
        } else if (mode == "n-up") {
            if (preset.columns == 0) {
                preset.columns = 2;
            }
            if (preset.rows == 0) {
                preset.rows = 2;
            }
        }
    }

    TryReadScopedDouble(sheetScope, "widthPoints", preset.sheetSize.widthPoints);
    TryReadScopedDouble(sheetScope, "heightPoints", preset.sheetSize.heightPoints);

    TryReadScopedUInt(presetScope, "columns", preset.columns);
    TryReadScopedUInt(presetScope, "rows", preset.rows);
    TryReadScopedBool(presetScope, "fitToSlot", preset.buildOptions.scaleToFit);
    TryReadScopedBool(presetScope, "autoRotateToFit", preset.buildOptions.autoRotateToFit);
    TryReadScopedBool(presetScope, "reverseOrder", preset.buildOptions.reverseOrder);

    std::string filter;
    if (TryReadScopedString(presetScope, "filter", filter)) {
        preset.buildOptions.filter = ParsePanelStateFilter(filter);
    }

    TryReadScopedDouble(presetScope, "bookletCreepPerSheetPoints", preset.buildOptions.bookletCreepPerSheetPoints);
    TryReadScopedString(presetScope, "outputDirectory", preset.outputDirectory);
    TryReadScopedString(presetScope, "outputStem", preset.outputStem);

    TryReadScopedBool(presetScope, "drawTrimMarks", preset.pdfOptions.drawTrimMarks);
    TryReadScopedDouble(presetScope, "trimMarkLengthPoints", preset.pdfOptions.trimMarkLengthPoints);
    TryReadScopedDouble(presetScope, "trimMarkOffsetPoints", preset.pdfOptions.trimMarkOffsetPoints);
    TryReadScopedBool(presetScope, "drawBleedBox", preset.pdfOptions.drawBleedBox);
    TryReadScopedDouble(presetScope, "bleedPoints", preset.pdfOptions.bleedPoints);
    TryReadScopedBool(presetScope, "failOnValidationIssues", preset.pdfOptions.failOnValidationIssues);
    TryReadScopedBool(presetScope, "failOnPreflightErrors", preset.pdfOptions.failOnPreflightErrors);

    std::string profile;
    if (TryReadScopedString(presetScope, "pdfxProfile", profile)) {
        PdfxProfile parsed {};
        if (TryParsePdfxProfile(profile, parsed)) {
            preset.pdfOptions.targetPdfxProfile = parsed;
        } else {
            outResult.warnings.emplace_back("pdfxProfile was invalid; keeping existing preset value");
        }
    }

    NormalizePanelPreset(preset, outResult);
    outResult.applied = true;
    return true;
}

} // namespace aimp

#include "aimp/VariableData.h"
#include "EscapeUtils.h"

#include <cctype>
#include <charconv>
#include <fstream>
#include <iostream>
#include <sstream>

namespace aimp {

namespace {

using internal::EscapeJson;

std::vector<std::string> ParseCsvLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    bool inQuotes = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (ch == '"') {
            if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') {
                current.push_back('"');
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
            continue;
        }
        if (ch == ',' && !inQuotes) {
            fields.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    fields.push_back(current);
    return fields;
}

std::string Trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())) != 0) {
        s.erase(s.begin());
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())) != 0) {
        s.pop_back();
    }
    return s;
}

bool IsImageField(const std::string& headerName) {
    const std::string lower = [&] {
        std::string s = headerName;
        for (char& c : s) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return s;
    }();
    return lower.find("image") != std::string::npos
        || lower.find("img") != std::string::npos
        || lower.find("photo") != std::string::npos
        || lower.find("logo") != std::string::npos;
}

} // namespace

bool LoadVariableDataSet(const std::string& csvPath,
                         VariableDataSet& outDataset,
                         std::string& errorMessage) {
    outDataset.headers.clear();
    outDataset.records.clear();
    errorMessage.clear();

    if (csvPath.empty()) {
        errorMessage = "CSV path is empty";
        return false;
    }

    std::ifstream in(csvPath);
    if (!in) {
        errorMessage = "Cannot open variable data CSV: " + csvPath;
        return false;
    }

    std::string headerLine;
    if (!std::getline(in, headerLine)) {
        errorMessage = "Variable data CSV is empty";
        return false;
    }

    const auto rawHeaders = ParseCsvLine(headerLine);
    if (rawHeaders.empty()) {
        errorMessage = "Variable data CSV header row is invalid";
        return false;
    }

    for (const auto& h : rawHeaders) {
        outDataset.headers.push_back(Trim(h));
    }

    // Case-normalize the first header for comparison ("Page", "PAGE" all accepted).
    if (!outDataset.headers.empty()) {
        std::string& firstHeader = outDataset.headers[0];
        std::string lower = firstHeader;
        for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (lower != "page") {
            errorMessage = "Variable data CSV must start with a 'page' column (got: '" + firstHeader + "')";
            return false;
        }
        firstHeader = "page"; // normalize to lowercase
    } else {
        errorMessage = "Variable data CSV header row is empty";
        return false;
    }

    std::string line;
    std::size_t lineNumber = 1;
    while (std::getline(in, line)) {
        ++lineNumber;
        if (Trim(line).empty()) {
            continue;
        }

        const auto fields = ParseCsvLine(line);
        if (fields.empty()) {
            continue;
        }

        const std::string pageStr = Trim(fields[0]);
        std::uint32_t humanPage = 0;
        const char* begin = pageStr.data();
        const char* end = pageStr.data() + pageStr.size();
        const auto parseResult = std::from_chars(begin, end, humanPage);
        if (parseResult.ec != std::errc{} || parseResult.ptr != end || humanPage == 0) {
            errorMessage = "Invalid page number at line " + std::to_string(lineNumber);
            return false;
        }

        const std::uint32_t pageIndex = humanPage - 1u;
        VarRecord record;
        for (std::size_t col = 1; col < outDataset.headers.size(); ++col) {
            const std::string& key = outDataset.headers[col];
            if (key.empty()) {
                continue;
            }
            const std::string value = col < fields.size() ? Trim(fields[col]) : std::string{};
            VarField field;
            field.name = key;
            field.value = value;
            field.type = IsImageField(key) ? VarFieldType::Image : VarFieldType::Text;
            record[key] = std::move(field);
        }
        if (outDataset.records.count(pageIndex) > 0) {
            std::cerr << "[imposr] Warning: variable data CSV has duplicate entry for page "
                      << humanPage << " (line " << lineNumber << "); later row overwrites earlier.\n";
        }
        outDataset.records[pageIndex] = std::move(record);
    }

    return true;
}

std::string ExpandTemplate(const std::string& templateStr, const VarRecord& record) {
    std::string out;
    out.reserve(templateStr.size() + 32);
    for (std::size_t i = 0; i < templateStr.size();) {
        if (i + 1 < templateStr.size() && templateStr[i] == '{' && templateStr[i + 1] == '{') {
            const std::size_t end = templateStr.find("}}", i + 2);
            if (end != std::string::npos) {
                const std::string key = templateStr.substr(i + 2, end - (i + 2));
                const auto it = record.find(key);
                if (it != record.end()) {
                    out += it->second.value;
                }
                i = end + 2;
                continue;
            }
        }
        out.push_back(templateStr[i]);
        ++i;
    }
    return out;
}

std::vector<VarMergeResult> MergeVariableData(const VariableDataSet& dataset,
                                               const std::string& overlayTemplate,
                                               std::uint32_t pageCount) {
    std::vector<VarMergeResult> results;
    results.reserve(pageCount);

    for (std::uint32_t pageIndex = 0; pageIndex < pageCount; ++pageIndex) {
        VarMergeResult entry;
        entry.pageIndex = pageIndex;

        const auto recIt = dataset.records.find(pageIndex);
        if (recIt != dataset.records.end()) {
            const VarRecord& record = recIt->second;
            if (!overlayTemplate.empty()) {
                entry.resolvedText = ExpandTemplate(overlayTemplate, record);
            }
            for (const auto& kv : record) {
                if (kv.second.type == VarFieldType::Image && !kv.second.value.empty()) {
                    entry.imageFields.push_back(kv.second.value);
                }
            }
        }

        results.push_back(std::move(entry));
    }

    return results;
}

std::string VariableDataSetToJson(const VariableDataSet& dataset) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"kind\": \"variable-data-set\",\n";
    out << "  \"headers\": [";
    for (std::size_t i = 0; i < dataset.headers.size(); ++i) {
        if (i > 0) out << ", ";
        out << "\"" << EscapeJson(dataset.headers[i]) << "\"";
    }
    out << "],\n";
    out << "  \"recordCount\": " << dataset.records.size() << ",\n";
    out << "  \"records\": [\n";
    bool firstRecord = true;
    for (const auto& [pageIndex, record] : dataset.records) {
        if (!firstRecord) out << ",\n";
        firstRecord = false;
        out << "    {\"pageIndex\": " << pageIndex << ", \"fields\": {";
        bool firstField = true;
        for (const auto& [key, field] : record) {
            if (!firstField) out << ", ";
            firstField = false;
            out << "\"" << EscapeJson(key) << "\": {"
                << "\"type\": \"" << (field.type == VarFieldType::Image ? "image" : "text") << "\""
                << ", \"value\": \"" << EscapeJson(field.value) << "\"}";
        }
        out << "}}";
    }
    out << "\n  ]\n";
    out << "}\n";
    return out.str();
}

} // namespace aimp

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace aimp {

enum class VarFieldType {
    Text,
    Image
};

struct VarField {
    std::string name;
    VarFieldType type {VarFieldType::Text};
    std::string value;
};

using VarRecord = std::unordered_map<std::string, VarField>;

struct VariableDataSet {
    std::vector<std::string> headers;
    std::unordered_map<std::uint32_t, VarRecord> records;
};

struct VarMergeResult {
    std::uint32_t pageIndex {0};
    std::string resolvedText;
    std::vector<std::string> imageFields;
};

bool LoadVariableDataSet(const std::string& csvPath,
                         VariableDataSet& outDataset,
                         std::string& errorMessage);

std::string ExpandTemplate(const std::string& templateStr,
                           const VarRecord& record);

std::vector<VarMergeResult> MergeVariableData(const VariableDataSet& dataset,
                                               const std::string& overlayTemplate,
                                               std::uint32_t pageCount);

std::string VariableDataSetToJson(const VariableDataSet& dataset);

} // namespace aimp

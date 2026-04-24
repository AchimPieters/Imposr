#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "aimp/ImpositionPlan.h"
#include "aimp/PdfComposer.h"

namespace aimp {

enum class PdfXSubtype {
    None,
    PdfX1a_2001,
    PdfX1a_2003,
    PdfX3_2002,
    PdfX3_2003,
    PdfX4_2010,
    PdfX4p_2010,
    PdfX5g_2010,
    PdfX5n_2010,
    PdfX5pg_2010,
    Unknown
};

struct PdfXMetadata {
    PdfXSubtype subtype {PdfXSubtype::None};
    std::string outputConditionIdentifier;
    std::string outputCondition;
    std::string registryName;
    std::string trapped;
    bool hasOutputIntent {false};
    std::string gtsConformance;
    std::string documentVersion;
};

struct PdfXComplianceIssue {
    std::string code;
    std::string message;
    bool isError {true};
};

struct PdfXPreservationConfig {
    bool preserveSubtype {true};
    bool preserveOutputIntent {true};
    bool validateOnOutput {true};
    PdfXSubtype requiredSubtype {PdfXSubtype::None};
};

const char* PdfXSubtypeName(PdfXSubtype subtype);

PdfXSubtype ParsePdfXSubtype(const std::string& gtsConformance);

PdfXMetadata DetectPdfXMetadata(const std::string& pdfHeaderSample);

std::vector<PdfXComplianceIssue> ValidatePdfXCompliance(const ImpositionPlan& plan,
                                                          const PdfXMetadata& metadata,
                                                          const PdfXPreservationConfig& config);

std::string PdfXMetadataToJson(const PdfXMetadata& metadata);

std::string PdfXOutputIntentStream(const PdfXMetadata& metadata);

} // namespace aimp

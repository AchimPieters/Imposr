#include "aimp/PdfX.h"
#include "EscapeUtils.h"

#include <algorithm>
#include <sstream>

namespace aimp {

namespace {

using internal::EscapeJson;
using internal::EscapePdf;

bool ContainsIgnoreCase(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    auto it = std::search(haystack.begin(), haystack.end(),
                          needle.begin(), needle.end(),
                          [](char a, char b) {
                              return std::tolower(static_cast<unsigned char>(a)) ==
                                     std::tolower(static_cast<unsigned char>(b));
                          });
    return it != haystack.end();
}

} // namespace

const char* PdfXSubtypeName(PdfXSubtype subtype) {
    switch (subtype) {
        case PdfXSubtype::None: return "none";
        case PdfXSubtype::PdfX1a_2001: return "PDF/X-1a:2001";
        case PdfXSubtype::PdfX1a_2003: return "PDF/X-1a:2003";
        case PdfXSubtype::PdfX3_2002: return "PDF/X-3:2002";
        case PdfXSubtype::PdfX3_2003: return "PDF/X-3:2003";
        case PdfXSubtype::PdfX4_2010: return "PDF/X-4:2010";
        case PdfXSubtype::PdfX4p_2010: return "PDF/X-4p:2010";
        case PdfXSubtype::PdfX5g_2010: return "PDF/X-5g:2010";
        case PdfXSubtype::PdfX5n_2010: return "PDF/X-5n:2010";
        case PdfXSubtype::PdfX5pg_2010: return "PDF/X-5pg:2010";
        case PdfXSubtype::Unknown: return "unknown";
        default: return "none";
    }
}

PdfXSubtype ParsePdfXSubtype(const std::string& conformance) {
    if (conformance == "PDF/X-1a:2001" || conformance == "GTS_PDFX" || conformance == "pdfx-1a") return PdfXSubtype::PdfX1a_2001;
    if (conformance == "PDF/X-1a:2003") return PdfXSubtype::PdfX1a_2003;
    if (conformance == "PDF/X-3:2002") return PdfXSubtype::PdfX3_2002;
    if (conformance == "PDF/X-3:2003") return PdfXSubtype::PdfX3_2003;
    if (conformance == "PDF/X-4" || conformance == "PDF/X-4:2010" || conformance == "pdfx-4") return PdfXSubtype::PdfX4_2010;
    if (conformance == "PDF/X-4p" || conformance == "PDF/X-4p:2010") return PdfXSubtype::PdfX4p_2010;
    if (conformance == "PDF/X-5g" || conformance == "PDF/X-5g:2010") return PdfXSubtype::PdfX5g_2010;
    if (conformance == "PDF/X-5n" || conformance == "PDF/X-5n:2010") return PdfXSubtype::PdfX5n_2010;
    if (conformance == "PDF/X-5pg" || conformance == "PDF/X-5pg:2010") return PdfXSubtype::PdfX5pg_2010;
    if (!conformance.empty() && conformance != "none") return PdfXSubtype::Unknown;
    return PdfXSubtype::None;
}

PdfXMetadata DetectPdfXMetadata(const std::string& pdfHeaderSample) {
    PdfXMetadata meta {};

    if (ContainsIgnoreCase(pdfHeaderSample, "PDF/X-1a:2001") ||
        ContainsIgnoreCase(pdfHeaderSample, "GTS_PDFX")) {
        meta.subtype = PdfXSubtype::PdfX1a_2001;
        meta.gtsConformance = "PDF/X-1a:2001";
    } else if (ContainsIgnoreCase(pdfHeaderSample, "PDF/X-1a:2003")) {
        meta.subtype = PdfXSubtype::PdfX1a_2003;
        meta.gtsConformance = "PDF/X-1a:2003";
    } else if (ContainsIgnoreCase(pdfHeaderSample, "PDF/X-3:2002")) {
        meta.subtype = PdfXSubtype::PdfX3_2002;
        meta.gtsConformance = "PDF/X-3:2002";
    } else if (ContainsIgnoreCase(pdfHeaderSample, "PDF/X-3:2003")) {
        meta.subtype = PdfXSubtype::PdfX3_2003;
        meta.gtsConformance = "PDF/X-3:2003";
    } else if (ContainsIgnoreCase(pdfHeaderSample, "PDF/X-4p")) {
        meta.subtype = PdfXSubtype::PdfX4p_2010;
        meta.gtsConformance = "PDF/X-4p:2010";
    } else if (ContainsIgnoreCase(pdfHeaderSample, "PDF/X-4")) {
        meta.subtype = PdfXSubtype::PdfX4_2010;
        meta.gtsConformance = "PDF/X-4:2010";
    } else if (ContainsIgnoreCase(pdfHeaderSample, "PDF/X-5g")) {
        meta.subtype = PdfXSubtype::PdfX5g_2010;
        meta.gtsConformance = "PDF/X-5g:2010";
    } else if (ContainsIgnoreCase(pdfHeaderSample, "PDF/X-5n")) {
        meta.subtype = PdfXSubtype::PdfX5n_2010;
        meta.gtsConformance = "PDF/X-5n:2010";
    } else if (ContainsIgnoreCase(pdfHeaderSample, "PDF/X-5pg")) {
        meta.subtype = PdfXSubtype::PdfX5pg_2010;
        meta.gtsConformance = "PDF/X-5pg:2010";
    }

    if (ContainsIgnoreCase(pdfHeaderSample, "OutputIntent")) {
        meta.hasOutputIntent = true;
    }
    if (ContainsIgnoreCase(pdfHeaderSample, "Trapped")) {
        meta.trapped = "True";
    }

    return meta;
}

std::vector<PdfXComplianceIssue> ValidatePdfXCompliance(const ImpositionPlan& plan,
                                                          const PdfXMetadata& metadata,
                                                          const PdfXPreservationConfig& config) {
    std::vector<PdfXComplianceIssue> issues;

    if (config.requiredSubtype != PdfXSubtype::None &&
        metadata.subtype != config.requiredSubtype) {
        issues.push_back({
            "pdfx.subtype-mismatch",
            std::string("Required PDF/X subtype '") + PdfXSubtypeName(config.requiredSubtype) +
            "' but source is '" + PdfXSubtypeName(metadata.subtype) + "'.",
            true
        });
    }

    if (metadata.subtype != PdfXSubtype::None) {
        if (config.preserveOutputIntent && !metadata.hasOutputIntent) {
            issues.push_back({
                "pdfx.missing-output-intent",
                "Source is declared PDF/X but no OutputIntent was detected.",
                true
            });
        }

        for (const auto& placement : plan.placements) {
            if (placement.sourcePage.pageIndex == kBlankPageIndex) {
                issues.push_back({
                    "pdfx.blank-page-in-pdfx",
                    "Plan contains blank placements; PDF/X output requires all pages to carry valid content.",
                    false
                });
                break;
            }
        }

        if (plan.outputSheet.widthPoints <= 0.0 || plan.outputSheet.heightPoints <= 0.0) {
            issues.push_back({
                "pdfx.invalid-output-sheet",
                "Output sheet dimensions must be positive for PDF/X compliance.",
                true
            });
        }
    }

    return issues;
}

std::string PdfXMetadataToJson(const PdfXMetadata& metadata) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"kind\": \"pdfx-metadata\",\n";
    out << "  \"subtype\": \"" << EscapeJson(PdfXSubtypeName(metadata.subtype)) << "\",\n";
    out << "  \"gtsConformance\": \"" << EscapeJson(metadata.gtsConformance) << "\",\n";
    out << "  \"hasOutputIntent\": " << (metadata.hasOutputIntent ? "true" : "false") << ",\n";
    out << "  \"outputConditionIdentifier\": \"" << EscapeJson(metadata.outputConditionIdentifier) << "\",\n";
    out << "  \"registryName\": \"" << EscapeJson(metadata.registryName) << "\",\n";
    out << "  \"trapped\": \"" << EscapeJson(metadata.trapped) << "\"\n";
    out << "}\n";
    return out.str();
}

std::string PdfXOutputIntentStream(const PdfXMetadata& metadata) {
    if (metadata.subtype == PdfXSubtype::None) {
        return {};
    }

    std::ostringstream out;
    out << "<< /Type /OutputIntent\n";
    out << "   /S /GTS_PDFX\n";
    out << "   /OutputConditionIdentifier ("
        << EscapePdf(metadata.outputConditionIdentifier.empty()
                     ? "Custom"
                     : metadata.outputConditionIdentifier) << ")\n";
    if (!metadata.registryName.empty()) {
        out << "   /RegistryName (" << EscapePdf(metadata.registryName) << ")\n";
    }
    if (!metadata.outputCondition.empty()) {
        out << "   /OutputCondition (" << EscapePdf(metadata.outputCondition) << ")\n";
    }
    out << ">>";
    return out.str();
}

} // namespace aimp

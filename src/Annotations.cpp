#include "aimp/Annotations.h"

#include <sstream>

namespace aimp {

const char* AnnotationTypeName(AnnotationType type) {
    switch (type) {
        case AnnotationType::Text: return "Text";
        case AnnotationType::FreeText: return "FreeText";
        case AnnotationType::Line: return "Line";
        case AnnotationType::Square: return "Square";
        case AnnotationType::Circle: return "Circle";
        case AnnotationType::Polygon: return "Polygon";
        case AnnotationType::Ink: return "Ink";
        case AnnotationType::Highlight: return "Highlight";
        case AnnotationType::Underline: return "Underline";
        case AnnotationType::StrikeOut: return "StrikeOut";
        case AnnotationType::Stamp: return "Stamp";
        case AnnotationType::Caret: return "Caret";
        case AnnotationType::Widget: return "Widget";
        case AnnotationType::Link: return "Link";
        case AnnotationType::Movie: return "Movie";
        case AnnotationType::Sound: return "Sound";
        case AnnotationType::FileAttachment: return "FileAttachment";
        case AnnotationType::Watermark: return "Watermark";
        case AnnotationType::PrinterMark: return "PrinterMark";
        case AnnotationType::Unknown: return "Unknown";
        default: return "Unknown";
    }
}

AnnotationType ParseAnnotationType(const std::string& subtype) {
    if (subtype == "Text") return AnnotationType::Text;
    if (subtype == "FreeText") return AnnotationType::FreeText;
    if (subtype == "Line") return AnnotationType::Line;
    if (subtype == "Square") return AnnotationType::Square;
    if (subtype == "Circle") return AnnotationType::Circle;
    if (subtype == "Polygon" || subtype == "PolyLine") return AnnotationType::Polygon;
    if (subtype == "Ink") return AnnotationType::Ink;
    if (subtype == "Highlight") return AnnotationType::Highlight;
    if (subtype == "Underline") return AnnotationType::Underline;
    if (subtype == "StrikeOut") return AnnotationType::StrikeOut;
    if (subtype == "Stamp") return AnnotationType::Stamp;
    if (subtype == "Caret") return AnnotationType::Caret;
    if (subtype == "Widget") return AnnotationType::Widget;
    if (subtype == "Link") return AnnotationType::Link;
    if (subtype == "Movie") return AnnotationType::Movie;
    if (subtype == "Sound") return AnnotationType::Sound;
    if (subtype == "FileAttachment") return AnnotationType::FileAttachment;
    if (subtype == "Watermark") return AnnotationType::Watermark;
    if (subtype == "PrinterMark") return AnnotationType::PrinterMark;
    return AnnotationType::Unknown;
}

AnnotationHandling ResolveHandling(AnnotationType type, const AnnotationConfig& config) {
    // Check specific filters first.
    for (const auto& filter : config.filters) {
        if (filter.handleAll || filter.type == type) {
            return filter.handling;
        }
    }

    // Built-in overrides.
    if (type == AnnotationType::Link && config.preserveLinks) {
        return AnnotationHandling::Preserve;
    }
    if (type == AnnotationType::PrinterMark && config.preservePrinterMarks) {
        return AnnotationHandling::Preserve;
    }
    if (type == AnnotationType::Widget && config.discardFormFields) {
        return AnnotationHandling::Discard;
    }

    return config.defaultHandling;
}

AnnotationProcessResult ProcessAnnotations(const std::vector<AnnotationRecord>& annotations,
                                            const AnnotationConfig& config) {
    AnnotationProcessResult result {};

    for (const auto& ann : annotations) {
        AnnotationRecord processed = ann;
        processed.resolvedHandling = ResolveHandling(ann.type, config);

        switch (processed.resolvedHandling) {
            case AnnotationHandling::Discard:
                ++result.discarded;
                break;
            case AnnotationHandling::Preserve:
                ++result.preserved;
                break;
            case AnnotationHandling::Flatten:
                ++result.flattened;
                break;
        }

        result.processed.push_back(std::move(processed));
    }

    return result;
}

std::string AnnotationProcessResultToJson(const AnnotationProcessResult& result) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"kind\": \"annotation-process-result\",\n";
    out << "  \"totalAnnotations\": " << result.processed.size() << ",\n";
    out << "  \"discarded\": " << result.discarded << ",\n";
    out << "  \"preserved\": " << result.preserved << ",\n";
    out << "  \"flattened\": " << result.flattened << ",\n";
    out << "  \"annotations\": [\n";
    for (std::size_t i = 0; i < result.processed.size(); ++i) {
        const auto& a = result.processed[i];
        const char* handlingStr = a.resolvedHandling == AnnotationHandling::Discard ? "discard"
                                : a.resolvedHandling == AnnotationHandling::Preserve ? "preserve"
                                : "flatten";
        out << "    {\"pageIndex\": " << a.pageIndex
            << ", \"type\": \"" << AnnotationTypeName(a.type) << "\""
            << ", \"handling\": \"" << handlingStr << "\""
            << ", \"rect\": {\"x\": " << a.rect.x
            << ", \"y\": " << a.rect.y
            << ", \"width\": " << a.rect.width
            << ", \"height\": " << a.rect.height << "}}";
        if (i + 1 < result.processed.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    return out.str();
}

AnnotationConfig DefaultAnnotationConfigForPdfX() {
    AnnotationConfig config {};
    config.defaultHandling = AnnotationHandling::Discard;
    config.discardFormFields = true;
    config.preserveLinks = false;
    config.preservePrinterMarks = true;
    config.flattenBeforeImposition = false;

    // Flatten stamps and watermarks so they become page content.
    AnnotationFilter stampFilter;
    stampFilter.type = AnnotationType::Stamp;
    stampFilter.handleAll = false;
    stampFilter.handling = AnnotationHandling::Flatten;
    config.filters.push_back(stampFilter);

    AnnotationFilter watermarkFilter;
    watermarkFilter.type = AnnotationType::Watermark;
    watermarkFilter.handleAll = false;
    watermarkFilter.handling = AnnotationHandling::Flatten;
    config.filters.push_back(watermarkFilter);

    return config;
}

} // namespace aimp

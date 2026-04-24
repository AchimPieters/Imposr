#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "aimp/ImpositionPlan.h"

namespace aimp {

enum class AnnotationHandling {
    Preserve,
    Discard,
    Flatten
};

enum class AnnotationType {
    Text,
    FreeText,
    Line,
    Square,
    Circle,
    Polygon,
    Ink,
    Highlight,
    Underline,
    StrikeOut,
    Stamp,
    Caret,
    Widget,
    Link,
    Movie,
    Sound,
    FileAttachment,
    Watermark,
    PrinterMark,
    Unknown
};

struct AnnotationFilter {
    AnnotationType type {AnnotationType::Unknown};
    bool handleAll {true};
    AnnotationHandling handling {AnnotationHandling::Discard};
};

struct AnnotationConfig {
    AnnotationHandling defaultHandling {AnnotationHandling::Discard};
    std::vector<AnnotationFilter> filters;
    bool discardFormFields {true};
    bool preserveLinks {false};
    bool preservePrinterMarks {true};
    bool flattenBeforeImposition {false};
};

struct AnnotationRecord {
    std::uint32_t pageIndex {0};
    AnnotationType type {AnnotationType::Unknown};
    std::string subtype;
    Rect rect;
    std::string contents;
    AnnotationHandling resolvedHandling {AnnotationHandling::Discard};
};

struct AnnotationProcessResult {
    std::vector<AnnotationRecord> processed;
    std::uint32_t discarded {0};
    std::uint32_t preserved {0};
    std::uint32_t flattened {0};
};

const char* AnnotationTypeName(AnnotationType type);
AnnotationType ParseAnnotationType(const std::string& subtype);

AnnotationHandling ResolveHandling(AnnotationType type, const AnnotationConfig& config);

AnnotationProcessResult ProcessAnnotations(const std::vector<AnnotationRecord>& annotations,
                                            const AnnotationConfig& config);

std::string AnnotationProcessResultToJson(const AnnotationProcessResult& result);

AnnotationConfig DefaultAnnotationConfigForPdfX();

} // namespace aimp

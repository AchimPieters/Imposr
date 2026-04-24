#include "aimp/AdjustPages.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>

namespace aimp {

namespace {

bool IsTargeted(const SlotPlacement& placement,
                const AdjustSpec& spec) {
    if (spec.applyToAllPlacements) return true;
    if (placement.sourcePage.pageIndex == kBlankPageIndex) return false;
    const auto& indices = spec.targetSourcePageIndices;
    return std::find(indices.begin(), indices.end(), placement.sourcePage.pageIndex) != indices.end();
}

} // namespace

AdjustResult ApplyAdjustSpec(ImpositionPlan& plan, const AdjustSpec& spec) {
    AdjustResult result {};

    for (auto& placement : plan.placements) {
        if (!IsTargeted(placement, spec)) continue;

        Rect& r = placement.targetRect;

        switch (spec.mode) {
            case AdjustMode::Scale:
                r.width *= spec.scaleX;
                r.height *= spec.scaleY;
                placement.scale *= std::min(spec.scaleX, spec.scaleY);
                break;

            case AdjustMode::Crop:
                if (spec.cropRect.width > 0.0 && spec.cropRect.height > 0.0) {
                    r.x += spec.cropRect.x;
                    r.y += spec.cropRect.y;
                    r.width = std::min(r.width - spec.cropRect.x, spec.cropRect.width);
                    r.height = std::min(r.height - spec.cropRect.y, spec.cropRect.height);
                    if (r.width <= 0.0 || r.height <= 0.0) {
                        result.warnings.push_back("Crop result has zero area for placement sheet="
                            + std::to_string(placement.sheetIndex));
                    }
                }
                break;

            case AdjustMode::Extend:
                r.x -= spec.extendLeft;
                r.y -= spec.extendBottom;
                r.width += spec.extendLeft + spec.extendRight;
                r.height += spec.extendBottom + spec.extendTop;
                break;

            case AdjustMode::ScaleToFit: {
                if (spec.targetWidth > 0.0 && spec.targetHeight > 0.0 &&
                    r.width > 0.0 && r.height > 0.0) {
                    const double sx = spec.targetWidth / r.width;
                    const double sy = spec.targetHeight / r.height;
                    const double s = std::min(sx, sy);
                    r.width *= s;
                    r.height *= s;
                    placement.scale *= s;
                }
                break;
            }

            case AdjustMode::ScaleToFill: {
                if (spec.targetWidth > 0.0 && spec.targetHeight > 0.0 &&
                    r.width > 0.0 && r.height > 0.0) {
                    const double sx = spec.targetWidth / r.width;
                    const double sy = spec.targetHeight / r.height;
                    const double s = std::max(sx, sy);
                    r.width *= s;
                    r.height *= s;
                    placement.scale *= s;
                }
                break;
            }
        }

        if (spec.rotationDegrees != 0.0) {
            placement.rotationDegrees = std::fmod(
                placement.rotationDegrees + spec.rotationDegrees, 360.0);
            if (placement.rotationDegrees < 0.0) placement.rotationDegrees += 360.0;
        }

        ++result.placementsModified;
    }

    return result;
}

std::string AdjustResultToJson(const AdjustResult& result) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"kind\": \"adjust-result\",\n";
    out << "  \"placementsModified\": " << result.placementsModified << ",\n";
    out << "  \"warningCount\": " << result.warnings.size() << ",\n";
    out << "  \"warnings\": [";
    for (std::size_t i = 0; i < result.warnings.size(); ++i) {
        if (i > 0) out << ", ";
        out << "\"" << result.warnings[i] << "\"";
    }
    out << "]\n";
    out << "}\n";
    return out.str();
}

} // namespace aimp

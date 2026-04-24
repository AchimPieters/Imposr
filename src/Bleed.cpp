#include "aimp/Bleed.h"

#include <sstream>

namespace aimp {

namespace {

const char* BleedMethodName(BleedMethod m) {
    switch (m) {
        case BleedMethod::Mirror: return "mirror";
        case BleedMethod::Scale: return "scale";
        case BleedMethod::Extend: return "extend";
        case BleedMethod::SolidColor: return "solid-color";
        default: return "unknown";
    }
}

} // namespace

Rect ComputeBleedBox(const Rect& trimBox, const BleedSpec& spec) noexcept {
    return Rect {
        trimBox.x - spec.left,
        trimBox.y - spec.bottom,
        trimBox.width + spec.left + spec.right,
        trimBox.height + spec.bottom + spec.top
    };
}

BleedResult GenerateBleedZones(const ImpositionPlan& plan, const BleedSpec& spec) {
    BleedResult result {};

    const bool hasBleed = spec.top > 0.0 || spec.bottom > 0.0 ||
                          spec.left > 0.0 || spec.right > 0.0;
    if (!hasBleed) {
        return result;
    }

    for (const auto& placement : plan.placements) {
        BleedZone zone {};
        zone.sheetIndex = placement.sheetIndex;
        zone.slotIndex = placement.slotIndex;
        zone.trimBox = placement.targetRect;
        zone.bleedBox = ComputeBleedBox(placement.targetRect, spec);
        zone.method = spec.method;
        result.zones.push_back(std::move(zone));
        ++result.zonesGenerated;
    }

    return result;
}

std::string RenderBleedPdfContent(const BleedResult& result,
                                   std::uint32_t sheetIndex,
                                   const BleedSpec& spec) {
    std::ostringstream c;
    c << "q\n";

    for (const auto& zone : result.zones) {
        if (zone.sheetIndex != sheetIndex) continue;

        const Rect& bb = zone.bleedBox;
        const Rect& tb = zone.trimBox;

        switch (zone.method) {
            case BleedMethod::SolidColor:
                c << spec.solidColorR << " " << spec.solidColorG << " " << spec.solidColorB << " rg\n";
                // Draw bleed margins as filled rects.
                if (spec.top > 0.0) {
                    c << tb.x << " " << (tb.y + tb.height) << " "
                      << tb.width << " " << spec.top << " re f\n";
                }
                if (spec.bottom > 0.0) {
                    c << bb.x << " " << bb.y << " "
                      << bb.width << " " << spec.bottom << " re f\n";
                }
                if (spec.left > 0.0) {
                    c << bb.x << " " << bb.y << " "
                      << spec.left << " " << bb.height << " re f\n";
                }
                if (spec.right > 0.0) {
                    c << (tb.x + tb.width) << " " << bb.y << " "
                      << spec.right << " " << bb.height << " re f\n";
                }
                break;

            case BleedMethod::Mirror:
                // Planning annotation only — actual pixel mirroring requires the Acrobat SDK
                // composer (M2 milestone). This comment is consumed by the native adapter.
                c << "% bleed-mirror trimBox=["
                  << tb.x << " " << tb.y << " " << tb.width << " " << tb.height << "] "
                  << "bleedBox=["
                  << bb.x << " " << bb.y << " " << bb.width << " " << bb.height << "]\n";
                break;

            case BleedMethod::Scale:
                // Planning annotation only — scale-based bleed fill requires the Acrobat SDK
                // composer (M2 milestone). This comment is consumed by the native adapter.
                c << "% bleed-scale trimBox=["
                  << tb.x << " " << tb.y << " " << tb.width << " " << tb.height << "] "
                  << "bleedBox=["
                  << bb.x << " " << bb.y << " " << bb.width << " " << bb.height << "]\n";
                break;

            case BleedMethod::Extend:
                // Planning annotation only — extend-based bleed fill requires the Acrobat SDK
                // composer (M2 milestone). This comment is consumed by the native adapter.
                c << "% bleed-extend trimBox=["
                  << tb.x << " " << tb.y << " " << tb.width << " " << tb.height << "] "
                  << "bleedBox=["
                  << bb.x << " " << bb.y << " " << bb.width << " " << bb.height << "]\n";
                break;
        }
    }

    c << "Q\n";
    return c.str();
}

std::string BleedZonesToJson(const BleedResult& result) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"kind\": \"bleed-zones\",\n";
    out << "  \"zonesGenerated\": " << result.zonesGenerated << ",\n";
    out << "  \"zones\": [\n";
    for (std::size_t i = 0; i < result.zones.size(); ++i) {
        const auto& z = result.zones[i];
        out << "    {\"sheetIndex\": " << z.sheetIndex
            << ", \"slotIndex\": " << z.slotIndex
            << ", \"method\": \"" << BleedMethodName(z.method) << "\""
            << ", \"trimBox\": {\"x\": " << z.trimBox.x
            << ", \"y\": " << z.trimBox.y
            << ", \"width\": " << z.trimBox.width
            << ", \"height\": " << z.trimBox.height << "}"
            << ", \"bleedBox\": {\"x\": " << z.bleedBox.x
            << ", \"y\": " << z.bleedBox.y
            << ", \"width\": " << z.bleedBox.width
            << ", \"height\": " << z.bleedBox.height << "}}";
        if (i + 1 < result.zones.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    return out.str();
}

} // namespace aimp

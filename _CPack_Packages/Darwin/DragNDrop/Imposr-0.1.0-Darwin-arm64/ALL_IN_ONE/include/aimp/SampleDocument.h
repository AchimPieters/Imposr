#pragma once

#include <cstdint>
#include <string>

namespace aimp {

struct SampleDocumentOptions {
    std::uint32_t pageCount {8};
    double pageWidthPoints  {595.28};   // A4 width
    double pageHeightPoints {841.89};   // A4 height
    bool   numberFromOne    {true};     // page labels start at 1
    bool   drawBorder       {true};
    bool   drawDiagonals    {false};
};

// Write a numbered test PDF to outputPath.
// Each page shows its page number centred, useful for verifying imposition layouts.
[[nodiscard]] bool CreateSampleDocument(const SampleDocumentOptions& options,
                                        const std::string& outputPath,
                                        std::string& errorMessage);

} // namespace aimp

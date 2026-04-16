#include "aimp/ImpositionPlan.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void PrintUsage() {
    std::cout
        << "Usage:\n"
        << "  imposr_cli two-up --pages <N> --sheet-width <pt> --sheet-height <pt> [--out <file>]\n"
        << "  imposr_cli n-up --pages <N> --sheet-width <pt> --sheet-height <pt> --columns <N> --rows <N> [--out <file>]\n"
        << "  imposr_cli booklet --pages <N> --sheet-width <pt> --sheet-height <pt> [--out <file>]\n";
}

bool ParseUInt(const std::string& value, std::uint32_t& output) {
    try {
        output = static_cast<std::uint32_t>(std::stoul(value));
        return true;
    } catch (...) {
        return false;
    }
}

bool ParseDouble(const std::string& value, double& output) {
    try {
        output = std::stod(value);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        PrintUsage();
        return 1;
    }

    const std::string mode = argv[1];
    std::uint32_t pages = 0;
    std::uint32_t columns = 0;
    std::uint32_t rows = 0;
    double sheetWidth = 0.0;
    double sheetHeight = 0.0;
    std::string outPath;

    for (int i = 2; i < argc; ++i) {
        const std::string key = argv[i];
        if (i + 1 >= argc) {
            std::cerr << "Missing value for argument: " << key << '\n';
            return 1;
        }

        const std::string value = argv[++i];
        if (key == "--pages") {
            if (!ParseUInt(value, pages)) {
                std::cerr << "Invalid value for --pages\n";
                return 1;
            }
        } else if (key == "--sheet-width") {
            if (!ParseDouble(value, sheetWidth)) {
                std::cerr << "Invalid value for --sheet-width\n";
                return 1;
            }
        } else if (key == "--sheet-height") {
            if (!ParseDouble(value, sheetHeight)) {
                std::cerr << "Invalid value for --sheet-height\n";
                return 1;
            }
        } else if (key == "--columns") {
            if (!ParseUInt(value, columns)) {
                std::cerr << "Invalid value for --columns\n";
                return 1;
            }
        } else if (key == "--rows") {
            if (!ParseUInt(value, rows)) {
                std::cerr << "Invalid value for --rows\n";
                return 1;
            }
        } else if (key == "--out") {
            outPath = value;
        } else {
            std::cerr << "Unknown argument: " << key << '\n';
            return 1;
        }
    }

    const aimp::SheetSize sheet {sheetWidth, sheetHeight};
    aimp::ImpositionPlan plan {};
    if (mode == "two-up") {
        plan = aimp::TwoUpPlanner::Build("cli-input", pages, sheet);
    } else if (mode == "n-up") {
        if (columns == 0 || rows == 0) {
            std::cerr << "n-up mode requires --columns and --rows\n";
            return 1;
        }
        plan = aimp::NUpPlanner::Build("cli-input", pages, sheet, columns, rows);
    } else if (mode == "booklet") {
        plan = aimp::BookletPlanner::Build("cli-input", pages, sheet);
    } else {
        std::cerr << "Unknown mode: " << mode << '\n';
        PrintUsage();
        return 1;
    }

    const std::string json = aimp::ToJson(plan);
    if (!outPath.empty()) {
        std::ofstream file(outPath);
        if (!file) {
            std::cerr << "Could not open output file: " << outPath << '\n';
            return 1;
        }
        file << json;
    } else {
        std::cout << json;
    }

    return 0;
}

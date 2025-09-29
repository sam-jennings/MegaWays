#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include "GameConfig.h"
#include "optimizer/ReelOptimizer.h"
#include "json.hpp"

namespace {
ReelSet loadBaseReelSet(GameConfig& cfg) {
    try {
        return cfg.parseReelSet("baseHigh");
    } catch (const std::exception&) {
    }
    try {
        return cfg.parseReelSet("base");
    } catch (const std::exception&) {
    }
    return cfg.parseReelSet("baseLow");
}

void applyOverrides(const nlohmann::json& j, ReelOptParams& params) {
    if (j.contains("evalSpins")) params.evalSpins = j["evalSpins"].get<uint64_t>();
    if (j.contains("maxIters")) params.maxIters = j["maxIters"].get<uint32_t>();
    if (j.contains("maxNoImprove")) params.maxNoImprove = j["maxNoImprove"].get<uint32_t>();
    if (j.contains("seed")) params.seed = j["seed"].get<uint64_t>();
    if (j.contains("targetBaseMin")) params.targetBaseMin = j["targetBaseMin"].get<double>();
    if (j.contains("targetBaseMax")) params.targetBaseMax = j["targetBaseMax"].get<double>();
    if (j.contains("targetTumbles")) params.targetTumbles = j["targetTumbles"].get<double>();
    if (j.contains("wBase")) params.wBase = j["wBase"].get<double>();
    if (j.contains("wTumble")) params.wTumble = j["wTumble"].get<double>();
    if (j.contains("wRtp")) params.wRtp = j["wRtp"].get<double>();
    if (j.contains("rtpMin")) params.rtpMin = j["rtpMin"].get<double>();
    if (j.contains("rtpMax")) params.rtpMax = j["rtpMax"].get<double>();
}

void printUsage() {
    std::cout << "Usage: reelopt [--config path] [--game config.json] [--spins N] [--seed S]\n";
}
}

int main(int argc, char** argv) {
    std::string toolConfigPath = "tools/reelopt/reelopt_config.json";
    std::string gameConfigPath = "config.json";
    std::optional<uint64_t> spinsOverride;
    std::optional<uint64_t> seedOverride;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            toolConfigPath = argv[++i];
        } else if (arg == "--game" && i + 1 < argc) {
            gameConfigPath = argv[++i];
        } else if (arg == "--spins" && i + 1 < argc) {
            spinsOverride = static_cast<uint64_t>(std::stoull(argv[++i]));
        } else if (arg == "--seed" && i + 1 < argc) {
            seedOverride = static_cast<uint64_t>(std::stoull(argv[++i]));
        } else if (arg == "--help") {
            printUsage();
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            printUsage();
            return 1;
        }
    }

    try {
        GameConfig cfg(gameConfigPath);
        ReelOptParams params;

        std::ifstream cfgStream(toolConfigPath);
        if (cfgStream.is_open()) {
            nlohmann::json j;
            cfgStream >> j;
            applyOverrides(j, params);
        }
        if (spinsOverride) {
            params.evalSpins = *spinsOverride;
        }
        if (seedOverride) {
            params.seed = *seedOverride;
        }

        ReelSet start = loadBaseReelSet(cfg);
        ReelOptimizer optimizer(cfg, params);
        ReelOptResult result = optimizer.run(start);

        std::filesystem::create_directories("out/reelopt");
        cfg.writeReelSetToFile(result.best, "out/reelopt/reelset_final.json", "optimized_base");

        std::cout << std::fixed << std::setprecision(4)
                  << "base=" << result.metrics.baseHitRate
                  << " tumbles=" << result.metrics.avgTumblesPerHit
                  << " rtp=" << result.metrics.rtp << "\n";

        bool success = result.metrics.baseHitRate >= params.targetBaseMin &&
                        result.metrics.baseHitRate <= params.targetBaseMax &&
                        result.metrics.avgTumblesPerHit >= params.targetTumbles;
        return success ? 0 : 2;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}

#include <cassert>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "RandomLogGenerator.h"
#include "GameConfig.h"
#include "optimizer/ReelOptimizer.h"

LogMode logMode = NO_LOGGING;

namespace {
std::vector<std::unordered_map<std::string, int>> countSymbols(const ReelSet& rs) {
    std::vector<std::unordered_map<std::string, int>> counts(rs.reels.size());
    for (size_t r = 0; r < rs.reels.size(); ++r) {
        for (const auto& sym : rs.reels[r].symbols) {
            counts[r][sym]++;
        }
    }
    return counts;
}

ReelSet loadBase(GameConfig& cfg) {
    try {
        return cfg.parseReelSet("baseHigh");
    } catch (...) {
    }
    try {
        return cfg.parseReelSet("base");
    } catch (...) {
    }
    return cfg.parseReelSet("baseLow");
}
}

int main() {
    GameConfig cfg("config.json");
    ReelSet original = loadBase(cfg);

    ReelOptParams params;
    params.evalSpins = 50000;
    params.maxIters = 1;
    params.maxNoImprove = 1;
    params.seed = 1337;

    ReelOptimizer optimizer(cfg, params);
    ReelOptResult result = optimizer.run(original);

    auto originalCounts = countSymbols(original);
    auto optimizedCounts = countSymbols(result.best);
    assert(originalCounts.size() == optimizedCounts.size());
    for (size_t i = 0; i < originalCounts.size(); ++i) {
        assert(originalCounts[i] == optimizedCounts[i]);
    }

    std::cout << "Test metrics: base=" << result.metrics.baseHitRate
              << " tumbles=" << result.metrics.avgTumblesPerHit
              << " rtp=" << result.metrics.rtp << "\n";
    return 0;
}

#include "../GameInstance.cpp"
#include "../src/optimizer/ReelOptimizer.cpp"

#include "GameInstance.h"
#include "RandomUtils.h"
#include "RandomLogGenerator.h"
#include <algorithm>
#include <numeric>
#include <unordered_map>

extern LogMode logMode;

namespace {
        void restoreReelSets(std::unordered_map<std::string, ReelSet>& target,
                const std::vector<std::pair<std::string, ReelSet>>& originals) {
                for (const auto& entry : originals) {
                        target[entry.first] = entry.second;
                }
        }
}

void GameInstance::setActiveBaseReelSet(const ReelSet& rs) {
        if (allReelSets.find("base") != allReelSets.end()) {
                allReelSets["base"] = rs;
        }
        if (allReelSets.find("baseHigh") != allReelSets.end()) {
                allReelSets["baseHigh"] = rs;
        }
        if (allReelSets.find("baseLow") != allReelSets.end()) {
                allReelSets["baseLow"] = rs;
        }
        baseReelSet = rs;
}

EvalMetrics GameInstance::evaluateReelSet(const ReelSet& rs, uint64_t spins, uint64_t seed) {
        EvalMetrics metrics;
        metrics.spins = spins;
        if (spins == 0) {
                return metrics;
        }

        std::vector<std::pair<std::string, ReelSet>> replaced;
        auto replaceIfExists = [&](const std::string& key) {
                auto it = allReelSets.find(key);
                if (it != allReelSets.end()) {
                        replaced.emplace_back(key, it->second);
                        it->second = rs;
                }
        };

        replaceIfExists("base");
        replaceIfExists("baseHigh");
        replaceIfExists("baseLow");
        baseReelSet = rs;

        auto previousState = getThreadRngState();
        seedThreadRng(seed);

        LogMode previousMode = logMode;
        logMode = NO_LOGGING;

        uint64_t baseHits = 0;
        uint64_t tumbleSum = 0;
        double totalPay = 0.0;

        boostPDVec.resize(boostWeights.size());
        for (size_t i = 0; i < boostWeights.size(); ++i) {
                boostPDVec[i] = PrizeDistribution<int>("BS_" + std::to_string(i + 1), std::vector<int>{0, 1}, boostWeights[i]);
        }

        for (uint64_t spin = 0; spin < spins; ++spin) {
                int globalMult = 1;
                RandomLogGenerator::startRound();

                std::vector<double> pays(payHeaders.size(), 0.0);
                std::vector<int> reelHeights(numReels);
                for (int r = 0; r < numReels; ++r) {
                        reelHeights[r] = reelHeightPD[r].getRandomPrize();
                }
                screen.resize(reelHeights);

                auto overIt = allReelSets.find("over");
                if (overIt != allReelSets.end()) {
                        overReelSet = overIt->second;
                        overReelSet.spinReels();
                }
                auto underIt = allReelSets.find("under");
                if (underIt != allReelSets.end()) {
                        underReelSet = underIt->second;
                        underReelSet.spinReels();
                }

                int reelID = 1;
                ReelSet activeReels;
                if (reelID == 0 && allReelSets.find("baseLow") != allReelSets.end()) {
                        activeReels = allReelSets["baseLow"];
                }
                else if (allReelSets.find("baseHigh") != allReelSets.end()) {
                        activeReels = allReelSets["baseHigh"];
                }
                else if (allReelSets.find("base") != allReelSets.end()) {
                        activeReels = allReelSets["base"];
                }
                else {
                        activeReels = rs;
                }

                activeReels.spinReels();

                boostVecOver.clear();
                boostVecUnder.clear();
                for (size_t b = 0; b < boostWeights.size(); ++b) {
                        boostVecOver.push_back(boostPDVec[b].getRandomPrize());
                        boostVecUnder.push_back(boostPDVec[b].getRandomPrize());
                }

                screen.generateScreen(activeReels);
                screen.addSideSymbols(true, overReelSet, boostVecOver);
                screen.addSideSymbols(false, underReelSet, boostVecUnder);

                int tumbleCount = 0;
                std::vector<double> baseVector = handleCascades(screen, activeReels, activeReels, false, true, globalMult, &tumbleCount);

                if (baseVector[0] > 0) {
                        ++baseHits;
                        tumbleSum += static_cast<uint64_t>(tumbleCount);
                }

                pays[BASE] += baseVector[0];
                pays[TUMBLE] += baseVector[1];

                RandomLogGenerator::endRound();

                pays[TOTAL] = std::accumulate(pays.begin(), pays.end() - 2, 0.0);
                totalPay += pays[TOTAL];
        }

        metrics.baseHitRate = static_cast<double>(baseHits) / static_cast<double>(spins);
        metrics.avgTumblesPerHit = baseHits ? static_cast<double>(tumbleSum) / static_cast<double>(baseHits) : 0.0;
        metrics.rtp = spins ? totalPay / (static_cast<double>(spins) * cost) : 0.0;

        logMode = previousMode;
        setThreadRngState(previousState);
        restoreReelSets(allReelSets, replaced);
        if (!replaced.empty()) {
                baseReelSet = replaced.front().second;
        }

        return metrics;
}

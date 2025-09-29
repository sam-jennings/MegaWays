#pragma once

#include "GameInstance.h"
#include "Stats.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

class RTPOptimizer {
public:
    struct OptimizationResult {
        std::vector<int> weights;
        double rtp;
    };

    struct Summary {
        OptimizationResult best;
        std::vector<OptimizationResult> allResults;
        std::size_t failedCandidates = 0;
    };

    RTPOptimizer(std::shared_ptr<GameConfig> config,
                 long long spinsPerCandidate,
                 int weightStep)
        : config(std::move(config)),
          symbolStructure(this->config->parseSymbolStructure()),
          rtpHeaders(this->config->getRTPHeaders()),
          costPerSpin(this->config->parseVar<double>("cost")),
          rtpKey(this->config->parseVar<std::string>("RTP")),
          spinsPerCandidate(spinsPerCandidate),
          weightStep(std::max(1, weightStep)) {
        if (this->spinsPerCandidate <= 0) {
            throw std::invalid_argument("spinsPerCandidate must be positive");
        }
    }

    Summary run() {
        auto candidates = buildCandidateWeights();
        if (candidates.empty()) {
            throw std::runtime_error("No reel weight candidates available for optimization.");
        }

        Summary summary;
        summary.best.rtp = std::numeric_limits<double>::lowest();

        for (const auto& weights : candidates) {
            try {
                double rtp = evaluateWeights(weights);
                summary.allResults.push_back({weights, rtp});
                if (rtp > summary.best.rtp) {
                    summary.best = {weights, rtp};
                }
            }
            catch (const std::exception& ex) {
                std::cerr << "Failed to evaluate weights [";
                for (std::size_t i = 0; i < weights.size(); ++i) {
                    if (i > 0) {
                        std::cerr << ", ";
                    }
                    std::cerr << weights[i];
                }
                std::cerr << "]: " << ex.what() << std::endl;
                summary.allResults.push_back({weights, std::numeric_limits<double>::quiet_NaN()});
                summary.failedCandidates++;
            }
        }

        std::sort(summary.allResults.begin(), summary.allResults.end(),
                  [](const OptimizationResult& lhs, const OptimizationResult& rhs) {
                      return lhs.rtp > rhs.rtp;
                  });

        return summary;
    }

    void printSummary(const Summary& summary, std::ostream& out = std::cout) const {
        if (summary.allResults.empty()) {
            out << "No optimizer results available." << std::endl;
            return;
        }

        std::vector<OptimizationResult> ranked;
        ranked.reserve(summary.allResults.size());
        for (const auto& result : summary.allResults) {
            if (!std::isfinite(result.rtp)) {
                continue;
            }
            bool seen = std::any_of(ranked.begin(), ranked.end(), [&](const OptimizationResult& existing) {
                return existing.weights == result.weights;
            });
            if (!seen) {
                ranked.push_back(result);
            }
        }

        out << "Optimizer evaluated " << summary.allResults.size()
            << " reel weight combinations" << std::endl;
        if (summary.failedCandidates > 0) {
            out << summary.failedCandidates << " combination(s) could not be evaluated." << std::endl;
        }

        if (ranked.empty()) {
            out << "No successful evaluations to report." << std::endl;
            out << "Spins per candidate: " << spinsPerCandidate
                << ", weight step: " << weightStep << std::endl;
            return;
        }

        out << "Top combination (estimated RTP):" << std::endl;
        out << "  " << formatResult(ranked.front()) << std::endl;

        std::size_t additional = std::min<std::size_t>(ranked.size(), static_cast<std::size_t>(3));
        if (additional > 1) {
            out << "Additional top combinations:" << std::endl;
            for (std::size_t i = 1; i < additional; ++i) {
                out << "  " << formatResult(ranked[i]) << std::endl;
            }
        }

        out << "Spins per candidate: " << spinsPerCandidate
            << ", weight step: " << weightStep << std::endl;
    }

private:
    std::shared_ptr<GameConfig> config;
    SymbolStructure symbolStructure;
    std::vector<std::string> rtpHeaders;
    double costPerSpin;
    std::string rtpKey;
    long long spinsPerCandidate;
    int weightStep;

    std::vector<std::vector<int>> buildCandidateWeights() const {
        std::vector<int> baseWeights = config->parseVec<int>("reelWeights", rtpKey);
        std::vector<std::vector<int>> candidates;

        if (baseWeights.empty()) {
            return candidates;
        }

        candidates.push_back(baseWeights);
        std::vector<int> current(baseWeights.size(), 0);
        generateWeightsRecursive(0, 100, current, candidates);
        return candidates;
    }

    void generateWeightsRecursive(int index,
                                  int remaining,
                                  std::vector<int>& current,
                                  std::vector<std::vector<int>>& candidates) const {
        if (index == static_cast<int>(current.size()) - 1) {
            current[index] = remaining;
            if (!containsCandidate(candidates, current)) {
                candidates.push_back(current);
            }
            return;
        }

        for (int value = 0; value <= remaining; value += weightStep) {
            current[index] = value;
            generateWeightsRecursive(index + 1, remaining - value, current, candidates);
        }
    }

    static bool containsCandidate(const std::vector<std::vector<int>>& candidates,
                                  const std::vector<int>& candidate) {
        return std::find(candidates.begin(), candidates.end(), candidate) != candidates.end();
    }

    double evaluateWeights(const std::vector<int>& weights) {
        if (std::accumulate(weights.begin(), weights.end(), 0) <= 0) {
            return std::numeric_limits<double>::lowest();
        }

        Stats stats(symbolStructure, rtpHeaders, costPerSpin, false);
        stats.setNumIterations(spinsPerCandidate);

        GameInstance instance(config, symbolStructure, stats);
        instance.setReelWeights(weights);
        instance.playBaseGame(spinsPerCandidate);

        return stats.getAverageRTP();
    }

    std::string formatResult(const OptimizationResult& result) const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6);
        oss << "RTP " << result.rtp << " with weights [";
        for (std::size_t i = 0; i < result.weights.size(); ++i) {
            if (i > 0) {
                oss << ", ";
            }
            oss << result.weights[i];
        }
        oss << "]";
        return oss.str();
    }
};

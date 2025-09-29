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
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

class RTPOptimizer {
public:
    struct OptimizationResult {
        std::vector<int> weights;
        std::string reelSetName;
        double rtp = std::numeric_limits<double>::quiet_NaN();

        bool isWeightBased() const { return !weights.empty(); }
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
        auto candidates = buildCandidates();
        if (candidates.empty()) {
            throw std::runtime_error("No optimizer candidates available. Configure reel sets or weights.");
        }

        Summary summary;
        summary.best.rtp = std::numeric_limits<double>::lowest();

        for (const auto& candidate : candidates) {
            try {
                double rtp = evaluateCandidate(candidate);
                OptimizationResult result;
                if (candidate.kind == Candidate::Kind::Weights) {
                    result.weights = candidate.weights;
                }
                else {
                    result.reelSetName = candidate.reelSetName;
                }
                result.rtp = rtp;
                summary.allResults.push_back(result);
                if (rtp > summary.best.rtp) {
                    summary.best = result;
                }
            }
            catch (const std::exception& ex) {
                OptimizationResult failed;
                if (candidate.kind == Candidate::Kind::Weights) {
                    failed.weights = candidate.weights;
                    std::cerr << "Failed to evaluate weights [";
                    for (std::size_t i = 0; i < candidate.weights.size(); ++i) {
                        if (i > 0) {
                            std::cerr << ", ";
                        }
                        std::cerr << candidate.weights[i];
                    }
                    std::cerr << "]";
                }
                else {
                    failed.reelSetName = candidate.reelSetName;
                    std::cerr << "Failed to evaluate reel set '" << candidate.reelSetName << "'";
                }
                std::cerr << ": " << ex.what() << std::endl;
                failed.rtp = std::numeric_limits<double>::quiet_NaN();
                summary.allResults.push_back(failed);
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
                return existing.weights == result.weights && existing.reelSetName == result.reelSetName;
            });
            if (!seen) {
                ranked.push_back(result);
            }
        }

        out << "Optimizer evaluated " << summary.allResults.size()
            << " candidate" << (summary.allResults.size() == 1 ? "" : "s") << std::endl;
        if (summary.failedCandidates > 0) {
            out << summary.failedCandidates << " candidate" << (summary.failedCandidates == 1 ? " was" : "s were")
                << " not evaluated successfully." << std::endl;
        }

        if (ranked.empty()) {
            out << "No successful evaluations to report." << std::endl;
            out << "Spins per candidate: " << spinsPerCandidate
                << ", weight step: " << weightStep << std::endl;
            return;
        }

        out << "Top candidate (estimated RTP):" << std::endl;
        out << "  " << formatResult(ranked.front()) << std::endl;

        std::size_t additional = std::min<std::size_t>(ranked.size(), static_cast<std::size_t>(3));
        if (additional > 1) {
            out << "Additional top candidates:" << std::endl;
            for (std::size_t i = 1; i < additional; ++i) {
                out << "  " << formatResult(ranked[i]) << std::endl;
            }
        }

        out << "Spins per candidate: " << spinsPerCandidate;
        if (summary.best.isWeightBased()) {
            out << ", weight step: " << weightStep;
        }
        out << std::endl;
    }

private:
    std::shared_ptr<GameConfig> config;
    SymbolStructure symbolStructure;
    std::vector<std::string> rtpHeaders;
    double costPerSpin;
    std::string rtpKey;
    long long spinsPerCandidate;
    int weightStep;

    struct Candidate {
        enum class Kind { Weights, ReelSet };

        static Candidate fromWeights(const std::vector<int>& w) {
            Candidate c;
            c.kind = Kind::Weights;
            c.weights = w;
            return c;
        }

        static Candidate fromReelSet(std::string name) {
            Candidate c;
            c.kind = Kind::ReelSet;
            c.reelSetName = std::move(name);
            return c;
        }

        Kind kind = Kind::Weights;
        std::vector<int> weights;
        std::string reelSetName;
    };

    std::vector<Candidate> buildCandidates() const {
        std::vector<Candidate> candidates;

        if (auto reelSets = config->getOptional<std::vector<std::string>>("optimizer/reelSets")) {
            for (const auto& name : *reelSets) {
                if (!config->hasReelSet(name)) {
                    throw std::invalid_argument("Unknown reel set configured for optimization: " + name);
                }
                candidates.push_back(Candidate::fromReelSet(name));
            }
            return candidates;
        }

        if (auto singleReelSet = config->getOptional<std::string>("optimizer/reelSet")) {
            if (!config->hasReelSet(*singleReelSet)) {
                throw std::invalid_argument("Unknown reel set configured for optimization: " + *singleReelSet);
            }
            candidates.push_back(Candidate::fromReelSet(*singleReelSet));
            return candidates;
        }

        std::vector<int> baseWeights;
        try {
            baseWeights = config->parseVec<int>("reelWeights", rtpKey);
        }
        catch (const std::exception&) {
            baseWeights.clear();
        }

        if (baseWeights.empty()) {
            return candidates;
        }

        candidates.push_back(Candidate::fromWeights(baseWeights));
        std::vector<int> current(baseWeights.size(), 0);
        generateWeightsRecursive(0, 100, current, candidates);
        return candidates;
    }

    void generateWeightsRecursive(int index,
                                  int remaining,
                                  std::vector<int>& current,
                                  std::vector<Candidate>& candidates) const {
        if (index == static_cast<int>(current.size()) - 1) {
            current[index] = remaining;
            if (!containsWeightsCandidate(candidates, current)) {
                candidates.push_back(Candidate::fromWeights(current));
            }
            return;
        }

        for (int value = 0; value <= remaining; value += weightStep) {
            current[index] = value;
            generateWeightsRecursive(index + 1, remaining - value, current, candidates);
        }
    }

    static bool containsWeightsCandidate(const std::vector<Candidate>& candidates,
                                         const std::vector<int>& candidate) {
        return std::any_of(candidates.begin(), candidates.end(), [&](const Candidate& existing) {
            return existing.kind == Candidate::Kind::Weights && existing.weights == candidate;
        });
    }

    double evaluateCandidate(const Candidate& candidate) {
        Stats stats(symbolStructure, rtpHeaders, costPerSpin, false);
        stats.setNumIterations(spinsPerCandidate);

        GameInstance instance(config, symbolStructure, stats);

        switch (candidate.kind) {
        case Candidate::Kind::Weights: {
            if (std::accumulate(candidate.weights.begin(), candidate.weights.end(), 0) <= 0) {
                return std::numeric_limits<double>::lowest();
            }
            instance.setReelWeights(candidate.weights);
            break;
        }
        case Candidate::Kind::ReelSet:
            instance.forceBaseReelSet(candidate.reelSetName);
            break;
        }

        instance.playBaseGame(spinsPerCandidate);
        return stats.getAverageRTP();
    }

    std::string formatResult(const OptimizationResult& result) const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6);
        oss << "RTP " << result.rtp;
        if (!result.reelSetName.empty()) {
            oss << " using reel set \"" << result.reelSetName << "\"";
        }
        else {
            oss << " with weights [";
            for (std::size_t i = 0; i < result.weights.size(); ++i) {
                if (i > 0) {
                    oss << ", ";
                }
                oss << result.weights[i];
            }
            oss << "]";
        }
        return oss.str();
    }
};

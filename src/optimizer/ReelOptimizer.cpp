#include "optimizer/ReelOptimizer.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <numeric>
#include <sstream>

ReelOptimizer::ReelOptimizer(GameConfig& cfg, const ReelOptParams& p)
    : cfg_(cfg),
      params_(p),
      symbolStructure_(cfg.parseSymbolStructure()),
      stats_(symbolStructure_, cfg.getRTPHeaders(), cfg.parseVar<double>("cost")),
      cfgPtr_(&cfg, [](GameConfig*) {}),
      instance_(std::make_unique<GameInstance>(cfgPtr_, symbolStructure_, stats_)) {}

ReelSet ReelOptimizer::applySeedHeuristics(const ReelSet& in) const {
    ReelSet out = in;

    for (size_t reelIdx = 0; reelIdx < out.reels.size(); ++reelIdx) {
        auto& symbols = out.reels[reelIdx].symbols;
        if (symbols.empty()) {
            continue;
        }
        size_t shift = reelIdx % symbols.size();
        if (shift > 0) {
            std::rotate(symbols.begin(), symbols.begin() + shift, symbols.end());
        }
    }

    for (auto& reel : out.reels) {
        auto& symbols = reel.symbols;
        if (symbols.size() < 3) {
            continue;
        }
        bool adjusted = true;
        int guard = 0;
        while (adjusted && guard < 3) {
            adjusted = false;
            size_t i = 0;
            while (i < symbols.size()) {
                size_t j = i + 1;
                while (j < symbols.size() && symbols[j] == symbols[i]) {
                    ++j;
                }
                size_t len = j - i;
                if (len > 2) {
                    size_t mid = i + len / 2;
                    size_t swapIdx = (mid + 1) % symbols.size();
                    size_t attempts = 0;
                    while (attempts < symbols.size() && symbols[swapIdx] == symbols[mid]) {
                        swapIdx = (swapIdx + 1) % symbols.size();
                        ++attempts;
                    }
                    if (attempts < symbols.size()) {
                        std::swap(symbols[mid], symbols[swapIdx]);
                        adjusted = true;
                        break;
                    }
                }
                i = j;
            }
            ++guard;
        }
    }

    auto isPremium = [](const std::string& sym) {
        return !sym.empty() && sym[0] == 'R';
    };

    if (out.reels.size() >= 3) {
        size_t limit = std::min({ out.reels[0].symbols.size(), out.reels[1].symbols.size(), out.reels[2].symbols.size() });
        for (size_t idx = 0; idx < limit; ++idx) {
            const std::string& refSymbol = out.reels[0].symbols[idx];
            if (!isPremium(refSymbol)) {
                continue;
            }
            if (out.reels[1].symbols.size() > 1 && out.reels[1].symbols[idx] == refSymbol) {
                size_t swapIdx = (idx + 1) % out.reels[1].symbols.size();
                size_t attempts = 0;
                while (attempts < out.reels[1].symbols.size() && out.reels[1].symbols[swapIdx] == refSymbol) {
                    swapIdx = (swapIdx + 1) % out.reels[1].symbols.size();
                    ++attempts;
                }
                if (attempts < out.reels[1].symbols.size()) {
                    std::swap(out.reels[1].symbols[idx], out.reels[1].symbols[swapIdx]);
                }
            }
            if (out.reels[2].symbols.size() > 1 && out.reels[2].symbols[idx] == refSymbol) {
                size_t swapIdx = (idx + 2) % out.reels[2].symbols.size();
                size_t attempts = 0;
                while (attempts < out.reels[2].symbols.size() && out.reels[2].symbols[swapIdx] == refSymbol) {
                    swapIdx = (swapIdx + 1) % out.reels[2].symbols.size();
                    ++attempts;
                }
                if (attempts < out.reels[2].symbols.size()) {
                    std::swap(out.reels[2].symbols[idx], out.reels[2].symbols[swapIdx]);
                }
            }
        }
    }

    return out;
}

double ReelOptimizer::fitness(const EvalMetrics& m) const {
    double score = 0.0;
    if (m.baseHitRate < params_.targetBaseMin) {
        score -= params_.wBase * (params_.targetBaseMin - m.baseHitRate);
    } else if (m.baseHitRate > params_.targetBaseMax) {
        score -= params_.wBase * (m.baseHitRate - params_.targetBaseMax);
    }
    score += params_.wTumble * std::max(0.0, m.avgTumblesPerHit - params_.targetTumbles);
    if (params_.wRtp > 0.0) {
        if (m.rtp < params_.rtpMin) {
            score -= params_.wRtp * (params_.rtpMin - m.rtp);
        }
        if (m.rtp > params_.rtpMax) {
            score -= params_.wRtp * (m.rtp - params_.rtpMax);
        }
    }
    return score;
}

ReelSet ReelOptimizer::proposeNeighbor(const ReelSet& in, std::mt19937_64& rng, std::string& moveDesc, int& reelIdx, int& a, int& b) const {
    ReelSet candidate = in;
    moveDesc = "noop";
    reelIdx = -1;
    a = -1;
    b = -1;

    if (candidate.reels.empty()) {
        return candidate;
    }

    std::uniform_int_distribution<size_t> reelDist(0, candidate.reels.size() - 1);
    size_t selectedReel = reelDist(rng);
    auto& symbols = candidate.reels[selectedReel].symbols;
    if (symbols.size() < 2) {
        moveDesc = "rotate";
        reelIdx = static_cast<int>(selectedReel);
        return candidate;
    }

    std::uniform_int_distribution<int> moveDist(0, 2);
    int moveType = moveDist(rng);

    if (moveType == 0) {
        std::uniform_int_distribution<size_t> indexDist(0, symbols.size() - 1);
        size_t i = indexDist(rng);
        size_t j = indexDist(rng);
        while (j == i) {
            j = indexDist(rng);
        }
        std::swap(symbols[i], symbols[j]);
        moveDesc = "swap";
        reelIdx = static_cast<int>(selectedReel);
        a = static_cast<int>(i);
        b = static_cast<int>(j);
    } else if (moveType == 1) {
        size_t maxShift = std::max<size_t>(1, symbols.size() / 6);
        std::uniform_int_distribution<size_t> shiftDist(1, maxShift);
        size_t shift = std::min<size_t>(symbols.size() - 1, shiftDist(rng));
        std::bernoulli_distribution dir(0.5);
        bool right = dir(rng);
        if (right) {
            std::rotate(symbols.rbegin(), symbols.rbegin() + static_cast<std::ptrdiff_t>(shift), symbols.rend());
        } else {
            std::rotate(symbols.begin(), symbols.begin() + static_cast<std::ptrdiff_t>(shift), symbols.end());
        }
        moveDesc = "rotate";
        reelIdx = static_cast<int>(selectedReel);
        a = static_cast<int>(shift);
        b = right ? 1 : -1;
    } else {
        bool changed = false;
        size_t i = 0;
        while (i < symbols.size()) {
            size_t j = i + 1;
            while (j < symbols.size() && symbols[j] == symbols[i]) {
                ++j;
            }
            size_t len = j - i;
            if (len > 2) {
                size_t mid = i + len / 2;
                size_t swapIdx = (mid + 1) % symbols.size();
                if (symbols[swapIdx] == symbols[mid]) {
                    swapIdx = (swapIdx + 1) % symbols.size();
                }
                std::swap(symbols[mid], symbols[swapIdx]);
                moveDesc = "break";
                reelIdx = static_cast<int>(selectedReel);
                a = static_cast<int>(mid);
                b = static_cast<int>(swapIdx);
                changed = true;
                break;
            }
            i = j;
        }
        if (!changed) {
            std::uniform_int_distribution<size_t> indexDist(0, symbols.size() - 1);
            size_t iSwap = indexDist(rng);
            size_t jSwap = indexDist(rng);
            while (jSwap == iSwap) {
                jSwap = indexDist(rng);
            }
            std::swap(symbols[iSwap], symbols[jSwap]);
            moveDesc = "swap";
            reelIdx = static_cast<int>(selectedReel);
            a = static_cast<int>(iSwap);
            b = static_cast<int>(jSwap);
        }
    }

    return candidate;
}

bool ReelOptimizer::acceptWorse(double delta, uint32_t iter, std::mt19937_64& rng) const {
    if (delta <= 0.0) {
        return true;
    }
    const double T0 = 0.1;
    double tau = std::max<uint32_t>(1u, params_.maxIters / 2u);
    double temperature = T0 * std::exp(-static_cast<double>(iter) / static_cast<double>(tau));
    if (temperature <= 1e-9) {
        return false;
    }
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    double probability = std::exp(-delta / temperature);
    return dist(rng) < probability;
}

ReelOptResult ReelOptimizer::run(const ReelSet& start) {
    std::filesystem::create_directories("out/reelopt");
    std::ofstream csv("out/reelopt/iterations.csv", std::ios::trunc);
    if (csv.is_open()) {
        csv << "iter,accept,base_hit_rate,avg_tumbles_per_hit,rtp,score,move,reel,indexA,indexB,seed\n";
    }

    ReelSet current = applySeedHeuristics(start);
    std::mt19937_64 rng(params_.seed);

    EvalMetrics currentMetrics = instance_->evaluateReelSet(current, params_.evalSpins, params_.seed);
    double currentScore = fitness(currentMetrics);
    ReelSet best = current;
    EvalMetrics bestMetrics = currentMetrics;
    double bestScore = currentScore;

    uint32_t logIter = 0;
    auto logEntry = [&](const EvalMetrics& m, bool accept, const std::string& move, int reelIdx, int ia, int ib, uint64_t seed, double score) {
        if (!csv.is_open()) {
            return;
        }
        std::ostringstream oss;
        oss << logIter++ << "," << (accept ? 1 : 0) << ",";
        oss << std::fixed << std::setprecision(6) << m.baseHitRate << ",";
        oss << std::fixed << std::setprecision(6) << m.avgTumblesPerHit << ",";
        oss << std::fixed << std::setprecision(6) << m.rtp << ",";
        oss << std::fixed << std::setprecision(6) << score << ",";
        oss << move << "," << reelIdx << "," << ia << "," << ib << "," << seed;
        csv << oss.str() << "\n";
    };

    logEntry(currentMetrics, true, "seed", -1, -1, -1, params_.seed, currentScore);

    uint32_t stagnant = 0;

    for (uint32_t iter = 1; iter <= params_.maxIters; ++iter) {
        std::string move;
        int reelIdx = -1;
        int ia = -1;
        int ib = -1;
        ReelSet candidate = proposeNeighbor(current, rng, move, reelIdx, ia, ib);
        uint64_t evalSeed = params_.seed + iter;
        EvalMetrics candMetrics = instance_->evaluateReelSet(candidate, params_.evalSpins, evalSeed);
        double candScore = fitness(candMetrics);
        bool accept = false;
        if (candScore >= currentScore) {
            accept = true;
        } else if (acceptWorse(currentScore - candScore, iter, rng)) {
            accept = true;
        }

        logEntry(candMetrics, accept, move, reelIdx, ia, ib, evalSeed, candScore);

        if (accept) {
            current = candidate;
            currentMetrics = candMetrics;
            currentScore = candScore;
        }

        if (candScore > bestScore) {
            best = candidate;
            bestMetrics = candMetrics;
            bestScore = candScore;
            stagnant = 0;
        } else {
            ++stagnant;
        }

        if (bestMetrics.baseHitRate >= params_.targetBaseMin && bestMetrics.baseHitRate <= params_.targetBaseMax &&
            bestMetrics.avgTumblesPerHit >= params_.targetTumbles) {
            break;
        }

        if (stagnant >= params_.maxNoImprove) {
            ReelSet restart = current;
            for (auto& reel : restart.reels) {
                if (reel.symbols.size() > 1) {
                    std::uniform_int_distribution<size_t> shiftDist(0, reel.symbols.size() - 1);
                    size_t shift = shiftDist(rng);
                    std::rotate(reel.symbols.begin(), reel.symbols.begin() + static_cast<std::ptrdiff_t>(shift), reel.symbols.end());
                }
            }
            uint64_t restartSeed = params_.seed + iter + 0x10000u;
            current = restart;
            currentMetrics = instance_->evaluateReelSet(current, params_.evalSpins, restartSeed);
            currentScore = fitness(currentMetrics);
            logEntry(currentMetrics, true, "restart", -1, -1, -1, restartSeed, currentScore);
            if (currentScore > bestScore) {
                best = current;
                bestMetrics = currentMetrics;
                bestScore = currentScore;
            }
            stagnant = 0;
        }
    }

    return { best, bestMetrics };
}

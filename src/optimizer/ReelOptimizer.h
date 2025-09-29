#pragma once

#include <vector>
#include <string>
#include <random>
#include <memory>

#include "Symbols.h"
#include "GameInstance.h"
#include "Stats.h"

struct ReelOptParams {
  double targetBaseMin = 1.0 / 5.5;
  double targetBaseMax = 1.0 / 4.5;
  double targetTumbles = 1.7;
  double wBase = 1.0;
  double wTumble = 1.0;
  double wRtp = 0.25;
  double rtpMin = 0.0;
  double rtpMax = 10.0;
  uint64_t evalSpins = 300000;
  uint32_t maxIters = 2000;
  uint32_t maxNoImprove = 150;
  uint64_t seed = 12345;
};

struct ReelOptResult {
  ReelSet best;
  EvalMetrics metrics;
};

class ReelOptimizer {
public:
  ReelOptimizer(GameConfig& cfg, const ReelOptParams& p);

  ReelOptResult run(const ReelSet& start);

private:
  double fitness(const EvalMetrics& m) const;
  ReelSet applySeedHeuristics(const ReelSet& in) const;
  ReelSet proposeNeighbor(const ReelSet& in, std::mt19937_64& rng, std::string& moveDesc, int& reelIdx, int& a, int& b) const;
  bool acceptWorse(double delta, uint32_t iter, std::mt19937_64& rng) const;

  GameConfig& cfg_;
  ReelOptParams params_;
  SymbolStructure symbolStructure_;
  Stats stats_;
  std::shared_ptr<GameConfig> cfgPtr_;
  std::unique_ptr<GameInstance> instance_;
};

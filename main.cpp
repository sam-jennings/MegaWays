#include <algorithm>
#include <iostream>
#include <memory>

#include "Optimizer.h"
#include "RandomLogGenerator.h"

LogMode logMode = NO_LOGGING;

int main() {
    try {
        auto config = std::make_shared<GameConfig>("config.json");

        long long requestedSpins = 0;
        try {
            requestedSpins = config->parseVar<long long>("simulations");
        }
        catch (const std::exception&) {
            requestedSpins = 10000;
        }

        long long spinsPerCandidate = std::clamp<long long>(requestedSpins, 100LL, 2000LL);
        int weightStep = 50;

        RTPOptimizer optimizer(config, spinsPerCandidate, weightStep);
        auto summary = optimizer.run();
        optimizer.printSummary(summary);
    }
    catch (const std::exception& ex) {
        std::cerr << "Optimizer failed: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}

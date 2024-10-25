#pragma once
#include "GameConfig.h"
#include "Stats.h"
#include "Screen.h"



class GameInstance {
    friend class GameConfig; // Declare GameConfig as a friend
public:
    explicit GameInstance(const GameConfig& config, int numIterations) : config(config), gameStats(config, numIterations), debugFile("debug.txt") { // Initialize with config
        initializeGame();
    }

    ~GameInstance() {
        if (debugFile.is_open()) {
            debugFile.close(); // Ensure the file is closed in the destructor
        }
    }

    Stats gameStats; // Stats object for tracking game statistics
    int numRows;
    int numReels;
    PrizeDistribution<int> wheelActivation, freeActivation, wildWinMult, wildWinMultFree, expWinMult, expWinMultFree, fgPrizeDistFree, directPrize, directPrizeFree, jackpotPrize, jackpotPrizeFree;
    PrizeDistribution<std::pair<int, int>> fgPrizeDist;

    Screen screen;
    int cost;
    SymbolStructure symbolStructure;
    ReelSet baseReelSet, freeReelSet, baseReelsLow, baseReelsHigh, tumbleReelSet, tumbleReelsLow, tumbleReelsHigh, tumbleFree, freeLow, freeHigh, wildWinsReels, expandedReels, premiumReels;
    vector<int> reelWeights, reelWeightsFree;
    vector<string> payHeaders;
    vector<std::string> symbols;
    std::map<std::string, std::vector<int>>paytable;
    ofstream debugFile;

    // Method to simulate a single spin and return the result
    double simulateSingleSpin() {
        playBaseGame(1);  // Simulate one spin
        double lastSpinPayout = gameStats.getLastSpinPayout();  // Retrieve payout from the last spin
        return lastSpinPayout - cost;  // Return net gain/loss (payout minus cost of one spin)
    }


    void playBaseGame(long long numSpins);

    double calculateWaysWins(Screen& screen, bool baseGame);

    vector<double> handleCascades(Screen& screen, ReelSet& reelSet, ReelSet& offScreenReelSet, bool useDifferentReelSet, bool baseGame);

    vector<double> playFreeGames(int numFreeGames, int numPremiumSpins);

private:
    GameConfig config; // Configuration for this game instance

    // Private methods for internal game mechanics
    void initializeGame() {
        // Use GameConfig methods to initialize game components
        numRows = config.numRows;
        numReels = config.numReels;
        wheelActivation = config.wheelActivation;
        freeActivation = config.freeActivation;
        payHeaders = config.payHeaders;
        symbolStructure = config.symbolStructure;
        baseReelsLow = config.baseReelsLow;
        baseReelsHigh = config.baseReelsHigh;
        tumbleReelsHigh = config.tumbleReelsHigh;
        tumbleReelsLow = config.tumbleReelsLow;
        tumbleFree = config.tumbleFree;
        freeLow = config.freeLow;
        freeHigh = config.freeHigh;
        premiumReels = config.premiumReels;
        wildWinsReels = config.wildWinsReels;
        expandedReels = config.expandedReels;
        wildWinMult = config.wildWinMult;
        wildWinMultFree = config.wildWinMultFree;
        expWinMult = config.expWinMult;
        expWinMultFree = config.expWinMultFree;
        directPrize = config.directPrize;
        directPrizeFree = config.directPrizeFree;
        jackpotPrize = config.jackpotPrize;
        jackpotPrizeFree = config.jackpotPrizeFree;
        fgPrizeDist = config.fgPrizeDist;
        fgPrizeDistFree = config.fgPrizeDistFree;
        reelWeights = config.reelWeights;
        reelWeightsFree = config.reelWeightsFree;
        screen.resize(numReels, numRows);
        cost = config.cost;
        symbols = symbolStructure.getSymbols();
        paytable = symbolStructure.getPaytable();

        if (!debugFile.is_open()) {
            cerr << "Failed to open debug file." << endl;
        }
    }

};

void GameInstance::playBaseGame(long long numSpins) {
    bool addMult;
    vector<double> baseVector;

    double temp_pay, multiplier;
    vector<double> temp_pays, freePays;

    for (long long i = 0; i < numSpins; ++i) {
        RandomLogGenerator::startRound();
        addMult = true;
        vector<double> pays(payHeaders.size(), 0); // {base_pay, free_pay, scatter_pay, total_pay}

        // Choose reelset. 
        if (getRand("R-WTS", reelWeights[0] + reelWeights[1]) < reelWeights[0]) {
            baseReelSet = baseReelsLow;
            tumbleReelSet = tumbleReelsLow;

        }
        else {
            baseReelSet = baseReelsHigh;
            tumbleReelSet = tumbleReelsLow;

        }

        if (getRand("FR-WTS", reelWeightsFree[0] + reelWeightsFree[1]) < reelWeightsFree[0]) {
            freeReelSet = freeLow;
        }
        else {
            freeReelSet = freeHigh;
        }
        /*  baseReelSet = baseReelsLow;
          tumbleReelSet = tumbleReelsLow;*/

          // Clear the screen before each spin (not necessary)
        Screen screen(numReels, numRows);
        screen.clearScreen();

        // Spin reels and fill the screen with symbols
        baseReelSet.spinReels();
        screen.generateScreen(baseReelSet);



        baseVector = handleCascades(screen, baseReelSet, tumbleReelSet, true, true);
        RandomLogGenerator::addWinAmount(baseVector[0] + baseVector[1]);

        pays[0] = baseVector[0];
        pays[1] = baseVector[1];

        //If 3 FG symbols appear, trigger a ladder
        int fgCount = screen.countSymbolOnScreen("F1", false);
        if (fgCount == 3) {
            gameStats.activateBonusGame(4, true);

        }

        RandomLogGenerator::endRound();
        pays[pays.size() - 1] = std::accumulate(pays.begin(), pays.end() - 1, 0.0);
        // pays[6] = std::accumulate(pays.begin(), pays.end() - 5, 0.0);

        gameStats.completeWager(pays);
    }

}

vector<double> GameInstance::handleCascades(Screen& screen, ReelSet& reelSet, ReelSet& offScreenReelSet, bool useDifferentReelSet, bool baseGame) {
    bool hasNewWins;
    double initialWin = 0, tumbleWin = 0;
    int tumbleCount = 0;


    // If we're using a different reel set, spin it once before cascading
    if (useDifferentReelSet) {
        offScreenReelSet.spinReels(); //removed const to make work
    }

    do {
        hasNewWins = false;
        screen.clearMarkedPositions(); // Clear previous positions

        if (tumbleCount == 0) {
            initialWin = calculateWaysWins(screen, baseGame);
        }
        else {
            tumbleWin += calculateWaysWins(screen, baseGame);
        }


        if (!screen.getMarkedPositions().empty()) {
            hasNewWins = true;
            tumbleCount++;
            screen.removeMarkedPositions();  // Remove the symbols at winning positions        
            screen.cascadeSymbols(reelSet, useDifferentReelSet, offScreenReelSet);  // Cascade new symbols down

        }
    } while (hasNewWins);

    if (initialWin && baseGame) {
        gameStats.recordTumbleFrequency(tumbleCount);
    }

    return { initialWin, tumbleWin };
}

double GameInstance::calculateWaysWins(Screen& screen, bool baseGame) {
    double totalPay = 0;
    int multiplier = 1;


    RandomLogGenerator::addScreen(screen.toJson());
    // Clear previous marked positions
    screen.clearMarkedPositions();

    for (const auto& symbol : symbols) {
        auto waysInfo = screen.getWaysForSymbol(symbol);
        int length = waysInfo.first;
        int ways = waysInfo.second;
        int payout = 0;

        if (length > 0) {
            payout = multiplier * ways * paytable[symbol][length - 1];
            if (payout > 0) {
                gameStats.trackResult(symbol, length, ways, payout, baseGame);

                screen.markSymbol(symbol, length);
            }
        }
        totalPay += payout;
    }


    //RandomLogGenerator::addWinAmount(totalPay);
    return totalPay;
}


vector<double> GameInstance::playFreeGames(int numFreeGames, int numPremiumSpins) {
    vector<double> pays(5, 0); // {baseWin, tumbleWin, wildWin, expandedWin, freeGamesWon}
    vector<double> tempPays;
    double tempPay, multiplier;


    vector<int> premiumSpins = getRandomPositions("PremChoices", numFreeGames, numPremiumSpins);
    std::sort(premiumSpins.begin(), premiumSpins.end());

    Screen screen(numReels, numRows);
    screen.clearScreen();

    int premiumIndex = 0;
    int currentFreeGame = 0;

    while (currentFreeGame < numFreeGames) {
        RandomLogGenerator::newSpin();

        if (premiumIndex < premiumSpins.size() && currentFreeGame == premiumSpins[premiumIndex]) {
            premiumReels.spinReels();
            screen.generateScreen(premiumReels);
            premiumIndex++;
        }
        else {
            freeReelSet.spinReels();
            screen.generateScreen(freeReelSet);
        }

        // Add the screen to the log
       // RandomLogGenerator::addScreen(screen.toJson());

        tempPays = handleCascades(screen, freeReelSet, tumbleFree, true, false);
        pays[0] += tempPays[0]; // baseWin
        pays[1] += tempPays[1]; // tumbleWin
        RandomLogGenerator::addWinAmount(tempPays[0] + tempPays[1]);

        // Check for free game symbols and trigger additional features
        int fgCount = screen.countSymbolOnScreen("F1", false);
        if (fgCount == 3) {

        }

        currentFreeGame++;
    }

    return pays;
}

class PlayerSimulation {
public:
    PlayerSimulation(int startCredits, int targetCredits, GameConfig& config)
        : startCredits(startCredits), targetCredits(targetCredits), gameConfig(config) {}

    bool simulate() {
        int currentCredits = startCredits;
        GameInstance gameInstance(gameConfig, 1); // Prepared to simulate game spins

        while (currentCredits > 0 && currentCredits < targetCredits) {

            double result = gameInstance.simulateSingleSpin();  // Simulate spin and get net result            
            currentCredits += result;  // Update current credits
            // cout << "Result: " << result << endl;
            // cout << "Current credits: " << currentCredits << endl;
            if (result > 0) gameInstance.gameStats.recordWin(result);  // Optionally track wins
        }
        return currentCredits >= targetCredits;
    }

private:
    int startCredits;
    int targetCredits;
    GameConfig& gameConfig;
};
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <numeric>
#include <unordered_map>
#include <random>
#include <iomanip> // For std::setw and std::left
#include "Stats.h" 
#include "SpinTracker.h"
#include "Screen.h" 

using namespace std;

#include <chrono> // For time measurements
#include <thread> // For multithreading
#include <future> // For std::promise and std::future



class Timer {
private:
    std::chrono::high_resolution_clock::time_point start_time;
public:
    void start() {
        start_time = std::chrono::high_resolution_clock::now();
    }

    double stop() {
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end_time - start_time;
        return duration.count();
    }
};


int instructionIndex = 0;

enum SimulationMode {
    EXACT_MODE,
    RANDOM_MODE,
    PLAYER_MODE
};

LogMode logMode = NO_LOGGING; // NO_LOGGING, LOGGING, REPLAY
SimulationMode simulationMode = RANDOM_MODE;



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


    void playBaseGame(int numSpins);
   // void playFullCycle();
    double calculateWaysWins(Screen& screen, bool baseGame);
   
    vector<double> handleCascades(Screen& screen, const ReelSet& reelSet, ReelSet& offScreenReelSet, bool useDifferentReelSet, bool baseGame);

    double wildWin(Screen screen);
    vector<double> expandedWin();
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





void GameInstance::playBaseGame(int numSpins) {
    bool addMult;
    vector<double> baseVector;
  
    double temp_pay, multiplier;
    vector<double> temp_pays, freePays;

    for (int i = 0; i < numSpins; ++i) {
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

      //  screen.display();

          // Add the screen to the log
        RandomLogGenerator::addScreen(screen.toJson());


        baseVector = handleCascades(screen, baseReelSet, tumbleReelSet, true, true);

        pays[0] = baseVector[0];
        pays[1] = baseVector[1];

        //If 3 FG symbols appear, trigger a ladder
        int fgCount = screen.countSymbolOnScreen("F1", false);
        if (fgCount == 3) {
            gameStats.activateBonusGame(4,true);
            int x = wheelActivation.getRandomPrize();
            gameStats.activateBonusGame(x, true);
            if (x == 0 ) {
               // gameStats.activateWW();
                multiplier = wildWinMult.getRandomPrize();
                temp_pay = multiplier * wildWin(screen);
                pays[2] += temp_pay;
            }
            else if (x == 1) {
				//gameStats.activateEW();
                multiplier = expWinMult.getRandomPrize();
				temp_pays = expandedWin();
				pays[3] += multiplier * temp_pays[0];
                pays[3] += multiplier * temp_pays[1];
            }
            else if (x == 2) {
			//	gameStats.activateFG();
                std::pair<int, int> numFreeGames = fgPrizeDist.getRandomPrize();
                freePays = playFreeGames(numFreeGames.first, numFreeGames.second);
                // sum freePays
                double free_pay = std::accumulate(freePays.begin(), freePays.end(), 0.0);
				pays[4] += free_pay;
                pays[7] += freePays[0];
                pays[8] += freePays[1];
                pays[9] += freePays[2];
                pays[10] += freePays[3];
                pays[11] += freePays[4];
			
			}
            else if (x == 3) {
               // gameStats.activateJP();
                double prize = directPrize.getRandomPrize();
                if (prize == 0) {
                    prize = jackpotPrize.getRandomPrize();
                }
				pays[5] = prize;
			}
            
			
		}
  

        RandomLogGenerator::endRound();
        RandomLogGenerator::resetRoundEndFlag();
       // pays[pays.size() - 1] = std::accumulate(pays.begin(), pays.end() - 1, 0.0);
        pays[6] = std::accumulate(pays.begin(), pays.end() - 5, 0.0);
       
        gameStats.completeWager(pays);
    }

}

vector<double> GameInstance::handleCascades(Screen& screen, const ReelSet& reelSet, ReelSet& offScreenReelSet, bool useDifferentReelSet, bool baseGame) {
    bool hasNewWins;
    double initialWin = 0, tumbleWin = 0;
   int tumbleCount = 0;

    std::vector<int> nextIndices(numReels, 0);
    // Initialize nextIndex for each reel
    /*for (int reel = 0; reel < numReels; ++reel) {
        nextIndices[reel] = reelSet.reels[reel].symbols.size() - 1;
    }*/
    for (int reel = 0; reel < numReels; ++reel) {
        nextIndices[reel] = (useDifferentReelSet ? offScreenReelSet : reelSet).reels[reel].symbols.size() - 1;
    }

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
            screen.cascadeSymbols(reelSet, useDifferentReelSet, offScreenReelSet, nextIndices);  // Cascade new symbols down
      
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

    

    RandomLogGenerator::addWinAmount(totalPay);
    return totalPay;
}

double GameInstance::wildWin(Screen screen) {
    bool baseGame = false;
    vector<double> pay;
    //Mark all F1 symbols on the screen
    screen.markSymbol("F1", numReels, true);
   
   /* if (screen.countSymbolOnScreen("F1", false) > 3) {
        screen.display(true);
        bool x = baseGame;
	}*/

    wildWinsReels.spinReels();
    screen.generateScreen(wildWinsReels); // create new reelset
    screen.fillMarkedSymbols("WL");
   // screen.display(true);
    screen.clearMarkedPositions();
    pay = handleCascades(screen, wildWinsReels, tumbleReelSet, true, baseGame);
    return pay[0] + pay[1];
   // return calculateWaysWins(screen, true);
}

vector<double> GameInstance::expandedWin() {
    bool baseGame = false;
    Screen bigScreen(numReels, numRows + 2);
    vector<double> pay;
   
    do {
        expandedReels.spinReels();
        bigScreen.generateScreen(expandedReels);
        pay = handleCascades(bigScreen, expandedReels, expandedReels, true, baseGame);
    } while (pay[0] == 0);

    return pay;
    // return calculateWaysWins(screen, true);
}

vector<double> GameInstance::playFreeGames(int numFreeGames, int numPremiumSpins) {
    vector<double> pays(5, 0); // {baseWin, tumbleWin, wildWin, expandedWin, freeGamesWon}
    vector<double> tempPays;
    double tempPay, multiplier;

    //numPremiumSpins = 2;
    vector<int> premiumSpins = getRandomPositions("PremChoices", numFreeGames, numPremiumSpins);
    std::sort(premiumSpins.begin(), premiumSpins.end());

    Screen screen(numReels, numRows);
    screen.clearScreen();

    int premiumIndex = 0;
    int currentFreeGame = 0;

    while (currentFreeGame < numFreeGames) {
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
        RandomLogGenerator::addScreen(screen.toJson());

        tempPays = handleCascades(screen, freeReelSet, tumbleFree, true, false);
        pays[0] += tempPays[0]; // baseWin
        pays[1] += tempPays[1]; // tumbleWin

        // Check for free game symbols and trigger additional features
        int fgCount = screen.countSymbolOnScreen("F1", false);
        if (fgCount == 3) {
            gameStats.activateBonusGame(4, false);
            int x = freeActivation.getRandomPrize();
            gameStats.activateBonusGame(x, false);
            if (x == 0) {
                multiplier = wildWinMultFree.getRandomPrize();
                tempPay = wildWin(screen);
                pays[2] += multiplier * tempPay; // wildWin
            }
            else if (x == 1) {
                multiplier = expWinMultFree.getRandomPrize();
                tempPays = expandedWin();
                pays[3] += multiplier * tempPays[0]; // expandedWin
                pays[3] += multiplier * tempPays[1]; // expandedWin
            }
            else if (x == 2) {                
                //std::pair<int, int> extraGames = fgPrizeDist.getRandomPrize();
                //numFreeGames += extraGames.first; // Track additional free games
                numFreeGames += fgPrizeDistFree.getRandomPrize();
            }
            else if (x == 3) {
                double prize = directPrizeFree.getRandomPrize();
                if (prize == 0) {
                    prize = jackpotPrizeFree.getRandomPrize();
                }
                pays[4] += prize;
               // debugFile  << prize << endl;
            }
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


void outputData(std::ofstream& file, Stats gameStats) {
    file << "RTP and Standard Deviation Breakdown\n";
    file << "Name\tRTP\tStDev\n";

    for (size_t i = 0; i < gameStats.payHeaders.size(); ++i) {
        double rtp = gameStats.payVector[i] / (gameStats.numIterations * gameStats.cost);
        double stDev = gameStats.standardDeviations[i];
        file << gameStats.payHeaders[i] << '\t' << std::setprecision(6) << rtp << '\t' << std::setprecision(4) << stDev << '\n';
    }
    file << "----------------------------------------\n";

    // Output the total pay
    file << "Iterations\t" << gameStats.numIterations << '\n';
    file << "Total Pay\t" << gameStats.payVector[3] << '\n';

    file << "Feature\tHits\tHit Rate\n";
    file << "Base Game\t" << gameStats.baseGameHits << '\t' << static_cast<double>(gameStats.numIterations) / gameStats.baseGameHits << '\n';
    file << "Bonus Game\t" << gameStats.bonusActivationsBase[4] << '\t' << static_cast<double>(gameStats.numIterations) / gameStats.bonusActivationsBase[4] << '\n';
    file << "Wild Win\t" << gameStats.bonusActivationsBase[0] << '\t' << static_cast<double>(gameStats.numIterations) / gameStats.bonusActivationsBase[0] << '\n';
    file << "Expanded Win\t" << gameStats.bonusActivationsBase[1] << '\t' << static_cast<double>(gameStats.numIterations) / gameStats.bonusActivationsBase[1] << '\n';
    file << "Free Game\t" << gameStats.bonusActivationsBase[2] << '\t' << static_cast<double>(gameStats.numIterations) / gameStats.bonusActivationsBase[2] << '\n';
    file << "JackPot\t" << gameStats.bonusActivationsBase[3] << '\t' << static_cast<double>(gameStats.numIterations) / gameStats.bonusActivationsBase[3] << '\n';

    file << "----------------------------------------\n";

    file << "Bonus Game\t" << gameStats.bonusActivationsFree[4] << '\t' << static_cast<double>(gameStats.numIterations) / gameStats.bonusActivationsFree[4] << '\n';
    file << "Wild Win\t" << gameStats.bonusActivationsFree[0] << '\t' << static_cast<double>(gameStats.numIterations) / gameStats.bonusActivationsFree[0] << '\n';
    file << "Expanded Win\t" << gameStats.bonusActivationsFree[1] << '\t' << static_cast<double>(gameStats.numIterations) / gameStats.bonusActivationsFree[1] << '\n';
    file << "Free Game\t" << gameStats.bonusActivationsFree[2] << '\t' << static_cast<double>(gameStats.numIterations) / gameStats.bonusActivationsFree[2] << '\n';
    file << "JackPot\t" << gameStats.bonusActivationsFree[3] << '\t' << static_cast<double>(gameStats.numIterations) / gameStats.bonusActivationsFree[3] << '\n';

    file << "----------------------------------------\n";

    // Output the table of symbol hits
    file << "Base Hits\n";
    file << "Symbol";
    for (size_t i = 0; i < gameStats.baseSymHits[0].size(); ++i) {
        file << '\t' << i + 1;
    }
    file << '\n';
    for (size_t i = 0; i < gameStats.baseSymHits.size(); ++i) {
        file << gameStats.symbolStructure.getSymbols()[i];
        for (const auto& hits : gameStats.baseSymHits[i]) {
            file << '\t' << hits;
        }
        file << '\n';
    }

    file << "----------------------------------------\n";

    // Output the table of symbol pays
    file << "Base Pays\n";
    file << "Symbol";
    for (size_t i = 0; i < gameStats.baseSymPays[0].size(); ++i) {
        file << '\t' << i + 1;
    }
    file << '\n';
    for (size_t i = 0; i < gameStats.baseSymPays.size(); ++i) {
        file << gameStats.symbolStructure.getSymbols()[i];
        for (const auto& hits : gameStats.baseSymPays[i]) {
            file << '\t' << hits;
        }
        file << '\n';
    }

    file << "----------------------------------------\n";

    // Output the table of symbol hits
    file << "Free Hits\n";
    file << "Symbol";
    for (size_t i = 0; i < gameStats.freeSymHits[0].size(); ++i) {
        file << '\t' << i + 1;
    }
    file << '\n';
    for (size_t i = 0; i < gameStats.freeSymHits.size(); ++i) {
        file << gameStats.symbolStructure.getSymbols()[i];
        for (const auto& hits : gameStats.freeSymHits[i]) {
            file << '\t' << hits;
        }
        file << '\n';
    }

    file << "----------------------------------------\n";

    // Output the table of symbol pays
    file << "Free Pays\n";
    file << "Symbol";
    for (size_t i = 0; i < gameStats.freeSymPays[0].size(); ++i) {
        file << '\t' << i + 1;
    }
    file << '\n';
    for (size_t i = 0; i < gameStats.freeSymPays.size(); ++i) {
        file << gameStats.symbolStructure.getSymbols()[i];
        for (const auto& hits : gameStats.freeSymPays[i]) {
            file << '\t' << hits;
        }
        file << '\n';
    }

    file << "----------------------------------------\n";

    // Output the tumble frequencies
    file << "Tumble Frequencies\n";
    file << "Tumbles\tFrequency\n";
    for (const auto& pair : gameStats.tumbleFrequencies) {
        file << pair.first << '\t' << pair.second << '\n';
    }

    // Output the average tumble frequency
    file << "Average Tumble Frequency: " << gameStats.calculateAverageTumbleFrequency() << '\n';
}

void printFrequencyTableToFile(const std::string& categoryName, const std::unordered_map<double, long long>& frequencyMap) {
    // Construct a unique filename for this category
    std::string filename = "pay_frequency_" + categoryName + ".txt";

    // Open the file for this category
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open " << filename << std::endl;
        return; // Error handling
    }

    // Sort the frequencies
    std::vector<std::pair<double, int>> freqVector(frequencyMap.begin(), frequencyMap.end());
    std::sort(freqVector.begin(), freqVector.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });

    // Print the frequency table to the file
    file << "Pay\tFrequency\n";
    for (const auto& pair : freqVector) {
        file << pair.first << "\t" << pair.second << "\n";
    }

    file.close();
}

void threadFunction(std::promise<Stats> promiseObj, GameConfig config, double numSpins) {
    GameInstance gameInstance(config, numSpins);
    gameInstance.playBaseGame(numSpins);
    Stats localStats = gameInstance.gameStats;
    promiseObj.set_value(localStats);
}

int main() {
    // Create a timer instance
    Timer timer;
    timer.start(); 

    GameConfig gameConfig("config.json");
    int numberOfSpins = 1000000000; //logging: 1000000 
    
    std::string outputFileBase = gameConfig.gameName + "_RTP" + gameConfig.RTP + "_" + gameConfig.gameMode;
    std::string outputFileName = outputFileBase + "_output.txt";
    std::string randomLogFileName = outputFileBase + "_randomLog.txt";
    std::string gameDetailsFileName = outputFileBase + "_gameDetails.txt";


    GameInstance fullInstance(gameConfig, numberOfSpins);
    Stats aggregatedStats(gameConfig, numberOfSpins); // Initialize with default or appropriate config

    vector<double> RTPVector(gameConfig.payHeaders.size(), 0);
    vector<double> payVector(gameConfig.payHeaders.size(), 0);


    // Open output file
    ofstream outputFile(outputFileName);
    if (!outputFile.is_open()) {
        cerr << "Failed to open output file." << endl; //good to check before running simulation
    }

    RandomLogGenerator::setLogFile(randomLogFileName);
    RandomLogGenerator::setGameDetailsFile(gameDetailsFileName);

    // Check the mode and act accordingly.
    RandomLogGenerator::handleLoggingMode(logMode);




    // Depending on the selected mode, execute the corresponding simulation
   
    if (simulationMode == RANDOM_MODE) { // Simulate number of spins
        
        int numThreads = 25;
        std::vector<std::future<Stats>> futures;

        for (int i = 0; i < numThreads; ++i) {
            std::promise<Stats> promiseObj;
            futures.push_back(promiseObj.get_future());
            std::thread(threadFunction, std::move(promiseObj), gameConfig, (double)numberOfSpins / numThreads).detach(); // Start thread
        }


        for (auto& fut : futures) {
            Stats threadStats = fut.get(); // Retrieve Stats from each thread
            aggregatedStats.aggregate(threadStats); // Aggregate into final Stats
        }

        // After all threads complete and results are aggregated, calculate standard deviations.
        aggregatedStats.calculateStandardDeviations();
    }
    else if (simulationMode == PLAYER_MODE) {
        int N = 10000; // Number of players 100000
        int X = 2000; // Starting credits (enough for 100 spins)
        int Y = 4000; // Target credits (enough for 200 spins)
        int successfulPlayers = 0;

        for (int i = 0; i < N; ++i) {
            PlayerSimulation sim(X, Y, gameConfig);
            if (sim.simulate()) {
                successfulPlayers++;
            }
        }

        double successPercentage = (double)successfulPlayers / N * 100.0;
        cout << "Percentage of players reaching target credits: " << successPercentage << "%\n";
        return 0;
    }
    else {
        cerr << "Invalid simulation mode" << endl;
        return 1;
    }


    outputData(outputFile, aggregatedStats);

    // Print each frequency table to its file
    for (size_t i = 0; i < aggregatedStats.payFrequencies.size(); ++i) {
        printFrequencyTableToFile(aggregatedStats.payHeaders[i], aggregatedStats.payFrequencies[i]);
    }


    // Stop the timer
    double elapsed_time = timer.stop();

    // Print the elapsed time to the output file
    outputFile << "Elapsed time: " << elapsed_time << " seconds" << endl;
    outputFile.close();

    RandomLogGenerator::closeLog();

    return 0;
}

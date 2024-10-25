#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <numeric>
#include <unordered_map>
#include <random>
#include <iomanip> // For std::setw and std::left
//#include "Stats.h" 
//#include "Screen.h" 
#include "GameInstance.h"

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


//int instructionIndex = 0;

enum SimulationMode {
    EXACT_MODE,
    RANDOM_MODE,
    PLAYER_MODE
};

LogMode logMode = LOGGING; // NO_LOGGING, LOGGING, REPLAY
SimulationMode simulationMode = RANDOM_MODE;





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
    std::vector<std::pair<double, long long>> freqVector(frequencyMap.begin(), frequencyMap.end());
    std::sort(freqVector.begin(), freqVector.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });

    // Print the frequency table to the file
    file << "Pay\tFrequency\n";
    for (const auto& pair : freqVector) {
        file << pair.first << "\t" << pair.second << "\n";
    }

    file.close();
}

void threadFunction(std::promise<Stats> promiseObj, GameConfig config, long long numSpins) {
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
    long long numberOfSpins = 1000LL; //logging: 100000 
    
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

    // Call handleLoggingMode to initialize the logging mode, whether LOGGING, REPLAY, or NO_LOGGING
    bool loggingInitialized = RandomLogGenerator::handleLoggingMode(logMode, randomLogFileName, gameDetailsFileName);

    if (!loggingInitialized && logMode != NO_LOGGING) {
        cerr << "Error initializing logging or replay mode!" << endl;
        return 1;  // Exit if there was an error initializing logging or replay mode
    }




    // Depending on the selected mode, execute the corresponding simulation
   
    if (simulationMode == RANDOM_MODE) { // Simulate number of spins
        
        int numThreads = 1;
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

    RandomLogGenerator::closeLogs();

    return 0;
}

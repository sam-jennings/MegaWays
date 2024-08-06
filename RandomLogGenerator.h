#ifndef RANDOMLOGGENERATOR_H
#define RANDOMLOGGENERATOR_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include "json.hpp"

using json = nlohmann::json;


struct RandTriple {
    std::string mask;
    int result;
    int range;
};

enum LogMode {
    NO_LOGGING,
    LOGGING,
    REPLAY
};

extern LogMode logMode;

class RandomLogGenerator {
public:
    static std::ofstream logFile;
    static std::ofstream gameDetailsFile;
    static int currentRound;
    static std::vector<std::vector<std::string>> spinsData;
    static std::vector<json> roundScreens;
    static bool newSpin;
    static bool loggingEnabled;
    
    static std::vector<double> roundWins;
    static std::vector<RandTriple> randomLogInstructions;
    static double maxRoundWin;
    

    static void openLog();
    static void closeLog();
    static void addLog(const RandTriple& randTriple);
    static void addWinAmount(int winAmount);
    static void endRound(bool maxWinTriggered);
    static void enableLogging();
    static void disableLogging();
    static void addScreen(json screen);
    static void readAndParseLog(const std::string& filename);
    static std::vector<RandTriple> getRandomLogInstructions();
    static void setLogFile(const std::string& filename);
    static void setGameDetailsFile(const std::string& filename);
    static void resetRoundEndFlag();
    static void handleLoggingMode(LogMode mode);

private:
    static std::string logFileName;
    static std::string gameDetailsFileName;
    static void parseLogLine(const std::string& line, std::vector<RandTriple>& entries);
    static bool roundAlreadyEnded;
};

// Method Definitions
std::string RandomLogGenerator::logFileName;
std::string RandomLogGenerator::gameDetailsFileName;
std::ofstream RandomLogGenerator::logFile;
std::ofstream RandomLogGenerator::gameDetailsFile;
int RandomLogGenerator::currentRound = 0;
std::vector<std::vector<std::string>> RandomLogGenerator::spinsData;
std::vector<json> RandomLogGenerator::roundScreens;
bool RandomLogGenerator::newSpin = true;
bool RandomLogGenerator::loggingEnabled = true;
std::vector<double> RandomLogGenerator::roundWins(0);
std::vector<RandTriple> RandomLogGenerator::randomLogInstructions;
double RandomLogGenerator::maxRoundWin = 300000.0;
bool RandomLogGenerator::roundAlreadyEnded = false;

void RandomLogGenerator::resetRoundEndFlag() {
	roundAlreadyEnded = false;
}
void RandomLogGenerator::openLog() {
    if (loggingEnabled) {
        logFile.open(logFileName);
        if (!logFile.is_open()) {
            throw std::runtime_error("Failed to open log file: " + logFileName);
        }
        gameDetailsFile.open(gameDetailsFileName);
        if (!gameDetailsFile.is_open()) {
            throw std::runtime_error("Failed to open game details file.");
        }
    }
}

void RandomLogGenerator::closeLog() {
    if (loggingEnabled) {
        logFile.close();
        gameDetailsFile.close();
    }
}

void RandomLogGenerator::addLog(const RandTriple& randTriple) {
    if (loggingEnabled) {
        if (!newSpin) {
            logFile << ",";
        }
        else {
            newSpin = false;
        }

        logFile << randTriple.mask << ":" << randTriple.result << ":" << randTriple.range;
    }
}

void RandomLogGenerator::addWinAmount(int winAmount) {
    if (loggingEnabled) {
        double projectedTotal = std::accumulate(roundWins.begin(), roundWins.end(), static_cast<double>(winAmount));
        if (projectedTotal > maxRoundWin) {
            // Log the full win amount, even if it exceeds the max limit
            logFile << "#" << (double)winAmount/100.0 << ";";

            // Force the end of the round with the max win scenario flag set
            endRound(true);
        }
        else {
            // Add win normally if under the max limit
            roundWins.push_back(winAmount);
            logFile << "#" << (double)winAmount / 100.0 << ";";
        }
        newSpin = true;
    }
}

void RandomLogGenerator::endRound(bool maxWinTriggered = false) {
    if (!loggingEnabled) {
        return;
    }

    if (!roundAlreadyEnded) {
        currentRound++;

        double finalWinAmount = maxWinTriggered ? maxRoundWin : std::accumulate(roundWins.begin(), roundWins.end(), 0.0);
        logFile << "#" << finalWinAmount / 100.0 << "\n";

        gameDetailsFile << "{" << std::endl;
        for (size_t i = 0; i < roundScreens.size(); ++i) {
            gameDetailsFile << "  \"spin_" << i << "\": [" << std::endl;
            for (const auto& row : roundScreens[i]) {
                gameDetailsFile << "    [";
                for (size_t j = 0; j < row.size(); ++j) {
                    gameDetailsFile << row[j];  // Directly write the symbol without additional quotes
                    if (j < row.size() - 1) gameDetailsFile << ", ";
                }
                gameDetailsFile << "]," << std::endl;
            }
            gameDetailsFile << "  ]," << std::endl;
        }
        gameDetailsFile << "}" << std::endl;
        gameDetailsFile << "========== end round: " << currentRound << " ===========" << std::endl;

        roundScreens.clear();
        newSpin = true;
    }
    roundWins.clear();
    roundAlreadyEnded = true;
    return;
}

//void RandomLogGenerator::endRound(bool maxWinTriggered = false) {
//    if (!loggingEnabled) {
//        return;
//    }
//    
//    if (!roundAlreadyEnded) {
//        currentRound++;
//
//        double finalWinAmount = maxWinTriggered ? maxRoundWin : std::accumulate(roundWins.begin(), roundWins.end(), 0.0);
//        logFile << "#" << finalWinAmount / 100.0 << "\n";
//        
//        json roundData; // JSON object to hold data for all spins
//
//        for (size_t i = 0; i < roundScreens.size(); ++i) {
//            json spinData; // JSON object to hold data for each spin
//            spinData["spin_" + std::to_string(i)] = roundScreens[i];
//            roundData.merge_patch(spinData); // Merge spin data into roundData
//        }
//
//        gameDetailsFile << roundData.dump(4) << std::endl; // Output round data
//        gameDetailsFile << "========== end round: " << currentRound << " ===========" << std::endl;
//
//        roundScreens.clear();
//
//        newSpin = true;
//    }
//    roundWins.clear();
//	roundAlreadyEnded = true;
//	return;
//    
//}

//void RandomLogGenerator::endRound() {
//    if (loggingEnabled) {
//        currentRound++;
//
//        double roundTotal = std::accumulate(roundWins.begin(), roundWins.end(), 0.0);
//        logFile << "#" << roundTotal << "\n";
//        roundWins.clear();
//
//        json roundData; // JSON object to hold data for all spins
//
//        for (size_t i = 0; i < roundScreens.size(); ++i) {
//            json spinData; // JSON object to hold data for each spin
//            spinData["spin_" + std::to_string(i)] = roundScreens[i];
//            roundData.merge_patch(spinData); // Merge spin data into roundData
//        }
//
//        gameDetailsFile << roundData.dump(4) << std::endl; // Output round data
//        gameDetailsFile << "========== end round: " << currentRound << " ===========" << std::endl;
//
//        roundScreens.clear();
//
//        newSpin = true;
//    }
//}

void RandomLogGenerator::enableLogging() {
    loggingEnabled = true;
}

void RandomLogGenerator::disableLogging() {
    loggingEnabled = false;
}

void RandomLogGenerator::addScreen(json screen) {
    if (loggingEnabled) {
        roundScreens.push_back(screen);
    }
}

void RandomLogGenerator::readAndParseLog(const std::string& filename) {
    std::ifstream file(filename);
    std::string line;

    if (!file.is_open()) {
        throw std::runtime_error("Could not open the log file: " + filename);
    }

    randomLogInstructions.clear();

    while (std::getline(file, line)) {
        std::vector<RandTriple> entries;
        parseLogLine(line, entries);
        randomLogInstructions.insert(randomLogInstructions.end(), entries.begin(), entries.end());
    }
}

std::vector<RandTriple> RandomLogGenerator::getRandomLogInstructions() {
    return randomLogInstructions;
}

void RandomLogGenerator::parseLogLine(const std::string& line, std::vector<RandTriple>& entries) {
    std::stringstream ss(line);
    std::string token;

    while (std::getline(ss, token, ';')) {
        std::stringstream tokenStream(token);
        std::string part;

        while (std::getline(tokenStream, part, '#')) {
            if (part.empty()) {
                continue;
            }
            std::vector<std::string> randTriples;
            std::stringstream randTripleStream(part);
            while (std::getline(randTripleStream, part, ',')) {
                randTriples.push_back(part);
            }
            for (const auto& randTriple : randTriples) {
                std::vector<std::string> parts;
                std::stringstream randTripleSS(randTriple);
                while (std::getline(randTripleSS, part, ':')) {
                    parts.push_back(part);
                }
                if (parts.size() == 3) {
                    RandTriple entry{ parts[0], std::stoi(parts[1]), std::stoi(parts[2]) };
                    entries.push_back(entry);
                }
            }
        }
    }
}

void RandomLogGenerator::setLogFile(const std::string& filename) {
    logFileName = filename;
}

void RandomLogGenerator::setGameDetailsFile(const std::string& filename) {
    gameDetailsFileName = filename;
}


void RandomLogGenerator::handleLoggingMode(LogMode mode) {
    if (mode == NO_LOGGING) {
        RandomLogGenerator::disableLogging(); // Disable logging
        std::cout << "Running in no logging mode" << std::endl;
    }
        
    else if (mode == LOGGING) {
        RandomLogGenerator::enableLogging(); // Enable logging
        RandomLogGenerator::openLog();
        std::cout << "Running in logging mode" << std::endl;
    }
    else if (mode == REPLAY) {
        RandomLogGenerator::readAndParseLog(logFileName);
        std::vector<RandTriple> randomLogInstructions = RandomLogGenerator::getRandomLogInstructions();
        std::cout << "Running in replay mode using " << logFileName << std::endl;
    }
	else {
		// Handle default or unknown mode
        std::cout << "Unknown mode" << std::endl;
    }
}
#endif // RANDOMLOGGENERATOR_H

#pragma once


#include <string>
#include <vector>
#include <unordered_map>
#include "Symbols.h"
#include "PrizeDistribution.h"

// Forward declaration if Game-related classes need to be referenced
class Stats; // Forward declaration of Stats class

class GameConfig {
    friend class Stats; // Declare Stats as a friend
private:
    json config;
    std::string filename;

    // Other configurations as necessary

    // Helper methods for parsing the configuration file
  
    SymbolStructure parseSymbolStructure() {
		std::vector<std::string> symbols;
        std::vector<std::vector<int>> paytable;
        std::vector<std::string> symbolNames = config["paytable"]["symbols"].get<std::vector<std::string>>();
        std::unordered_map<std::string, std::vector<std::string>> wildSubs; // To store wild substitutions
        //auto wildSubsConfig = config["paytable"]["wildSubs"].get<std::unordered_map<std::string, std::vector<std::string>>>();

        for (int i = 0; i < symbolNames.size(); i++) {
			symbols.push_back(symbolNames[i]);
            std::vector<int> payout = config["paytable"]["pays"][symbolNames[i]].get<std::vector<int>>();
			paytable.push_back(payout);
		}
        // Parse wild substitutions
       /* for (auto& item : wildSubsConfig) {
			std::string wildSymbol = item.first;
			std::vector<std::string> subs = item.second;
			wildSubs[wildSymbol] = subs;
		}*/
        for (const auto& item : config["paytable"]["wildSubs"].items()) {
            std::string wildSymbol = item.key();
            std::vector<std::string> substitutes = item.value().get<std::vector<std::string>>();
            wildSubs[wildSymbol] = substitutes;
        }

		return SymbolStructure(symbols, paytable, wildSubs);
        
	}

    ReelSet parseReelSet(const std::string& reelSetName, std::string maskName = "") {
        auto& reelSetConfig = this->config["reel_sets"];
        for (auto& item : reelSetConfig) {
            if (item["name"] == reelSetName) {
                std::vector<Reel> reels;
                auto& reelsConfig = item["reels"];
                for (auto& reelConfig : reelsConfig) {
                    std::vector<std::string> symbols = reelConfig["symbols"].get<std::vector<std::string>>();
                    std::vector<int> weights;
                    if (reelConfig.contains("weights")) {
                        weights = reelConfig["weights"].get<std::vector<int>>();
                    }
                    reels.push_back(Reel(symbols, weights));
                }
                std::string mask = maskName.empty() ? static_cast<std::string>(item["mask"]) : maskName;
                return ReelSet(reels, mask);
            }
        }
        throw std::invalid_argument("ReelSet not found: " + reelSetName);
    }

    std::vector<ReelSet> parseReelSequence() {
        std::vector<ReelSet> reelSets;
        for (auto& item : this->config["reel_sequence"].items()) {
            std::string key = item.key();
            // Use 'key' as needed here
            reelSets.push_back(parseReelSet(key));
        }
        return reelSets;
    }

    // Get PrizeDistribution object from config
    template <typename PrizeType>
    PrizeDistribution<PrizeType> parsePrizeDistribution(const std::string& prizeDistName, std::string subLevel = "") {
        // Get the reference to the config JSON node, starting from the root
        json prizeDistConfig = this->config;

        // If subLevel is provided, navigate to the specific sublevel
        if (!subLevel.empty()) {
            std::istringstream subLevelStream(subLevel);
            std::string level;
            while (getline(subLevelStream, level, '/')) {
                prizeDistConfig = prizeDistConfig[level];
            }
        }
        prizeDistConfig = prizeDistConfig[prizeDistName];

        std::string mask = prizeDistConfig["mask"];
        std::vector<PrizeType> prizes = prizeDistConfig["prizes"].get<std::vector<PrizeType>>();
        // Check if weights exist, default to 1s if not
        std::vector<int> weights;
        if (prizeDistConfig.contains("weights")) {
            weights = prizeDistConfig["weights"].get<std::vector<int>>();
        }
        else {
            weights = std::vector<int>(prizes.size(), 1); // Default weights to 1 for each prize
        }
        return PrizeDistribution<PrizeType>(mask, prizes, weights);
    }
    

    // Get vector of PrizeDistribution objects from config
    template <typename PrizeType>
    std::vector<PrizeDistribution<PrizeType>> parsePDVec(const std::string& prizeDistName) {
        std::vector<PrizeDistribution<PrizeType>> prizeDists;
        json& prizeDistConfig = this->config[prizeDistName];
        for (auto& item : prizeDistConfig.items()) {
            std::string key = item.key();
            prizeDists.push_back(parsePrizeDistribution<PrizeType>(key, prizeDistName));
        }
        return prizeDists;
    }

  
    // Get variable of any type from config
    template<typename T>
    T parseVar(const std::string& key) const {
        return config[key].get<T>();
    }

    // Get vector of a specific type from config  
    template<typename T>
    std::vector<T> parseVec(const std::string& key, std::string subLevel = "") const {
        if (!subLevel.empty()) {
			return config[key][subLevel].get<std::vector<T>>();
		} else
        return config[key].get<std::vector<T>>();
    }

    /*template<typename T>
    std::vector<T> parseVec(const std::string& key) const {
        return config[key].get<std::vector<T>>();
    }*/

    // Get array of a specific type and size from config
    template<typename T, size_t N>
    std::array<T, N> parseArray(const std::string& key) const {
        std::array<T, N> result;
        auto values = config[key];
        for (size_t i = 0; i < values.size() && i < N; ++i) {
            result[i] = values[i].get<T>();
        }
        return result;
    }

public:
    GameConfig(const std::string& configFile) {  // Constructor to load and parse configuration
        filename = configFile;
		openFile();
        gameName = parseVar<std::string>("gameName");
        RTP = parseVar<std::string>("RTP");
        gameMode = parseVar<std::string>("gameMode");
        numSpins = parseVar<int>("simulations");

        baseReelsLow = parseReelSet("baseLow");
        baseReelsHigh = parseReelSet("baseHigh");
        tumbleReelsHigh = parseReelSet("tumbleHigh");
        tumbleReelsLow = parseReelSet("tumbleLow");
        tumbleFree = parseReelSet("tumbleFree");
        freeHigh = parseReelSet("freeHigh");
        freeLow = parseReelSet("freeLow");
        wildWinsReels = parseReelSet("wildWins");
        expandedReels = parseReelSet("expandedReels");
        premiumReels = parseReelSet("premiumReels");
		symbolStructure = parseSymbolStructure();
        payHeaders = parseVec<std::string>("payHeaders");
        numReels = parseVar<int>("reels");
        numRows = parseVar<int>("rows");
        cost = parseVar<int>("cost");
        wheelActivation = parsePrizeDistribution<int>("wheelActivation");
        freeActivation = parsePrizeDistribution<int>("wheelActivationFree");
        fgPrizeDist = parsePrizeDistribution<std::pair<int, int>>("fgPrizeDist");
        fgPrizeDistFree = parsePrizeDistribution<int>("fgPrizeDistFree");
        wildWinMult = parsePrizeDistribution<int>("wildWinMult");
        wildWinMultFree = parsePrizeDistribution<int>("wildWinMultFree");
        expWinMult = parsePrizeDistribution<int>("expWinMult");
        expWinMultFree = parsePrizeDistribution<int>("expWinMultFree");
        directPrize = parsePrizeDistribution<int>("directPrize");
        directPrizeFree = parsePrizeDistribution<int>("directPrizeFree");
        jackpotPrize = parsePrizeDistribution<int>("jackpotPrize");
        jackpotPrizeFree = parsePrizeDistribution<int>("jackpotPrizeFree");
        reelWeights = parseVec<int>("reelWeights", RTP); //change for different RTPs
        reelWeightsFree = parseVec<int>("reelWeightsFree", RTP); //change for different RTPs
        symbolList = symbolStructure.getSymbols();
        paytable = symbolStructure.getPaytable();
	}

    std::string gameName;
    std::string RTP;
    std::string gameMode;
    int numSpins;

    int numReels;
    int numRows;
    int cost; //maybe double
    ReelSet baseReelsLow, baseReelsHigh, tumbleReelsLow, tumbleReelsHigh,tumbleFree, wildWinsReels, expandedReels, freeHigh, freeLow, premiumReels;
    std::vector<int> reelWeights, reelWeightsFree;
    SymbolStructure symbolStructure;
    std::vector<std::string> payHeaders;
    PrizeDistribution<int> wheelActivation, freeActivation,  wildWinMult, wildWinMultFree, expWinMult, expWinMultFree, fgPrizeDistFree, directPrize,directPrizeFree, jackpotPrize, jackpotPrizeFree;
    PrizeDistribution<std::pair<int, int>> fgPrizeDist;
    std::vector<std::string> symbolList;
    std::map<std::string, std::vector<int>> paytable;

    void openFile() {        
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to open " << filename << std::endl;
            return;
        }
        file >> config;
        file.close();        
    }
    
   

    

};
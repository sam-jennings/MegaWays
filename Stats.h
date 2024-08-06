#pragma once
#include "GameConfig.h" // Include GameConfig to access configuration data
#include <vector>
#include <algorithm> 
#include <fstream>
#include <cmath> // Include for std::sqrt
#include <unordered_map>
#include <deque>

template<typename T>
class RollingBuffer {
public:
    RollingBuffer(size_t size) : maxSize(size) {}

    void add(T value) {
        if (buffer.size() >= maxSize) {
            buffer.pop_front();
        }
        buffer.push_back(value);
    }

    const std::deque<T>& getBuffer() const { return buffer; }

private:
    size_t maxSize;
    std::deque<T> buffer;
};

class Stats {
public:
    std::vector<std::string> payHeaders;
    std::vector<double> payVector;
   // std::vector<std::vector<double>> individualPays; // Each inner vector stores individual pays for "Base", "Wheel", "Free", "Total"
    RollingBuffer<double> individualPaysBuffer;
    std::vector< std::unordered_map<double, long long>> payFrequencies; // For storing frequencies of pays
    std::vector<double> standardDeviations; // For storing calculated standard deviations
    std::vector<std::vector<int>> baseSymHits, freeSymHits, baseSymPays, freeSymPays; // 2D array for tracking hits, size: numSymbols x numReels
    int baseGameHits, bonusGameActivated, fgActivated, wwActivated, ewActivated, jpActivated;
    std::vector<int> bonusActivations;
    SymbolStructure symbolStructure;
    std::unordered_map<int, int> scatterHits, tumbleFrequencies;
    int numIterations;
    double cost;

    //explicit Stats(const GameConfig& config, ofstream* file, int numIterations) {
    explicit Stats(const GameConfig& config, int _numIterations ) :individualPaysBuffer(1000) {
        numIterations = _numIterations;
        baseGameHits = 0;
        bonusGameActivated = 0;
        fgActivated = 0;
        wwActivated = 0;
        ewActivated = 0;
        jpActivated = 0;
        bonusActivations.resize(5, 0);
        numIterations = numIterations;
        cost = config.cost;
        payHeaders = config.payHeaders;
        payVector.resize(payHeaders.size());
       // individualPays.resize(payHeaders.size());
        payFrequencies.resize(payHeaders.size());
        baseSymHits.resize(config.symbolList.size(), std::vector<int>(config.numReels, 0));
        freeSymHits.resize(config.symbolList.size(), std::vector<int>(config.numReels, 0));
        baseSymPays.resize(config.symbolList.size(), std::vector<int>(config.numReels, 0));
        freeSymPays.resize(config.symbolList.size(), std::vector<int>(config.numReels, 0));
        symbolStructure = config.symbolStructure;
        //scatterHits.resize(config.scatterPrizeDistribution.getPrizes().size(), 0);
    }

    void trackResult(const std::string& symbol, int length, int ways, double pay, bool base) {
        int symbolIndex = symbolStructure.findSymbolIndex(symbol);
        if (base) {
            baseSymHits[symbolIndex][length - 1] += ways;
            baseSymPays[symbolIndex][length - 1] += pay;
        }
        else {
            freeSymHits[symbolIndex][length - 1] += ways;
            freeSymPays[symbolIndex][length - 1] += pay;
        }
    }

    void recordScatterHit(int prize) { //check if prize already exists in map, otherwise add it and increment
        if (scatterHits.find(prize) == scatterHits.end()) {
            scatterHits[prize] = 1;
        }
        else {
            scatterHits[prize]++;
        }
    }

    void recordTumbleFrequency(int tumble) {
        if (tumbleFrequencies.find(tumble) == tumbleFrequencies.end()) {
			tumbleFrequencies[tumble] = 1;
		}
        else {
			tumbleFrequencies[tumble]++;
		}
	}

    double calculateAverageTumbleFrequency() {
		double totalTumbles = 0;
        double totalHits = 0;
        for (const auto& pair : tumbleFrequencies) {
			totalTumbles += pair.first * pair.second;
			totalHits += pair.second;
		}
		return totalTumbles / totalHits;
	}

    void completeWager(std::vector<double> pays) {
        for (int i = 0; i < pays.size(); i++) {
            payVector[i] += pays[i];
            //individualPays[i].push_back(pays[i]);
            individualPaysBuffer.add(pays[i]);
            payFrequencies[i][pays[i]]++;
        }
        if (pays[0] > 0) { //check what counts as a base game hit
            baseGameHits++;
        }
    }

    void activateFG() {
        fgActivated++;
    }

    void activateBonusGame() {
        bonusGameActivated++;
    }

    void activateWW() {
		wwActivated++;
	}

    void activateEW() {
        ewActivated++;
    }

    void activateJP() {
		jpActivated++;
	}


    double calculateStandardDeviation(const std::vector<double>& pays) {
        if (pays.empty()) return 0.0;
        double mean = std::accumulate(pays.begin(), pays.end(), 0.0) / pays.size();
        double variance = 0.0;
        for (auto pay : pays) {
            variance += std::pow(pay - mean, 2);
        }
        variance /= pays.size();
        return std::sqrt(variance);
    }

    /*void calculateStandardDeviations() {
        standardDeviations.resize(individualPays.size());
        for (size_t i = 0; i < individualPays.size(); ++i) {
            standardDeviations[i] = calculateStandardDeviation(individualPays[i]);
        }
    }*/

    void calculateStandardDeviations() {
        standardDeviations.clear(); // Make sure to clear previous standard deviations
        standardDeviations.resize(payFrequencies.size(), 0.0);

        for (size_t i = 0; i < payFrequencies.size(); ++i) {
            double mean = 0.0;
            double variance = 0.0;
            double totalWeight = 0.0;

            // Calculate the mean
            for (const auto& pair : payFrequencies[i]) {
                mean += pair.first * pair.second;
                totalWeight += pair.second;
            }
            mean /= totalWeight;

            // Calculate the variance
            for (const auto& pair : payFrequencies[i]) {
                variance += pair.second * std::pow(pair.first - mean, 2);
            }
            variance /= totalWeight;

            // Calculate the standard deviation
            standardDeviations[i] = std::sqrt(variance);
        }
    }

    void aggregate(const Stats& other) {

        for (size_t symbolIndex = 0; symbolIndex < baseSymHits.size(); ++symbolIndex) {
            for (size_t reelIndex = 0; reelIndex < baseSymHits[symbolIndex].size(); ++reelIndex) {
                // Sum the corresponding values from 'other' into this instance
                this->baseSymHits[symbolIndex][reelIndex] += other.baseSymHits[symbolIndex][reelIndex];
            }
        }

        for (size_t symbolIndex = 0; symbolIndex < freeSymHits.size(); ++symbolIndex) {
            for (size_t reelIndex = 0; reelIndex < freeSymHits[symbolIndex].size(); ++reelIndex) {
                // Sum the corresponding values from 'other' into this instance
                this->freeSymHits[symbolIndex][reelIndex] += other.freeSymHits[symbolIndex][reelIndex];
            }
        }

        for (size_t symbolIndex = 0; symbolIndex < baseSymPays.size(); ++symbolIndex) {
            for (size_t reelIndex = 0; reelIndex < baseSymPays[symbolIndex].size(); ++reelIndex) {
                // Sum the corresponding values from 'other' into this instance
                this->baseSymPays[symbolIndex][reelIndex] += other.baseSymPays[symbolIndex][reelIndex];
            }
        }

        for (size_t symbolIndex = 0; symbolIndex < freeSymPays.size(); ++symbolIndex) {
            for (size_t reelIndex = 0; reelIndex < freeSymPays[symbolIndex].size(); ++reelIndex) {
                // Sum the corresponding values from 'other' into this instance
                this->freeSymPays[symbolIndex][reelIndex] += other.freeSymPays[symbolIndex][reelIndex];
            }
        }
        for (int i = 0; i < payVector.size(); i++) {
            this->payVector[i] += other.payVector[i];
        }

        /*for (size_t i = 0; i < individualPays.size(); ++i) {
            individualPays[i].insert(individualPays[i].end(), other.individualPays[i].begin(), other.individualPays[i].end());
        }*/

        // Aggregate tumble frequencies
        for (const auto& pair : other.tumbleFrequencies) {
			this->tumbleFrequencies[pair.first] += pair.second;
		}

        // Aggregate the pay frequencies
        for (size_t i = 0; i < other.payFrequencies.size(); ++i) {
            for (const auto& pair : other.payFrequencies[i]) {
                this->payFrequencies[i][pair.first] += pair.second;
            }
        }

        this->baseGameHits += other.baseGameHits;
        this->fgActivated += other.fgActivated;
        this->bonusGameActivated += other.bonusGameActivated;
        this->wwActivated += other.wwActivated;
        this->ewActivated += other.ewActivated;
        this->jpActivated += other.jpActivated;
    }

    // Method to get payout from the last spin
    double getLastSpinPayout() const {
        if (individualPaysBuffer.getBuffer().empty()) return 0.0;
        return individualPaysBuffer.getBuffer().back();
    }

    
    void recordWin(double amount) {
        // Increment total wins
        totalWins++;
        // Add the win amount to the total winnings
        totalWinnings += amount;
        // Optionally, you could track wins over a certain threshold or record details of each win
    }

private:
    int totalWins = 0;
    double totalWinnings = 0.0;

};
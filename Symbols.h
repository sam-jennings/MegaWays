#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <numeric>
#include <unordered_map>
#include <random>
#include "RandomUtils.h"

struct Symbol {
    std::string name;
    int counter;
    double value; // For PAY symbols if needed

    Symbol() : name(""), counter(1), value(0.0) {}
    Symbol(const std::string& name, int counter = 1, double value = 0.0)
        : name(name), counter(counter), value(value) {}
};

class SymbolStructure {
private:
    std::vector<std::string> symbols;
    std::vector<std::vector<int>> paytable_vec;
    std::map<std::string, std::vector<int>> paytable;
    std::vector<int> scatterPrizes;
    std::unordered_map<std::string, std::vector<std::string>> wildSubstitutions;

public:
    SymbolStructure() = default;

    SymbolStructure(const std::vector<std::string>& symbolNames, const std::vector<std::vector<int>>& symbolPayouts) {
        for (size_t i = 0; i < symbolNames.size(); ++i) {
            symbols.push_back(symbolNames[i]);
            paytable_vec.push_back(symbolPayouts[i]);
            paytable[symbolNames[i]] = symbolPayouts[i];
        }
    }

    SymbolStructure(const std::vector<std::string>& symbolNames,
        const std::vector<std::vector<int>>& symbolPayouts,
        const std::unordered_map<std::string, std::vector<std::string>>& wildSubs)
        : symbols(symbolNames), paytable_vec(symbolPayouts), wildSubstitutions(wildSubs) {
        for (size_t i = 0; i < symbolNames.size(); ++i) {
            paytable[symbolNames[i]] = symbolPayouts[i];
        }
    }

    // Check if a symbol is wild and get its substitutions
    std::vector<std::string> getWildSubstitutions(const std::string& wildSymbol) const {
        auto it = wildSubstitutions.find(wildSymbol);
        if (it != wildSubstitutions.end()) {
            return it->second;
        }
        return {};
    }

    const std::vector<std::string>& getSymbols() const { return symbols; }
    const std::vector< std::vector<int>>& getPaytableVec() const { return paytable_vec; }
    const std::map<std::string, std::vector<int>>& getPaytable() const { return paytable; }
    const std::vector<int>& getScatterPrizes() const { return scatterPrizes; }

    // Additional functionality for SymbolStructure can go here
    // For example, a method to find a symbol by name and return its index or payouts
    int findSymbolIndex(const std::string& name) const {
        for (size_t i = 0; i < symbols.size(); ++i) {
            if (symbols[i] == name) return i;
        }
        return -1; // Symbol not found
    }

    const std::vector<int>* findSymbolPayouts(const std::string& name) const {
        for (size_t i = 0; i < symbols.size(); ++i) {
            if (symbols[i] == name) return &paytable_vec[i];
        }
        return nullptr; // Symbol not found
    }

    const int getNumSymbols() const {
        return symbols.size();
    }

    const int getWinLength() const {
        return paytable_vec[0].size();
    }
};

struct Reel {
    std::vector<std::string> symbols;
    std::vector<int> weights;

    // Constructor to accept a vector of strings
    Reel(const std::vector<std::string>& _symbols, const std::vector<int>& _weights = {})
        : symbols(_symbols), weights(_weights) {}

    bool isWeighted() const { return !weights.empty(); }
};

class ReelSet {
private:

    std::string mask;

public:
    std::vector<Reel> reels;
    std::vector<int> currentIndices; // Store current indices

    // Constructor
    ReelSet(const std::vector<Reel>& reels, const std::string& mask) : reels(reels), mask(mask), currentIndices(reels.size(), 0) {}

    // Default constructor
    ReelSet() {}

    // Calculate complete cycle
    int getCycle() const {
        int cycle = 1;
        for (const auto& reel : reels) {
            cycle *= reel.isWeighted() ? std::accumulate(reel.weights.begin(), reel.weights.end(), 0) : reel.symbols.size();
        }
        return cycle;
    }

    // Spin reels method
    void spinReels() {
        std::vector<int> chosenIndices;
        for (int reelIndex = 0; reelIndex < reels.size(); ++reelIndex) {
            int index;
            if (reels[reelIndex].isWeighted()) {
                // Use weighted distribution
                index = getRandFromDist(mask, reels[reelIndex].weights); // chosen index, random number, total weight
            }
            else {
                // Use uniform distribution
                index = getRand(mask, reels[reelIndex].symbols.size());
            }
            chosenIndices.push_back(index);
        }
        currentIndices = chosenIndices; // Update current indices
        //reorderReels(chosenIndices);
    }


};





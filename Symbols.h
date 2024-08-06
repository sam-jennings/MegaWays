#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <numeric>
#include <unordered_map>
#include <random>
#include "RandomUtils.h"

//struct Symbol {
//    std::string name;
//    int index;
//    std::vector<int> payouts;
//    bool isWild = false;
//    bool isScatter = false;
//
//    Symbol(const std::string& name, int index, const std::vector<int>& payouts) : name(name), index(index), payouts(payouts) {}
//};
//
//class SymbolStructure {
//private:
//    std::vector<Symbol> symbols;
//    std::vector< std::vector<int>> paytable;
//    std::vector<int> scatterPrizes;
//
//public:
//    // Constructors
//    SymbolStructure() = default;
//
//    SymbolStructure(const std::vector<std::string>& symbolNames, const std::vector<std::vector<int>>& symbolPayouts) {
//        for (size_t i = 0; i < symbolNames.size(); ++i) {
//            symbols.emplace_back(symbolNames[i], i, symbolPayouts[i]);
//            paytable.push_back(symbolPayouts[i]);
//        }
//    }
//
//    SymbolStructure(const std::vector<Symbol>) {
//        for (size_t i = 0; i < symbols.size(); ++i) {
//			paytable.push_back(symbols[i].payouts);
//		}
//	}
//
//    const std::vector<Symbol>& getSymbols() const { return symbols; }
//    const std::vector< std::vector<int>>& getPaytable() const { return paytable; }
//    const std::vector<int>& getScatterPrizes() const { return scatterPrizes; }
//
//    // Additional functionality for SymbolStructure can go here
//    // For example, a method to find a symbol by name and return its index or payouts
//    int findSymbolIndex(const std::string& name) const {
//        for (const auto& symbol : symbols) {
//            if (symbol.name == name) return symbol.index;
//        }
//        return -1; // Symbol not found
//    }
//
//    const std::vector<int>* findSymbolPayouts(const std::string& name) const {
//        for (const auto& symbol : symbols) {
//            if (symbol.name == name) return &symbol.payouts;
//        }
//        return nullptr; // Symbol not found
//    }
//};

//struct Reel {
//    std::vector<Symbol> symbols; // Use Symbol objects instead of strings
//    std::vector<int> weights; // Keep weights as is for weighted reels
//
//    // Updated constructor to accept a vector of Symbols
//    Reel(const std::vector<Symbol>& _symbols, const std::vector<int>& _weights = {})
//        : symbols(_symbols), weights(_weights) {}
//
//    bool isWeighted() const { return !weights.empty(); }
//};

class SymbolStructure {
private:
    std::vector<std::string> symbols;
    std::vector< std::vector<int>> paytable_vec;
    std::map<std::string, std::vector<int>> paytable;
    std::vector<int> scatterPrizes;
    std::unordered_map<std::string, std::vector<std::string>> wildSubstitutions;

public:
    // Constructors
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
        return {}; // Return empty if not found or not a wild
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
};

struct Reel {
    std::vector<std::string> symbols; 
    std::vector<int> weights; 

    // Constructor to accept a vector of strings
    Reel(const std::vector<std::string>& _symbols, const std::vector<int>& _weights = {})
		: symbols(_symbols), weights(_weights) {}

    // Updated constructor to accept a vector of Symbols
    //std::vector<Symbol> symbols; // Use Symbol objects instead of strings
    //Reel(const std::vector<Symbol>& _symbols, const std::vector<int>& _weights = {})
    //    : symbols(_symbols), weights(_weights) {}

    bool isWeighted() const { return !weights.empty(); }
};

class ReelSet {
private:
    
    std::string mask;

public:
    std::vector<Reel> reels;
    // Constructor
    ReelSet(const std::vector<Reel>& reels, const std::string& mask) : reels(reels), mask(mask) {}
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
            } else {
                // Use uniform distribution
                index = getRand(mask, reels[reelIndex].symbols.size());
            }
            chosenIndices.push_back(index);
        }
        reorderReels(chosenIndices);
    }

    // Helper method to reorder reels based on chosen indices
    void reorderReels(const std::vector<int>& indices) {
        for (int reelIndex = 0; reelIndex < reels.size(); ++reelIndex) {
            std::vector<std::string> originalSymbols = reels[reelIndex].symbols;
            std::vector<int> originalWeights = reels[reelIndex].weights;
            std::rotate(originalSymbols.begin(), originalSymbols.begin() + indices[reelIndex], originalSymbols.end());
            if (!originalWeights.empty()) {
                std::rotate(originalWeights.begin(), originalWeights.begin() + indices[reelIndex], originalWeights.end());
            }
            reels[reelIndex].symbols = originalSymbols;
            reels[reelIndex].weights = originalWeights;
        }
    }
};





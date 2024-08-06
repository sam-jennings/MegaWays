#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <numeric>
#include <unordered_map>
#include <random>

class SpinTracker {
private:
    struct SymbolStats {
        std::vector<int> hits;
        std::vector<int> pays;

        SymbolStats() = default; // Default constructor

        SymbolStats(int length) : hits(length, 0), pays(length, 0) {}
    };

    std::unordered_map<std::string, SymbolStats> symbolStats;
    std::unordered_map<int, std::unordered_map<int, int>> scatterCounts; // Scatter count length -> Pay -> Frequency
    std::unordered_map<int, std::unordered_map<int, int>> scatterPays;   // Scatter count length -> Pay -> Frequency

public:
    SpinTracker() = default;

    // Method to track hits and payouts for a given symbol
    void trackResult(const std::string& symbol, int length, int ways, int pay) {
        if (symbolStats.find(symbol) == symbolStats.end()) {
            symbolStats.emplace(symbol, SymbolStats(length));
        }

        auto& stats = symbolStats[symbol];
        if (length > 0 && length <= stats.hits.size()) {
            stats.hits[length - 1] += ways;
            stats.pays[length - 1] += ways * pay;
        }
    }

    // Method to track scatter wins
    void trackScatterWin(int scatterCount, int pay) {
        scatterCounts[scatterCount][pay]++;
        scatterPays[scatterCount][pay] += pay;
    }
    // Method to access scatter count frequency for a given count and pay
    int getScatterCountFrequency(int scatterCount, int pay) const {
        auto it = scatterCounts.find(scatterCount);
        if (it != scatterCounts.end()) {
            auto& payFreq = it->second;
            auto payIt = payFreq.find(pay);
            if (payIt != payFreq.end()) {
                return payIt->second;
            }
        }
        return 0;
    }

    // Method to access total scatter count frequency for a given count
    int getTotalScatterCountFrequency(int scatterCount) const {
        auto it = scatterCounts.find(scatterCount);
        if (it != scatterCounts.end()) {
            return accumulate(it->second.begin(), it->second.end(), 0, [](int sum, const std::pair<int, int>& p) {
                return sum + p.second;
                });
        }
        return 0;
    }

    // Method to access total scatter pay amount for a given count
    int getTotalScatterPay(int scatterCount) const {
        auto it = scatterPays.find(scatterCount);
        if (it != scatterPays.end()) {
            return accumulate(it->second.begin(), it->second.end(), 0, [](int sum, const std::pair<int, int>& p) {
                return sum + p.second;
                });
        }
        return 0;
    }

    // Method to count total number of scatter games played
    int getTotalScatterGames() const {
        return accumulate(scatterCounts.begin(), scatterCounts.end(), 0, [](int sum, const std::pair<int, std::unordered_map<int, int>>& p) {
            return sum + std::accumulate(p.second.begin(), p.second.end(), 0, [](int innerSum, const std::pair<int, int>& innerP) {
                return innerSum + innerP.second;
                });
            });
    }


    // Method to track hits and payouts involving the wild symbol
    void trackWildSymbol(const std::string& symbol, int length, int ways, int pay) {
        trackResult(symbol, length, ways, pay);
        trackResult("W1", length, ways, pay);
    }

    int getTotalWays(const std::string& symbol, int length) const {
        auto it = symbolStats.find(symbol);
        if (it != symbolStats.end() && length > 0 && length <= it->second.hits.size()) {
            return it->second.hits[length - 1];
        }
        return 0;
    }

    int getTotalPay(const std::string& symbol, int length) const {
        auto it = symbolStats.find(symbol);
        if (it != symbolStats.end() && length > 0 && length <= it->second.pays.size()) {
            return it->second.pays[length - 1];
        }
        return 0;
    }


};
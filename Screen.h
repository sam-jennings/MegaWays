#pragma once

#include <string>
#include <vector>
#include <iostream>
#include "RandomLogGenerator.h"
#include "Symbols.h"



using namespace std;
using json = nlohmann::json;

class Screen {
private:
    int numReels;
    int numRows;
    vector<vector<std::string>> grid;
    

public:
    std::vector<std::pair<int, int>> markedPositions;

    // Default constructor
    Screen() {}

    // Constructor to initialize the screen with the specified number of reels and rows
    Screen(int _numReels, int _numRows) : numReels(_numReels), numRows(_numRows) {
        // Initialize the grid with placeholders
        grid.resize(numReels, vector<std::string>(numRows, ""));
    }

    // Function to resize the screen with the specified number of reels and rows
    /*void resize(int _numReels, int _numRows) {
        numReels = _numReels;
        numRows = _numRows;
        grid.resize(numReels, vector<std::string>(numRows, ""));
    }*/
    void resize(int _numReels, int _numRows) {
        // Resize the outer vector to have _numReels elements
        grid.resize(_numReels);

        // Resize each inner vector to have _numRows elements
        for (int i = 0; i < _numReels; ++i) {
            grid[i].resize(_numRows, ""); // Ensure all new elements are initialized to empty strings
        }

        // Update the dimensions
        numReels = _numReels;
        numRows = _numRows;
    }


   

    void display(bool displayMarkedPositions = false) {
        cout << "Current Screen:" << endl;
        for (int i = 0; i < numRows; ++i) {
            for (int j = 0; j < numReels; ++j) {
                if (displayMarkedPositions) {
                    bool marked = false;
                    for (const auto& pos : markedPositions) {
                        if (pos.first == j && pos.second == i) {
                            marked = true;
                            break;
                        }
                    }
                    if (marked) {
                       cout << setw(5) << "[" << grid[j][i] << "] ";
//cout << setw(5) << ("[" + grid[j][i] + "]");
                    }
                    else {
                        cout << setw(5) << grid[j][i] << "  ";
                    }
                }
                else {
                    cout << setw(5) << grid[j][i] << "  ";
                }
            }
            cout << endl;
        }
    }

    // Function to update a cell in the screen with a new symbol
    void updateCell(int reel, int row, const std::string& symbol) {
        if (row >= 0 && row < numRows && reel >= 0 && reel < numReels) {
            grid[reel][row] = symbol;
        }
    }

    // Function to clear the screen
    void clearScreen() {
        for (int i = 0; i < numReels; ++i) {
            for (int j = 0; j < numRows; ++j) {
                grid[i][j] = "";
            }
        }
    }

    // Function to fill the screen with symbols from spinning reel sets
    void fillScreen(const vector<vector<string>>& spinResults) {
        // Clear the screen
        clearScreen();

        // Fill the screen with symbols from spinResults
        for (int i = 0; i < min(numReels, (int)spinResults[i].size()); ++i) { 
            for (int j = 0; j < min(numRows, (int)spinResults.size()); ++j) {
                grid[i][j] = spinResults[i][j];
            }
        }
    }

    // Method to generate the screen based on the chosen indices for spinning the reels
    void generateScreen(ReelSet& reelSet) {
        clearScreen();
        for (int reelIndex = 0; reelIndex < numReels; ++reelIndex) {
            for (int rowIndex = 0; rowIndex < numRows; ++rowIndex) {
                int currentIndex = (reelSet.currentIndices[reelIndex] + rowIndex) % reelSet.reels[reelIndex].symbols.size();
                updateCell(reelIndex, rowIndex, reelSet.reels[reelIndex].symbols[currentIndex]);
            }
        }
    }
    /*void generateScreen(ReelSet& reelSet) {
        clearScreen();
        for (int reelIndex = 0; reelIndex < numReels; ++reelIndex) {
            for (int rowIndex = 0; rowIndex < numRows; ++rowIndex) {
                updateCell(reelIndex, rowIndex, reelSet.reels[reelIndex].symbols[rowIndex]);
            }
        }
    }*/

    // Function to count the number of times a symbol appears on a reel
    int countSymbolOnReel(int reelIndex, const string& symbol, bool includeWild = true) const {
        if (reelIndex < 0 || reelIndex >= numReels) {
            //  cerr << "Invalid reel index" << endl;
            return 0;
        }
        int count = 0;
        for (int i = 0; i < numRows; ++i) {
            if (includeWild) {
                if (grid[reelIndex][i] == symbol || grid[reelIndex][i] == "WL") {
                    ++count;
                }
            }
            else {
                if (grid[reelIndex][i] == symbol) {
                    ++count;
                }
            }
        }
        return count;
    }

   

    // Function to count the number of times a symbol appears on the screen
    int countSymbolOnScreen(const string& symbol, bool includeWild = true) const {
        int count = 0;
        for (int i = 0; i < numReels; ++i) {
            count += countSymbolOnReel(i, symbol, includeWild);
        }
        return count;
    }

    // Function to count the length and number of ways for a given symbol
    pair<int, int> getWaysForSymbol(const string& symbol)const {
        int length = 0;
        int ways = 1;
        for (int i = 0; i < numReels; ++i) {
            int count = countSymbolOnReel(i, symbol);
            if (count > 0) {
                length++;
                ways *= count;
            }
            else
                break;
        }
        if (length == 0) {
            ways = 0;
        }
        return make_pair(length, ways);
    }

   
    json toJson() const {
        json screenJson;

        // Convert the grid into a JSON array of arrays
        for (int i = 0; i < numRows; ++i) {
            json rowJson = json::array();
            for (int j = 0; j < numReels; ++j) {
                rowJson.push_back(grid[j][i]);
            }
            screenJson.push_back(rowJson);
        }

        return screenJson;
    }

    //// Method to cascade the screen by moving symbols down. Drop symbols to fill empty cells. New cells are filled with next symbols from the reel set.
    //void cascadeSymbols(const ReelSet& reelSet, bool useDifferentReelSet, ReelSet alternateReelSet, std::vector<int>& nextIndices) {
    //    for (int reel = 0; reel < numReels; ++reel) {
    //        // Track where the new symbols should come from in the reel
    //        int nextIndex = reelSet.reels[reel].symbols.size() - 1;

    //        for (int row = numRows - 1; row >= 0; --row) {
    //            while (grid[reel][row] == "") {
    //                // Shift symbols above down to fill this empty position
    //                for (int aboveRow = row; aboveRow > 0; aboveRow--) {
    //                    grid[reel][aboveRow] = grid[reel][aboveRow - 1];
    //                }

    //                // Fill the topmost position with a new symbol
    //                if (useDifferentReelSet) {
    //                    alternateReelSet.spinReels();
    //                    grid[reel][0] = alternateReelSet.reels[reel].symbols[0];
    //                    /*int randomIndex = getRand("ALT_REEL", alternateReelSet.reels[reel].symbols.size());
    //                    grid[reel][0] = alternateReelSet.reels[reel].symbols[randomIndex];*/
    //                }
    //                else {
    //                    grid[reel][0] = reelSet.reels[reel].symbols[nextIndices[reel]];
    //                    nextIndices[reel]--;
    //                    if (nextIndices[reel] < 0) {
    //                        nextIndices[reel] = reelSet.reels[reel].symbols.size() - 1;
    //                    }
    //                }
    //            }
    //        }
    //    }
    //}
    void cascadeSymbols(ReelSet& reelSet, bool useDifferentReelSet, ReelSet& alternateReelSet) {
        ReelSet& activeReelSet = useDifferentReelSet ? alternateReelSet : reelSet;

        for (int reel = 0; reel < numReels; ++reel) {
            for (int row = numRows - 1; row >= 0; --row) {
                while (grid[reel][row] == "") {
                    // Shift symbols above down to fill this empty position
                    for (int aboveRow = row; aboveRow > 0; aboveRow--) {
                        grid[reel][aboveRow] = grid[reel][aboveRow - 1];
                    }
                    activeReelSet.currentIndices[reel]--;
                    if (activeReelSet.currentIndices[reel] < 0) {
                        activeReelSet.currentIndices[reel] = activeReelSet.reels[reel].symbols.size() - 1;
                    }
                    // Fill the topmost position with a new symbol
                    grid[reel][0] = activeReelSet.reels[reel].symbols[activeReelSet.currentIndices[reel]];
                    
                }
            }
        }
    }
   

    void markPosition(int reel, int row) {
		markedPositions.push_back(make_pair(reel, row));
	}

    void clearMarkedPositions() {
        markedPositions.clear();
    }
    
    // Get marked positions
    const std::vector<std::pair<int, int>>& getMarkedPositions() const {
		return markedPositions;
	}

    // Mark given symbol up to length on the screen. includeWild as parameter
    void markSymbol(const string& symbol, int length, bool includeWild = true) {        
        for (int i = 0; i < length; ++i) {
            for (int j = 0; j < numRows; ++j) {
                if (grid[i][j] == symbol || (includeWild && grid[i][j] == "WL")) {
                    markedPositions.push_back(make_pair(i, j));
                }
            }
        }
    }

    void removeMarkedPositions() {
        for (const auto& position : markedPositions) {
            int reel = position.first;
            int row = position.second;
            grid[reel][row] = "";  // Clear the winning symbol
        }
	}

    // Function to fill all marked symbols with a specified symbol
    void fillMarkedSymbols(const string& symbol) {
        for (const auto& position : markedPositions) {
			int reel = position.first;
			int row = position.second;
			grid[reel][row] = symbol;
		}
	}
};


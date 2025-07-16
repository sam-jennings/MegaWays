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
    int maxHeight;
    std::vector<int> heights;
    vector<vector<std::string>> grid;
    

public:
    std::vector<std::pair<int, int>> markedPositions;

    // Default constructor
    Screen() {}

    // Constructor for screen with equal rows
    Screen(int _numReels, int _numRows) : numReels(_numReels), maxHeight(_numRows) {
        heights.reserve(_numReels);
        for (int i = 0; i < _numReels; ++i) {
			heights.push_back(_numRows); // Initialize all reels with the same height
		}
        // Initialize the grid with placeholders
        resize(heights);
    }

    // Constructor for screen with variable heights
    Screen(const std::vector<int>& _heights) : numReels(_heights.size()), heights(_heights) {
        resize(heights);
	}

    // Resize the screen based on fixed number of rows
    void resize(int _numReels, int _numRows) {
		numReels = _numReels;
		maxHeight = _numRows;
		heights.resize(numReels, _numRows); // Initialize all reels with the same height
		grid.resize(numReels, vector<string>(_numRows, "")); // Initialize the grid with empty strings
	}

    // Resize the screen with variable heights
    void resize(std::vector<int> heights) {
        grid.resize(heights.size());
        maxHeight = 0;
        // Resize each inner vector to have _numRows elements
        for (int i = 0; i < numReels; ++i) {
            if (heights[i] > maxHeight) {
				maxHeight = heights[i]; // Update maxHeight if the current reel's height is greater
			}
            grid[i].resize(heights[i], ""); // Ensure all new elements are initialized to empty strings
        }
    }

    void setReelHeight(int r, int h) { heights[r] = h; grid[r].resize(h); }

    int getReelHeight(int r) const { return heights[r]; }


    void display(bool displayMarkedPositions = false) {
        cout << "Current Screen:" << endl;
        for (int i = 0; i < maxHeight; ++i) {
            for (int j = 0; j < numReels; ++j) {
                if (i >= heights[j]) {
					cout << setw(5) << "     "; // Print empty space for reels that are shorter than the current row
					continue;
				}
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
        if (row >= 0 && row < heights[reel] && reel >= 0 && reel < numReels) {
            grid[reel][row] = symbol;
        }
    }

    // Function to clear the screen
    void clearScreen() {
        for (int i = 0; i < numReels; ++i) {
            for (int j = 0; j < heights[i]; ++j) {
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
            for (int j = 0; j < min(heights[i], (int)spinResults.size()); ++j) {
                grid[i][j] = spinResults[i][j];
            }
        }
    }

    // Method to generate the screen based on the chosen indices for spinning the reels
    void generateScreen(ReelSet& reelSet) {
        clearScreen();
        for (int reelIndex = 0; reelIndex < numReels; ++reelIndex) {
            for (int rowIndex = 0; rowIndex < heights[reelIndex]; ++rowIndex) {
                int currentIndex = (reelSet.currentIndices[reelIndex] + rowIndex) % reelSet.reels[reelIndex].symbols.size();
                updateCell(reelIndex, rowIndex, reelSet.reels[reelIndex].symbols[currentIndex]);
            }
        }
    }


    // Function to count the number of times a symbol appears on a reel
    int countSymbolOnReel(int reelIndex, const string& symbol, bool includeWild = true) const {
        if (reelIndex < 0 || reelIndex >= numReels) {
            //  cerr << "Invalid reel index" << endl;
            return 0;
        }
        int count = 0;
        for (int i = 0; i < heights[reelIndex]; ++i) {
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
        for (int i = 0; i < maxHeight; ++i) {
            json rowJson = json::array();
            for (int j = 0; j < numReels; ++j) {
                if (i >= heights[j]) {
					rowJson.push_back(" "); // Might need to change spacing
					//continue;
				} else
                rowJson.push_back(grid[j][i]);
            }
            screenJson.push_back(rowJson);
        }

        return screenJson;
    }

    void cascadeSymbols(ReelSet& reelSet, bool useDifferentReelSet, ReelSet& alternateReelSet) {
        ReelSet& activeReelSet = useDifferentReelSet ? alternateReelSet : reelSet;

        for (int reel = 0; reel < numReels; ++reel) {
            for (int row = heights[reel] - 1; row >= 0; --row) {
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
            for (int j = 0; j < heights[i]; ++j) {
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


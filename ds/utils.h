#ifndef UTILS_H
#define UTILS_H

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <limits>

namespace Utils {

    // Trims leading and trailing whitespace, tabs, \r, and \n
    inline std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }

    // Converts a string to lowercase for case-insensitive matching
    inline std::string toLower(const std::string& str) {
        std::string res = str;
        std::transform(res.begin(), res.end(), res.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return res;
    }

    // Splits a string by a delimiter character and trims tokens
    inline std::vector<std::string> split(const std::string& str, char delimiter) {
        std::vector<std::string> tokens;
        std::stringstream ss(str);
        std::string token;
        while (std::getline(ss, token, delimiter)) {
            std::string trimmed = trim(token);
            if (!trimmed.empty()) {
                tokens.push_back(trimmed);
            }
        }
        return tokens;
    }

    // Joins a vector of strings with a delimiter
    inline std::string join(const std::vector<std::string>& elements, const std::string& delimiter) {
        std::string result;
        for (size_t i = 0; i < elements.size(); ++i) {
            result += elements[i];
            if (i + 1 < elements.size()) {
                result += delimiter;
            }
        }
        return result;
    }

    // Robust integer input with range validation
    inline int getIntInput(const std::string& prompt, int minVal, int maxVal) {
        int value;
        while (true) {
            std::cout << prompt;
            if (std::cin >> value) {
                if (value >= minVal && value <= maxVal) {
                    // Clear the rest of the line
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    return value;
                } else {
                    std::cout << " [!] Please enter a number between " << minVal << " and " << maxVal << ".\n";
                }
            } else {
                std::cout << " [!] Invalid input. Please enter a valid number.\n";
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            }
        }
    }

    // Robust string line input (trims result)
    inline std::string getLineInput(const std::string& prompt) {
        std::cout << prompt;
        std::string line;
        std::getline(std::cin, line);
        return trim(line);
    }

    // Pauses for user acknowledgment
    inline void pause() {
        std::cout << "\nPress Enter to continue...";
        std::string dummy;
        std::getline(std::cin, dummy);
    }

    // Visual dividers and headers
    inline void printHeader(const std::string& title) {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "  " << title << "\n";
        std::cout << std::string(60, '=') << "\n";
    }

    inline void printSection(const std::string& title) {
        std::cout << "\n--- [ " << title << " ] ---\n";
    }

} // namespace Utils

#endif // UTILS_H

#ifndef LOADER_H
#define LOADER_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include "utils.h"

// Record representing a course from syllabus.txt
// Format: Year|Sem|Code|Name|Credits|Type|ElectiveGroup|PrereqCodes|ReferenceBook|BookSource
struct CourseRecord {
    int year = 1;
    int sem = 1;
    std::string code;
    std::string name;
    int credits = 0;
    std::string type;
    std::string electiveGroup = "-"; // "-" if not an elective
    std::vector<std::string> prereqCodes;
    std::string referenceBook;
    char bookSource = 'N'; // S = in syllabus, G = suggested, N = not specified
};

/**
 * Purpose: Loads the official B.Tech CSE syllabus from syllabus.txt.
 * Format: Year|Sem|Code|Name|Credits|Type|ElectiveGroup|PrereqCodes|ReferenceBook|BookSource
 * Validates format, trims whitespace, and reports exact line numbers for any malformed rows.
 * Time Complexity: O(L) where L is total characters in file.
 */
inline bool loadSyllabusFile(
    const std::string& filename,
    std::unordered_map<std::string, CourseRecord>& catalog,
    std::vector<std::string>& courseOrder,
    std::string& errorMsg)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        errorMsg = "Could not open file: " + filename;
        return false;
    }

    catalog.clear();
    courseOrder.clear();

    std::string line;
    int lineNum = 0;
    while (std::getline(file, line)) {
        lineNum++;
        std::string trimmed = Utils::trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue; // Skip comments and blank lines
        }

        std::vector<std::string> tokens = Utils::split(trimmed, '|');
        if (tokens.size() < 10) {
            errorMsg = "Line " + std::to_string(lineNum) + ": Expected 10 fields separated by '|', but found " +
                       std::to_string(tokens.size()) + " in line: " + trimmed;
            file.close();
            return false;
        }

        try {
            CourseRecord rec;
            rec.year = std::stoi(tokens[0]);
            rec.sem = std::stoi(tokens[1]);
            rec.code = tokens[2];
            rec.name = tokens[3];
            rec.credits = std::stoi(tokens[4]);
            rec.type = tokens[5];
            rec.electiveGroup = tokens[6].empty() ? "-" : tokens[6];

            // Parse comma-separated prerequisite codes
            std::string prereqStr = tokens[7];
            if (!prereqStr.empty() && prereqStr != "-") {
                std::vector<std::string> pList = Utils::split(prereqStr, ',');
                for (const auto& p : pList) {
                    if (!p.empty() && p != "-") {
                        rec.prereqCodes.push_back(p);
                    }
                }
            }

            rec.referenceBook = tokens[8].empty() ? "Not specified" : tokens[8];
            rec.bookSource = tokens[9].empty() ? 'N' : tokens[9][0];

            catalog[rec.code] = rec;
            courseOrder.push_back(rec.code);
        } catch (const std::exception& ex) {
            errorMsg = "Line " + std::to_string(lineNum) + ": Data conversion error (" + ex.what() + ") in line: " + trimmed;
            file.close();
            return false;
        }
    }

    file.close();
    return true;
}

/**
 * Purpose: Saves updated syllabus catalog back to syllabus.txt in exact same format.
 * Time Complexity: O(N) where N is number of courses.
 */
inline bool saveSyllabusFile(
    const std::string& filename,
    const std::unordered_map<std::string, CourseRecord>& catalog,
    const std::vector<std::string>& courseOrder)
{
    std::ofstream file(filename);
    if (!file.is_open()) return false;

    file << "# KKWIEER Nashik | B.Tech Computer Engineering | 2023 Pattern | 172 credits, 8 semesters\n";
    file << "# Year|Sem|Code|Name|Credits|Type|ElectiveGroup|PrereqCodes|ReferenceBook|BookSource\n";
    file << "# BookSource: S = listed in syllabus, G = suggested famous book (verify), N = not specified\n";

    std::unordered_map<std::string, bool> written;
    for (const std::string& code : courseOrder) {
        auto it = catalog.find(code);
        if (it != catalog.end()) {
            const CourseRecord& rec = it->second;
            file << rec.year << "|" << rec.sem << "|" << rec.code << "|" << rec.name << "|"
                 << rec.credits << "|" << rec.type << "|" << rec.electiveGroup << "|";

            if (rec.prereqCodes.empty()) {
                file << "-";
            } else {
                for (size_t i = 0; i < rec.prereqCodes.size(); ++i) {
                    file << rec.prereqCodes[i];
                    if (i + 1 < rec.prereqCodes.size()) file << ",";
                }
            }

            file << "|" << rec.referenceBook << "|" << rec.bookSource << "\n";
            written[code] = true;
        }
    }

    // Write any new courses added that weren't in courseOrder
    for (const auto& pair : catalog) {
        if (!written[pair.first]) {
            const CourseRecord& rec = pair.second;
            file << rec.year << "|" << rec.sem << "|" << rec.code << "|" << rec.name << "|"
                 << rec.credits << "|" << rec.type << "|" << rec.electiveGroup << "|";

            if (rec.prereqCodes.empty()) {
                file << "-";
            } else {
                for (size_t i = 0; i < rec.prereqCodes.size(); ++i) {
                    file << rec.prereqCodes[i];
                    if (i + 1 < rec.prereqCodes.size()) file << ",";
                }
            }

            file << "|" << rec.referenceBook << "|" << rec.bookSource << "\n";
        }
    }

    file.close();
    return true;
}

/**
 * Purpose: Exports syllabus to web/data.js as `const SYLLABUS = [...]` for zero-dependency offline web use.
 * Time Complexity: O(N) where N is number of courses.
 */
inline bool exportWebData(
    const std::string& outputFile,
    const std::unordered_map<std::string, CourseRecord>& catalog,
    const std::vector<std::string>& courseOrder)
{
    std::ofstream file(outputFile);
    if (!file.is_open()) return false;

    file << "// KKWIEER Nashik | B.Tech Computer Engineering | 2023 Pattern | 172 Credits\n";
    file << "// Generated automatically by Course Prerequisite Planner (--export-web)\n";
    file << "const SYLLABUS = [\n";

    for (size_t idx = 0; idx < courseOrder.size(); ++idx) {
        const std::string& code = courseOrder[idx];
        auto it = catalog.find(code);
        if (it == catalog.end()) continue;
        const CourseRecord& r = it->second;

        file << "  {\n";
        file << "    \"year\": " << r.year << ",\n";
        file << "    \"sem\": " << r.sem << ",\n";
        file << "    \"code\": \"" << r.code << "\",\n";
        file << "    \"name\": \"" << r.name << "\",\n";
        file << "    \"credits\": " << r.credits << ",\n";
        file << "    \"type\": \"" << r.type << "\",\n";
        file << "    \"electiveGroup\": \"" << r.electiveGroup << "\",\n";
        file << "    \"prereqCodes\": [";
        for (size_t i = 0; i < r.prereqCodes.size(); ++i) {
            file << "\"" << r.prereqCodes[i] << "\"";
            if (i + 1 < r.prereqCodes.size()) file << ", ";
        }
        file << "],\n";
        file << "    \"referenceBook\": \"" << r.referenceBook << "\",\n";
        file << "    \"bookSource\": \"" << r.bookSource << "\"\n";
        file << "  }" << (idx + 1 < courseOrder.size() ? "," : "") << "\n";
    }

    file << "];\n";
    file.close();
    return true;
}

#endif // LOADER_H

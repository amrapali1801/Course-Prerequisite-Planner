#include <iostream>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

#include "utils.h"
#include "tree.h"
#include "loader.h"
#include "planner.h"

// Displays formatted main menu
void displayMainMenu() {
    std::cout << "\n============================================================\n";
    std::cout << "  KKWIEER NASHIK B.TECH COMPUTER ENGINEERING (2023 PATTERN) \n";
    std::cout << "         COURSE PREREQUISITE PLANNER (Tree DSA)             \n";
    std::cout << "============================================================\n";
    std::cout << " 1. Show prerequisite tree of a course\n";
    std::cout << " 2. Minimum semesters needed for a course (Tree Height)\n";
    std::cout << " 3. Show all learning paths to a course (DFS + Backtracking)\n";
    std::cout << " 4. What can I take now? (enter completed courses)\n";
    std::cout << " 5. Show curriculum with credits (hierarchical post-order)\n";
    std::cout << " 6. Search course (DFS / BFS) & View Course Details\n";
    std::cout << " 7. Semester-wise plan (BFS level-order)\n";
    std::cout << " 8. Add / remove a course or prerequisite\n";
    std::cout << " 9. Common ancestor of two courses (LCA)\n";
    std::cout << " 10. Undo last change (Stack-based)\n";
    std::cout << " 11. Validate data (Integrity & progression checks)\n";
    std::cout << " 12. Save and exit\n";
    std::cout << "============================================================\n";
}

// Submenu for Add/Remove operations (Option 8)
void handleAddRemoveSubmenu(CoursePlanner& planner) {
    while (true) {
        std::cout << "\n--- [ Modify Syllabus & Prerequisites ] ---\n";
        std::cout << " 1. Add a new course\n";
        std::cout << " 2. Remove an existing course\n";
        std::cout << " 3. Add a prerequisite to a course\n";
        std::cout << " 4. Remove a prerequisite from a course\n";
        std::cout << " 5. Return to main menu\n";
        std::cout << "-------------------------------------------\n";

        int subChoice = Utils::getIntInput(" Enter sub-choice (1-5): ", 1, 5);

        if (subChoice == 1) {
            std::cout << "\n[Add Course]\n";
            std::string code = Utils::getLineInput(" Course code (e.g. 2301499): ");
            if (code.empty()) {
                std::cout << " [!] Course code cannot be empty.\n";
                continue;
            }
            std::string name = Utils::getLineInput(" Course name (e.g. Quantum Computing): ");
            int year = Utils::getIntInput(" Academic Year (1-4): ", 1, 4);
            int sem = Utils::getIntInput(" Semester (" + std::to_string((year-1)*2+1) + "-" + std::to_string(year*2) + "): ",
                                         (year - 1) * 2 + 1, year * 2);
            int credits = Utils::getIntInput(" Credits (1-12): ", 1, 12);
            std::string type = Utils::getLineInput(" Course Type (e.g. PCC, PEC, OE, BSC, ESC): ");
            if (type.empty()) type = "PCC";
            std::string elec = Utils::getLineInput(" Elective Group (e.g. Elective I, or - if none): ");
            if (elec.empty()) elec = "-";
            std::string prereqInput = Utils::getLineInput(" Prerequisite Codes (comma-separated, - if none): ");
            std::vector<std::string> prereqs;
            if (!prereqInput.empty() && prereqInput != "-") {
                prereqs = Utils::split(prereqInput, ',');
            }
            std::string book = Utils::getLineInput(" Reference Book: ");
            if (book.empty()) book = "Not specified";
            std::string sourceStr = Utils::getLineInput(" Book Source (S = Syllabus, G = Suggested, N = Not specified): ");
            char source = 'N';
            if (!sourceStr.empty()) source = std::toupper(sourceStr[0]);

            CourseRecord rec;
            rec.year = year;
            rec.sem = sem;
            rec.code = code;
            rec.name = name;
            rec.credits = credits;
            rec.type = type;
            rec.electiveGroup = elec;
            rec.prereqCodes = prereqs;
            rec.referenceBook = book;
            rec.bookSource = source;

            planner.addCourse(rec);

        } else if (subChoice == 2) {
            std::cout << "\n[Remove Course]\n";
            std::string query = Utils::getLineInput(" Enter course code or name to remove: ");
            if (!query.empty()) {
                planner.removeCourse(query);
            }

        } else if (subChoice == 3) {
            std::cout << "\n[Add Prerequisite]\n";
            std::string courseQ = Utils::getLineInput(" Enter target course (code or name): ");
            std::string prereqQ = Utils::getLineInput(" Enter prerequisite course (code or name): ");
            if (!courseQ.empty() && !prereqQ.empty()) {
                planner.addPrerequisite(courseQ, prereqQ);
            }

        } else if (subChoice == 4) {
            std::cout << "\n[Remove Prerequisite]\n";
            std::string courseQ = Utils::getLineInput(" Enter target course (code or name): ");
            std::string prereqQ = Utils::getLineInput(" Enter prerequisite course (code or name): ");
            if (!courseQ.empty() && !prereqQ.empty()) {
                planner.removePrerequisite(courseQ, prereqQ);
            }

        } else if (subChoice == 5) {
            break;
        }
    }
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    CoursePlanner planner("data/syllabus.txt");
    std::string errorMsg;
    if (!planner.init(errorMsg)) {
        std::cerr << " [FATAL ERROR] Failed to load syllabus: " << errorMsg << "\n";
        return 1;
    }

    // CLI Option: Export to web/data.js
    if (argc > 1 && std::string(argv[1]) == "--export-web") {
        if (planner.exportWeb("web/data.js")) {
            std::cout << " [✓] Exported syllabus data to web/data.js successfully.\n";
            return 0;
        } else {
            std::cerr << " [!] Failed to export to web/data.js\n";
            return 1;
        }
    }

    // CLI Option: Dump metrics for Parity Test
    // Format: code,height,subtreeSize,uniquePrereqs,foundationCount
    if (argc > 1 && std::string(argv[1]) == "--dump-metrics") {
        planner.dumpMetrics(std::cout);
        return 0;
    }

    std::cout << "\n============================================================\n";
    std::cout << "     KKWIEER NASHIK - B.TECH COMPUTER ENGINEERING (2023)    \n";
    std::cout << "                 COURSE PREREQUISITE PLANNER                \n";
    std::cout << "============================================================\n";

    // Run and display start-up checks
    planner.runStartupChecks(true);
    planner.printSummary();

    bool running = true;
    while (running) {
        displayMainMenu();
        int choice = Utils::getIntInput(" Select an option (1-12): ", 1, 12);

        switch (choice) {
            case 1: {
                // Feature 1 & 2: Show prerequisite tree
                std::string q = Utils::getLineInput(" Enter course code or name (e.g. Deep Learning, 2301401): ");
                if (!q.empty()) {
                    planner.displayPrerequisiteTree(q);
                }
                Utils::pause();
                break;
            }
            case 2: {
                // Feature 3: Minimum semesters needed
                std::string q = Utils::getLineInput(" Enter course code or name: ");
                if (!q.empty()) {
                    planner.showMinimumSemesters(q);
                }
                Utils::pause();
                break;
            }
            case 3: {
                // Feature 4, 5, 6: Learning paths, foundation courses, prereq counts
                std::string q = Utils::getLineInput(" Enter course code or name: ");
                if (!q.empty()) {
                    planner.showAllLearningPaths(q);
                }
                Utils::pause();
                break;
            }
            case 4: {
                // Feature 7: What can I take now?
                std::cout << "\n Enter completed courses separated by commas.\n"
                          << " (e.g., 2300101A, Programming in C, 2300118A)\n";
                std::string line = Utils::getLineInput(" Completed courses: ");
                std::vector<std::string> completed = Utils::split(line, ',');
                planner.whatCanITakeNow(completed);
                Utils::pause();
                break;
            }
            case 5: {
                // Feature 8: Curriculum view with credits (hierarchical post-order sum)
                planner.displayCurriculumWithCredits();
                Utils::pause();
                break;
            }
            case 6: {
                // Feature 9: Search course using DFS or BFS & View Details
                std::string q = Utils::getLineInput(" Enter course code or name to search: ");
                if (!q.empty()) {
                    std::cout << " Choose traversal strategy:\n";
                    std::cout << "   1. Depth-First Search (DFS)\n";
                    std::cout << "   2. Breadth-First Search (BFS)\n";
                    int strat = Utils::getIntInput(" Select strategy (1 or 2): ", 1, 2);
                    planner.searchCourse(q, strat);
                }
                Utils::pause();
                break;
            }
            case 7: {
                // Feature 10: Semester-wise plan (BFS level-order)
                planner.displaySemesterPlan();
                Utils::pause();
                break;
            }
            case 8: {
                // Feature 11: Add / remove course or prerequisite
                handleAddRemoveSubmenu(planner);
                break;
            }
            case 9: {
                // Feature 12: Lowest Common Ancestor
                std::cout << "\n[Lowest Common Ancestor in Curriculum Tree]\n";
                std::string c1 = Utils::getLineInput(" Enter first course (code or name): ");
                std::string c2 = Utils::getLineInput(" Enter second course (code or name): ");
                if (!c1.empty() && !c2.empty()) {
                    planner.showLowestCommonAncestor(c1, c2);
                }
                Utils::pause();
                break;
            }
            case 10: {
                // Feature 13: Undo last change using stack
                planner.undoLastAction();
                Utils::pause();
                break;
            }
            case 11: {
                // Feature 14: Validate data
                planner.runStartupChecks(true);
                Utils::pause();
                break;
            }
            case 12: {
                // Save and exit
                std::cout << "\n Saving syllabus and freeing all allocated tree memory...\n";
                planner.saveFiles();
                std::cout << " Thank you for using Course Prerequisite Planner.\n";
                running = false;
                break;
            }
            default:
                break;
        }
    }

    return 0;
}

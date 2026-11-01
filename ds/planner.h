#ifndef PLANNER_H
#define PLANNER_H

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <stack>
#include <algorithm>
#include "tree.h"
#include "loader.h"
#include "utils.h"

// Enumeration of reversible actions for Undo stack
enum class ActionType {
    ADD_COURSE,
    REMOVE_COURSE,
    ADD_PREREQUISITE,
    REMOVE_PREREQUISITE
};

// Undo action record
struct UndoAction {
    ActionType type;
    CourseRecord course;                 // Course record for add/remove
    std::string targetCourse;            // Target course code for prereq change
    std::string targetPrereq;            // Prereq code for prereq change
    std::vector<std::string> referencingCourses; // Dependent courses affected
};

class CoursePlanner {
private:
    std::string syllabusFilename;

    // Code -> CourseRecord catalog
    std::unordered_map<std::string, CourseRecord> catalog;
    std::vector<std::string> courseOrder;

    // Root of Curriculum Tree (Degree -> Year -> Semester -> ElectiveGroup -> Course)
    CurNode* curriculumRoot = nullptr;

    // Undo stack for reversible commands
    std::stack<UndoAction> undoStack;

    // Helper: Finds a Semester node in curriculum tree
    CurNode* findSemesterNode(int semesterNum) {
        if (!curriculumRoot) return nullptr;
        std::string semTarget = "Semester " + std::to_string(semesterNum);
        for (CurNode* yearNode : curriculumRoot->children) {
            for (CurNode* semNode : yearNode->children) {
                if (semNode->name == semTarget) {
                    return semNode;
                }
            }
        }
        return nullptr;
    }

    // Helper: Finds or creates an ElectiveGroup node under a Semester node
    CurNode* findOrCreateElectiveNode(CurNode* semNode, const std::string& groupName) {
        if (!semNode) return nullptr;
        for (CurNode* child : semNode->children) {
            if (child->isElectiveGroup && child->name == groupName) {
                return child;
            }
        }
        CurNode* groupNode = new CurNode(groupName, 0, semNode, "", true);
        semNode->children.push_back(groupNode);
        return groupNode;
    }

    // Helper: Extracts semester number from parent pointers
    int getCourseSemesterViaParent(CurNode* courseNode) const {
        if (!courseNode) return -1;
        CurNode* p = courseNode->parent;
        while (p != nullptr) {
            if (p->name.rfind("Semester ", 0) == 0) {
                try {
                    return std::stoi(p->name.substr(9));
                } catch (...) {
                    return -1;
                }
            }
            p = p->parent;
        }
        return -1;
    }

public:
    explicit CoursePlanner(const std::string& sFile = "data/syllabus.txt")
        : syllabusFilename(sFile) {}

    ~CoursePlanner() {
        if (curriculumRoot) {
            destroyCurTree(curriculumRoot);
            curriculumRoot = nullptr;
        }
    }

    // Builds the official Curriculum Tree with 4 Years, 8 Semesters, and Elective Groups
    void buildCurriculumTreeStructure() {
        if (curriculumRoot) {
            destroyCurTree(curriculumRoot);
            curriculumRoot = nullptr;
        }

        curriculumRoot = new CurNode("B.Tech Computer Engineering", 0, nullptr);

        // Create 4 Years and 8 Semesters
        for (int y = 1; y <= 4; ++y) {
            CurNode* yearNode = new CurNode("Year " + std::to_string(y), 0, curriculumRoot);
            curriculumRoot->children.push_back(yearNode);

            for (int s = (y - 1) * 2 + 1; s <= y * 2; ++s) {
                CurNode* semNode = new CurNode("Semester " + std::to_string(s), 0, yearNode);
                yearNode->children.push_back(semNode);
            }
        }

        // Attach courses (regular courses go directly to semester; electives group together)
        for (const std::string& code : courseOrder) {
            auto it = catalog.find(code);
            if (it == catalog.end()) continue;
            const CourseRecord& rec = it->second;

            CurNode* semNode = findSemesterNode(rec.sem);
            if (!semNode) continue;

            if (rec.electiveGroup != "-" && !rec.electiveGroup.empty()) {
                CurNode* elecNode = findOrCreateElectiveNode(semNode, rec.electiveGroup);
                CurNode* courseNode = new CurNode(rec.name, rec.credits, elecNode, rec.code);
                elecNode->children.push_back(courseNode);
            } else {
                CurNode* courseNode = new CurNode(rec.name, rec.credits, semNode, rec.code);
                semNode->children.push_back(courseNode);
            }
        }

        // Post-order rollup: Courses -> Elective Groups (counted once) -> Semesters -> Years -> Degree (172)
        computeCurriculumCredits(curriculumRoot);
    }

    // Initializes database from syllabus.txt
    bool init(std::string& errorMsg) {
        if (!loadSyllabusFile(syllabusFilename, catalog, courseOrder, errorMsg)) {
            // Fallback for Windows if file was created as syllabus.txt.txt
            if (syllabusFilename == "data/syllabus.txt") {
                std::string fallback = "data/syllabus.txt.txt";
                std::string fbErr;
                if (loadSyllabusFile(fallback, catalog, courseOrder, fbErr)) {
                    syllabusFilename = fallback;
                    buildCurriculumTreeStructure();
                    return true;
                }
            }
            return false;
        }
        buildCurriculumTreeStructure();
        return true;
    }

    // Exports data to web/data.js
    bool exportWeb(const std::string& path = "web/data.js") const {
        return exportWebData(path, catalog, courseOrder);
    }

    // Dumps metrics in CSV format: code,height,subtreeSize,uniquePrereqs,foundationCount
    void dumpMetrics(std::ostream& out) {
        for (const std::string& code : courseOrder) {
            bool cycle = false;
            std::unordered_set<std::string> visiting;
            PreNode* tree = buildPrereqTree(code, catalog, visiting, cycle);

            int h = getTreeHeight(tree);
            int sz = countSubtreeSize(tree);
            std::unordered_set<std::string> uniqPrereqs;
            collectUniquePrereqs(tree, uniqPrereqs);
            std::unordered_set<std::string> foundations;
            getFoundationCourses(tree, foundations);

            out << code << "," << h << "," << sz << "," << uniqPrereqs.size() << "," << foundations.size() << "\n";
            destroyPreTree(tree);
        }
    }

    // ========================================================================
    // START-UP VERIFICATION CHECKS (PASS / FAIL)
    // ========================================================================
    bool runStartupChecks(bool printOutput = true) {
        bool allPass = true;
        std::vector<int> expectedCredits = {20, 22, 22, 22, 22, 22, 22, 20};
        std::vector<int> actualCredits(8, 0);

        // Calculate semester credits from curriculum tree
        for (int s = 1; s <= 8; ++s) {
            CurNode* semNode = findSemesterNode(s);
            if (semNode) {
                actualCredits[s - 1] = semNode->credits;
            }
        }

        int degreeTotal = curriculumRoot ? curriculumRoot->credits : 0;

        // Check 1: Semester credits pattern
        bool semCreditsOk = (actualCredits == expectedCredits);
        if (!semCreditsOk) allPass = false;

        // Check 2: Degree total
        bool degreeTotalOk = (degreeTotal == 172);
        if (!degreeTotalOk) allPass = false;

        // Check 3: Every prerequisite exists
        bool prereqsExistOk = true;
        std::string missingInfo;
        for (const auto& pair : catalog) {
            for (const std::string& pr : pair.second.prereqCodes) {
                if (catalog.find(pr) == catalog.end()) {
                    prereqsExistOk = false;
                    missingInfo += "Course " + pair.first + " references missing prerequisite: " + pr + "; ";
                }
            }
        }
        if (!prereqsExistOk) allPass = false;

        // Check 4: No self-loop or cycle
        bool noCyclesOk = true;
        std::string cycleInfo;
        for (const auto& pair : catalog) {
            for (const std::string& pr : pair.second.prereqCodes) {
                if (pr == pair.first) {
                    noCyclesOk = false;
                    cycleInfo += "Self-loop: " + pair.first + " requires itself; ";
                }
            }
            std::unordered_set<std::string> visiting;
            bool localCycle = false;
            PreNode* temp = buildPrereqTree(pair.first, catalog, visiting, localCycle);
            if (localCycle) {
                noCyclesOk = false;
                cycleInfo += "Cycle in chain of " + pair.first + "; ";
            }
            destroyPreTree(temp);
        }
        if (!noCyclesOk) allPass = false;

        // Check 5: Every prerequisite is in an EARLIER semester (via parent pointers)
        bool earlierSemOk = true;
        std::string earlierInfo;
        for (const auto& pair : catalog) {
            int v1 = 0;
            CurNode* cNode = searchCurriculumDFS(curriculumRoot, pair.first, v1);
            int cSem = getCourseSemesterViaParent(cNode);

            for (const std::string& pr : pair.second.prereqCodes) {
                int v2 = 0;
                CurNode* pNode = searchCurriculumDFS(curriculumRoot, pr, v2);
                int pSem = getCourseSemesterViaParent(pNode);

                if (pSem >= cSem) {
                    earlierSemOk = false;
                    earlierInfo += pair.first + " (Sem " + std::to_string(cSem) + ") requires " +
                                   pr + " (Sem " + std::to_string(pSem) + "); ";
                }
            }
        }
        if (!earlierSemOk) allPass = false;

        if (printOutput) {
            std::cout << "\n============================================================\n";
            std::cout << "          START-UP DATA INTEGRITY VERIFICATION              \n";
            std::cout << "============================================================\n";

            std::cout << (semCreditsOk ? " [PASS]" : " [FAIL]")
                      << " Semester Credits Match Pattern (20,22,22,22,22,22,22,20):\n"
                      << "        Actual:   ";
            for (size_t i = 0; i < actualCredits.size(); ++i) {
                std::cout << actualCredits[i] << (i + 1 < actualCredits.size() ? ", " : "\n");
            }

            std::cout << (degreeTotalOk ? " [PASS]" : " [FAIL]")
                      << " Degree Total Credits: Expected 172 | Actual " << degreeTotal << "\n";

            std::cout << (prereqsExistOk ? " [PASS]" : " [FAIL]")
                      << " Prerequisite Existence: All referenced course codes exist\n";
            if (!prereqsExistOk) std::cout << "        " << missingInfo << "\n";

            std::cout << (noCyclesOk ? " [PASS]" : " [FAIL]")
                      << " Cycle Detection: Prerequisite structure is completely acyclic\n";
            if (!noCyclesOk) std::cout << "        " << cycleInfo << "\n";

            std::cout << (earlierSemOk ? " [PASS]" : " [FAIL]")
                      << " Semester Progression: Every prerequisite in EARLIER semester\n"
                      << "        (Verified via Curriculum Tree Parent Pointers)\n";
            if (!earlierSemOk) std::cout << "        " << earlierInfo << "\n";

            std::cout << "============================================================\n";
        }

        return allPass;
    }

    // ========================================================================
    // COURSE RESOLUTION & LOOKUP (By Code or Partial Name)
    // ========================================================================
    std::string resolveCourse(const std::string& query) const {
        std::string trimmed = Utils::trim(query);
        if (trimmed.empty()) return "";

        std::string lowerQ = Utils::toLower(trimmed);

        // 1. Direct exact code match
        for (const auto& pair : catalog) {
            if (Utils::toLower(pair.first) == lowerQ) {
                return pair.first;
            }
        }

        // 2. Exact name match
        for (const auto& pair : catalog) {
            if (Utils::toLower(pair.second.name) == lowerQ) {
                return pair.first;
            }
        }

        // 3. Partial matches (by code or name)
        std::vector<std::string> matches;
        for (const std::string& code : courseOrder) {
            const CourseRecord& r = catalog.at(code);
            if (Utils::toLower(r.code).find(lowerQ) != std::string::npos ||
                Utils::toLower(r.name).find(lowerQ) != std::string::npos) {
                matches.push_back(code);
            }
        }

        if (matches.empty()) {
            std::cout << " [!] No courses matching query: \"" << query << "\"\n";
            return "";
        }

        if (matches.size() == 1) {
            return matches[0];
        }

        // Multiple matches: list options for user
        std::cout << "\n Multiple courses matched \"" << query << "\":\n";
        for (size_t i = 0; i < matches.size(); ++i) {
            const CourseRecord& r = catalog.at(matches[i]);
            std::cout << "   " << std::setw(2) << (i + 1) << ". " << r.name << " [" << r.code
                      << "] (Sem " << r.sem << ", " << r.credits << " cr)\n";
        }

        int choice = Utils::getIntInput(" Select a course (1-" + std::to_string(matches.size()) + ") or 0 to cancel: ",
                                         0, static_cast<int>(matches.size()));
        if (choice == 0) return "";
        return matches[choice - 1];
    }

    // ========================================================================
    // FEATURE 1 & 2: Show Prerequisite Tree
    // ========================================================================
    void displayPrerequisiteTree(const std::string& courseInput) {
        std::string code = resolveCourse(courseInput);
        if (code.empty()) return;

        const CourseRecord& r = catalog.at(code);
        bool cycle = false;
        std::unordered_set<std::string> visiting;
        PreNode* root = buildPrereqTree(code, catalog, visiting, cycle);

        Utils::printSection("Prerequisite Tree for: " + r.name + " [" + r.code + "]");
        std::cout << "Structure (pre-order view with branch visualization):\n\n";
        printPrereqTree(root);
        std::cout << "\n";

        int height = getTreeHeight(root);
        int sz = countSubtreeSize(root);
        std::unordered_set<std::string> uniquePrereqs;
        collectUniquePrereqs(root, uniquePrereqs);
        std::unordered_set<std::string> foundations;
        getFoundationCourses(root, foundations);

        std::cout << "------------------------------------------------------------\n";
        std::cout << " Tree Metrics (Tree Concepts Applied):\n";
        std::cout << "  * Tree Height (Min Semesters)    : " << height << " semester(s)\n";
        std::cout << "  * Subtree Size (Total Tree Nodes): " << sz << " (including root, with branch replication)\n";
        std::cout << "  * Unique Prerequisite Courses    : " << uniquePrereqs.size() << "\n";
        std::cout << "  * Foundation Prerequisites       : " << foundations.size() << "\n";
        std::cout << "------------------------------------------------------------\n";

        destroyPreTree(root);
    }

    // ========================================================================
    // FEATURE 3: Minimum Semesters Needed
    // ========================================================================
    void showMinimumSemesters(const std::string& courseInput) {
        std::string code = resolveCourse(courseInput);
        if (code.empty()) return;

        const CourseRecord& r = catalog.at(code);
        bool cycle = false;
        std::unordered_set<std::string> visiting;
        PreNode* root = buildPrereqTree(code, catalog, visiting, cycle);

        int height = getTreeHeight(root);
        auto critPath = getCriticalPath(root);

        Utils::printSection("Minimum Semesters Analysis for: " + r.name + " [" + r.code + "]");
        std::cout << " [Tree Concept: Tree Height / Maximum Depth]\n";
        std::cout << " Minimum semesters required sequentially: " << height << " semester(s)\n\n";

        std::cout << " Longest Critical Prerequisite Chain:\n   ";
        for (size_t i = 0; i < critPath.size(); ++i) {
            std::cout << "Step " << (i + 1) << ": " << critPath[i].first << " [" << critPath[i].second << "]";
            if (i + 1 < critPath.size()) std::cout << "  ==>  ";
        }
        std::cout << "\n\n (Note: Assumes prerequisites in sequence are taken one per semester).\n";

        destroyPreTree(root);
    }

    // ========================================================================
    // FEATURE 4, 5, 6: Foundation Courses, Learning Paths, Prereq Counts
    // ========================================================================
    void showAllLearningPaths(const std::string& courseInput) {
        std::string code = resolveCourse(courseInput);
        if (code.empty()) return;

        const CourseRecord& r = catalog.at(code);
        bool cycle = false;
        std::unordered_set<std::string> visiting;
        PreNode* root = buildPrereqTree(code, catalog, visiting, cycle);

        std::unordered_set<std::string> foundations;
        getFoundationCourses(root, foundations);

        Utils::printSection("Foundation Courses for: " + r.name + " [" + r.code + "]");
        std::cout << " [Tree Concept: Leaf-Node Identification]\n";
        std::cout << " Foundation courses (entry points with no prerequisites):\n";
        if (foundations.empty()) {
            std::cout << "   None (Course itself is a foundation course or standalone entry).\n";
        } else {
            int idx = 1;
            for (const std::string& fCode : foundations) {
                std::string fName = catalog.count(fCode) ? catalog.at(fCode).name : fCode;
                std::cout << "   " << idx++ << ". " << fName << " [" << fCode << "]\n";
            }
        }

        Utils::printSection("All Learning Trajectories to: " + r.name + " [" + r.code + "]");
        std::cout << " [Tree Concept: Root-to-Leaf Path Printing via DFS with Backtracking]\n";
        std::cout << " Trajectories from Foundation Courses (Leaves) -> Target Course (Root):\n\n";

        std::vector<std::string> path;
        int pathCount = 0;
        printAllLearningPathsDFS(root, path, pathCount);

        if (pathCount == 0) {
            std::cout << "  (Course is a foundation entry with no prerequisite path).\n";
        }

        int sz = countSubtreeSize(root);
        std::unordered_set<std::string> uniquePrereqs;
        collectUniquePrereqs(root, uniquePrereqs);

        Utils::printSection("Prerequisite Counts Summary");
        std::cout << " [Tree Concept: Subtree Size (Post-order Traversal)]\n";
        std::cout << " Total tree nodes (with branch replication) : " << sz << "\n";
        std::cout << " Total unique prerequisite courses required : " << uniquePrereqs.size() << "\n";
        std::cout << " Total distinct learning trajectories      : " << pathCount << "\n";

        destroyPreTree(root);
    }

    // ========================================================================
    // FEATURE 7: "What can I take now?"
    // ========================================================================
    void whatCanITakeNow(const std::vector<std::string>& completedInputs) {
        std::unordered_set<std::string> completedCodes;
        int earnedCredits = 0;

        for (const std::string& input : completedInputs) {
            std::string code = resolveCourse(input);
            if (!code.empty()) {
                completedCodes.insert(code);
                earnedCredits += catalog.at(code).credits;
            }
        }

        Utils::printSection("Course Eligibility Evaluation: What Can I Take Now?");
        std::cout << " [Tree Concept: Node Evaluation / Prerequisite Satisfaction Check]\n";

        std::cout << "\n Completed Courses: " << completedCodes.size() << " / " << catalog.size()
                  << " | Earned Credits: " << earnedCredits << " / 172 Credits\n";

        // Check eligibility for each course
        std::vector<std::string> eligible;
        for (const std::string& code : courseOrder) {
            if (completedCodes.count(code) > 0) continue; // Already taken

            const CourseRecord& r = catalog.at(code);
            bool canTake = true;
            for (const std::string& pr : r.prereqCodes) {
                if (completedCodes.count(pr) == 0) {
                    canTake = false;
                    break;
                }
            }
            if (canTake) {
                eligible.push_back(code);
            }
        }

        std::cout << "\n Eligible Courses You Can Take Immediately (" << eligible.size() << "):\n";
        if (eligible.empty()) {
            std::cout << "   No new courses are eligible. Complete more prerequisites first!\n";
        } else {
            // Group by semester
            for (int s = 1; s <= 8; ++s) {
                bool headerPrinted = false;
                int idx = 1;
                for (const std::string& code : eligible) {
                    const CourseRecord& r = catalog.at(code);
                    if (r.sem == s) {
                        if (!headerPrinted) {
                            std::cout << "\n  --- [ Semester " << s << " ] ---\n";
                            headerPrinted = true;
                        }
                        std::string electiveNote = (r.electiveGroup != "-") ? (" (Elective: " + r.electiveGroup + " - pick 1)") : "";
                        std::cout << "   " << std::right << std::setw(2) << idx++ << ". "
                                  << std::left << std::setw(42) << (r.name + " [" + r.code + "]")
                                  << " [" << r.credits << " Credits]" << electiveNote << "\n";
                    }
                }
            }
        }
    }

    // ========================================================================
    // FEATURE 8: Curriculum view with total credits (Post-order sum)
    // ========================================================================
    void displayCurriculumWithCredits() {
        if (!curriculumRoot) return;
        computeCurriculumCredits(curriculumRoot);

        Utils::printSection("Complete Curriculum Hierarchy with Credit Rollup");
        std::cout << " [Tree Concept: Post-order Traversal Sum (Elective Groups Count Once)]\n\n";
        printCurriculumHierarchical(curriculumRoot);
        std::cout << "\n";
    }

    // ========================================================================
    // FEATURE 9: Search course (DFS / BFS) & Course Details Screen
    // ========================================================================
    void searchCourse(const std::string& query, int strategy) {
        std::string code = resolveCourse(query);
        if (code.empty()) return;

        const CourseRecord& r = catalog.at(code);
        int visitCount = 0;
        CurNode* resNode = nullptr;

        Utils::printSection("Searching Curriculum Hierarchy: " + r.name + " [" + r.code + "]");
        if (strategy == 1) {
            std::cout << " [Strategy: Depth-First Search (DFS)]\n";
            resNode = searchCurriculumDFS(curriculumRoot, code, visitCount);
        } else {
            std::cout << " [Strategy: Breadth-First Search (BFS)]\n";
            resNode = searchCurriculumBFS(curriculumRoot, code, visitCount);
        }

        std::cout << " Tree nodes examined: " << visitCount << "\n";

        if (resNode) {
            std::cout << " Result: Found course in curriculum!\n\n";
            auto path = getPathFromRoot(resNode);
            std::cout << " Ancestral Hierarchy (via Parent Pointers):\n   ";
            for (size_t i = 0; i < path.size(); ++i) {
                std::cout << path[i]->name;
                if (!path[i]->code.empty()) std::cout << " [" << path[i]->code << "]";
                if (i + 1 < path.size()) std::cout << "  ->  ";
            }
            std::cout << "\n";
        }

        // Display full Course Details Screen
        displayCourseDetails(code);
    }

    // Course Details Screen
    void displayCourseDetails(const std::string& code) const {
        auto it = catalog.find(code);
        if (it == catalog.end()) return;
        const CourseRecord& r = it->second;

        Utils::printSection("Course Details: " + r.name + " [" + r.code + "]");
        std::cout << "  * Course Code     : " << r.code << "\n";
        std::cout << "  * Course Title    : " << r.name << "\n";
        std::cout << "  * Placement       : Year " << r.year << ", Semester " << r.sem << "\n";
        std::cout << "  * Credits         : " << r.credits << " Credits\n";
        std::cout << "  * Course Type     : " << r.type << "\n";
        std::cout << "  * Elective Group  : " << (r.electiveGroup == "-" ? "None (Core Course)" : (r.electiveGroup + " (Alternatives)")) << "\n";

        std::cout << "  * Prerequisites   : ";
        if (r.prereqCodes.empty()) {
            std::cout << "None (Foundation Course)\n";
        } else {
            for (size_t i = 0; i < r.prereqCodes.size(); ++i) {
                std::string pCode = r.prereqCodes[i];
                std::string pName = catalog.count(pCode) ? catalog.at(pCode).name : pCode;
                std::cout << pName << " [" << pCode << "]";
                if (i + 1 < r.prereqCodes.size()) std::cout << ", ";
            }
            std::cout << "\n";
        }

        // Direct Dependents (Courses unlocked)
        std::vector<std::string> dependents;
        for (const auto& pair : catalog) {
            for (const std::string& pr : pair.second.prereqCodes) {
                if (pr == r.code) {
                    dependents.push_back(pair.first);
                    break;
                }
            }
        }

        std::cout << "  * Unlocks (Direct): ";
        if (dependents.empty()) {
            std::cout << "None (Terminal course or elective)\n";
        } else {
            std::cout << dependents.size() << " course(s):\n";
            for (size_t i = 0; i < dependents.size(); ++i) {
                const CourseRecord& dep = catalog.at(dependents[i]);
                std::cout << "       - " << dep.name << " [" << dep.code << "] (Sem " << dep.sem << ")\n";
            }
        }

        // Reference Book & Source Tag
        std::string sourceTag = "[Not specified]";
        if (r.bookSource == 'S') sourceTag = "[Syllabus]";
        else if (r.bookSource == 'G') sourceTag = "[Suggested - verify]";

        std::cout << "  * Reference Book  : " << r.referenceBook << "\n";
        std::cout << "  * Book Source Tag : " << sourceTag << "\n";
    }

    // ========================================================================
    // FEATURE 10: Semester-wise plan display (BFS level-order)
    // ========================================================================
    void displaySemesterPlan() {
        printLevelOrderPlan(curriculumRoot);
    }

    // ========================================================================
    // FEATURE 11: Add / Remove Course or Prerequisite
    // ========================================================================
    bool addCourse(const CourseRecord& rec, bool recordUndo = true) {
        if (catalog.find(rec.code) != catalog.end()) {
            std::cout << " [!] Course code '" << rec.code << "' already exists in catalog!\n";
            return false;
        }

        catalog[rec.code] = rec;
        courseOrder.push_back(rec.code);
        buildCurriculumTreeStructure();

        if (recordUndo) {
            UndoAction act;
            act.type = ActionType::ADD_COURSE;
            act.course = rec;
            undoStack.push(act);
        }

        std::cout << " [+] Added course: " << rec.name << " [" << rec.code << "]\n";
        return true;
    }

    bool removeCourse(const std::string& query, bool recordUndo = true) {
        std::string code = resolveCourse(query);
        if (code.empty()) return false;

        CourseRecord saved = catalog[code];
        catalog.erase(code);
        courseOrder.erase(std::remove(courseOrder.begin(), courseOrder.end(), code), courseOrder.end());

        // Remove code as prerequisite from referencing courses
        std::vector<std::string> affected;
        for (auto& pair : catalog) {
            auto& list = pair.second.prereqCodes;
            auto it = std::find(list.begin(), list.end(), code);
            if (it != list.end()) {
                list.erase(it);
                affected.push_back(pair.first);
            }
        }

        buildCurriculumTreeStructure();

        if (recordUndo) {
            UndoAction act;
            act.type = ActionType::REMOVE_COURSE;
            act.course = saved;
            act.referencingCourses = affected;
            undoStack.push(act);
        }

        std::cout << " [-] Removed course: " << saved.name << " [" << saved.code << "]\n";
        return true;
    }

    bool addPrerequisite(const std::string& courseQ, const std::string& prereqQ, bool recordUndo = true) {
        std::string cCode = resolveCourse(courseQ);
        if (cCode.empty()) return false;
        std::string pCode = resolveCourse(prereqQ);
        if (pCode.empty()) return false;

        if (cCode == pCode) {
            std::cout << " [!] Course cannot require itself.\n";
            return false;
        }

        auto& list = catalog[cCode].prereqCodes;
        if (std::find(list.begin(), list.end(), pCode) != list.end()) {
            std::cout << " [!] Prerequisite already exists.\n";
            return false;
        }

        list.push_back(pCode);

        if (recordUndo) {
            UndoAction act;
            act.type = ActionType::ADD_PREREQUISITE;
            act.targetCourse = cCode;
            act.targetPrereq = pCode;
            undoStack.push(act);
        }

        std::cout << " [+] Added prerequisite " << pCode << " to " << cCode << "\n";
        return true;
    }

    bool removePrerequisite(const std::string& courseQ, const std::string& prereqQ, bool recordUndo = true) {
        std::string cCode = resolveCourse(courseQ);
        if (cCode.empty()) return false;
        std::string pCode = resolveCourse(prereqQ);
        if (pCode.empty()) return false;

        auto& list = catalog[cCode].prereqCodes;
        auto it = std::find(list.begin(), list.end(), pCode);
        if (it == list.end()) {
            std::cout << " [!] Prerequisite not attached to course.\n";
            return false;
        }

        list.erase(it);

        if (recordUndo) {
            UndoAction act;
            act.type = ActionType::REMOVE_PREREQUISITE;
            act.targetCourse = cCode;
            act.targetPrereq = pCode;
            undoStack.push(act);
        }

        std::cout << " [-] Removed prerequisite " << pCode << " from " << cCode << "\n";
        return true;
    }

    // ========================================================================
    // FEATURE 12: Lowest Common Ancestor (LCA)
    // ========================================================================
    void showLowestCommonAncestor(const std::string& q1, const std::string& q2) {
        std::string code1 = resolveCourse(q1);
        if (code1.empty()) return;
        std::string code2 = resolveCourse(q2);
        if (code2.empty()) return;

        int v1 = 0, v2 = 0;
        CurNode* n1 = searchCurriculumDFS(curriculumRoot, code1, v1);
        CurNode* n2 = searchCurriculumDFS(curriculumRoot, code2, v2);

        if (!n1 || !n2) {
            std::cout << " [!] Failed to locate nodes in curriculum tree.\n";
            return;
        }

        CurNode* lca = findLCA(n1, n2);
        const CourseRecord& r1 = catalog.at(code1);
        const CourseRecord& r2 = catalog.at(code2);

        Utils::printSection("Lowest Common Ancestor (LCA) in Curriculum Tree");
        std::cout << " Course 1: " << r1.name << " [" << r1.code << "] (Sem " << r1.sem << ")\n";
        std::cout << " Course 2: " << r2.name << " [" << r2.code << "] (Sem " << r2.sem << ")\n\n";

        if (lca) {
            std::cout << " Lowest Common Ancestor Node: \"" << lca->name << "\"\n\n";
            std::cout << " Academic Relationship Interpretation:\n   ";
            if (lca->name.find("Semester") != std::string::npos || lca->isElectiveGroup) {
                std::cout << "-> Both courses are scheduled in the SAME SEMESTER (" << lca->name << ")!\n"
                          << "   They can be taken concurrently in the same academic term.\n";
            } else if (lca->name.find("Year") != std::string::npos) {
                std::cout << "-> Both courses belong to the SAME ACADEMIC YEAR (" << lca->name << "),\n"
                          << "   but they are scheduled in different semesters.\n";
            } else {
                std::cout << "-> Both courses belong to the SAME DEGREE PROGRAM (" << lca->name << "),\n"
                          << "   but belong to DIFFERENT ACADEMIC YEARS.\n";
            }
        }
    }

    // ========================================================================
    // FEATURE 13: Undo last change using stack
    // ========================================================================
    bool undoLastAction() {
        if (undoStack.empty()) {
            std::cout << " [!] Nothing to undo. Undo history stack is empty.\n";
            return false;
        }

        UndoAction act = undoStack.top();
        undoStack.pop();

        Utils::printSection("Undo Operation");
        std::cout << " [Tree Concept: Stack (LIFO) Action Reversal]\n";

        switch (act.type) {
            case ActionType::ADD_COURSE:
                std::cout << " Undoing course addition: Removing '" << act.course.name << "'...\n";
                removeCourse(act.course.code, false);
                break;
            case ActionType::REMOVE_COURSE:
                std::cout << " Undoing course deletion: Restoring '" << act.course.name << "'...\n";
                addCourse(act.course, false);
                for (const std::string& ref : act.referencingCourses) {
                    catalog[ref].prereqCodes.push_back(act.course.code);
                }
                break;
            case ActionType::ADD_PREREQUISITE:
                std::cout << " Undoing prerequisite addition...\n";
                removePrerequisite(act.targetCourse, act.targetPrereq, false);
                break;
            case ActionType::REMOVE_PREREQUISITE:
                std::cout << " Undoing prerequisite removal...\n";
                addPrerequisite(act.targetCourse, act.targetPrereq, false);
                break;
        }

        std::cout << " [✓] Action successfully undone!\n";
        return true;
    }

    // Saves back to syllabus.txt
    bool saveFiles() {
        if (saveSyllabusFile(syllabusFilename, catalog, courseOrder)) {
            std::cout << " [✓] Successfully saved changes to '" << syllabusFilename << "'.\n";
            return true;
        }
        std::cout << " [!] Error saving to " << syllabusFilename << "\n";
        return false;
    }

    void printSummary() const {
        std::cout << " Database Loaded: " << catalog.size()
                  << " courses mapped across 8 semesters in KKWIEER B.Tech CSE (172 Credits).\n";
    }
};

#endif // PLANNER_H

#ifndef TREE_H
#define TREE_H

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <algorithm>
#include "utils.h"
#include "loader.h"

// ============================================================================
// DATA STRUCTURE: Prerequisite Tree Node
// ============================================================================
// Root = Chosen Target Course
// Children = Immediate Prerequisites
// Recursive expansion: Each prerequisite node's children are its prerequisites.
// If a prerequisite is shared by multiple courses, it appears under each branch,
// maintaining a strict, acyclic N-ary Tree structure.
struct PreNode {
    std::string code;
    std::string name;
    int credits = 0;
    int sem = 1;
    int year = 1;
    std::vector<PreNode*> prereqs;

    PreNode(const std::string& c, const std::string& n = "", int cr = 0, int s = 1, int y = 1)
        : code(c), name(n), credits(cr), sem(s), year(y), prereqs() {}
};

// ============================================================================
// DATA STRUCTURE: Curriculum Tree Node
// ============================================================================
// Hierarchy: Degree -> Year -> Semester -> (ElectiveGroup) -> Course
// Parent pointers allow bottom-up upward traversal (used in LCA and semester validation).
struct CurNode {
    std::string name;
    std::string code;          // Course code if course node, else empty
    int credits = 0;
    std::vector<CurNode*> children;
    CurNode* parent = nullptr;
    bool isElectiveGroup = false;

    CurNode(const std::string& n, int c = 0, CurNode* p = nullptr, const std::string& cd = "", bool isElective = false)
        : name(n), code(cd), credits(c), children(), parent(p), isElectiveGroup(isElective) {}
};

// ============================================================================
// PREREQUISITE TREE FUNCTIONS
// ============================================================================

/**
 * Purpose: Recursively builds the prerequisite tree for a given course code.
 * Tree Concept Used: Recursive Tree Construction (N-ary Tree).
 * Time Complexity: O(n) where n is the number of nodes generated in the tree.
 * Cycle Protection: Checks visitingSet (recursion stack) to prevent infinite loops.
 */
inline PreNode* buildPrereqTree(
    const std::string& courseCode,
    const std::unordered_map<std::string, CourseRecord>& catalog,
    std::unordered_set<std::string>& visitingSet,
    bool& cycleDetected)
{
    if (visitingSet.count(courseCode) > 0) {
        cycleDetected = true;
        std::cerr << " [!] Cycle detected in prerequisite chain involving: " << courseCode << "\n";
        return nullptr;
    }

    auto it = catalog.find(courseCode);
    std::string name = (it != catalog.end()) ? it->second.name : courseCode;
    int credits = (it != catalog.end()) ? it->second.credits : 0;
    int sem = (it != catalog.end()) ? it->second.sem : 1;
    int year = (it != catalog.end()) ? it->second.year : 1;

    PreNode* node = new PreNode(courseCode, name, credits, sem, year);
    visitingSet.insert(courseCode);

    if (it != catalog.end()) {
        for (const std::string& prereqCode : it->second.prereqCodes) {
            PreNode* child = buildPrereqTree(prereqCode, catalog, visitingSet, cycleDetected);
            if (child != nullptr) {
                node->prereqs.push_back(child);
            }
        }
    }

    visitingSet.erase(courseCode);
    return node;
}

/**
 * Purpose: Recursively frees all memory allocated for a prerequisite tree.
 * Tree Concept Used: Post-order Tree Deletion / Deallocation.
 * Time Complexity: O(n) where n is the total number of nodes in the tree.
 */
inline void destroyPreTree(PreNode* root) {
    if (!root) return;
    for (PreNode* child : root->prereqs) {
        destroyPreTree(child);
    }
    delete root;
}

/**
 * Purpose: Prints the prerequisite tree with clear indentation and box-drawing branches.
 * Formats nodes as "Name [Code]".
 * Tree Concept Used: Pre-order Traversal (Root visited before children).
 * Time Complexity: O(n) where n is the number of nodes in the tree.
 */
inline void printPrereqTree(const PreNode* root, const std::string& prefix = "", bool isLast = true, bool isRoot = true) {
    if (!root) return;

    std::string label = root->name.empty() ? root->code : (root->name + " [" + root->code + "]");

    if (isRoot) {
        std::cout << label << "\n";
    } else {
        std::cout << prefix << (isLast ? "+-- " : "|-- ") << label << "\n";
    }

    std::string childPrefix = isRoot ? "" : prefix + (isLast ? "    " : "|   ");
    size_t count = root->prereqs.size();
    for (size_t i = 0; i < count; ++i) {
        printPrereqTree(root->prereqs[i], childPrefix, (i == count - 1), false);
    }
}

/**
 * Purpose: Calculates the minimum number of sequential semesters needed for a course.
 * Tree Concept Used: Tree Height / Maximum Depth.
 * Time Complexity: O(n) where n is the number of nodes in the tree.
 * Logic: A course with no prerequisites takes 1 semester. Otherwise, height = 1 + max(child heights).
 */
inline int getTreeHeight(const PreNode* root) {
    if (!root) return 0;
    int maxChildHeight = 0;
    for (const PreNode* child : root->prereqs) {
        maxChildHeight = std::max(maxChildHeight, getTreeHeight(child));
    }
    return 1 + maxChildHeight;
}

/**
 * Purpose: Finds the critical learning path (longest root-to-leaf path) determining the tree height.
 * Tree Concept Used: Longest Path in Tree (DFS).
 * Time Complexity: O(n) where n is the number of nodes in the tree.
 */
inline std::vector<std::pair<std::string, std::string>> getCriticalPath(const PreNode* root) {
    if (!root) return {};
    std::vector<std::pair<std::string, std::string>> longestChildPath;
    for (const PreNode* child : root->prereqs) {
        auto childPath = getCriticalPath(child);
        if (childPath.size() > longestChildPath.size()) {
            longestChildPath = childPath;
        }
    }
    longestChildPath.push_back({root->name, root->code});
    return longestChildPath;
}

/**
 * Purpose: Counts the total number of prerequisite nodes in the tree (including root).
 * Tree Concept Used: Subtree Size Calculation (Post-order Traversal).
 * Time Complexity: O(n) where n is the number of nodes in the tree.
 */
inline int countSubtreeSize(const PreNode* root) {
    if (!root) return 0;
    int total = 1;
    for (const PreNode* child : root->prereqs) {
        total += countSubtreeSize(child);
    }
    return total;
}

/**
 * Purpose: Collects unique prerequisite course codes across all subtrees (excluding root).
 * Tree Concept Used: Tree Traversal with Set Accumulation.
 * Time Complexity: O(n) where n is the number of nodes in the tree.
 */
inline void collectUniquePrereqs(const PreNode* root, std::unordered_set<std::string>& uniqueSet) {
    if (!root) return;
    for (const PreNode* child : root->prereqs) {
        uniqueSet.insert(child->code);
        collectUniquePrereqs(child, uniqueSet);
    }
}

/**
 * Purpose: Helper to collect all leaf course codes under a prerequisite branch.
 */
inline void collectLeafCodes(const PreNode* node, std::unordered_set<std::string>& leafSet) {
    if (!node) return;
    if (node->prereqs.empty()) {
        leafSet.insert(node->code);
        return;
    }
    for (const PreNode* child : node->prereqs) {
        collectLeafCodes(child, leafSet);
    }
}

/**
 * Purpose: Identifies all foundation courses (leaf nodes with zero prerequisites) in the prerequisite tree.
 * If course has prerequisites, returns unique leaf codes in its prerequisite branches.
 * If course has no prerequisites, returns empty set (0 foundation dependencies).
 * Tree Concept Used: Leaf-Node Identification.
 * Time Complexity: O(n) where n is the number of nodes in the tree.
 */
inline void getFoundationCourses(const PreNode* root, std::unordered_set<std::string>& foundationSet) {
    if (!root) return;
    for (const PreNode* child : root->prereqs) {
        collectLeafCodes(child, foundationSet);
    }
}

/**
 * Purpose: Finds and displays every learning trajectory from a foundation course to the target course.
 * Tree Concept Used: Root-to-Leaf Path Printing using Depth-First Search (DFS) with Backtracking.
 * Time Complexity: O(n * h) where n is tree size and h is maximum tree depth.
 */
inline void printAllLearningPathsDFS(
    const PreNode* node,
    std::vector<std::string>& currentPath,
    int& pathIndex)
{
    if (!node) return;

    std::string label = node->name + " [" + node->code + "]";
    currentPath.push_back(label);

    // If leaf node (foundation course), print the reversed path: Leaf -> ... -> Target Root
    if (node->prereqs.empty()) {
        pathIndex++;
        std::cout << "  Path " << pathIndex << ": ";
        for (int i = static_cast<int>(currentPath.size()) - 1; i >= 0; --i) {
            std::cout << currentPath[i];
            if (i > 0) std::cout << " -> ";
        }
        std::cout << " (" << currentPath.size() << " steps)\n";
    } else {
        // Recurse on children prerequisites
        for (const PreNode* child : node->prereqs) {
            printAllLearningPathsDFS(child, currentPath, pathIndex);
        }
    }

    // Backtracking step: pop current course
    currentPath.pop_back();
}

// ============================================================================
// CURRICULUM TREE FUNCTIONS
// ============================================================================

/**
 * Purpose: Recursively calculates and assigns total credits for Courses, Elective Groups,
 *          Semesters, Years, and Degree nodes.
 *          Crucial Rule: Elective Group credits count ONCE (alternatives).
 * Tree Concept Used: Post-order Traversal Sum (Bottom-Up Credit Aggregation).
 * Time Complexity: O(N) where N is the total number of nodes in the curriculum tree.
 */
inline int computeCurriculumCredits(CurNode* root) {
    if (!root) return 0;

    // If leaf node (Course), its credits are already set
    if (root->children.empty()) {
        return root->credits;
    }

    if (root->isElectiveGroup) {
        // Elective group credits count ONCE in rollup: take credits of first child
        for (CurNode* child : root->children) {
            computeCurriculumCredits(child);
        }
        root->credits = root->children.empty() ? 0 : root->children[0]->credits;
        return root->credits;
    }

    // Non-leaf (Semester, Year, Degree): post-order sum of children
    int total = 0;
    for (CurNode* child : root->children) {
        total += computeCurriculumCredits(child);
    }
    root->credits = total;
    return total;
}

/**
 * Purpose: Recursively frees all memory allocated for the curriculum tree.
 * Tree Concept Used: Post-order Tree Deletion / Deallocation.
 * Time Complexity: O(N) where N is the total number of nodes in the curriculum tree.
 */
inline void destroyCurTree(CurNode* root) {
    if (!root) return;
    for (CurNode* child : root->children) {
        destroyCurTree(child);
    }
    delete root;
}

/**
 * Purpose: Displays the full curriculum tree hierarchy with credits at every level.
 * Tree Concept Used: Pre-order Traversal with Indentation.
 * Time Complexity: O(N) where N is the total number of nodes in the curriculum tree.
 */
inline void printCurriculumHierarchical(const CurNode* root, const std::string& prefix = "", bool isLast = true, bool isRoot = true) {
    if (!root) return;

    if (isRoot) {
        std::cout << "[Degree] " << root->name << " (Total Credits: " << root->credits << ")\n";
    } else {
        std::string label;
        if (root->isElectiveGroup) {
            label = "[Elective Group] " + root->name + " (" + std::to_string(root->credits) + " cr, pick 1 option)";
        } else if (root->children.empty()) {
            label = "[Course] " + root->name + " [" + root->code + "] (" + std::to_string(root->credits) + " credits)";
        } else if (root->name.find("Year") != std::string::npos) {
            label = "[" + root->name + "] (Year Total: " + std::to_string(root->credits) + " credits)";
        } else {
            label = "[" + root->name + "] (Semester Total: " + std::to_string(root->credits) + " credits)";
        }
        std::cout << prefix << (isLast ? "+-- " : "|-- ") << label << "\n";
    }

    std::string childPrefix = isRoot ? "" : prefix + (isLast ? "    " : "|   ");
    size_t count = root->children.size();
    for (size_t i = 0; i < count; ++i) {
        printCurriculumHierarchical(root->children[i], childPrefix, (i == count - 1), false);
    }
}

/**
 * Purpose: Searches for a course node in the curriculum tree using Depth-First Search.
 * Matches by code or by name (case-insensitive).
 * Tree Concept Used: Depth-First Search (DFS Traversal).
 * Time Complexity: O(N) where N is the total number of nodes in the curriculum tree.
 */
inline CurNode* searchCurriculumDFS(CurNode* root, const std::string& query, int& visitCount) {
    if (!root) return nullptr;
    visitCount++;

    std::string lowerQ = Utils::toLower(query);
    if (!root->code.empty() && Utils::toLower(root->code) == lowerQ) {
        return root;
    }
    if (Utils::toLower(root->name) == lowerQ) {
        return root;
    }

    for (CurNode* child : root->children) {
        CurNode* res = searchCurriculumDFS(child, query, visitCount);
        if (res != nullptr) return res;
    }
    return nullptr;
}

/**
 * Purpose: Searches for a course node in the curriculum tree using Breadth-First Search.
 * Matches by code or by name (case-insensitive).
 * Tree Concept Used: Breadth-First Search (BFS Traversal using Queue).
 * Time Complexity: O(N) where N is the total number of nodes in the curriculum tree.
 */
inline CurNode* searchCurriculumBFS(CurNode* root, const std::string& query, int& visitCount) {
    if (!root) return nullptr;
    std::queue<CurNode*> q;
    q.push(root);

    std::string lowerQ = Utils::toLower(query);

    while (!q.empty()) {
        CurNode* curr = q.front();
        q.pop();
        visitCount++;

        if (!curr->code.empty() && Utils::toLower(curr->code) == lowerQ) {
            return curr;
        }
        if (Utils::toLower(curr->name) == lowerQ) {
            return curr;
        }

        for (CurNode* child : curr->children) {
            q.push(child);
        }
    }
    return nullptr;
}

/**
 * Purpose: Reconstructs the ancestral path from degree root down to the target node using parent pointers.
 * Tree Concept Used: Parent Pointer Traversal.
 * Time Complexity: O(h) where h is the tree height (h <= 5).
 */
inline std::vector<CurNode*> getPathFromRoot(CurNode* node) {
    std::vector<CurNode*> path;
    CurNode* curr = node;
    while (curr != nullptr) {
        path.push_back(curr);
        curr = curr->parent;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

/**
 * Purpose: Computes the Lowest Common Ancestor (LCA) of two courses in the curriculum tree.
 * Tree Concept Used: Lowest Common Ancestor (LCA) using Parent Pointers / Path Alignment.
 * Time Complexity: O(h) where h is tree height (h <= 5).
 * Significance:
 *   - LCA is Semester / ElectiveGroup => Courses are scheduled in the same semester.
 *   - LCA is Year                     => Courses belong to same academic year (different semesters).
 *   - LCA is Degree                   => Courses belong to different academic years.
 */
inline CurNode* findLCA(CurNode* nodeA, CurNode* nodeB) {
    if (!nodeA || !nodeB) return nullptr;

    std::vector<CurNode*> pathA = getPathFromRoot(nodeA);
    std::vector<CurNode*> pathB = getPathFromRoot(nodeB);

    CurNode* lca = nullptr;
    size_t minLen = std::min(pathA.size(), pathB.size());
    for (size_t i = 0; i < minLen; ++i) {
        if (pathA[i] == pathB[i]) {
            lca = pathA[i];
        } else {
            break;
        }
    }
    return lca;
}

/**
 * Purpose: Displays the complete academic roadmap organized semester-by-semester.
 * Tree Concept Used: Level-Order Traversal (BFS) using a Queue.
 * Time Complexity: O(N) where N is the total number of nodes in the curriculum tree.
 */
inline void printLevelOrderPlan(CurNode* degreeRoot) {
    if (!degreeRoot) return;

    std::cout << "\n============================================================\n";
    std::cout << "          SEMESTER-WISE ACADEMIC ROADMAP (BFS)\n";
    std::cout << "============================================================\n";

    // Level-order traversal queue
    std::queue<CurNode*> q;
    for (CurNode* yearNode : degreeRoot->children) {
        q.push(yearNode);
    }

    while (!q.empty()) {
        CurNode* yearNode = q.front();
        q.pop();

        std::cout << "\n+----------------------------------------------------------+\n";
        std::cout << "| " << yearNode->name << " (Total Credits: " << yearNode->credits << ")\n";
        std::cout << "+----------------------------------------------------------+\n";

        for (CurNode* semNode : yearNode->children) {
            std::cout << "\n  [" << semNode->name << "] - " << semNode->credits << " Credits:\n";
            int idx = 1;
            for (CurNode* child : semNode->children) {
                if (child->isElectiveGroup) {
                    std::cout << "    * " << child->name << " (" << child->credits << " credits, pick 1 option):\n";
                    for (CurNode* opt : child->children) {
                        std::cout << "       - " << opt->name << " [" << opt->code << "] (" << opt->credits << " cr)\n";
                    }
                } else {
                    std::cout << "    " << idx++ << ". " << child->name << " [" << child->code
                              << "] (" << child->credits << " credits)\n";
                }
            }
        }
    }
    std::cout << "\n" << std::string(60, '=') << "\n";
}

#endif // TREE_H

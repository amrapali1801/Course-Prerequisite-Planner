/**
 * Parity Test Suite (Node.js, Zero External Packages)
 * Verifies that JavaScript frontend algorithms and C++ backend algorithms
 * produce 100% IDENTICAL tree metrics across all 82 syllabus courses:
 *   - Tree Height (Minimum Semesters)
 *   - Subtree Size (Total Nodes with Replication)
 *   - Unique Prerequisites Count
 *   - Foundation Courses Count (Leaves in Prerequisite Subtrees)
 */

const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

// 1. Load web/data.js
const dataJsPath = path.resolve(__dirname, '../web/data.js');
if (!fs.existsSync(dataJsPath)) {
  console.error("FATAL: web/data.js not found. Run planner --export-web first.");
  process.exit(1);
}

const dataJsContent = fs.readFileSync(dataJsPath, 'utf8');
const context = {};
const loader = new Function('ctx', dataJsContent + '; ctx.SYLLABUS = SYLLABUS;');
loader(context);
const SYLLABUS = context.SYLLABUS;

if (!Array.isArray(SYLLABUS) || SYLLABUS.length === 0) {
  console.error("FATAL: SYLLABUS array in web/data.js is empty or invalid.");
  process.exit(1);
}

// Map courses by code
const COURSES = {};
SYLLABUS.forEach(c => {
  COURSES[c.code] = c;
});

// 2. Pure Tree Algorithms in JS
function buildTree(code, visiting = new Set()) {
  if (visiting.has(code)) return null;
  visiting.add(code);

  const c = COURSES[code];
  const node = {
    code: code,
    children: []
  };

  if (c && c.prereqCodes) {
    for (const pr of c.prereqCodes) {
      const child = buildTree(pr, new Set(visiting));
      if (child) node.children.push(child);
    }
  }

  return node;
}

function getTreeHeight(node) {
  if (!node) return 0;
  let maxChild = 0;
  for (const ch of node.children) {
    maxChild = Math.max(maxChild, getTreeHeight(ch));
  }
  return 1 + maxChild;
}

function getSubtreeSize(node) {
  if (!node) return 0;
  let sum = 1;
  for (const ch of node.children) {
    sum += getSubtreeSize(ch);
  }
  return sum;
}

function collectUniquePrereqs(node, set) {
  if (!node) return;
  for (const ch of node.children) {
    set.add(ch.code);
    collectUniquePrereqs(ch, set);
  }
}

function collectLeafCodes(node, set) {
  if (!node) return;
  if (node.children.length === 0) {
    set.add(node.code);
    return;
  }
  for (const ch of node.children) {
    collectLeafCodes(ch, set);
  }
}

function getFoundationCourses(node) {
  const set = new Set();
  if (!node) return set;
  for (const ch of node.children) {
    collectLeafCodes(ch, set);
  }
  return set;
}

// 3. Execute C++ planner --dump-metrics
const rootDir = path.resolve(__dirname, '..');
const plannerBin = process.platform === 'win32'
  ? path.join(rootDir, 'planner.exe')
  : path.join(rootDir, 'planner');

if (!fs.existsSync(plannerBin)) {
  console.error(`FATAL: C++ binary not found at ${plannerBin}. Compile planner first.`);
  process.exit(1);
}

let dumpOutput = '';
try {
  dumpOutput = execSync(`"${plannerBin}" --dump-metrics`, { cwd: rootDir, encoding: 'utf8' });
} catch (err) {
  console.error("FATAL: Failed to execute planner --dump-metrics:", err.message);
  process.exit(1);
}

// Parse C++ dump: code,height,subtreeSize,uniquePrereqs,foundationCount
const cppMetrics = {};
const lines = dumpOutput.split(/\r?\n/);
lines.forEach(line => {
  const trimmed = line.trim();
  if (!trimmed) return;
  const parts = trimmed.split(',');
  if (parts.length >= 5) {
    cppMetrics[parts[0]] = {
      height: parseInt(parts[1], 10),
      subtreeSize: parseInt(parts[2], 10),
      uniquePrereqs: parseInt(parts[3], 10),
      foundationCount: parseInt(parts[4], 10)
    };
  }
});

// 4. Compare all 82 courses
console.log("============================================================");
console.log("      SYLLABUS TREE METRICS PARITY TEST SUITE (82 COURSES)  ");
console.log("============================================================");

let mismatches = 0;
let evaluated = 0;

SYLLABUS.forEach(c => {
  evaluated++;
  const tree = buildTree(c.code);

  const jsHeight = getTreeHeight(tree);
  const jsSize = getSubtreeSize(tree);

  const jsUniqueSet = new Set();
  collectUniquePrereqs(tree, jsUniqueSet);
  const jsUnique = jsUniqueSet.size;

  const jsFoundations = getFoundationCourses(tree).size;

  const cpp = cppMetrics[c.code];
  if (!cpp) {
    console.error(`[FAIL] Course ${c.code} (${c.name}) missing in C++ dump output!`);
    mismatches++;
    return;
  }

  const match = (
    jsHeight === cpp.height &&
    jsSize === cpp.subtreeSize &&
    jsUnique === cpp.uniquePrereqs &&
    jsFoundations === cpp.foundationCount
  );

  if (!match) {
    console.error(`[FAIL] Course ${c.code} (${c.name}) MISMATCH:`);
    console.error(`       JS:  height=${jsHeight}, size=${jsSize}, unique=${jsUnique}, foundations=${jsFoundations}`);
    console.error(`       C++: height=${cpp.height}, size=${cpp.subtreeSize}, unique=${cpp.uniquePrereqs}, foundations=${cpp.foundationCount}`);
    mismatches++;
  }
});

console.log(`\n Total Courses Evaluated: ${evaluated}`);
if (mismatches === 0) {
  console.log(" [PASS] Tree Height matches for all 82 courses!");
  console.log(" [PASS] Subtree Size matches for all 82 courses!");
  console.log(" [PASS] Unique Prerequisites Count matches for all 82 courses!");
  console.log(" [PASS] Foundation Courses Count matches for all 82 courses!");
  console.log("============================================================");
  console.log(" PARITY TEST: PASS (100% C++ & Web Tree Metrics Agreement)");
  console.log("============================================================");
  process.exit(0);
} else {
  console.error("============================================================");
  console.error(` PARITY TEST: FAIL (${mismatches} mismatches found)`);
  console.error("============================================================");
  process.exit(1);
}

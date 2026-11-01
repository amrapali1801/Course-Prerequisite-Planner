/**
 * Course Prerequisite Planner — Web Frontend
 * KKWIEER Nashik | B.Tech Computer Engineering (2023 Pattern)
 * Pure Vanilla JavaScript (Zero Frameworks, Zero CDNs, Works 100% Offline)
 * Powered by data.js (Single Source of Truth)
 */

// ============================================================================
// 1. DATA INITIALIZATION & INDEXING
// ============================================================================

// Global error tracker to verify zero console errors
window.__consoleErrors = [];
window.addEventListener("error", (e) => {
  window.__consoleErrors.push(e.message || "Unknown error");
  console.error("Runtime error caught:", e);
});

// Verify SYLLABUS is loaded from data.js
if (typeof SYLLABUS === "undefined" || !Array.isArray(SYLLABUS)) {
  console.error("FATAL: SYLLABUS dataset not found. Ensure data.js is loaded before script.js.");
}

// Index courses by code and name
const COURSES = {};
const COURSES_BY_NAME = {};
SYLLABUS.forEach(c => {
  COURSES[c.code] = c;
  COURSES_BY_NAME[c.name.toLowerCase()] = c;
});

// Global state
let currentTreeRoot = null;
let completedCourses = new Set();
let nodeIdCounter = 0;
let visualNodes = [];
let visualEdges = [];
let currentZoom = 1.0;

// ============================================================================
// 2. TREE DATA STRUCTURE ALGORITHMS
// ============================================================================

/**
 * Recursive Tree Construction:
 * Builds an N-ary prerequisite tree for courseCode.
 * Shared prerequisites are replicated in each branch to preserve a true tree.
 * Visiting set detects cycles.
 * Tree Concept: Recursive Tree Construction - O(n)
 */
function buildPrereqTree(courseCode, parent = null, depth = 0, visiting = new Set()) {
  if (visiting.has(courseCode)) {
    console.warn("Cycle detected involving course:", courseCode);
    return null;
  }
  visiting.add(courseCode);

  const meta = COURSES[courseCode] || {
    year: 1, sem: 1, code: courseCode, name: courseCode, credits: 0,
    type: "Core", electiveGroup: "-", prereqCodes: [], referenceBook: "Not specified", bookSource: "N"
  };

  const node = {
    id: "node_" + (++nodeIdCounter),
    code: courseCode,
    name: meta.name,
    credits: meta.credits,
    year: meta.year,
    sem: meta.sem,
    type: meta.type,
    electiveGroup: meta.electiveGroup,
    referenceBook: meta.referenceBook,
    bookSource: meta.bookSource,
    depth: depth,
    parent: parent,
    children: [],
    x: 0,
    y: 0,
    width: 184,
    height: 78
  };

  if (meta.prereqCodes && meta.prereqCodes.length > 0) {
    for (const prereqCode of meta.prereqCodes) {
      const childNode = buildPrereqTree(prereqCode, node, depth + 1, new Set(visiting));
      if (childNode) {
        node.children.push(childNode);
      }
    }
  }

  return node;
}

/**
 * Tree Height / Maximum Depth (Tree Concept: Tree Height)
 * Height = 1 + max(child heights). If leaf, height = 1.
 */
function getTreeHeight(node) {
  if (!node) return 0;
  let maxChild = 0;
  for (const child of node.children) {
    maxChild = Math.max(maxChild, getTreeHeight(child));
  }
  return 1 + maxChild;
}

/**
 * Subtree Size (Tree Concept: Post-order Traversal)
 * Counts total nodes in subtree including root.
 */
function countSubtreeSize(node) {
  if (!node) return 0;
  let count = 1;
  for (const child of node.children) {
    count += countSubtreeSize(child);
  }
  return count;
}

/**
 * Unique Prerequisites (Tree Concept: Set Accumulation)
 * Gathers distinct prerequisite course codes (excluding root).
 */
function collectUniquePrereqs(node, set) {
  if (!node) return;
  for (const child of node.children) {
    set.add(child.code);
    collectUniquePrereqs(child, set);
  }
}

/**
 * Helper to collect all leaf course codes under a prerequisite branch.
 */
function collectLeafCodes(node, set) {
  if (!node) return;
  if (node.children.length === 0) {
    set.add(node.code);
    return;
  }
  for (const child of node.children) {
    collectLeafCodes(child, set);
  }
}

/**
 * Foundation Courses (Tree Concept: Leaf-Node Identification)
 * Identifies unique leaf courses under the root's prerequisite subtrees.
 */
function getFoundationCourses(root) {
  const set = new Set();
  if (!root) return set;
  for (const child of root.children) {
    collectLeafCodes(child, set);
  }
  return set;
}

/**
 * Longest Critical Prerequisite Path (Tree Concept: Longest Path via DFS)
 */
function getCriticalPath(node) {
  if (!node) return [];
  let longestChild = [];
  for (const child of node.children) {
    const cp = getCriticalPath(child);
    if (cp.length > longestChild.length) {
      longestChild = cp;
    }
  }
  return [...longestChild, { code: node.code, name: node.name }];
}

/**
 * All Learning Paths (Tree Concept: DFS with Backtracking)
 * Returns array of paths from foundation course (leaf) up to root course.
 */
function getAllLearningPaths(node, currentPath = [], allPaths = []) {
  if (!node) return allPaths;

  currentPath.push({ code: node.code, name: node.name });

  if (node.children.length === 0) {
    // Leaf reached: reverse to display Foundation -> ... -> Root
    allPaths.push([...currentPath].reverse());
  } else {
    for (const child of node.children) {
      getAllLearningPaths(child, currentPath, allPaths);
    }
  }

  currentPath.pop(); // Backtrack
  return allPaths;
}

// ============================================================================
// 3. TREE LAYOUT & VISUAL RENDERING (SVG + HTML)
// ============================================================================

const NODE_W = 184;
const NODE_H = 78;
const H_GAP = 28;
const V_GAP = 65;

/**
 * Tidy Tree Coordinates Layout:
 * Bottom-up positioning with level-by-level collision prevention and parent centering.
 */
function computeTreeLayout(root) {
  let nextLeafX = 40;

  // Post-order traversal to position leaves and center parents
  function postOrder(node) {
    node.y = 40 + node.depth * (NODE_H + V_GAP);

    if (node.children.length === 0) {
      node.x = nextLeafX;
      nextLeafX += NODE_W + H_GAP;
    } else {
      for (const child of node.children) {
        postOrder(child);
      }
      const firstX = node.children[0].x;
      const lastX = node.children[node.children.length - 1].x;
      node.x = (firstX + lastX) / 2;
    }
  }

  postOrder(root);

  // Level-by-level collision prevention pass
  const depthMap = {};
  function collectByDepth(node) {
    if (!depthMap[node.depth]) depthMap[node.depth] = [];
    depthMap[node.depth].push(node);
    for (const c of node.children) collectByDepth(c);
  }
  collectByDepth(root);

  for (const d of Object.keys(depthMap)) {
    const list = depthMap[d];
    list.sort((a, b) => a.x - b.x);
    for (let i = 1; i < list.length; i++) {
      const minRequiredX = list[i - 1].x + NODE_W + H_GAP;
      if (list[i].x < minRequiredX) {
        const shiftAmount = minRequiredX - list[i].x;
        function shiftSubtree(n, s) {
          n.x += s;
          for (const c of n.children) shiftSubtree(c, s);
        }
        shiftSubtree(list[i], shiftAmount);
      }
    }
  }

  // Bottom-up parent recentering pass
  function recenterParents(node) {
    if (node.children.length > 0) {
      for (const c of node.children) recenterParents(c);
      const firstX = node.children[0].x;
      const lastX = node.children[node.children.length - 1].x;
      node.x = Math.max(node.x, (firstX + lastX) / 2);
    }
  }
  recenterParents(root);

  // Normalize x-coordinates so minimum x is at least 40px
  let minX = Infinity;
  let maxX = -Infinity;
  let maxY = -Infinity;

  function findBounds(node) {
    minX = Math.min(minX, node.x);
    maxX = Math.max(maxX, node.x);
    maxY = Math.max(maxY, node.y);
    for (const c of node.children) findBounds(c);
  }
  findBounds(root);

  const xShift = minX < 40 ? 40 - minX : 0;

  function applyShift(node) {
    node.x += xShift;
    for (const c of node.children) applyShift(c);
  }
  if (xShift > 0) {
    applyShift(root);
    maxX += xShift;
  }

  return {
    totalWidth: Math.max(900, maxX + NODE_W + 60),
    totalHeight: Math.max(480, maxY + NODE_H + 60)
  };
}

/**
 * Renders the visual prerequisite tree into DOM
 */
function renderPrerequisiteTree(courseCode) {
  nodeIdCounter = 0;
  visualNodes = [];
  visualEdges = [];

  const container = document.getElementById("treeContainer");
  const svg = document.getElementById("treeSvg");
  const nodesLayer = document.getElementById("treeNodes");

  svg.innerHTML = "";
  nodesLayer.innerHTML = "";

  currentTreeRoot = buildPrereqTree(courseCode);
  if (!currentTreeRoot) return;

  const dims = computeTreeLayout(currentTreeRoot);
  container.style.width = dims.totalWidth + "px";
  container.style.height = dims.totalHeight + "px";
  svg.setAttribute("width", dims.totalWidth);
  svg.setAttribute("height", dims.totalHeight);

  // Flatten nodes and collect edges
  function traverse(node) {
    visualNodes.push(node);
    for (const child of node.children) {
      visualEdges.push({ from: node, to: child, id: `edge_${node.id}_${child.id}` });
      traverse(child);
    }
  }
  traverse(currentTreeRoot);

  // 1. Draw SVG Connector Curves
  for (const edge of visualEdges) {
    const x1 = edge.from.x + NODE_W / 2;
    const y1 = edge.from.y + NODE_H;
    const x2 = edge.to.x + NODE_W / 2;
    const y2 = edge.to.y;
    const midY = (y1 + y2) / 2;

    const path = document.createElementNS("http://www.w3.org/2000/svg", "path");
    path.setAttribute("d", `M ${x1} ${y1} C ${x1} ${midY}, ${x2} ${midY}, ${x2} ${y2}`);
    path.setAttribute("class", "tree-edge");
    path.setAttribute("id", edge.id);
    svg.appendChild(path);
  }

  // 2. Draw HTML Node Cards
  for (const node of visualNodes) {
    const div = document.createElement("div");
    div.className = "tree-node";
    div.id = node.id;
    div.style.left = node.x + "px";
    div.style.top = node.y + "px";

    if (node === currentTreeRoot) {
      div.classList.add("is-root");
    } else if (node.children.length === 0) {
      div.classList.add("is-leaf");
    }

    const isLeaf = node.children.length === 0 && node !== currentTreeRoot;
    const leafTag = isLeaf ? `<span class="badge badge-green" style="font-size:0.65rem; padding:0.1rem 0.35rem;">Foundation</span>` : "";

    div.innerHTML = `
      <div class="node-title" title="${node.name} [${node.code}]">${node.name}</div>
      <div class="node-code-sub">[${node.code}]</div>
      <div class="node-footer">
        <span class="node-sem-badge">Sem ${node.sem}</span>
        ${leafTag}
        <span class="node-credits">${node.credits} Cr</span>
      </div>
    `;

    // Click handler to highlight path
    div.addEventListener("click", () => handleNodeClick(node));

    nodesLayer.appendChild(div);
  }

  // 3. Update Metrics Ribbon
  const height = getTreeHeight(currentTreeRoot);
  const totalSubtree = countSubtreeSize(currentTreeRoot); // Size including root
  const uniqueSet = new Set();
  collectUniquePrereqs(currentTreeRoot, uniqueSet);
  const foundationSet = getFoundationCourses(currentTreeRoot);

  document.getElementById("valTreeHeight").innerText = height;
  document.getElementById("valSubtreeNodes").innerText = totalSubtree;
  document.getElementById("valUniquePrereqs").innerText = uniqueSet.size;
  document.getElementById("valFoundationCount").innerText = foundationSet.size;

  // 4. Update Critical Path
  const critPath = getCriticalPath(currentTreeRoot);
  renderCriticalPath(critPath);

  // 5. Update All Learning Paths
  const allPaths = getAllLearningPaths(currentTreeRoot);
  renderLearningPaths(allPaths);

  // Center scroll in viewport
  const viewport = document.getElementById("treeViewport");
  viewport.scrollLeft = (dims.totalWidth - viewport.clientWidth) / 2;
  viewport.scrollTop = 0;
}

/**
 * Highlights a course node and the full ancestral branch to the root
 */
function handleNodeClick(node) {
  resetTreeHighlights();

  // Find all ancestor nodes and edges up to root
  const activeNodes = new Set();
  const activeEdges = new Set();

  let curr = node;
  activeNodes.add(curr.id);

  while (curr.parent) {
    const edgeId = `edge_${curr.parent.id}_${curr.id}`;
    activeEdges.add(edgeId);
    curr = curr.parent;
    activeNodes.add(curr.id);
  }

  // Apply highlight classes
  for (const nId of activeNodes) {
    const el = document.getElementById(nId);
    if (el) el.classList.add("highlighted");
  }

  for (const eId of activeEdges) {
    const el = document.getElementById(eId);
    if (el) el.classList.add("highlighted");
  }

  // Show Details Banner
  const banner = document.getElementById("nodeDetailBanner");
  const title = document.getElementById("detailTitle");
  const badge = document.getElementById("detailBadge");
  const typeBadge = document.getElementById("detailType");
  const bookEl = document.getElementById("detailBook");
  const sourceEl = document.getElementById("detailBookSource");
  const tags = document.getElementById("detailPrereqsList");

  const meta = COURSES[node.code] || node;

  title.innerText = `${meta.name} [${meta.code}]`;
  badge.innerText = `Semester ${meta.sem} • ${meta.credits} Credits • Year ${meta.year}`;
  typeBadge.innerText = meta.type || "PCC";

  bookEl.innerText = meta.referenceBook || "Not specified";

  let sourceLabel = "[Not specified]";
  let sourceClass = "badge-amber";
  if (meta.bookSource === "S") {
    sourceLabel = "[Syllabus]";
    sourceClass = "badge-green";
  } else if (meta.bookSource === "G") {
    sourceLabel = "[Suggested - verify]";
    sourceClass = "badge-purple";
  }
  sourceEl.innerText = sourceLabel;
  sourceEl.className = `badge ${sourceClass}`;

  tags.innerHTML = "";
  if (meta.prereqCodes && meta.prereqCodes.length > 0) {
    for (const pCode of meta.prereqCodes) {
      const pMeta = COURSES[pCode];
      const pName = pMeta ? `${pMeta.name} [${pCode}]` : pCode;
      const chip = document.createElement("span");
      chip.className = "badge badge-cyan";
      chip.innerText = pName;
      tags.appendChild(chip);
    }
  } else {
    tags.innerHTML = `<span class="badge badge-green">None (Foundation Course)</span>`;
  }

  banner.style.display = "flex";
}

function resetTreeHighlights() {
  document.querySelectorAll(".tree-node.highlighted").forEach(el => el.classList.remove("highlighted"));
  document.querySelectorAll(".tree-edge.highlighted").forEach(el => el.classList.remove("highlighted"));
}

/**
 * Highlights the longest critical path on the tree canvas
 */
function highlightCriticalPath() {
  if (!currentTreeRoot) return;
  resetTreeHighlights();

  const critPath = getCriticalPath(currentTreeRoot); // [leaf, ..., root]
  if (critPath.length === 0) return;

  const reversed = [...critPath].reverse(); // [root, ..., leaf]
  let currentSearch = currentTreeRoot;

  const nodeIds = [currentSearch.id];
  const edgeIds = [];

  for (let i = 1; i < reversed.length; i++) {
    const targetCode = reversed[i].code;
    const childMatch = currentSearch.children.find(c => c.code === targetCode);
    if (childMatch) {
      edgeIds.push(`edge_${currentSearch.id}_${childMatch.id}`);
      nodeIds.push(childMatch.id);
      currentSearch = childMatch;
    }
  }

  for (const nId of nodeIds) {
    const el = document.getElementById(nId);
    if (el) el.classList.add("highlighted");
  }

  for (const eId of edgeIds) {
    const el = document.getElementById(eId);
    if (el) el.classList.add("highlighted");
  }
}

function renderCriticalPath(path) {
  const container = document.getElementById("criticalPathList");
  container.innerHTML = "";

  if (path.length === 0) {
    container.innerHTML = `<p class="text-dim">No prerequisites required.</p>`;
    return;
  }

  for (let i = 0; i < path.length; i++) {
    const card = document.createElement("div");
    card.className = "step-card";
    card.innerHTML = `
      <span class="step-num">Step ${i + 1}</span>
      <span class="step-name">${path[i].name} [${path[i].code}]</span>
    `;
    container.appendChild(card);

    if (i + 1 < path.length) {
      const arrow = document.createElement("span");
      arrow.className = "step-arrow";
      arrow.innerHTML = "&rarr;";
      container.appendChild(arrow);
    }
  }
}

function renderLearningPaths(paths) {
  const container = document.getElementById("learningPathsContainer");
  const countBadge = document.getElementById("pathCountBadge");

  container.innerHTML = "";
  countBadge.innerText = `${paths.length} Path${paths.length === 1 ? "" : "s"}`;

  if (paths.length === 0) {
    container.innerHTML = `<div class="path-item">Course is a standalone foundation entry.</div>`;
    return;
  }

  paths.forEach((p, idx) => {
    const item = document.createElement("div");
    item.className = "path-item";

    const formattedChain = p.map((c, i) => {
      const label = `${c.name} [${c.code}]`;
      if (i === 0) return `<span class="leaf-badge">${label}</span>`;
      if (i === p.length - 1) return `<span class="root-badge">${label}</span>`;
      return label;
    }).join(" &rarr; ");

    item.innerHTML = `
      <div class="path-chain">
        <strong>P${idx + 1}:</strong> ${formattedChain}
      </div>
      <span class="path-len">${p.length} steps</span>
    `;
    container.appendChild(item);
  });
}

// Zoom Controls
function applyZoom(delta) {
  currentZoom = Math.min(1.8, Math.max(0.4, currentZoom + delta));
  const container = document.getElementById("treeContainer");
  container.style.transform = `scale(${currentZoom})`;
  container.style.transformOrigin = "top center";
  document.getElementById("zoomLevelLabel").innerText = `${Math.round(currentZoom * 100)}%`;
}

function resetZoom() {
  currentZoom = 1.0;
  const container = document.getElementById("treeContainer");
  container.style.transform = "scale(1)";
  document.getElementById("zoomLevelLabel").innerText = "100%";
}

// ============================================================================
// 4. "WHAT CAN I TAKE NOW?" PLANNER LOGIC (172 CREDITS, ELECTIVE ALTERNATIVES)
// ============================================================================

function initChecklist() {
  const accordion = document.getElementById("checklistAccordion");
  accordion.innerHTML = "";

  // Group courses by semester (1 through 8)
  const semGroups = {};
  for (let s = 1; s <= 8; s++) semGroups[s] = [];

  SYLLABUS.forEach(c => {
    semGroups[c.sem].push(c);
  });

  for (let s = 1; s <= 8; s++) {
    const groupDiv = document.createElement("div");
    groupDiv.className = "sem-group";

    const yearNum = Math.ceil(s / 2);
    const header = document.createElement("div");
    header.className = "sem-group-header";
    header.innerHTML = `
      <div class="sem-group-title">
        <span>Semester ${s} (Year ${yearNum})</span>
      </div>
      <span class="badge badge-purple">${semGroups[s].length} Courses</span>
    `;

    const listDiv = document.createElement("div");
    listDiv.className = "sem-courses-list";

    for (const course of semGroups[s]) {
      const label = document.createElement("label");
      label.className = "course-checkbox-label";
      label.id = `label_chk_${course.code.replace(/[^a-zA-Z0-9]/g, '_')}`;

      const elecTag = course.electiveGroup !== "-" ? `<span class="elective-tag">${course.electiveGroup}</span>` : "";

      label.innerHTML = `
        <div class="course-check-info">
          <input type="checkbox" value="${course.code}" class="course-chk" ${completedCourses.has(course.code) ? "checked" : ""}>
          <span><strong>${course.name}</strong> <small style="color:var(--text-dim);">[${course.code}]</small></span>
        </div>
        <div style="display:flex; align-items:center; gap:0.5rem;">
          ${elecTag}
          <span class="node-credits">${course.credits} Cr</span>
        </div>
      `;

      const chk = label.querySelector("input");
      chk.addEventListener("change", (e) => {
        if (e.target.checked) {
          completedCourses.add(course.code);
          label.classList.add("is-checked");
        } else {
          completedCourses.delete(course.code);
          label.classList.remove("is-checked");
        }
        updateEligibility();
      });

      listDiv.appendChild(label);
    }

    groupDiv.appendChild(header);
    groupDiv.appendChild(listDiv);
    accordion.appendChild(groupDiv);
  }
}

/**
 * Calculates earned credits out of 172:
 * Elective group credits count ONCE per semester (alternatives).
 */
function calculateEarnedCredits() {
  let total = 0;
  const countedElectives = new Set(); // key: "sem_electiveGroup"

  for (const code of completedCourses) {
    const c = COURSES[code];
    if (!c) continue;

    if (c.electiveGroup !== "-") {
      const key = `${c.sem}_${c.electiveGroup}`;
      if (!countedElectives.has(key)) {
        countedElectives.add(key);
        total += c.credits;
      }
    } else {
      total += c.credits;
    }
  }

  return total;
}

/**
 * Live evaluation of which courses can be taken right now
 */
function updateEligibility() {
  const earnedCredits = calculateEarnedCredits();
  const progressPercent = Math.min(100, Math.round((earnedCredits / 172) * 100));

  // Update progress bar
  document.getElementById("creditsProgressBar").style.width = `${progressPercent}%`;
  document.getElementById("creditsProgressText").innerText = `${earnedCredits} / 172 Credits`;
  document.getElementById("creditsPercentBadge").innerText = `${progressPercent}%`;

  const eligibleBySem = {};
  for (let s = 1; s <= 8; s++) eligibleBySem[s] = [];

  const locked = [];
  let eligibleTotal = 0;

  for (const c of SYLLABUS) {
    if (completedCourses.has(c.code)) {
      continue; // Already taken
    }

    // Check if all prerequisites are completed
    let canTake = true;
    const missing = [];

    for (const prereqCode of c.prereqCodes) {
      if (!completedCourses.has(prereqCode)) {
        canTake = false;
        const pMeta = COURSES[prereqCode];
        missing.push(pMeta ? `${pMeta.name} [${prereqCode}]` : prereqCode);
      }
    }

    if (canTake) {
      eligibleBySem[c.sem].push(c);
      eligibleTotal++;
    } else {
      locked.push({ ...c, missing });
    }
  }

  // Update status numbers
  document.getElementById("statCompletedCount").innerText = completedCourses.size;
  document.getElementById("statCompletedCredits").innerText = earnedCredits;
  document.getElementById("statEligibleCount").innerText = eligibleTotal;
  document.getElementById("eligibleBadge").innerText = `${eligibleTotal} Available`;

  // Render Eligible Courses grouped by semester
  const eligibleContainer = document.getElementById("eligibleList");
  eligibleContainer.innerHTML = "";

  if (eligibleTotal === 0) {
    eligibleContainer.innerHTML = `
      <div style="padding: 1.5rem; text-align: center; color: var(--text-dim);">
        No eligible courses available. Complete more prerequisites above!
      </div>
    `;
  } else {
    for (let s = 1; s <= 8; s++) {
      const list = eligibleBySem[s];
      if (list.length === 0) continue;

      const semDiv = document.createElement("div");
      semDiv.className = "eligible-sem-group";

      const header = document.createElement("div");
      header.className = "eligible-sem-header";
      header.innerHTML = `
        <span>Semester ${s} (Year ${Math.ceil(s / 2)})</span>
        <span class="badge badge-cyan">${list.length} Eligible</span>
      `;
      semDiv.appendChild(header);

      const grid = document.createElement("div");
      grid.className = "eligible-courses-grid";

      for (const c of list) {
        const card = document.createElement("div");
        card.className = "eligible-card-item";

        const elecNote = c.electiveGroup !== "-" ? `<span class="elective-tag">${c.electiveGroup} (Pick 1)</span>` : "";

        card.innerHTML = `
          <div>
            <div class="eligible-item-title">${c.name}</div>
            <small style="color:var(--text-dim); display:block; margin-bottom:0.35rem;">[${c.code}]</small>
            <div class="eligible-item-meta">
              <span>Sem ${c.sem} • ${c.type}</span>
              <span class="node-credits">${c.credits} Credits</span>
            </div>
            ${elecNote ? `<div style="margin-top:0.4rem;">${elecNote}</div>` : ""}
          </div>
          <button class="btn btn-sm btn-primary btn-mark-done" style="width: 100%; justify-content: center; margin-top:0.5rem;">
            ✓ Mark Completed
          </button>
        `;

        card.querySelector(".btn-mark-done").addEventListener("click", () => {
          completedCourses.add(c.code);
          const safeId = `label_chk_${c.code.replace(/[^a-zA-Z0-9]/g, '_')}`;
          const lbl = document.getElementById(safeId);
          if (lbl) {
            lbl.classList.add("is-checked");
            const chk = lbl.querySelector("input");
            if (chk) chk.checked = true;
          }
          updateEligibility();
        });

        grid.appendChild(card);
      }

      semDiv.appendChild(grid);
      eligibleContainer.appendChild(semDiv);
    }
  }

  // Render Locked Courses List
  const lockedContainer = document.getElementById("lockedList");
  lockedContainer.innerHTML = "";

  if (locked.length === 0) {
    lockedContainer.innerHTML = `<div style="padding: 0.5rem; color: var(--text-dim);">All courses completed or eligible!</div>`;
  } else {
    for (const c of locked) {
      const item = document.createElement("div");
      item.className = "locked-item";
      const chips = c.missing.map(m => `<span class="chip">${m}</span>`).join("");

      item.innerHTML = `
        <div>
          <span class="locked-title">${c.name} [${c.code}]</span>
          <small style="color: var(--text-dim); margin-left: 0.5rem;">(Sem ${c.sem})</small>
        </div>
        <div class="missing-chips">
          ${chips}
        </div>
      `;
      lockedContainer.appendChild(item);
    }
  }
}

// ============================================================================
// 5. CURRICULUM HIERARCHY TREE VIEW (POST-ORDER SUM WITH ELECTIVE GROUPS)
// ============================================================================

function buildCurriculumHierarchy() {
  const treeRoot = {
    name: "B.Tech Computer Engineering",
    code: "",
    level: "degree",
    credits: 0,
    children: []
  };

  // 4 Years
  for (let y = 1; y <= 4; y++) {
    const yearNode = {
      name: `Year ${y}`,
      code: "",
      level: "year",
      credits: 0,
      children: []
    };
    treeRoot.children.push(yearNode);

    // 2 Semesters per year
    for (let s = (y - 1) * 2 + 1; s <= y * 2; s++) {
      const semNode = {
        name: `Semester ${s}`,
        code: "",
        level: "sem",
        credits: 0,
        children: []
      };
      yearNode.children.push(semNode);

      // Group electives under semester
      const electiveGroupNodes = {};

      SYLLABUS.forEach(c => {
        if (c.sem === s) {
          if (c.electiveGroup !== "-") {
            if (!electiveGroupNodes[c.electiveGroup]) {
              const grpNode = {
                name: c.electiveGroup,
                code: "",
                level: "elective",
                credits: 0,
                isElectiveGroup: true,
                children: []
              };
              electiveGroupNodes[c.electiveGroup] = grpNode;
              semNode.children.push(grpNode);
            }
            electiveGroupNodes[c.electiveGroup].children.push({
              name: c.name,
              code: c.code,
              level: "course",
              credits: c.credits,
              children: []
            });
          } else {
            semNode.children.push({
              name: c.name,
              code: c.code,
              level: "course",
              credits: c.credits,
              children: []
            });
          }
        }
      });
    }
  }

  // Post-order sum of credits: Elective group credits count ONCE
  function postOrderCredits(node) {
    if (node.level === "course") {
      return node.credits;
    }
    if (node.isElectiveGroup) {
      for (const child of node.children) {
        postOrderCredits(child);
      }
      node.credits = node.children.length > 0 ? node.children[0].credits : 0;
      return node.credits;
    }

    let sum = 0;
    for (const child of node.children) {
      sum += postOrderCredits(child);
    }
    node.credits = sum;
    return sum;
  }

  postOrderCredits(treeRoot);
  return treeRoot;
}

function renderCurriculumTree(filterQuery = "") {
  const container = document.getElementById("curriculumTreeView");
  container.innerHTML = "";

  const root = buildCurriculumHierarchy();
  const lowerFilter = filterQuery.toLowerCase().trim();

  function createBranch(node, isRoot = false) {
    const isLeaf = node.children.length === 0;
    const wrapper = document.createElement("div");
    wrapper.className = "tree-branch-wrapper";

    const row = document.createElement("div");
    row.className = `cur-node-row level-${node.level}`;

    const chevron = isLeaf ? "" : `
      <svg class="chevron-icon expanded" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
        <polyline points="9 18 15 12 9 6"></polyline>
      </svg>
    `;

    let labelText = node.name;
    if (node.code) {
      labelText = `${node.name} [${node.code}]`;
    } else if (node.isElectiveGroup) {
      labelText = `[Elective Group] ${node.name} (Pick 1 option)`;
    }

    row.innerHTML = `
      <div class="cur-node-left">
        ${chevron}
        <span>${labelText}</span>
      </div>
      <span class="credit-pill">${node.credits} Credits</span>
    `;

    // Clicking a course opens details banner
    if (node.level === "course" && node.code) {
      row.style.cursor = "pointer";
      row.addEventListener("click", () => {
        const meta = COURSES[node.code];
        if (meta) {
          // Switch to tree tab or show details
          const sel = document.getElementById("courseSelect");
          if (sel) {
            sel.value = meta.code;
            renderPrerequisiteTree(meta.code);
            // Switch to tree tab
            document.querySelectorAll(".tab-btn").forEach(b => b.classList.remove("active"));
            document.querySelectorAll(".tab-pane").forEach(p => p.classList.remove("active"));
            document.querySelector("[data-tab='tab-tree']").classList.add("active");
            document.getElementById("tab-tree").classList.add("active");
          }
        }
      });
    }

    wrapper.appendChild(row);

    if (!isLeaf) {
      const childContainer = document.createElement("div");
      childContainer.className = "tree-branch-group";

      let hasMatchInChildren = false;
      for (const child of node.children) {
        const childBranch = createBranch(child);
        childContainer.appendChild(childBranch);
      }

      wrapper.appendChild(childContainer);

      // Collapse / expand click on non-leaf row
      row.addEventListener("click", () => {
        const icon = row.querySelector(".chevron-icon");
        if (childContainer.style.display === "none") {
          childContainer.style.display = "flex";
          if (icon) icon.classList.add("expanded");
        } else {
          childContainer.style.display = "none";
          if (icon) icon.classList.remove("expanded");
        }
      });
    }

    // Filter matching highlight
    if (lowerFilter && (labelText.toLowerCase().includes(lowerFilter))) {
      row.style.borderColor = "var(--primary)";
      row.style.background = "#1a2542";
    }

    return wrapper;
  }

  container.appendChild(createBranch(root, true));
}

// Lowest Common Ancestor (LCA)
function initLcaTool() {
  const sel1 = document.getElementById("lcaCourse1");
  const sel2 = document.getElementById("lcaCourse2");
  const btn = document.getElementById("btnFindLca");
  const resBox = document.getElementById("lcaResultBox");
  const resName = document.getElementById("lcaNodeName");
  const resExp = document.getElementById("lcaExplanation");

  sel1.innerHTML = "";
  sel2.innerHTML = "";

  SYLLABUS.forEach(c => {
    const text = `${c.name} [${c.code}] (Sem ${c.sem})`;
    sel1.add(new Option(text, c.code));
    sel2.add(new Option(text, c.code));
  });

  sel1.value = "2300101A"; // Linear Algebra
  sel2.value = "2301401";  // Deep Learning

  btn.addEventListener("click", () => {
    const c1 = sel1.value;
    const c2 = sel2.value;

    const m1 = COURSES[c1];
    const m2 = COURSES[c2];

    let lcaName = "";
    let explanation = "";

    if (c1 === c2) {
      lcaName = `${m1.name} [${c1}]`;
      explanation = "Both dropdowns selected the exact same course.";
    } else if (m1.sem === m2.sem) {
      lcaName = `Semester ${m1.sem}`;
      explanation = `Both courses are scheduled in the SAME SEMESTER (Semester ${m1.sem})! They can be registered concurrently in the same academic term.`;
    } else if (m1.year === m2.year) {
      lcaName = `Year ${m1.year}`;
      explanation = `Both courses belong to the SAME ACADEMIC YEAR (Year ${m1.year}), but are scheduled in different semesters (Semester ${m1.sem} vs Semester ${m2.sem}).`;
    } else {
      lcaName = "B.Tech Computer Engineering (Degree Root)";
      explanation = `Both courses belong to the SAME DEGREE PROGRAM, but belong to DIFFERENT ACADEMIC YEARS (Year ${m1.year} vs Year ${m2.year}).`;
    }

    resName.innerText = lcaName;
    resExp.innerText = explanation;
    resBox.style.display = "block";
  });
}

// ============================================================================
// 6. TAB 4: LIVE DATA VALIDATION CHECKS (MATCHING C++ STARTUP CHECKS)
// ============================================================================

function runLiveDataValidation() {
  const container = document.getElementById("validationResultsList");
  if (!container) return;
  container.innerHTML = "";

  const results = [];

  // Check 1: Semester credits pattern
  const semCredits = { 1: 0, 2: 0, 3: 0, 4: 0, 5: 0, 6: 0, 7: 0, 8: 0 };
  const countedElectives = new Set();

  SYLLABUS.forEach(c => {
    if (c.electiveGroup !== "-") {
      const key = `${c.sem}_${c.electiveGroup}`;
      if (!countedElectives.has(key)) {
        countedElectives.add(key);
        semCredits[c.sem] += c.credits;
      }
    } else {
      semCredits[c.sem] += c.credits;
    }
  });

  const expectedSem = [20, 22, 22, 22, 22, 22, 22, 20];
  const actualSem = Object.values(semCredits);
  const semOk = JSON.stringify(actualSem) === JSON.stringify(expectedSem);

  results.push({
    title: "1. Semester Credits Distribution",
    pass: semOk,
    detail: semOk ? `PASS: Exact pattern matched: ${actualSem.join(", ")}` : `FAIL: Found ${actualSem.join(", ")}`
  });

  // Check 2: Degree total credits
  const totalCredits = actualSem.reduce((a, b) => a + b, 0);
  const totalOk = (totalCredits === 172);

  results.push({
    title: "2. Total Degree Credits",
    pass: totalOk,
    detail: totalOk ? "PASS: Exactly 172 credits across all 8 semesters." : `FAIL: Total is ${totalCredits}`
  });

  // Check 3: Prerequisite existence
  let missingPrereqs = 0;
  SYLLABUS.forEach(c => {
    c.prereqCodes.forEach(p => {
      if (!COURSES[p]) missingPrereqs++;
    });
  });
  const prereqExistOk = (missingPrereqs === 0);

  results.push({
    title: "3. Prerequisite Code Integrity",
    pass: prereqExistOk,
    detail: prereqExistOk ? "PASS: Every referenced prerequisite code exists in syllabus.txt." : `FAIL: Found ${missingPrereqs} missing prerequisite reference(s).`
  });

  // Check 4: Cycle & self-loop detection
  let cycleFound = false;
  SYLLABUS.forEach(c => {
    c.prereqCodes.forEach(p => {
      if (p === c.code) cycleFound = true;
    });
    // Check cycle using visiting set
    const visiting = new Set();
    function checkCycle(code) {
      if (visiting.has(code)) return true;
      visiting.add(code);
      const m = COURSES[code];
      if (m) {
        for (const pr of m.prereqCodes) {
          if (checkCycle(pr)) return true;
        }
      }
      visiting.delete(code);
      return false;
    }
    if (checkCycle(c.code)) cycleFound = true;
  });
  const noCycleOk = !cycleFound;

  results.push({
    title: "4. Cycle & Self-Loop Detection",
    pass: noCycleOk,
    detail: noCycleOk ? "PASS: Zero circular dependencies or self-prerequisites detected." : "FAIL: Circular prerequisite detected."
  });

  // Check 5: Semester progression check
  let progressionViolations = 0;
  SYLLABUS.forEach(c => {
    c.prereqCodes.forEach(p => {
      const pMeta = COURSES[p];
      if (pMeta && pMeta.sem >= c.sem) {
        progressionViolations++;
      }
    });
  });
  const progressionOk = (progressionViolations === 0);

  results.push({
    title: "5. Semester Progression Ordering",
    pass: progressionOk,
    detail: progressionOk ? "PASS: Every prerequisite is strictly scheduled in an earlier semester." : `FAIL: ${progressionViolations} prerequisite(s) not in earlier semester.`
  });

  // Render cards
  results.forEach(res => {
    const card = document.createElement("div");
    card.className = `val-check-item ${res.pass ? "pass" : "fail"}`;
    card.innerHTML = `
      <div class="val-icon ${res.pass ? "pass" : "fail"}">${res.pass ? "✓" : "✗"}</div>
      <div>
        <div class="val-title">${res.title}</div>
        <div class="val-detail">${res.detail}</div>
      </div>
    `;
    container.appendChild(card);
  });

  const overallBadge = document.getElementById("validationOverallBadge");
  const allPass = results.every(r => r.pass);
  if (overallBadge) {
    overallBadge.innerText = allPass ? "ALL 5 CHECKS PASS" : "VALIDATION ISSUES DETECTED";
    overallBadge.className = `badge ${allPass ? "badge-green" : "badge-amber"}`;
  }
}

// ============================================================================
// 7. INITIALIZATION & EVENT LISTENERS
// ============================================================================

document.addEventListener("DOMContentLoaded", () => {
  // Populate course dropdown grouped by semester
  const select = document.getElementById("courseSelect");
  select.innerHTML = "";

  const semGroups = {};
  for (let s = 1; s <= 8; s++) semGroups[s] = [];
  SYLLABUS.forEach(c => semGroups[c.sem].push(c));

  for (let s = 1; s <= 8; s++) {
    const grp = document.createElement("optgroup");
    grp.label = `Semester ${s} (Year ${Math.ceil(s / 2)})`;
    semGroups[s].forEach(c => {
      const opt = document.createElement("option");
      opt.value = c.code;
      opt.textContent = `${c.name} [${c.code}]`;
      grp.appendChild(opt);
    });
    select.appendChild(grp);
  }

  // Set default course: Deep Learning (2301401)
  select.value = "2301401";
  renderPrerequisiteTree(select.value);

  // Course dropdown change
  select.addEventListener("change", (e) => {
    renderPrerequisiteTree(e.target.value);
    document.getElementById("nodeDetailBanner").style.display = "none";
  });

  // Course filter input on Tab 1
  const filterInput = document.getElementById("treeCourseFilter");
  if (filterInput) {
    filterInput.addEventListener("input", (e) => {
      const q = e.target.value.toLowerCase().trim();
      if (!q) return;
      for (const c of SYLLABUS) {
        if (c.code.toLowerCase().includes(q) || c.name.toLowerCase().includes(q)) {
          select.value = c.code;
          renderPrerequisiteTree(c.code);
          break;
        }
      }
    });
  }

  // Tree Action buttons
  document.getElementById("btnHighlightCritical").addEventListener("click", highlightCriticalPath);
  document.getElementById("btnResetTree").addEventListener("click", resetTreeHighlights);
  document.getElementById("btnCloseDetail").addEventListener("click", () => {
    document.getElementById("nodeDetailBanner").style.display = "none";
    resetTreeHighlights();
  });

  // Zoom Buttons
  document.getElementById("btnZoomIn").addEventListener("click", () => applyZoom(0.15));
  document.getElementById("btnZoomOut").addEventListener("click", () => applyZoom(-0.15));
  document.getElementById("btnZoomReset").addEventListener("click", resetZoom);

  // Tab switching
  function switchTab(tabId) {
    if (!tabId) return;
    const cleanId = tabId.startsWith("#") ? tabId.substring(1) : tabId;
    const target = document.getElementById(cleanId);
    if (!target) return;

    document.querySelectorAll(".tab-btn").forEach(b => {
      if (b.getAttribute("data-tab") === cleanId) {
        b.classList.add("active");
      } else {
        b.classList.remove("active");
      }
    });

    document.querySelectorAll(".tab-pane").forEach(p => {
      if (p.id === cleanId) {
        p.classList.add("active");
      } else {
        p.classList.remove("active");
      }
    });

    if (cleanId === "tab-concepts") {
      runLiveDataValidation();
    }
  }

  document.querySelectorAll(".tab-btn").forEach(btn => {
    btn.addEventListener("click", () => {
      const tabId = btn.getAttribute("data-tab");
      window.location.hash = tabId;
      switchTab(tabId);
    });
  });

  // Activate tab from URL hash if provided
  if (window.location.hash) {
    switchTab(window.location.hash);
  }


  // Generate Quick Presets for every semester in Tab 2 (Pass Sem 1 .. Pass Sem 8)
  const quickRow = document.getElementById("quickSemButtons");
  quickRow.innerHTML = "";
  for (let s = 1; s <= 8; s++) {
    const btn = document.createElement("button");
    btn.className = "btn btn-sm btn-ghost";
    btn.innerText = `Pass Sem ${s}`;
    btn.addEventListener("click", () => {
      SYLLABUS.forEach(c => {
        if (c.sem === s) completedCourses.add(c.code);
      });
      initChecklist();
      updateEligibility();
    });
    quickRow.appendChild(btn);
  }

  document.getElementById("btnClearAll").addEventListener("click", () => {
    completedCourses.clear();
    initChecklist();
    updateEligibility();
  });

  // Course Filter Input in Tab 2
  document.getElementById("courseFilterInput").addEventListener("input", (e) => {
    const q = e.target.value.toLowerCase().trim();
    document.querySelectorAll(".course-checkbox-label").forEach(lbl => {
      const text = lbl.textContent.toLowerCase();
      lbl.style.display = text.includes(q) ? "flex" : "none";
    });
  });

  // Init Planner & Eligibility
  initChecklist();
  updateEligibility();

  // Init Curriculum Tree & Search in Tab 3
  renderCurriculumTree();
  initLcaTool();

  document.getElementById("curriculumSearchInput").addEventListener("input", (e) => {
    renderCurriculumTree(e.target.value);
  });

  document.getElementById("btnExpandAllCurriculum").addEventListener("click", () => {
    document.querySelectorAll(".tree-branch-group").forEach(el => el.style.display = "flex");
    document.querySelectorAll(".chevron-icon").forEach(el => el.classList.add("expanded"));
  });

  document.getElementById("btnCollapseAllCurriculum").addEventListener("click", () => {
    document.querySelectorAll(".tree-branch-group").forEach(el => el.style.display = "none");
    document.querySelectorAll(".chevron-icon").forEach(el => el.classList.remove("expanded"));
  });

  // Run live validation on Tab 4
  runLiveDataValidation();
});

"""
Parity Test Suite (Python, Zero External Dependencies)
Verifies that web data.js and C++ planner --dump-metrics produce identical metrics.
"""

import os
import sys
import subprocess
import json

root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
data_js_path = os.path.join(root_dir, 'web', 'data.js')

if not os.path.exists(data_js_path):
    print("FATAL: web/data.js not found.")
    sys.exit(1)

# Read web/data.js and parse SYLLABUS JSON
with open(data_js_path, 'r', encoding='utf-8') as f:
    content = f.read()

start_idx = content.find('[')
end_idx = content.rfind(']')
if start_idx == -1 or end_idx == -1:
    print("FATAL: Cannot parse SYLLABUS in web/data.js")
    sys.exit(1)

json_str = content[start_idx:end_idx+1]
syllabus = json.loads(json_str)

courses = {c['code']: c for c in syllabus}

# Pure Tree Algorithms in Python
def build_tree(code, visiting=None):
    if visiting is None:
        visiting = set()
    if code in visiting:
        return None
    visiting.add(code)
    
    meta = courses.get(code, {'prereqCodes': []})
    children = []
    for pr in meta.get('prereqCodes', []):
        ch = build_tree(pr, set(visiting))
        if ch:
            children.append(ch)
    return (code, children)

def get_height(node):
    if not node:
        return 0
    code, children = node
    if not children:
        return 1
    return 1 + max(get_height(ch) for ch in children)

def get_subtree_size(node):
    if not node:
        return 0
    code, children = node
    return 1 + sum(get_subtree_size(ch) for ch in children)

def collect_unique_prereqs(node, s):
    if not node:
        return
    code, children = node
    for ch in children:
        s.add(ch[0])
        collect_unique_prereqs(ch, s)

def collect_leaf_codes(node, s):
    if not node:
        return
    code, children = node
    if not children:
        s.add(code)
        return
    for ch in children:
        collect_leaf_codes(ch, s)

def get_foundations(node):
    s = set()
    if not node:
        return s
    code, children = node
    for ch in children:
        collect_leaf_codes(ch, s)
    return s

# Run C++ --dump-metrics
planner_bin = os.path.join(root_dir, 'planner.exe' if os.name == 'nt' else 'planner')
if not os.path.exists(planner_bin):
    print(f"FATAL: Binary not found: {planner_bin}")
    sys.exit(1)

res = subprocess.run([planner_bin, '--dump-metrics'], cwd=root_dir, capture_output=True, text=True)
if res.returncode != 0:
    print("FATAL: Failed to run planner --dump-metrics")
    sys.exit(1)

cpp_metrics = {}
for line in res.stdout.strip().splitlines():
    parts = line.strip().split(',')
    if len(parts) >= 5:
        cpp_metrics[parts[0]] = {
            'height': int(parts[1]),
            'subtreeSize': int(parts[2]),
            'uniquePrereqs': int(parts[3]),
            'foundationCount': int(parts[4])
        }

print("============================================================")
print("      SYLLABUS TREE METRICS PARITY TEST SUITE (82 COURSES)  ")
print("============================================================")

mismatches = 0
for c in syllabus:
    code = c['code']
    tree = build_tree(code)
    h = get_height(tree)
    sz = get_subtree_size(tree)
    u_set = set()
    collect_unique_prereqs(tree, u_set)
    u = len(u_set)
    f_cnt = len(get_foundations(tree))
    
    cpp = cpp_metrics.get(code)
    if not cpp:
        print(f"[FAIL] Missing course in C++ metrics: {code}")
        mismatches += 1
        continue
        
    match = (
        h == cpp['height'] and
        sz == cpp['subtreeSize'] and
        u == cpp['uniquePrereqs'] and
        f_cnt == cpp['foundationCount']
    )
    if not match:
        print(f"[FAIL] Course {code} ({c['name']}) MISMATCH:")
        print(f"       JS/Data: height={h}, size={sz}, unique={u}, foundations={f_cnt}")
        print(f"       C++:     height={cpp['height']}, size={cpp['subtreeSize']}, unique={cpp['uniquePrereqs']}, foundations={cpp['foundationCount']}")
        mismatches += 1

print(f"\n Total Courses Evaluated: {len(syllabus)}")
if mismatches == 0:
    print(" [PASS] Tree Height matches for all 82 courses!")
    print(" [PASS] Subtree Size matches for all 82 courses!")
    print(" [PASS] Unique Prerequisites Count matches for all 82 courses!")
    print(" [PASS] Foundation Courses Count matches for all 82 courses!")
    print("============================================================")
    print(" PARITY TEST: PASS (100% C++ & Web Tree Metrics Agreement)")
    print("============================================================")
    sys.exit(0)
else:
    print(f"PARITY TEST: FAIL ({mismatches} mismatches found)")
    sys.exit(1)

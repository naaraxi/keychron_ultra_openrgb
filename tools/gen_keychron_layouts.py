#!/usr/bin/env python3
"""
gen_keychron_layouts.py

Mechanical code-generator: reads Keychron *_ultra ZMK shield firmware sources and
emits C++ layout data (KeychronLayouts.h / KeychronLayouts.cpp) for the OpenRGB
Keychron V6 Ultra plugin.

Method
------
matrix_map (geometry):
    From g_led_config "LED Index to Physical Position" {x,y} list.
    rows  = greedy y-clustering (anchor + tolerance)  [top -> bottom]
    cols  = round(x / PITCH)                           [left -> right]
    Grid is height x width, row-major; unfilled cells = NO_LED.

led_names (identity):
    The authoritative source is the matrix->LED-index transform, NOT physical
    position. For each key-position i in the overlay's default_transform map
    (a list of RC(r,c)), block #1 "Key Matrix to LED Index" gives the LED index
    at (r,c). The keycode for key-position i comes from the *Windows* keymap
    layer's flat, row-major bindings list. name = keycode->human-name.
    Non-&kp behaviours (and unknown keycodes) -> "LED <n>" fallback.

Only standard keyboard keys are named; anything ambiguous falls back to "LED n"
("correct-but-generic beats wrong").
"""

import os
import re
import sys

SHIELDS_DIR = "/tmp/claude-1000/-home-alex/b9f2fdbc-d932-4ba3-b2d1-03b44b17dbf3/scratchpad/zmk/app/boards/shields"
OUT_DIR     = "/tmp/claude-1000/-home-alex/b9f2fdbc-d932-4ba3-b2d1-03b44b17dbf3/scratchpad/kv6u"
REF_CPP     = os.path.join(OUT_DIR, "RGBController_KeychronV6Ultra.cpp")

PITCH   = 12.0   # nominal key spacing (physical-position units)
Y_TOL   = 6.0    # rows: same band if within this of band anchor

NO_LED = 0xFFFFFFFF

# shield -> (pid, name).  description is derived below.
BOARDS = [
    ("keychron_v0_ultra_ansi",   0x0c00, "Keychron V0 Ultra 8K",         "V0"),
    ("keychron_v2_ultra_ansi",   0x0c20, "Keychron V2 Ultra 8K",         "V2"),
    ("keychron_z270_ultra_ansi", 0x0d20, "Keychron Z2-70 Ultra 8K",      "Z2-70"),
    ("keychron_v1_ultra_ansi",   0x0c10, "Keychron V1 Ultra 8K",         "V1"),
    ("keychron_q1_ultra_ansi",   0x1210, "Keychron Q1 Ultra 8K",         "Q1"),
    ("keychron_v1_ultra_iso",    0x0c11, "Keychron V1 Ultra 8K (ISO)",   "V1"),
    ("keychron_v1_ultra_jis",    0x0c12, "Keychron V1 Ultra 8K (JIS)",   "V1"),
    ("keychron_v3_ultra_ansi",   0x0c30, "Keychron V3 Ultra 8K",         "V3"),
    ("keychron_q3_ultra_ansi",   0x1230, "Keychron Q3 Ultra 8K",         "Q3"),
    ("keychron_v10_ultra_ansi",  0x0ca0, "Keychron V10 Ultra 8K",        "V10"),
    ("keychron_v5_ultra_ansi",   0x0c50, "Keychron V5 Ultra 8K",         "V5"),
    ("keychron_v6_ultra_ansi",   0x0c60, "Keychron V6 Ultra 8K",         "V6"),
    ("keychron_q6_ultra_ansi",   0x1260, "Keychron Q6 Ultra 8K",         "Q6"),
]

# ---------------------------------------------------------------------------
# ZMK keycode -> human name (OpenRGB "Key: <name>" style, matching the curated
# v6u_led_names strings). Aliases share targets.
# ---------------------------------------------------------------------------
def _build_keymap_table():
    t = {}
    # letters
    for c in "ABCDEFGHIJKLMNOPQRSTUVWXYZ":
        t[c] = c
    # number row
    for d, name in zip("1234567890", "1234567890"):
        t["N" + d] = name
        t["NUMBER_" + d] = name
    t.update({
        "GRAVE": "`", "TILDE": "`",
        "MINUS": "-", "EQUAL": "=",
        "LBKT": "[", "LEFT_BRACKET": "[",
        "RBKT": "]", "RIGHT_BRACKET": "]",
        "BSLH": "\\ (ANSI)", "BACKSLASH": "\\ (ANSI)",
        "SEMI": ";", "SEMICOLON": ";",
        "SQT": "'", "APOS": "'", "APOSTROPHE": "'", "SINGLE_QUOTE": "'",
        "COMMA": ",", "DOT": ".", "PERIOD": ".",
        "FSLH": "/", "SLASH": "/",
        "TAB": "Tab",
        "CLCK": "Caps Lock", "CAPS": "Caps Lock", "CAPSLOCK": "Caps Lock",
        "RET": "Enter", "RETURN": "Enter", "ENTER": "Enter",
        "ESC": "Escape", "ESCAPE": "Escape",
        "BSPC": "Backspace", "BACKSPACE": "Backspace",
        "SPACE": "Space",
        "DEL": "Delete", "DELETE": "Delete",
        "INS": "Insert", "INSERT": "Insert",
        "HOME": "Home", "END": "End",
        "PG_UP": "Page Up", "PGUP": "Page Up", "PAGE_UP": "Page Up",
        "PG_DN": "Page Down", "PGDN": "Page Down", "PAGE_DOWN": "Page Down",
        "UP": "Up Arrow", "UP_ARROW": "Up Arrow",
        "DOWN": "Down Arrow", "DOWN_ARROW": "Down Arrow",
        "LEFT": "Left Arrow", "LEFT_ARROW": "Left Arrow",
        "RIGHT": "Right Arrow", "RIGHT_ARROW": "Right Arrow",
        "LSHFT": "Left Shift", "LSHIFT": "Left Shift", "LEFT_SHIFT": "Left Shift",
        "RSHFT": "Right Shift", "RSHIFT": "Right Shift", "RIGHT_SHIFT": "Right Shift",
        "LCTRL": "Left Control", "LCTL": "Left Control", "LEFT_CONTROL": "Left Control",
        "RCTRL": "Right Control", "RCTL": "Right Control", "RIGHT_CONTROL": "Right Control",
        "LALT": "Left Alt", "LEFT_ALT": "Left Alt",
        "RALT": "Right Alt", "RIGHT_ALT": "Right Alt",
        "LGUI": "Left Windows", "LEFT_GUI": "Left Windows", "LWIN": "Left Windows", "LMETA": "Left Windows",
        "RGUI": "Right Windows", "RIGHT_GUI": "Right Windows", "RWIN": "Right Windows", "RMETA": "Right Windows",
        "K_APP": "Menu", "K_CONTEXT_MENU": "Menu", "K_CMENU": "Menu", "K_APPLICATION": "Menu",
        "PRSC": "Print Screen", "PSCRN": "Print Screen", "PRINTSCREEN": "Print Screen",
        "SLCK": "Scroll Lock", "SCROLLLOCK": "Scroll Lock",
        "PAUSE_BREAK": "Pause/Break", "PAUSE": "Pause/Break",
        # numpad
        "KP_NUMLOCK": "Num Lock", "KP_NUM": "Num Lock", "KP_NLCK": "Num Lock",
        "KP_SLASH": "Number Pad /", "KP_DIVIDE": "Number Pad /",
        "KP_MULTIPLY": "Number Pad *", "KP_ASTERISK": "Number Pad *",
        "KP_MINUS": "Number Pad -", "KP_SUBTRACT": "Number Pad -",
        "KP_PLUS": "Number Pad +", "KP_ADD": "Number Pad +",
        "KP_DOT": "Number Pad .", "KP_DECIMAL": "Number Pad .",
        "KP_ENTER": "Number Pad Enter",
        "KP_EQUAL": "Number Pad =",
        # ISO / JIS extras (unvalidated - best effort)
        "NON_US_BSLH": "\\ (ISO)", "NUBS": "\\ (ISO)",
        "NON_US_HASH": "# (ISO)", "NUHS": "# (ISO)",
    })
    for i in range(0, 10):
        t["KP_N%d" % i] = "Number Pad %d" % i
        t["KP_NUMBER_%d" % i] = "Number Pad %d" % i
    for i in range(1, 25):
        t["F%d" % i] = "F%d" % i
    return t

KEYMAP_TABLE = _build_keymap_table()

# ---------------------------------------------------------------------------
# parsing helpers
# ---------------------------------------------------------------------------
def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    return text

def read(path):
    with open(path, "r") as f:
        return f.read()

def parse_led_count(shield_dir):
    txt = read(os.path.join(shield_dir, "rgb_index.h"))
    m = re.search(r"RGB_MATRIX_LED_COUNT\s*\(\s*(\d+)\s*\)", txt)
    return int(m.group(1))

def _top_level_groups(inner):
    """Given text inside a brace, return the depth-1 {...} child groups (contents)."""
    groups = []
    depth = 0
    start = None
    for i, ch in enumerate(inner):
        if ch == "{":
            if depth == 0:
                start = i + 1
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                groups.append(inner[start:i])
    return groups

def parse_led_config(shield_dir):
    """Return (matrix_to_led rows [list of list of (int|None)], positions [(x,y)]).

    The g_led_config initializer has three top-level brace groups:
      [0] Key Matrix to LED Index   (nested rows)
      [1] LED Index to Physical Position  ({x,y} pairs)  <- geometry
      [2] RGB LED Index to Flag     (ignored)
    Marker comments are not present on every board, so the groups are taken
    positionally. #if 0 ... #endif dead-code (alternate layouts) is stripped.
    """
    raw = read(os.path.join(shield_dir, "rgb_matrix_config.h"))
    txt = re.sub(r"/\*.*?\*/", " ", raw, flags=re.S)     # block comments
    txt = re.sub(r"//[^\n]*", " ", txt)                  # line comments
    txt = re.sub(r"#if\s+0\b.*?#endif", " ", txt, flags=re.S)  # dead code

    # bound to the g_led_config struct literal (brace-match from its '{')
    gi = txt.find("g_led_config")
    bo = txt.find("{", gi)
    depth = 0
    j = bo
    while j < len(txt):
        if txt[j] == "{":
            depth += 1
        elif txt[j] == "}":
            depth -= 1
            if depth == 0:
                break
        j += 1
    inner = txt[bo + 1:j]

    groups = _top_level_groups(inner)
    if len(groups) < 2:
        raise RuntimeError("g_led_config: expected >=2 top-level groups in %s" % shield_dir)

    # block 1 : rows of {...}
    rows = []
    for m in re.finditer(r"\{([^{}]*)\}", groups[0]):
        body = m.group(1).strip()
        if not body:
            continue
        row = []
        for tok in body.split(","):
            tok = tok.strip()
            if tok == "":
                continue
            row.append(None if tok in ("__", "NO_LED") else int(tok, 0))
        rows.append(row)

    # block 2 : {x, y} pairs
    positions = [(int(a), int(b))
                 for a, b in re.findall(r"\{\s*(\d+)\s*,\s*(\d+)\s*\}", groups[1])]

    return rows, positions

def parse_transform(shield_dir, shield):
    """Return ordered list of (row, col) from default_transform map."""
    txt = read(os.path.join(shield_dir, shield + ".overlay"))
    txt = strip_comments(txt)
    m = re.search(r"default_transform\s*:\s*matrix_transform\s*\{(.*?)\};", txt, flags=re.S)
    if not m:
        # fall back: first matrix_transform node
        m = re.search(r"matrix_transform\s*\{(.*?)\};", txt, flags=re.S)
    node = m.group(1)
    mm = re.search(r"map\s*=\s*<(.*?)>", node, flags=re.S)
    body = mm.group(1)
    return [(int(r), int(c)) for r, c in re.findall(r"RC\(\s*(\d+)\s*,\s*(\d+)\s*\)", body)]

WIN_MARKER = "WINBASEMARKERZZ"

def parse_layers(shield_dir, shield):
    """Return (layers, win_marker_pos).

    layers = ordered list of (name, tokens, start_offset).
    win_marker_pos = offset (in the keymap node) of the '//win' section comment
    that precedes the Windows *base* layer, or -1 if absent. The '//win' marker
    is preserved through comment-stripping via a sentinel token so we can select
    the correct (Windows, not Mac) base layer even when both use &kp LGUI.
    """
    raw = read(os.path.join(shield_dir, shield + ".keymap"))

    # preserve the exact "//win" section header (not "//win fn ...") as a sentinel
    raw = re.sub(r"//[ \t]*win[ \t]*(?=\r?\n)", WIN_MARKER, raw)
    txt = strip_comments(raw)

    # isolate the keymap node so we don't pick up macro/behavior bindings
    km = re.search(r"keymap\s*\{", txt)
    if not km:
        raise RuntimeError("no keymap node in %s" % shield)
    start = km.end()
    depth = 1
    i = start
    while i < len(txt) and depth > 0:
        if txt[i] == "{":
            depth += 1
        elif txt[i] == "}":
            depth -= 1
        i += 1
    node = txt[start:i]

    win_marker_pos = node.find(WIN_MARKER)

    layers = []
    # layer:  name { ... bindings = < ... > ; ... }
    # (?<![-\w])bindings avoids matching "sensor-bindings"
    pat = re.compile(r"(\w+)\s*\{[^{}]*?(?<![-\w])bindings\s*=\s*<([^<>]*)>", re.S)
    for m in pat.finditer(node):
        name = m.group(1)
        body = m.group(2)
        tokens = tokenize_bindings(body)
        layers.append((name, tokens, m.start()))
    return layers, win_marker_pos

def tokenize_bindings(body):
    """Split a bindings body into one entry per '&behaviour ...'."""
    parts = body.split("&")
    toks = []
    for p in parts[1:]:          # parts[0] is whitespace before first &
        words = p.split()
        if not words:
            toks.append(("", []))
        else:
            toks.append((words[0], words[1:]))
    return toks

def choose_windows_layer(layers, win_marker_pos):
    """Pick the Windows base layer. Returns (tokens, tag).

    Priority:
      1. the first base layer AFTER the '//win' section marker (authoritative:
         the firmware groups mac layers first, then win layers);
      2. else the base layer that looks most Windows-ish: has PrtSc and/or the
         most plain &kp F1..F12 keys (the Mac base maps that row to media/
         brightness consumer codes instead);
      3. else the base layer with the most &kp keys (numpads with no mac/win
         split, e.g. V0).
    A 'base layer' is one whose &kp count is a large fraction of its keys (fn
    layers are mostly &trans / &rgb_ug and are excluded)."""
    def kp_count(toks):
        return sum(1 for n, p in toks if n == "kp")
    def has(toks, code):
        return any(n == "kp" and p and p[0] == code for n, p in toks)
    def fkeys(toks):
        return sum(1 for n, p in toks
                   if n == "kp" and p and re.fullmatch(r"F([1-9]|1[0-9]|2[0-4])", p[0]))
    def is_base(toks):
        return len(toks) > 0 and kp_count(toks) >= max(6, len(toks) // 2)

    bases = [l for l in layers if is_base(l[1])]
    if not bases:
        bases = layers

    # 1. marker-selected
    if win_marker_pos >= 0:
        after = [l for l in bases if l[2] > win_marker_pos]
        if after:
            best = min(after, key=lambda l: l[2])   # first base after //win
            return best[1], "windows-layer:%s (//win)" % best[0]

    # 2. windows-ish heuristic
    def win_score(l):
        return (has(l[1], "PRSC") or has(l[1], "PSCRN"), fkeys(l[1]))
    best = max(bases, key=win_score)
    if win_score(best)[0] or win_score(best)[1] > 0:
        return best[1], "windows-layer:%s (heuristic)" % best[0]

    # 3. fallback: most kp keys, no mac/win distinction
    best = max(layers, key=lambda l: kp_count(l[1]))
    return best[1], "base-layer:%s (no win layer)" % best[0]

def keycode_name(behavior):
    """behavior = (name, [params]).  Return human name or None (=> fallback)."""
    name, params = behavior
    if name != "kp":
        return None
    if len(params) != 1:
        return None
    code = params[0]
    if "(" in code or ")" in code:
        return None
    return KEYMAP_TABLE.get(code)

# ---------------------------------------------------------------------------
# geometry
# ---------------------------------------------------------------------------
def round_half_up(v):
    import math
    return int(math.floor(v + 0.5))

def cluster_rows(positions):
    """Greedy y-clustering. Return dict y->row_index and row count."""
    ys = sorted(set(y for _, y in positions))
    bands = []
    anchor = None
    for y in ys:
        if anchor is None or (y - anchor) > Y_TOL:
            bands.append([])
            anchor = y
        bands[-1].append(y)
    y_to_row = {}
    for idx, band in enumerate(bands):
        for y in band:
            y_to_row[y] = idx
    return y_to_row, len(bands)

def build_matrix_map(positions):
    """Bin LEDs to a height x width grid.

    rows : greedy y-clustering (top->bottom).
    cols : c = round(x / PITCH). Within a row, LEDs are placed left->right; if the
           rounded column is already claimed by a smaller-x key, the LED is bumped
           to the next free column (the grid widens). This never drops an LED and,
           because V6 has no in-row collisions, leaves the V6 grid identical to a
           plain round(x/PITCH) — preserving the byte-exact validation gate.
    """
    y_to_row, height = cluster_rows(positions)

    # group LED indices by row, ordered left->right by x
    by_row = [[] for _ in range(height)]
    for led, (x, y) in enumerate(positions):
        by_row[y_to_row[y]].append((x, led))
    for r in range(height):
        by_row[r].sort()

    assign = {}          # led -> (row, col)
    bumped = 0
    for r in range(height):
        prev_col = -1
        for x, led in by_row[r]:
            c = round_half_up(x / PITCH)
            if c <= prev_col:
                c = prev_col + 1
                bumped += 1
            prev_col = c
            assign[led] = (r, c)

    width = max(c for _, c in assign.values()) + 1
    grid = [[NO_LED] * width for _ in range(height)]
    for led, (r, c) in assign.items():
        grid[r][c] = led
    flat = [cell for row in grid for cell in row]
    return flat, height, width, bumped

# ---------------------------------------------------------------------------
# names
# ---------------------------------------------------------------------------
def build_led_names(led_count, matrix_to_led, transform, win_tokens):
    names = ["LED %d" % i for i in range(led_count)]
    mapped = [False] * led_count
    n = min(len(transform), len(win_tokens))
    for i in range(n):
        r, c = transform[i]
        if r >= len(matrix_to_led) or c >= len(matrix_to_led[r]):
            continue
        led = matrix_to_led[r][c]
        if led is None:
            continue
        if led >= led_count:
            continue
        nm = keycode_name(win_tokens[i])
        if nm is not None:
            names[led] = "Key: " + nm
            mapped[led] = True
    real = sum(1 for m in mapped if m)
    return names, real, mapped

# ---------------------------------------------------------------------------
# curated reference (V6) for validation + override of V6/Q6
#
# This is the hand-authored ground-truth data (v6u_matrix_map 6x22 and the 108
# v6u_led_names) taken from the original RGBController_KeychronV6Ultra.cpp. It is
# embedded here because that file has since been refactored to consume this
# generator's output (KeychronLayout), so the arrays no longer live in it.
# ---------------------------------------------------------------------------
_X = NO_LED
CURATED_MAP = [
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, _X, _X, 13, 14, 15, 16, 17, 18, 19,
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, _X, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, _X, 54, 55, 56, 57, 58, 59, 60, 77,
    61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, _X, _X, 73, _X, _X, _X, 74, 75, 76, _X,
    78, _X, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, _X, _X, 89, _X, 90, _X, 91, 92, 93, 107,
    94, 95, 96, _X, _X, _X, 97, _X, _X, _X, 98, 99, 100, _X, 101, 102, 103, 104, _X, 105, 106, _X,
]
CURATED_NAMES = [
    "Key: Escape", "Key: F1", "Key: F2", "Key: F3", "Key: F4", "Key: F5",
    "Key: F6", "Key: F7", "Key: F8", "Key: F9", "Key: F10", "Key: F11", "Key: F12",
    "Key: Print Screen", "Key: Scroll Lock", "Key: Pause/Break",
    "Key: Media Mute", "Key: Media Play/Pause", "Key: Media Previous", "Key: Media Next",
    "Key: `", "Key: 1", "Key: 2", "Key: 3", "Key: 4", "Key: 5", "Key: 6", "Key: 7",
    "Key: 8", "Key: 9", "Key: 0", "Key: -", "Key: =", "Key: Backspace",
    "Key: Insert", "Key: Home", "Key: Page Up",
    "Key: Num Lock", "Key: Number Pad /", "Key: Number Pad *", "Key: Number Pad -",
    "Key: Tab", "Key: Q", "Key: W", "Key: E", "Key: R", "Key: T", "Key: Y", "Key: U",
    "Key: I", "Key: O", "Key: P", "Key: [", "Key: ]", "Key: \\ (ANSI)",
    "Key: Delete", "Key: End", "Key: Page Down",
    "Key: Number Pad 7", "Key: Number Pad 8", "Key: Number Pad 9",
    "Key: Caps Lock", "Key: A", "Key: S", "Key: D", "Key: F", "Key: G", "Key: H",
    "Key: J", "Key: K", "Key: L", "Key: ;", "Key: '", "Key: Enter",
    "Key: Number Pad 4", "Key: Number Pad 5", "Key: Number Pad 6", "Key: Number Pad +",
    "Key: Left Shift", "Key: Z", "Key: X", "Key: C", "Key: V", "Key: B", "Key: N",
    "Key: M", "Key: ,", "Key: .", "Key: /", "Key: Right Shift",
    "Key: Up Arrow",
    "Key: Number Pad 1", "Key: Number Pad 2", "Key: Number Pad 3",
    "Key: Left Control", "Key: Left Windows", "Key: Left Alt", "Key: Space",
    "Key: Right Alt", "Key: Right Windows", "Key: Menu", "Key: Right Control",
    "Key: Left Arrow", "Key: Down Arrow", "Key: Right Arrow",
    "Key: Number Pad 0", "Key: Number Pad Enter", "Key: Number Pad .",
]

def parse_curated():
    """Return (matrix_map flat ints, led_names list) ground-truth for V6."""
    return list(CURATED_MAP), list(CURATED_NAMES)

# ---------------------------------------------------------------------------
# C++ emission
# ---------------------------------------------------------------------------
def c_escape(s):
    return s.replace("\\", "\\\\").replace('"', '\\"')

def fmt_matrix(flat, width):
    out = []
    for i in range(0, len(flat), width):
        row = flat[i:i + width]
        cells = ["NO_LED" if v == NO_LED else str(v) for v in row]
        out.append("    " + ", ".join(cells) + ",")
    return "\n".join(out)

def fmt_names(names):
    return "\n".join('    "%s",' % c_escape(n) for n in names)

HEADER = '''#pragma once
#ifndef NO_LED
#define NO_LED 0xFFFFFFFFu
#endif
struct KeychronLayout {
    unsigned short pid;
    const char*    name;         // e.g. "Keychron V6 Ultra 8K"
    const char*    description;  // "<Board> Ultra (custom ZMK firmware, OpenRGB direct control)"
    unsigned int   led_count;
    unsigned int   map_height;
    unsigned int   map_width;
    unsigned int*  matrix_map;   // map_height*map_width, row-major, NO_LED = gap
    const char**   led_names;    // led_count entries
};
extern const KeychronLayout KEYCHRON_LAYOUTS[];
extern const unsigned int   KEYCHRON_LAYOUT_COUNT;
'''

def main():
    curated_map, curated_names = parse_curated()

    results = []   # dicts per board
    report = []

    for shield, pid, name, short in BOARDS:
        sd = os.path.join(SHIELDS_DIR, shield)
        led_count = parse_led_count(sd)
        m2l, positions = parse_led_config(sd)
        transform = parse_transform(sd, shield)
        layers, win_marker_pos = parse_layers(sd, shield)
        win_tokens, win_tag = choose_windows_layer(layers, win_marker_pos)

        flat, height, width, bumped = build_matrix_map(positions)
        names, real, mapped = build_led_names(led_count, m2l, transform, win_tokens)

        results.append({
            "shield": shield, "pid": pid, "name": name, "short": short,
            "led_count": led_count, "height": height, "width": width,
            "matrix": flat, "names": names, "real": real,
            "gen_names": list(names), "gen_mapped": mapped,
            "bumped": bumped, "win_tag": win_tag,
            "n_positions": len(positions),
            "placed": sum(1 for v in flat if v != NO_LED),
        })

    # ---- validation against curated V6 ----
    v6 = next(r for r in results if r["shield"] == "keychron_v6_ultra_ansi")
    map_match = (v6["matrix"] == curated_map and
                 v6["height"] == 6 and v6["width"] == 22)
    name_match = sum(1 for a, b in zip(v6["names"], curated_names) if a == b)

    # ---- V6 / Q6 name handling -------------------------------------------
    # Generator name reproduction vs curated is below the 95% gate, so per the
    # brief we do NOT ship the generated names for V6/Q6; we keep the curated,
    # human-verified labels (incl. the deliberate top-right custom-cluster
    # mapping). EXCEPTION: where the curated label is a standard key that the
    # authoritative transform proves belongs to a DIFFERENT LED (curated bug),
    # we take the transform-derived name. This corrects LED106/LED107 on V6/Q6,
    # whose curated "." / "Enter" labels are swapped relative to both the
    # firmware transform and the byte-exact geometry (tall Enter = LED107).
    overrides = set()
    corrections = {}
    STD = set(KEYMAP_TABLE.values())
    for r in results:
        if r["shield"] in ("keychron_v6_ultra_ansi", "keychron_q6_ultra_ansi"):
            if r["matrix"] == curated_map and r["led_count"] == len(curated_names):
                names = list(curated_names)
                fixed = []
                for i in range(r["led_count"]):
                    gen = r["gen_names"][i]
                    # only correct when the generator is confident (real &kp key)
                    # and the curated label names a different standard key
                    if (r["gen_mapped"][i]
                            and names[i] != gen
                            and names[i].startswith("Key: ")
                            and names[i][5:] in STD):
                        fixed.append((i, names[i], gen))
                        names[i] = gen
                r["names"] = names
                r["real"] = r["led_count"]
                overrides.add(r["shield"])
                corrections[r["shield"]] = fixed

    # ---- emit ----
    emit(results)

    # ---- report ----
    print("=" * 70)
    print("VALIDATION")
    print("=" * 70)
    print("V6 matrix_map exact match (6x22): %s" % ("YES" if map_match else "NO"))
    print("V6 led_names match vs curated: %d / %d (%.1f%%)"
          % (name_match, len(curated_names), 100.0 * name_match / len(curated_names)))
    print("V6/Q6 curated-name override applied to: %s"
          % (", ".join(sorted(overrides)) if overrides else "(none)"))
    for sh in sorted(corrections):
        for i, was, now in corrections[sh]:
            print("    corrected %s LED %d: curated %r -> transform %r"
                  % (sh, i, was, now))
    print()
    print("%-26s %4s %6s %6s %8s %8s  %s"
          % ("shield", "leds", "HxW", "cells", "named", "fallbk", "name-source"))
    for r in sorted(results, key=lambda x: x["led_count"]):
        cells = r["height"] * r["width"]
        fb = r["led_count"] - r["real"]
        note = r["win_tag"]
        if r["shield"] in overrides:
            note = "curated (override)"
        print("%-26s %4d %6s %6d %8d %8d  %s"
              % (r["shield"], r["led_count"],
                 "%dx%d" % (r["height"], r["width"]), cells,
                 r["real"], fb, note))
        if r["bumped"]:
            print("    (grid widened: %d key(s) bumped to a free column)" % r["bumped"])
        if r["placed"] != r["led_count"]:
            print("    !! DROPPED LEDS: placed %d of %d" % (r["placed"], r["led_count"]))
        if r["n_positions"] != r["led_count"]:
            print("    !! positions(%d) != led_count(%d)"
                  % (r["n_positions"], r["led_count"]))
    print()
    print("Wrote %s and %s"
          % (os.path.join(OUT_DIR, "KeychronLayouts.h"),
             os.path.join(OUT_DIR, "KeychronLayouts.cpp")))


def emit(results):
    with open(os.path.join(OUT_DIR, "KeychronLayouts.h"), "w") as f:
        f.write(HEADER)

    order = sorted(results, key=lambda x: x["led_count"])
    lines = []
    lines.append('#include "KeychronLayouts.h"')
    lines.append("")
    lines.append("/* Generated by tools/gen_keychron_layouts.py - do not edit by hand. */")
    lines.append("")
    for r in results:
        ident = r["shield"]
        lines.append("static unsigned int %s_matrix_map[] =" % ident)
        lines.append("{")
        lines.append(fmt_matrix(r["matrix"], r["width"]))
        lines.append("};")
        lines.append("")
        lines.append("static const char* %s_led_names[] =" % ident)
        lines.append("{")
        lines.append(fmt_names(r["names"]))
        lines.append("};")
        lines.append("")

    lines.append("const KeychronLayout KEYCHRON_LAYOUTS[] =")
    lines.append("{")
    for r in order:
        ident = r["shield"]
        desc = "%s Ultra (custom ZMK firmware, OpenRGB direct control)" % r["short"]
        lines.append("    {")
        lines.append("        0x%04x," % r["pid"])
        lines.append('        "%s",' % c_escape(r["name"]))
        lines.append('        "%s",' % c_escape(desc))
        lines.append("        %d, %d, %d," % (r["led_count"], r["height"], r["width"]))
        lines.append("        %s_matrix_map," % ident)
        lines.append("        %s_led_names," % ident)
        lines.append("    },")
    lines.append("};")
    lines.append("")
    lines.append("const unsigned int KEYCHRON_LAYOUT_COUNT = %d;" % len(order))
    lines.append("")
    with open(os.path.join(OUT_DIR, "KeychronLayouts.cpp"), "w") as f:
        f.write("\n".join(lines))


if __name__ == "__main__":
    main()

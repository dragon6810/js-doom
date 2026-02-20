#!/usr/bin/env python3
"""
multigen_to_info.py

Parse DOOM's official multigen.txt format (id's MULTIGEN) and generate:
  - info.h
  - info.c

What this supports (quirks seen in real multigen.txt files):
- State lines start with "S_":
    S_<STATENAME>  <SPRITENAME>  <FRAME>[*]  <TICS>  <ACTION>  <NEXTSTATE>  [MISC1 [MISC2]]
  * Fullbright marker '*' may appear on FRAME (e.g. A*, [*) OR (in some scripts) on TICS (e.g. 8*).
  * FRAME is not limited to A-Z; '[' '\\' ']' etc are accepted and mapped as (ord(ch)-ord('A')).

- Info blocks:
  * Start with: $ <TYPENAME>   or   $ +
    "$ +" creates MT_MISC<n>.
  * The "$ <TYPENAME>" line may also contain inline assignments:
      $ + doomednum 2018 spawnstate S_ARM1 flags MF_SPECIAL
  * Non-$, non-state lines can contain multiple assignments and multi-token expressions:
      height 68*FRACUNIT radius 16*FRACUNIT

- The first block MUST be:
    $ DEFAULT
    <field> <default_value>
    ...
  which defines the ordered field list and defaults.

Customizations for your engine:
- You are NOT using fixed-point. Any expression containing FRACUNIT is rewritten to floating-point
  literals (FRACUNIT -> 1.0f, integers -> N.0f, and redundant *1.0f removed).
- You don't have sounds yet: any field whose name suggests a sound (contains "sound" case-insensitive)
  is forced to literal 0 in the generated C.

Notes:
- This generator emits "classic" Doom-shaped structs (state_t, mobjinfo_t) but with numeric
  fields as float (except flags-like fields that are clearly bitmasks/integers).
  Specifically:
    - Fields starting with "str_" are char*
    - Fields whose name contains "flags" or ends with "_flags" or is "flags" are int
    - Fields whose name contains "sound" are int (and forced to 0 in output)
    - Everything else is float

Usage:
  python3 multigen_to_info.py multigen.txt
  python3 multigen_to_info.py multigen.txt --outdir ./out
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple


class MultigenError(RuntimeError):
    pass


@dataclass
class LineTokens:
    line_no: int
    toks: List[str]


def _strip_comment(line: str) -> str:
    # In multigen scripts, ';' begins a comment to end of line.
    i = line.find(";")
    if i >= 0:
        line = line[:i]
    return line.strip()


def read_lines_as_tokens(path: Path) -> List[LineTokens]:
    out: List[LineTokens] = []
    with path.open("r", encoding="utf-8", errors="replace") as f:
        for ln, raw in enumerate(f, start=1):
            s = _strip_comment(raw.rstrip("\r\n"))
            if not s:
                continue
            toks = s.split()
            if toks:
                out.append(LineTokens(ln, toks))
    return out


def c_ident(name: str) -> str:
    out = []
    for ch in name:
        out.append(ch if (ch.isalnum() or ch == "_") else "_")
    s = "".join(out) or "X"
    if s[0].isdigit():
        s = "_" + s
    return s


def is_sound_field(field: str) -> bool:
    return "sound" in field.lower()


def is_flags_field(field: str) -> bool:
    f = field.lower()
    return f == "flags" or f.endswith("_flags") or "flags" in f


def fixed_to_float_expr(expr: str) -> str:
    """
    Convert common fixed-point expressions using FRACUNIT into float-friendly expressions.
    This is intentionally conservative (string rewrite), not a full expression parser.

    - FRACUNIT -> 1.0f
    - Bare integers -> N.0f
    - Strip redundant *1.0f or 1.0f*
    """
    e = expr

    # Replace FRACUNIT with 1.0f
    e = re.sub(r"\bFRACUNIT\b", "1.0f", e)

    # Convert integer tokens to float tokens (avoid touching identifiers)
    # Examples: "68*1.0f" -> "68.0f*1.0f"
    def repl_int(m: re.Match) -> str:
        return f"{m.group(1)}.0f"

    e = re.sub(r"(?<![\w.])(\d+)(?![\w.])", repl_int, e)

    # Simplify redundant multiplications by 1.0f
    e = e.replace("*1.0f", "")
    e = e.replace("1.0f*", "")

    return e


@dataclass
class State:
    name: str
    sprite_i: int
    frame: int
    tics: int
    action_i: int
    next_name: str
    next_i: int = -1
    misc1: int = 0
    misc2: int = 0


def parse_multigen(path: Path):
    lines = read_lines_as_tokens(path)
    if not lines:
        raise MultigenError("empty script")

    spritenames: List[str] = []
    actionnames: List[str] = ["NULL"]  # action 0 is NULL like doom
    states: List[State] = []
    statename_to_index: Dict[str, int] = {}

    typenam: List[str] = []
    # base fields from DEFAULT: (fieldname, default_value, is_string)
    base_fields: List[Tuple[str, str, bool]] = []
    field_index: Dict[str, int] = {}
    # per-type overrides aligned to base_fields: Optional[str] per field
    overrides: List[List[Optional[str]]] = []

    misc_counter = 0

    def sprite_index(s: str) -> int:
        for i, n in enumerate(spritenames):
            if n.lower() == s.lower():
                return i
        spritenames.append(s)
        return len(spritenames) - 1

    def action_index(s: str) -> int:
        for i, n in enumerate(actionnames):
            if n.lower() == s.lower():
                return i
        actionnames.append(s)
        return len(actionnames) - 1

    # ---- Parse $ DEFAULT first
    i = 0
    if lines[i].toks[0] != "$":
        raise MultigenError(f"line {lines[i].line_no}: first directive must be '$ DEFAULT'")
    if len(lines[i].toks) < 2 or lines[i].toks[1] != "DEFAULT":
        raise MultigenError(f"line {lines[i].line_no}: first directive must be '$ DEFAULT'")
    i += 1

    while i < len(lines):
        toks = lines[i].toks
        if toks[0] == "$" or toks[0].startswith("S_"):
            break
        if len(toks) != 2:
            raise MultigenError(f"line {lines[i].line_no}: DEFAULT entries must be '<field> <value>'")
        field, val = toks
        if field in field_index:
            raise MultigenError(f"line {lines[i].line_no}: duplicate DEFAULT field '{field}'")
        is_str = field.startswith("str_")
        field_index[field] = len(base_fields)
        base_fields.append((field, val, is_str))
        i += 1

    if not base_fields:
        raise MultigenError("DEFAULT block contained no fields")

    def apply_assignments(token_list: List[str], line_no: int, current_type: int) -> None:
        """
        Parse a sequence of assignments:
          field <expr...> field2 <expr...>
        where <expr...> continues until the next recognized field name or end of list.
        Expressions are joined with no spaces (so '68 * FRACUNIT' becomes '68*FRACUNIT').
        """
        j = 0
        while j < len(token_list):
            field = token_list[j]
            fi = field_index.get(field)
            if fi is None:
                raise MultigenError(f"line {line_no}: unknown field '{field}' (not in DEFAULT)")
            j += 1
            if j >= len(token_list):
                raise MultigenError(f"line {line_no}: missing value for field '{field}'")

            start = j
            while j < len(token_list) and (token_list[j] not in field_index):
                j += 1

            expr_tokens = token_list[start:j]
            val = "".join(expr_tokens)
            overrides[current_type][fi] = val

    # ---- Parse rest (interleaved $ blocks and S_ lines)
    current_type: Optional[int] = None

    while i < len(lines):
        lt = lines[i]
        toks = lt.toks

        # State line
        if toks[0].startswith("S_"):
            if len(toks) < 6:
                raise MultigenError(
                    f"line {lt.line_no}: state line must have at least 6 tokens "
                    "(S_NAME SPRT FRAME TICS ACTION NEXTSTATE)"
                )
            st_name = toks[0]
            if st_name.lower() in statename_to_index:
                raise MultigenError(f"line {lt.line_no}: duplicate state '{st_name}'")

            spr = toks[1]
            action_tok = toks[4]
            next_tok = toks[5]

            # frame token: single ASCII char, optional trailing '*'
            raw_frame_tok = toks[2]
            fullbright = False
            if raw_frame_tok.endswith("*"):
                fullbright = True
                raw_frame_tok = raw_frame_tok[:-1]
            if len(raw_frame_tok) != 1:
                raise MultigenError(f"line {lt.line_no}: bad frame token '{toks[2]}'")

            ch = raw_frame_tok[0]
            frame = ord(ch) - ord("A")
            if frame < 0:
                raise MultigenError(f"line {lt.line_no}: bad frame token '{toks[2]}'")

            # tics token: integer, but tolerate trailing '*'
            raw_tics_tok = toks[3]
            if raw_tics_tok.endswith("*"):
                fullbright = True
                raw_tics_tok = raw_tics_tok[:-1]
            try:
                tics = int(raw_tics_tok, 10)
            except ValueError:
                raise MultigenError(f"line {lt.line_no}: bad tics '{toks[3]}'")

            if fullbright:
                frame |= 0x8000

            act_i = action_index(action_tok)
            spr_i = sprite_index(spr)

            misc1 = 0
            misc2 = 0
            if len(toks) >= 7:
                try:
                    misc1 = int(toks[6], 10)
                except ValueError:
                    raise MultigenError(f"line {lt.line_no}: bad misc1 '{toks[6]}'")
            if len(toks) >= 8:
                try:
                    misc2 = int(toks[7], 10)
                except ValueError:
                    raise MultigenError(f"line {lt.line_no}: bad misc2 '{toks[7]}'")
            if len(toks) > 8:
                raise MultigenError(f"line {lt.line_no}: too many tokens on state line")

            idx = len(states)
            states.append(
                State(
                    name=st_name,
                    sprite_i=spr_i,
                    frame=frame,
                    tics=tics,
                    action_i=act_i,
                    next_name=next_tok,
                    misc1=misc1,
                    misc2=misc2,
                )
            )
            statename_to_index[st_name.lower()] = idx
            i += 1
            continue

        # Type block start (with optional inline assignments)
        if toks[0] == "$":
            if len(toks) < 2:
                raise MultigenError(f"line {lt.line_no}: '$' line must be '$ <NAME>' or '$ +'")

            name = toks[1]
            if name == "+":
                name = f"MT_MISC{misc_counter}"
                misc_counter += 1

            typenam.append(name)
            overrides.append([None] * len(base_fields))
            current_type = len(typenam) - 1

            # Inline assignments after "$ <NAME>"
            extra = toks[2:]
            if extra:
                apply_assignments(extra, lt.line_no, current_type)

            i += 1
            continue

        # Regular info line(s)
        if current_type is None:
            raise MultigenError(f"line {lt.line_no}: info fields appear before any '$ <TYPE>' block")
        apply_assignments(toks, lt.line_no, current_type)
        i += 1

    # ---- Resolve nextstate references
    for st in states:
        j = statename_to_index.get(st.next_name.lower())
        if j is None:
            raise MultigenError(f"state {st.name}: unresolved nextstate '{st.next_name}'")
        st.next_i = j

    return spritenames, actionnames, states, typenam, base_fields, overrides


def write_info_h(out: Path, spritenames, states, typenam, base_fields) -> None:
    guard = "INFO_H"
    with out.open("w", encoding="utf-8", newline="\n") as f:
        f.write("// generated by multigen_to_info.py\n\n")
        f.write(f"#ifndef {guard}\n#define {guard}\n\n")
        f.write("#include <stddef.h>\n\n")

        f.write(
            "#define	MF_SPECIAL	1         // call P_SpecialThing when touched\n"
            "#define	MF_SOLID	2\n"
            "#define	MF_SHOOTABLE	4\n"
            "#define	MF_NOSECTOR	8         // don't use the sector links\n"
                                            "// (invisible but touchable)\n"
            "#define	MF_NOBLOCKMAP	16        // don't use the blocklinks\n"
                                            "// (inert but displayable)\n"
#define	MF_AMBUSH	32
            "#define	MF_JUSTHIT	64        // try to attack right back\n"
            "#define	MF_JUSTATTACKED	128       // take at least one step before attacking\n"
            "#define	MF_SPAWNCEILING	256       // hang from ceiling instead of floor\n"
            "#define	MF_NOGRAVITY	512       // don't apply gravity every tic\n"

            "// movement flags\n"
            "#define	MF_DROPOFF	0x400     // allow jumps from high places\n"
            "#define	MF_PICKUP	0x800     // for players to pick up items\n"
            "#define	MF_NOCLIP	0x1000    // player cheat\n"
            "#define	MF_SLIDE	0x2000    // keep info about sliding along walls\n"
            "#define	MF_FLOAT	0x4000    // allow moves to any height, no gravity\n"
            "#define	MF_TELEPORT	0x8000    // don't cross lines or look at heights\n"
            "#define MF_MISSILE	0x10000   // don't hit same species, explode on block\n"

            "#define	MF_DROPPED	0x20000   // dropped by a demon, not level spawned\n"
            "#define	MF_SHADOW	0x40000   // use translucent draw (shadow demons / invis)\n"
            "#define	MF_NOBLOOD	0x80000   // don't bleed when shot (use puff)\n"
            "#define	MF_CORPSE	0x100000  // don't stop moving halfway off a step\n"
            "#define	MF_INFLOAT	0x200000  // floating to a height for a move, don't\n"
                                            "// auto float to target's height\n"

            "#define	MF_COUNTKILL	0x400000  // count towards intermission kill total\n"
            "#define	MF_COUNTITEM	0x800000  // count towards intermission item total\n"

            "#define	MF_SKULLFLY	0x1000000 // skull in flight\n"
            "#define	MF_NOTDMATCH	0x2000000 // don't spawn in death match (key cards)\n"

            "#define	MF_TRANSLATION	0xc000000 // if 0x4 0x8 or 0xc, use a translation\n"
            "#define	MF_TRANSSHIFT	26      // table for player colormaps\n"
        )

        # Sprite enum
        f.write("typedef enum {\n")
        for s in spritenames:
            f.write(f"    SPR_{c_ident(s)},\n")
        f.write("    NUMSPRITES\n} spritenum_t;\n\n")

        # State enum (state names already include S_)
        f.write("typedef enum {\n")
        for st in states:
            f.write(f"    {c_ident(st.name)},\n")
        f.write("    NUMSTATES\n} statenum_t;\n\n")

        # state_t (keep tics as int; frame is bit-packed; misc are ints)
        f.write(
            "typedef struct\n"
            "{\n"
            "    spritenum_t sprite;\n"
            "    int         frame;\n"
            "    int         tics;\n"
            "    void      (*action)();\n"
            "    statenum_t  nextstate;\n"
            "    int         misc1, misc2;\n"
            "} state_t;\n\n"
        )

        f.write("extern state_t states[NUMSTATES];\n")
        f.write("extern char *sprnames[NUMSPRITES];\n\n")

        # mobjtype enum
        f.write("typedef enum {\n")
        for t in typenam:
            f.write(f"    {c_ident(t)},\n")
        f.write("    NUMMOBJTYPES\n} mobjtype_t;\n\n")

        # mobjinfo_t derived from DEFAULT fields, but with your float-vs-int policy
        f.write("typedef struct {\n")
        for (field, _base, is_str) in base_fields:
            fld = c_ident(field)
            if is_str:
                f.write(f"    char *{fld};\n")
            elif is_sound_field(field) or is_flags_field(field):
                f.write(f"    int   {fld};\n")
            else:
                f.write(f"    float {fld};\n")
        f.write("} mobjinfo_t;\n\n")

        f.write("extern mobjinfo_t mobjinfo[NUMMOBJTYPES];\n\n")
        f.write(f"#endif // {guard}\n")


def write_info_c(
    out: Path,
    spritenames,
    actionnames,
    states,
    typenam,
    base_fields,
    overrides,
) -> None:
    with out.open("w", encoding="utf-8", newline="\n") as f:
        f.write('#include "info.h"\n')
        f.write("// generated by multigen_to_info.py\n\n")

        # sprnames
        f.write("char *sprnames[NUMSPRITES] = {\n")
        for i, s in enumerate(spritenames):
            f.write(f'    "{s}"')
            f.write(",\n" if i != len(spritenames) - 1 else "\n")
        f.write("};\n\n")

        # action prototypes (skip NULL)
        for a in actionnames[1:]:
            f.write(f"void {c_ident(a)}();\n")
        f.write("\n")

        # states
        f.write("state_t states[NUMSTATES] = {\n")
        for st in states:
            spr = f"SPR_{c_ident(spritenames[st.sprite_i])}"
            act = c_ident(actionnames[st.action_i])
            nxt = c_ident(states[st.next_i].name)
            f.write(
                f"    {{{spr},{st.frame},{st.tics},{act},{nxt},{st.misc1},{st.misc2}}},"
                f"    // {st.name}\n"
            )
        f.write("};\n\n")

        # mobjinfo
        f.write("mobjinfo_t mobjinfo[NUMMOBJTYPES] = {\n")
        for ti, tname in enumerate(typenam):
            f.write(f"    {{   // {tname}\n")
            ov = overrides[ti]
            for fi, (field, base, is_str) in enumerate(base_fields):
                raw = ov[fi] if ov[fi] is not None else base

                if is_str:
                    val = raw
                elif is_sound_field(field):
                    val = "0"  # force sounds to 0 for now
                elif is_flags_field(field):
                    val = raw  # keep bitmask-like expressions intact
                else:
                    val = fixed_to_float_expr(raw)

                    # Ensure a trailing 'f' exists for plain numeric literals (optional hygiene)
                    # If it's an expression (contains operators/identifiers), we leave it alone.
                    # Example: "68.0f" OK; "(20.0f/35.0f)" OK.
                    pass

                comma = "," if fi != len(base_fields) - 1 else ""
                f.write(f"        {val}{comma}   // {field}\n")
            f.write("    }")
            f.write(",\n" if ti != len(typenam) - 1 else "\n")
        f.write("};\n")


def main() -> int:
    ap = argparse.ArgumentParser(description="Parse DOOM multigen.txt and emit info.h/info.c")
    ap.add_argument("multigen_txt", help="Path to DOOM multigen.txt")
    ap.add_argument("--outdir", default=".", help="Output directory (default: current dir)")
    args = ap.parse_args()

    src = Path(args.multigen_txt)
    if not src.is_file():
        print(f"ERROR: not found: {src}", file=sys.stderr)
        return 2

    try:
        spritenames, actionnames, states, typenam, base_fields, overrides = parse_multigen(src)
    except MultigenError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 2

    outdir = Path(args.outdir)
    outdir.mkdir(parents=True, exist_ok=True)

    info_h = outdir / "info.h"
    info_c = outdir / "info.c"

    write_info_h(info_h, spritenames, states, typenam, base_fields)
    write_info_c(info_c, spritenames, actionnames, states, typenam, base_fields, overrides)

    print(f"Wrote {info_h}")
    print(f"Wrote {info_c}")
    print(
        f"{len(states)} states, {len(typenam)} mobj types, "
        f"{len(spritenames)} sprites, {len(actionnames)-1} actions"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
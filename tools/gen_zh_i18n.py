#!/usr/bin/env python3
"""
Generate the Simplified Chinese translation files for SatDump's gettext-based
i18n system, from a string dictionary and the master satdump.pot catalog.

Usage:
    python tools/gen_zh_i18n.py [--import-old-cpp path/to/ui_translation.cpp] [pot_file] [dict_file]

Defaults:
    pot_file  = resources/i18n/po/satdump.pot
    dict_file = resources/i18n/zh_dict.tsv

Outputs (always UTF-8):
    resources/i18n/zh_CN/LC_MESSAGES/satdump.mo  (directly usable by libintl-tiny)
    resources/i18n/zh_CN/LC_MESSAGES/zh_CN.po    (maintainable copy, msgfmt-compatible)

The .mo writer emits the "simple" table format (no hash table), which
libintl-tiny (src-core/libs/libintl-tiny) reads just fine:
    magic 0x950412de, revision 0, number of strings, offset of original
    string table, offset of translated string table.
Entries are sorted by the original string (byte order), as
MessageCatalog::getTranslatedStrPtr() uses std::lower_bound.
"""

import os
import re
import struct
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

POT_DEFAULT = os.path.join(REPO_ROOT, "resources", "i18n", "po", "satdump.pot")
DICT_DEFAULT = os.path.join(REPO_ROOT, "resources", "i18n", "zh_dict.tsv")
OUT_DIR = os.path.join(REPO_ROOT, "resources", "i18n", "zh_CN", "LC_MESSAGES")


EXTRA_DICT_DEFAULT = os.path.join(REPO_ROOT, "resources", "i18n", "zh_extra.tsv")


def translate_msgid(dict_map, msgid):
    """Look up a msgid in the dictionary.

    Mirrors the old zh package's ui::t() behavior : ImGui labels ending in
    '###xxx'/'##xxx' (invisible IDs) are matched on the visible part only,
    keeping the suffix in the translation.
    """
    if msgid in dict_map:
        return dict_map[msgid]

    for sep in ("###", "##"):
        pos = msgid.find(sep)
        if pos != -1:
            visible = msgid[:pos]
            suffix = msgid[pos:]
            if visible in dict_map:
                return dict_map[visible] + suffix

    return None


def unescape_c(s):
    """Unescape the C-escape sequences used in .pot / old C++ string literals."""
    out = []
    i = 0
    while i < len(s):
        c = s[i]
        if c == "\\" and i + 1 < len(s):
            n = s[i + 1]
            mapping = {"n": "\n", "t": "\t", "r": "\r", '"': '"', "\\": "\\"}
            if n in mapping:
                out.append(mapping[n])
                i += 2
                continue
        out.append(c)
        i += 1
    return "".join(out)


def escape_po_basic(s):
    """Escape a string for embedding in a .po msgstr (msgfmt-compatible)."""
    return s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n").replace("\r", "\\r").replace("\t", "\\t")


def parse_pot(path):
    """Parse a .pot file into a list of msgid strings (header entry skipped)."""
    entries = []
    cur_lines = None
    in_msgid = False

    def flush():
        nonlocal cur_lines, in_msgid
        if cur_lines is not None:
            text = unescape_c("".join(cur_lines))
            if text.strip() != "":  # Skip the catalog header (empty msgid)
                entries.append(text)
            cur_lines = None
            in_msgid = False

    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if line.startswith("msgid "):
                flush()
                in_msgid = True
                cur_lines = [line[6:].strip('"')]
            elif in_msgid and line.startswith('"') and line.endswith('"'):
                cur_lines.append(line.strip('"'))
            elif in_msgid and line.startswith("msgstr"):
                flush()
            else:
                if in_msgid and not (line.startswith('#') or line.startswith("msgctxt")):
                    flush()
    flush()
    return entries


def parse_old_cpp(path):
    """Extract {'english', 'chinese'} pairs from the old ui_translation.cpp."""
    with open(path, "r", encoding="utf-8-sig") as f:
        text = f.read()

    pairs = []
    for m in re.finditer(r'\{\s*"((?:[^"\\]|\\.)*)"\s*,\s*"((?:[^"\\]|\\.)*)"\s*\}', text):
        en = unescape_c(m.group(1)).strip()
        zh = unescape_c(m.group(2)).strip()
        if en and zh and en != zh:
            pairs.append((en, zh))

    # De-duplicate, keeping the first occurrence
    seen = set()
    deduped = []
    for en, zh in pairs:
        if en not in seen:
            seen.add(en)
            deduped.append((en, zh))
    return deduped


def escape_tsv(s):
    return s.replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n").replace("\r", "\\r")


def unescape_tsv(s):
    out = []
    i = 0
    while i < len(s):
        c = s[i]
        if c == "\\" and i + 1 < len(s):
            n = s[i + 1]
            mapping = {"n": "\n", "t": "\t", "r": "\r", "\\": "\\"}
            if n in mapping:
                out.append(mapping[n])
                i += 2
                continue
        out.append(c)
        i += 1
    return "".join(out)


def load_dict(path):
    pairs = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            en, zh = line.split("\t", 1)
            pairs.append((unescape_tsv(en), unescape_tsv(zh)))
    # Merge duplicates (last wins), keep order
    merged = {}
    for en, zh in pairs:
        merged[en] = zh
    return list(merged.items())


def save_dict(path, pairs):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        for en, zh in pairs:
            assert "\t" not in en and "\t" not in zh, "TSV values must not contain tabs"
            f.write(escape_tsv(en) + "\t" + escape_tsv(zh) + "\n")


def write_mo(path, entries):
    """Write a simple-format GNU .mo file (no hash table)."""
    entries = sorted(entries, key=lambda e: e[0].encode("utf-8"))
    n = len(entries)

    header_size = 28
    orig_table_offset = header_size
    trans_table_offset = orig_table_offset + 8 * n
    total_orig = sum(len(en.encode("utf-8")) + 1 for en, _zh in entries)
    strings_start = trans_table_offset + 8 * n

    orig_blob = b""
    trans_blob = b""
    orig_offsets = []
    trans_offsets = []
    orig_off = strings_start
    trans_off = strings_start + total_orig
    for en, zh in entries:
        en_b = en.encode("utf-8")
        zh_b = zh.encode("utf-8")
        orig_offsets.append(orig_off)
        orig_off += len(en_b) + 1
        trans_offsets.append(trans_off)
        trans_off += len(zh_b) + 1
        orig_blob += en_b + b"\x00"
        trans_blob += zh_b + b"\x00"

    data = bytearray()
    data += struct.pack("<I", 0x950412DE)
    data += struct.pack("<I", 0)
    data += struct.pack("<I", n)
    data += struct.pack("<I", orig_table_offset)
    data += struct.pack("<I", trans_table_offset)
    data += struct.pack("<I", 0)  # Hash table size : 0 = simple format
    data += struct.pack("<I", 0)  # Hash table offset : unused with size 0
    for off in orig_offsets:
        data += struct.pack("<I", len(entries[orig_offsets.index(off)][0].encode("utf-8")) + 1)
        data += struct.pack("<I", off)
    for off in trans_offsets:
        data += struct.pack("<I", len(entries[trans_offsets.index(off)][1].encode("utf-8")) + 1)
        data += struct.pack("<I", off)
    data += orig_blob
    data += trans_blob

    with open(path, "wb") as f:
        f.write(bytes(data))


def write_po(path, entries):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write('msgid ""\n')
        f.write('msgstr ""\n')
        f.write('"Project-Id-Version: SatDump\\n"\n')
        f.write('"Language: zh_CN\\n"\n')
        f.write('"MIME-Version: 1.0\\n"\n')
        f.write('"Content-Type: text/plain; charset=UTF-8\\n"\n')
        f.write('"Content-Transfer-Encoding: 8bit\\n"\n')
        f.write('"Plural-Forms: nplurals=1; plural=0;\\n"\n\n')
        for en, zh in entries:
            f.write('msgid "%s"\n' % escape_po_basic(en))
            f.write('msgstr "%s"\n\n' % escape_po_basic(zh))


def scan_source_strings():
    """Collect every translatable string literal from the source tree (_("..."))."""
    source_dirs = [
        os.path.join(REPO_ROOT, "src-core"),
        os.path.join(REPO_ROOT, "src-interface"),
        os.path.join(REPO_ROOT, "src-cli"),
        os.path.join(REPO_ROOT, "plugins"),
    ]
    pattern = re.compile(r'_\s*\(\s*"((?:[^"\\]|\\.)*)"')
    found = set()
    for src_dir in source_dirs:
        if not os.path.isdir(src_dir):
            continue
        for root, _dirs, files in os.walk(src_dir):
            for name in files:
                if not name.endswith((".cpp", ".h", ".c", ".hpp", ".cxx")):
                    continue
                path = os.path.join(root, name)
                try:
                    with open(path, "r", encoding="utf-8", errors="replace") as f:
                        text = f.read()
                except OSError:
                    continue
                for m in pattern.finditer(text):
                    found.add(unescape_c(m.group(1)))
    return sorted(found)


def main():
    argv = sys.argv[1:]
    old_cpp = None
    if "--import-old-cpp" in argv:
        idx = argv.index("--import-old-cpp")
        old_cpp = argv[idx + 1]
        del argv[idx:idx + 2]

    pot_file = argv[0] if argv else POT_DEFAULT
    dict_file = argv[1] if len(argv) > 1 else DICT_DEFAULT

    if old_cpp:
        pairs = parse_old_cpp(old_cpp)
        save_dict(dict_file, pairs)
        print("Imported %d entries from %s" % (len(pairs), old_cpp))

    # Candidate msgids = catalog (.pot) entries + strings found in the source tree
    strid = set(parse_pot(pot_file))
    strid.update(scan_source_strings())

    dictionary = load_dict(dict_file)
    dict_map = dict(dictionary)
    if os.path.exists(EXTRA_DICT_DEFAULT):
        dict_map.update(load_dict(EXTRA_DICT_DEFAULT))

    translated = []
    matched = 0
    for msgid in sorted(strid):
        zh = translate_msgid(dict_map, msgid)
        if zh is not None:
            translated.append((msgid, zh))
            matched += 1

    print("candidate msgids : %d" % len(strid))
    print("dictionary       : %d entries" % len(dict_map))
    print("matched          : %d translations" % matched)

    os.makedirs(OUT_DIR, exist_ok=True)
    mo_path = os.path.join(OUT_DIR, "satdump.mo")
    po_path = os.path.join(OUT_DIR, "zh_CN.po")
    write_mo(mo_path, translated)
    write_po(po_path, translated)
    print("Wrote %s (%d entries)" % (mo_path, len(translated)))
    print("Wrote %s" % po_path)


if __name__ == "__main__":
    main()
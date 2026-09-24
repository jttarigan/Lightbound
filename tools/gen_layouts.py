#!/usr/bin/env python3
"""Generate src/gfx layouts.gen.h from slangc -reflection-json output.

Usage: gen_layouts.py -o layouts.gen.h spirv.json [metal.json ...]

Every struct reachable from the probe shader's parameters is collected from each JSON
file. If two targets disagree on any size or offset the script fails: that is the
build-time guard for research invariant R1 (identical buffer layouts on both backends).
"""
import argparse
import json
import sys


def uniform_size(t):
    for s in t.get("sizes", []):
        if s.get("kind") == "uniform":
            return s.get("value"), s.get("alignment")
    return None, None


def collect(node, out):
    if not isinstance(node, dict):
        return
    if node.get("kind") == "struct" and "fields" in node:
        size, align = uniform_size(node)
        fields = []
        for f in node["fields"]:
            b = f.get("binding", {})
            fields.append((f["name"], b.get("offset"), b.get("size")))
            collect(f.get("type"), out)
        entry = {"size": size, "align": align, "fields": fields}
        prev = out.get(node["name"])
        if prev is not None and prev != entry:
            raise SystemExit(f"struct {node['name']} reflected twice with different layouts in one file")
        out[node["name"]] = entry
    for key in ("resultType", "elementType", "type"):
        if key in node:
            collect(node[key], out)


def load(path):
    with open(path, "r", encoding="utf-8") as fh:
        doc = json.load(fh)
    out = {}
    for p in doc.get("parameters", []):
        collect(p.get("type"), out)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-o", "--output", required=True)
    ap.add_argument("jsons", nargs="+")
    args = ap.parse_args()

    layouts = [(p, load(p)) for p in args.jsons]
    ref_path, ref = layouts[0]
    if not ref:
        raise SystemExit(f"{ref_path}: no structs reflected")

    # Cross-target check (R1). Struct alignment may legitimately differ (Metal packs
    # float2 at 4 B); sizes and field offsets/sizes must not.
    for path, other in layouts[1:]:
        for name, entry in ref.items():
            if name not in other:
                raise SystemExit(f"{path}: struct {name} missing")
            o = other[name]
            if o["size"] != entry["size"] or o["fields"] != entry["fields"]:
                raise SystemExit(
                    f"LAYOUT MISMATCH for {name}:\n  {ref_path}: {entry}\n  {path}: {o}\n"
                    "Fix shaders/shared/layouts.slang (see layout rules at top of file).")

    lines = []
    w = lines.append
    w("// GENERATED FILE — do not edit. Produced by tools/gen_layouts.py from Slang reflection of")
    w("// shaders/shared/layouts.slang for targets: " + ", ".join(p.rsplit("/", 1)[-1] for p, _ in layouts))
    w("#pragma once")
    w("#include <cstddef>")
    w("")
    w("namespace lb::gfx::slang_layout {")
    w("")
    w("struct FieldInfo { const char* name; std::size_t offset; std::size_t size; };")
    w("")
    for name in sorted(ref):
        e = ref[name]
        w(f"struct {name} {{")
        w(f"    static constexpr std::size_t size = {e['size']};")
        w(f"    static constexpr std::size_t field_count = {len(e['fields'])};")
        for fname, off, sz in e["fields"]:
            w(f"    static constexpr std::size_t {fname}_offset = {off};")
            w(f"    static constexpr std::size_t {fname}_size = {sz};")
        w("    static constexpr FieldInfo fields[] = {")
        for fname, off, sz in e["fields"]:
            w(f'        {{"{fname}", {off}, {sz}}},')
        w("    };")
        w("};")
        w("")
    w("} // namespace lb::gfx::slang_layout")
    w("")
    text = "\n".join(lines)

    try:
        with open(args.output, "r", encoding="utf-8") as fh:
            if fh.read() == text:
                return 0
    except OSError:
        pass
    with open(args.output, "w", encoding="utf-8") as fh:
        fh.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())

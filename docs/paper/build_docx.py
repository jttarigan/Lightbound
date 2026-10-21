"""Build the Journal of Supercomputing Word manuscript from the Markdown drafts in docs/paper/.

  python3 docs/paper/build_docx.py          -> docs/paper/lightbound_jsc.docx (citations from zotero_refs.json)
  python3 docs/paper/build_docx.py --sync   -> first refresh zotero_refs.json from the Zotero web API
                                               (key in ~/.zotero-api-key, never printed), then build

The drafts stay the source; rerun after every change. The section order and numbering come from SECTIONS
below (OUTLINE.md's table); sections without a draft get a placeholder paragraph. Drafting notes
(HTML comments, the H1 line, the H2 blocks named in SECTIONS as dropped) are left out.

Citations: the drafts cite by refs.bib key, "[Beischl26]" or "[Schieffer24, Wahlgren25]". Each group becomes
one live ZOTERO_ITEM field (numbered in order of first citation, the journal's style) and the reference list
one ZOTERO_BIBL field in the Springer basic numeric style, so the Zotero Word plugin can refresh and restyle
them (LAB.md "References: Zotero + Word"). The field format and the table layout follow NPUBenchSwarm's
paper/study3/build_docx.py (same user, ACM template); this file writes a plain Word document instead of
filling a template, since Springer accepts a plain Word manuscript.

Markdown understood: paragraphs, **bold**, *italic*, `code`, "- " and "n. " lists with one level of
nesting, pipe tables (":---:"/"---:" alignment), fenced code blocks, "![..](fig/x.png)" followed by a
"**Fig. n** caption" line, "**Table n** caption" lines above tables, "<!-- -->" comments (dropped).
"§n" becomes "Sect. n" in the text.
"""
import hashlib
import html
import json
import os
import re
import struct
import sys
import urllib.parse
import urllib.request
import zipfile
from datetime import datetime, timezone
from xml.dom import minidom
from xml.sax.saxutils import escape

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "lightbound_jsc.docx")
REFS = os.path.join(HERE, "zotero_refs.json")
ZMAP = os.path.join(HERE, "zotero_map.json")
STYLE = "springer-basic-brackets"          # Zotero style id; the journal's "Springer Basic" numeric style
ZOTERO_VERSION = "10.0.6"

TITLE = ("When does shared CPU–GPU memory pay off? Fine-grained round trips on an Apple M5 and PCIe GPUs, "
         "measured in a real-time agent simulation")
AUTHOR = "Jos Timanta Tarigan"
AFFILIATION = ("Faculty of Computer Science and Information Technology, Universitas Sumatera Utara, Medan, "
               "Indonesia")                    # from the user's NPUBenchSwarm manuscript; title page = user's decision
CONTACT = "Corresponding author: [e-mail], [telephone]"

# (number, heading, draft file, what to take from the file)
#   None              -> whole body (a file without H2 blocks)
#   ["H2 title", ..]  -> those H2 blocks, bodies only, in that order
#   "subsections"     -> every H2 block becomes a numbered subsection ("4.1 ..." as in the file)
#   a string starting with "[" -> no draft yet: that placeholder paragraph
SECTIONS = [
    (1, "Introduction", "introduction.md", None),
    (2, "Background and related work", "related_work.md", ["What they establish, and the gap we fill"]),
    (3, "Platforms", "platforms_and_m1_results.md", ["§3 Platforms"]),
    (4, "Microbenchmark method", "methods_microbench.md", "subsections"),
    (5, "Results with the GPU idle: the refuted hypothesis", "platforms_and_m1_results.md",
     ["§5 Results with the GPU idle: H1 is refuted"]),
    (6, "Payload crossover and GPU state", None,
     "[Written after the extension series on both platforms (H1′, predictions of 2026-10-13 in the protocol).]"),
    (7, "Simulation workload and pipeline", "game_pipeline.md", "subsections"),
    (8, "Step-time results", None, "[Written after the M4 and M8 run matrices (H2, H3, H5).]"),
    (9, "Cost model", "cost_model.md", "subsections"),
    (10, "Decision staleness", None, "[Written after the M3 oracle and the M8 audit runs (H4).]"),
    (11, "Energy", None, "[Written after M8 (H6, secondary).]"),
    (12, "Threats to validity", "threats_to_validity.md", "subsections"),
    (13, "Conclusion", None, "[Written last.]"),
]
DECLARATIONS = [
    ("Funding", "[None / to state.]"),
    ("Competing interests", "[The author declares no competing interests. / to state]"),
    ("Data and code availability", "The benchmark, the simulation, the analysis scripts and the measured "
     "series are in the public repository https://github.com/jttarigan/Lightbound [archived DOI: to decide]."),
    ("Use of AI tools", "[Wording: the author's decision. Springer Nature asks for AI use beyond copy-editing "
     "to be declared in the Methods section; this draft was written with an AI assistant.]"),
]

FIG_DIR = os.path.join(HERE, "fig")
PAGE_W, PAGE_H, MARGIN = 11906, 16838, 1440          # A4, 2.54 cm margins
TEXT_WIDTH = PAGE_W - 2 * MARGIN                      # dxa
CHAR_DXA = 100                                        # rough width of one character of table text
CSL_SCHEMA = "https://github.com/citation-style-language/schema/raw/master/csl-citation.json"
NS = ('xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main" '
      'xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships" '
      'xmlns:wp="http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing" '
      'xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" '
      'xmlns:pic="http://schemas.openxmlformats.org/drawingml/2006/picture"')

# ------------------------------------------------------------------ Zotero

ZOTERO = None       # zotero_refs.json
CITES = {}          # refs.bib key -> citation number, in order of first citation
FIELDS = []         # (field code, visible text) per citation group
UNRESOLVED = set()
PLACEHOLDER = re.compile("(\\d+)")
CITE_RE = re.compile(r"\[((?:[A-Z][A-Za-z]+\d{2}[a-z]?)(?:,\s*[A-Z][A-Za-z]+\d{2}[a-z]?)*)\]")


def sync_refs():
    """zotero_refs.json from the web API: CSL-JSON per item and its Springer-style bibliography entry."""
    zmap = json.load(open(ZMAP, encoding="utf-8"))
    keys = {bib: zk[0] for bib, zk in zmap["keys"].items()}
    user = zmap["zotero_user"]
    api_key = open(os.path.expanduser("~/.zotero-api-key")).read().strip()

    def get(params):
        url = f"https://api.zotero.org/users/{user}/items?" + urllib.parse.urlencode(params)
        req = urllib.request.Request(url, headers={"Zotero-API-Key": api_key, "Zotero-API-Version": "3"})
        with urllib.request.urlopen(req, timeout=60) as r:
            return r.read().decode("utf-8")

    items, bib = {}, {}
    zkeys = list(keys.values())
    for chunk in (zkeys[i:i + 25] for i in range(0, len(zkeys), 25)):
        data = json.loads(get({"itemKey": ",".join(chunk), "format": "csljson"}))
        for it in data["items"] if isinstance(data, dict) else data:
            k = it["id"].split("/")[-1]
            it["id"] = k
            items[k] = it
    for k in zkeys:
        page = get({"itemKey": k, "format": "bib", "style": STYLE, "linkwrap": "0"})
        m = re.search(r'csl-right-inline"[^>]*>(.*?)</div>', page, re.S)
        if not m:
            raise SystemExit(f"no bibliography entry for {k}")
        bib[k] = " ".join(m.group(1).split())
    missing = [b for b, k in keys.items() if k not in items]
    if missing:
        raise SystemExit(f"not in the Zotero library: {missing}")
    out = {"zotero_user": user, "style": STYLE, "synced": datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M UTC"),
           "keys": keys, "items": items, "bib": bib}
    with open(REFS, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=1, ensure_ascii=False)
        f.write("\n")
    print(f"{len(items)} items -> {os.path.relpath(REFS)}")


def collapse(nums):
    """CSL collapse="citation-number": runs of three or more become a range."""
    out, i = [], 0
    while i < len(nums):
        j = i
        while j + 1 < len(nums) and nums[j + 1] == nums[j] + 1:
            j += 1
        out += [f"{nums[i]}–{nums[j]}"] if j - i >= 2 else [str(n) for n in nums[i:j + 1]]
        i = j + 1
    return ", ".join(out)


def zotero_field(bibkeys):
    """Placeholder for one ZOTERO_ITEM field over refs.bib keys."""
    for b in bibkeys:
        CITES.setdefault(b, len(CITES) + 1)
    bibkeys = sorted(set(bibkeys), key=lambda b: CITES[b])
    text = "[" + collapse([CITES[b] for b in bibkeys]) + "]"
    cid = hashlib.sha1(("|".join(bibkeys) + str(len(FIELDS))).encode()).hexdigest()[:8]
    user = ZOTERO["zotero_user"]
    items = []
    for b in bibkeys:
        k = ZOTERO["keys"][b]
        items.append({"id": k, "uris": [f"http://zotero.org/users/{user}/items/{k}"],
                      "itemData": dict(ZOTERO["items"][k], id=k)})
    code = {"citationID": cid, "properties": {"formattedCitation": text, "plainCitation": text, "noteIndex": 0},
            "citationItems": items, "schema": CSL_SCHEMA}
    FIELDS.append((json.dumps(code, ensure_ascii=False, separators=(",", ":")), text))
    return f"{len(FIELDS) - 1}"


def field_xml(instr, result):
    return ('<w:r><w:fldChar w:fldCharType="begin"/></w:r>'
            f'<w:r><w:instrText xml:space="preserve"> {escape(instr)} </w:instrText></w:r>'
            '<w:r><w:fldChar w:fldCharType="separate"/></w:r>'
            f'<w:r><w:t xml:space="preserve">{escape(result)}</w:t></w:r>'
            '<w:r><w:fldChar w:fldCharType="end"/></w:r>')


def cite(text):
    def group(m):
        keys = [k.strip() for k in m.group(1).split(",")]
        unknown = [k for k in keys if k not in ZOTERO["keys"]]
        if unknown:
            UNRESOLVED.update(unknown)
            return m.group(0)
        return zotero_field(keys)
    return CITE_RE.sub(group, text)

# ------------------------------------------------------------------ inline runs

INLINE = re.compile(r"(\*\*.+?\*\*|`[^`]+`|(?<![\w*\\])\*(?!\s)[^*\n]+?(?<!\s)\*(?![\w*]))")


def fix_text(text):
    text = re.sub(r"§(\d)", r"Sect. \1", text)
    return text.replace("\\*", "*")


def run(text, bold=False, italic=False, style=None):
    rpr = ""
    if style or bold or italic:
        rpr = ("<w:rPr>" + (f'<w:rStyle w:val="{style}"/>' if style else "") + ("<w:b/><w:bCs/>" if bold else "")
               + ("<w:i/><w:iCs/>" if italic else "") + "</w:rPr>")
    return f'<w:r>{rpr}<w:t xml:space="preserve">{escape(text)}</w:t></w:r>'


def runs(text, bold=False, italic=False):
    out = []
    for part in INLINE.split(cite(text)):
        if not part:
            continue
        if part.startswith("**") and part.endswith("**") and len(part) > 4:
            out.append(run(fix_text(part[2:-2]), bold=True, italic=italic))
        elif part.startswith("`") and part.endswith("`"):
            out.append(run(part[1:-1], style="CodeChar", bold=bold))
        elif part.startswith("*") and part.endswith("*") and len(part) > 2:
            out.append(run(fix_text(part[1:-1]), bold=bold, italic=True))
        else:
            for i, piece in enumerate(PLACEHOLDER.split(part)):
                if i % 2:
                    out.append(field_xml("ADDIN ZOTERO_ITEM CSL_CITATION " + FIELDS[int(piece)][0], FIELDS[int(piece)][1]))
                elif piece:
                    out.append(run(fix_text(piece), bold=bold, italic=italic))
    return "".join(out)


def para(style, text="", inner=None, extra_ppr=""):
    body = inner if inner is not None else runs(text)
    return f'<w:p><w:pPr><w:pStyle w:val="{style}"/>{extra_ppr}</w:pPr>{body}</w:p>'

# ------------------------------------------------------------------ blocks

NUMS = []       # numbered-list instances (each restarts at 1); numId = 2 + index; bullets use numId 1
FIGS = []       # (rId, file name) for document.xml.rels


def list_para(text, level, num_id):
    numpr = f'<w:numPr><w:ilvl w:val="{level}"/><w:numId w:val="{num_id}"/></w:numPr>'
    return para("ListParagraph", text, extra_ppr=numpr)


def table_xml(rows, align):
    ncol = max(len(r) for r in rows)
    rows = [r + [""] * (ncol - len(r)) for r in rows]
    col = [[re.sub(r"[`*]", "", r[c]) for r in rows] for c in range(ncol)]
    # every column fits its longest word; the rest of the width follows text length
    token = [max(len(w) for cell in cells for w in (cell.split() or [""])) + 1 for cells in col]
    full = [min(max(len(cell) for cell in cells), 45) for cells in col]
    floor = [t * CHAR_DXA for t in token]
    nowrap = max(len(cell) for cell in col[0][1:]) <= 16 if len(rows) > 1 else False
    if nowrap:
        floor[0] = (max(len(cell) for cell in col[0][1:]) + 2) * CHAR_DXA
        token[0] = full[0]
    spare = max(0, TEXT_WIDTH - sum(floor))
    weight = [max(f - t, 0) + 1 for f, t in zip(full, token)]
    widths = [int(fl + spare * w / sum(weight)) for fl, w in zip(floor, weight)]
    widths[-1] = TEXT_WIDTH - sum(widths[:-1])
    line = '<w:{0} w:val="single" w:sz="4" w:space="0" w:color="auto"/>'
    out = ['<w:tbl><w:tblPr><w:tblStyle w:val="TableGrid"/>'
           f'<w:tblW w:w="{TEXT_WIDTH}" w:type="dxa"/><w:tblBorders>'
           + "".join(f'<w:{b} w:val="none" w:sz="0" w:space="0" w:color="auto"/>'
                     for b in ("top", "left", "bottom", "right", "insideH", "insideV"))
           + '</w:tblBorders><w:tblLayout w:type="fixed"/><w:tblLook w:val="04A0" w:firstRow="1" '
           'w:lastRow="0" w:firstColumn="1" w:lastColumn="0" w:noHBand="0" w:noVBand="1"/></w:tblPr>'
           "<w:tblGrid>" + "".join(f'<w:gridCol w:w="{w}"/>' for w in widths) + "</w:tblGrid>"]
    for i, r in enumerate(rows):
        head, last = i == 0, i == len(rows) - 1
        out.append("<w:tr>" + ("<w:trPr><w:cantSplit/><w:tblHeader/></w:trPr>" if head else "<w:trPr><w:cantSplit/></w:trPr>"))
        for c in range(ncol):
            borders = ""
            if head:
                borders = line.format("top") + line.format("bottom")
            elif last:
                borders = line.format("bottom")
            cell = r[c]
            if nowrap and c == 0 and not head:
                cell = cell.replace(" ", " ")
            jc = align[c] if c < len(align) and align[c] else "left"
            out.append(f'<w:tc><w:tcPr><w:tcW w:w="{widths[c]}" w:type="dxa"/>'
                       f"<w:tcBorders>{borders}</w:tcBorders></w:tcPr>"
                       + para("TableCell", inner=runs(cell, bold=head), extra_ppr=f'<w:jc w:val="{jc}"/>')
                       + "</w:tc>")
        out.append("</w:tr>")
    out.append("</w:tbl>")
    out.append(para("Spacer"))      # Word wants a paragraph after a table
    return "".join(out)


def png_size(path):
    with open(path, "rb") as f:
        head = f.read(24)
    return struct.unpack(">II", head[16:24])


def image_xml(path):
    n = len(FIGS) + 1
    rid = f"rIdFig{n}"
    FIGS.append((rid, path))
    w, h = png_size(path)
    cx = int(TEXT_WIDTH / 1440 * 914400)
    cy = int(cx * h / w)
    drawing = (
        f'<w:r><w:drawing><wp:inline distT="0" distB="0" distL="0" distR="0">'
        f'<wp:extent cx="{cx}" cy="{cy}"/><wp:docPr id="{100 + n}" name="Figure {n}"/>'
        '<wp:cNvGraphicFramePr><a:graphicFrameLocks noChangeAspect="1"/></wp:cNvGraphicFramePr>'
        '<a:graphic><a:graphicData uri="http://schemas.openxmlformats.org/drawingml/2006/picture">'
        f'<pic:pic><pic:nvPicPr><pic:cNvPr id="{100 + n}" name="{os.path.basename(path)}"/><pic:cNvPicPr/></pic:nvPicPr>'
        f'<pic:blipFill><a:blip r:embed="{rid}"/><a:stretch><a:fillRect/></a:stretch></pic:blipFill>'
        f'<pic:spPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="{cx}" cy="{cy}"/></a:xfrm>'
        '<a:prstGeom prst="rect"><a:avLst/></a:prstGeom></pic:spPr></pic:pic>'
        "</a:graphicData></a:graphic></wp:inline></w:drawing></w:r>")
    return para("Figure", inner=drawing, extra_ppr="<w:keepNext/>")


def caption_para(text, style):
    """'**Fig. 1** *caption*' / '**Table 2** caption': bold label, plain text."""
    m = re.match(r"\*\*((?:Fig|Table)\.? \d+)\*\*\s*(.*)$", text, re.S)
    label, rest = m.group(1), m.group(2).strip()
    if rest.startswith("*") and rest.endswith("*"):
        rest = rest[1:-1]
    return para(style, inner=run(label + " ", bold=True) + runs(rest), extra_ppr="<w:keepNext/>" if style == "Caption" else "")


LIST_RE = re.compile(r"^(\s*)(- |\d+\. )(.*)$")


def blocks_xml(lines):
    out, i = [], 0
    while i < len(lines):
        line = lines[i]
        if not line.strip():
            i += 1
            continue
        if line.startswith("```"):
            i += 1
            while i < len(lines) and not lines[i].startswith("```"):
                out.append(para("Code", inner=run(lines[i].replace("\\*", "*"))))
                i += 1
            i += 1
            continue
        m = re.match(r"^!\[[^\]]*\]\(([^)]+)\)\s*$", line)
        if m:
            out.append(image_xml(os.path.join(HERE, m.group(1))))
            i += 1
            continue
        if line.startswith("|"):
            rows, align = [], []
            while i < len(lines) and lines[i].startswith("|"):
                cells = [c.strip() for c in lines[i].strip().strip("|").split("|")]
                if all(re.fullmatch(r":?-{3,}:?", c) for c in cells):
                    align = ["center" if c.startswith(":") and c.endswith(":") else "right" if c.endswith(":") else ""
                             for c in cells]
                else:
                    rows.append(cells)
                i += 1
            out.append(table_xml(rows, align))
            continue
        m = LIST_RE.match(line)
        if m:
            num_id = 1
            if m.group(2) != "- ":
                NUMS.append(len(NUMS))
                num_id = 2 + NUMS[-1]
            while i < len(lines):
                m = LIST_RE.match(lines[i])
                if m:
                    level = 1 if len(m.group(1)) >= 2 else 0
                    item = [m.group(3)]
                    i += 1
                    while i < len(lines) and lines[i].strip() and not LIST_RE.match(lines[i]):
                        item.append(lines[i].strip())
                        i += 1
                    out.append(list_para(" ".join(item), level, 1 if m.group(2) == "- " else num_id))
                    continue
                if not lines[i].strip():
                    # blank lines inside a list: the next item continues it ("loose" list), and an
                    # indented paragraph belongs to the item above
                    j = i
                    while j < len(lines) and not lines[j].strip():
                        j += 1
                    if j < len(lines) and LIST_RE.match(lines[j]):
                        i = j
                        continue
                    if j < len(lines) and lines[j].startswith("  ") and not LIST_RE.match(lines[j]):
                        chunk = []
                        while j < len(lines) and lines[j].strip() and not LIST_RE.match(lines[j]):
                            chunk.append(lines[j].strip())
                            j += 1
                        out.append(para("Normal", " ".join(chunk), extra_ppr='<w:ind w:left="720"/>'))
                        i = j
                        continue
                    break
                break
            continue
        chunk = []
        while i < len(lines) and lines[i].strip() and not lines[i].startswith(("|", "```", "![")) \
                and not LIST_RE.match(lines[i]):
            chunk.append(lines[i].strip())
            i += 1
        text = " ".join(chunk)
        if re.match(r"\*\*Fig\.? \d+\*\*", text):
            out.append(caption_para(text, "FigureCaption"))
        elif re.match(r"\*\*Table \d+\*\*", text):
            out.append(caption_para(text, "Caption"))
        else:
            out.append(para("Normal", text))
    return out

# ------------------------------------------------------------------ drafts


def load_draft(name):
    """(preamble lines, [(H2 title, lines)]) of a draft without its comments and H1 line."""
    md = open(os.path.join(HERE, name), encoding="utf-8").read()
    md = re.sub(r"<!--.*?-->", "", md, flags=re.S)
    pre, blocks, cur = [], [], None
    for line in md.splitlines():
        if line.startswith("# "):
            continue
        m = re.match(r"^## (.*)$", line)
        if m:
            cur = (m.group(1).strip(), [])
            blocks.append(cur)
        elif cur is None:
            pre.append(line)
        else:
            cur[1].append(line)
    return pre, blocks


def abstract_block():
    pre, _ = load_draft("abstract.md")
    text = "\n".join(pre)
    m = re.search(r"\*\*Keywords\*\*[^:]*:\s*(.*)", text, re.S)
    keywords = " ".join(m.group(1).split()) if m else ""
    body = text[:m.start()] if m else text
    paras = [" ".join(p.split()) for p in re.split(r"\n\s*\n", body) if p.strip()]
    out = [para("Heading1", "Abstract", extra_ppr='<w:keepNext/>')]
    out += [para("Abstract", p) for p in paras]
    out.append(para("Keywords", inner=run("Keywords ", bold=True) + runs(keywords)))
    return out


def body_xml():
    xml = [para("Title", TITLE), para("Author", AUTHOR), para("Affiliation", AFFILIATION),
           para("Affiliation", CONTACT)]
    xml += abstract_block()
    for num, heading, fname, pick in SECTIONS:
        xml.append(para("Heading1", f"{num} {heading}", extra_ppr="<w:keepNext/>"))
        if fname is None:
            xml.append(para("Normal", inner=run(pick, italic=True)))
            continue
        pre, blocks = load_draft(fname)
        if pick is None:
            xml += blocks_xml(pre + [l for _, ls in blocks for l in ls])
        elif pick == "subsections":
            xml += blocks_xml(pre)
            for title, lines in blocks:
                title = re.sub(r"\s*\((?:draft|outline|plan|to write|checked)[^)]*\)\s*$", "", title)
                xml.append(para("Heading2", fix_text(title), extra_ppr="<w:keepNext/>"))
                xml += blocks_xml(lines)
        else:
            titles = {t: ls for t, ls in blocks}
            for t in pick:
                if t not in titles:
                    raise SystemExit(f"{fname}: no H2 block {t!r}; has {list(titles)}")
                xml += blocks_xml(titles[t])
    xml.append(para("Heading1", "Declarations", extra_ppr="<w:keepNext/>"))
    for head, text in DECLARATIONS:
        xml.append(para("Normal", inner=run(head + ". ", bold=True) + runs(text)))
    xml.append(para("Heading1", "References", extra_ppr="<w:keepNext/>"))
    xml += zotero_bibliography()
    return "".join(xml)


def zotero_bibliography():
    """One ZOTERO_BIBL field over all entry paragraphs, in citation order, text as Zotero renders it."""
    order = sorted(CITES, key=CITES.get)
    out = []
    for i, b in enumerate(order):
        body = run(f"{CITES[b]}.") + "<w:r><w:tab/></w:r>"
        for part in re.split(r"(<i>.*?</i>)", ZOTERO["bib"][ZOTERO["keys"][b]], flags=re.S):
            text = html.unescape(re.sub(r"<[^>]+>", "", part))
            if text:
                body += run(text, italic=part.startswith("<i>"))
        if i == 0:
            body = ('<w:r><w:fldChar w:fldCharType="begin"/></w:r><w:r><w:instrText xml:space="preserve">'
                    ' ADDIN ZOTERO_BIBL {"uncited":[],"omitted":[],"custom":[]} CSL_BIBLIOGRAPHY </w:instrText></w:r>'
                    '<w:r><w:fldChar w:fldCharType="separate"/></w:r>') + body
        if i == len(order) - 1:
            body += '<w:r><w:fldChar w:fldCharType="end"/></w:r>'
        out.append(para("Bibliography", inner=body))
    return out

# ------------------------------------------------------------------ package parts


def style(sid, name, kind="paragraph", based="Normal", next_="Normal", ppr="", rpr="", default=False):
    return (f'<w:style w:type="{kind}" w:styleId="{sid}"' + (' w:default="1"' if default else "") + ">"
            f'<w:name w:val="{name}"/>' + (f'<w:basedOn w:val="{based}"/>' if based and not default else "")
            + (f'<w:next w:val="{next_}"/>' if kind == "paragraph" and next_ and not default else "")
            + '<w:qFormat/>' + (f"<w:pPr>{ppr}</w:pPr>" if ppr else "") + (f"<w:rPr>{rpr}</w:rPr>" if rpr else "")
            + "</w:style>")


def styles_xml():
    font = lambda f: f'<w:rFonts w:ascii="{f}" w:hAnsi="{f}" w:cs="{f}" w:eastAsia="{f}"/>'
    size = lambda pt: f'<w:sz w:val="{int(pt * 2)}"/><w:szCs w:val="{int(pt * 2)}"/>'
    sp = lambda before=0, after=120, line=240: f'<w:spacing w:before="{before}" w:after="{after}" w:line="{line}" w:lineRule="auto"/>'
    # element order inside pPr (keepNext, keepLines, spacing, ind, contextualSpacing, jc, outlineLvl) and
    # rPr (rFonts, b, i, sz) follows the schema; Word reports out-of-order parts as unreadable
    keep = "<w:keepNext/><w:keepLines/>"
    s = [style("Normal", "Normal", default=True, ppr=sp(), rpr=font("Times New Roman") + size(11)),
         style("Title", "Title", ppr="<w:keepNext/>" + sp(0, 240), rpr="<w:b/><w:bCs/>" + size(16)),
         style("Author", "Author", ppr=sp(0, 60), rpr=size(12)),
         style("Affiliation", "Affiliation", ppr=sp(0, 60), rpr="<w:i/><w:iCs/>" + size(10)),
         style("Abstract", "Abstract", ppr=sp(0, 120) + '<w:ind w:left="567" w:right="567"/>', rpr=size(10.5)),
         style("Keywords", "Keywords", ppr=sp(0, 240) + '<w:ind w:left="567" w:right="567"/>', rpr=size(10.5)),
         style("Heading1", "heading 1", ppr=keep + sp(360, 120) + '<w:outlineLvl w:val="0"/>',
               rpr="<w:b/><w:bCs/>" + size(13)),
         style("Heading2", "heading 2", ppr=keep + sp(240, 120) + '<w:outlineLvl w:val="1"/>',
               rpr="<w:b/><w:bCs/>" + size(11.5)),
         style("Heading3", "heading 3", ppr=keep + sp(200, 80) + '<w:outlineLvl w:val="2"/>',
               rpr="<w:b/><w:bCs/><w:i/><w:iCs/>" + size(11)),
         style("ListParagraph", "List Paragraph", ppr=sp(0, 60) + '<w:contextualSpacing/>'),
         style("Caption", "caption", ppr="<w:keepNext/>" + sp(120, 60), rpr=size(10)),
         style("FigureCaption", "Figure Caption", ppr=sp(60, 240), rpr=size(10)),
         style("Figure", "Figure", ppr="<w:keepNext/>" + sp(120, 0) + '<w:jc w:val="center"/>'),
         style("TableCell", "Table Cell", ppr=sp(20, 20), rpr=size(9.5)),
         style("Spacer", "Spacer", ppr=sp(0, 0), rpr=size(4)),
         style("Code", "Code", ppr=sp(0, 0) + '<w:ind w:left="567"/><w:contextualSpacing/>',
               rpr=font("Courier New") + size(9.5)),
         style("Bibliography", "Bibliography", ppr=sp(0, 60) + '<w:ind w:left="360" w:hanging="360"/>', rpr=size(10)),
         style("CodeChar", "Code Char", kind="character", based=None, rpr=font("Courier New") + size(10))]
    return ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
            f'<w:styles {NS}><w:docDefaults><w:rPrDefault><w:rPr>{font("Times New Roman")}{size(11)}'
            '<w:lang w:val="en-GB"/></w:rPr></w:rPrDefault><w:pPrDefault><w:pPr>' + sp() + '</w:pPr></w:pPrDefault>'
            '</w:docDefaults>' + "".join(s) + "</w:styles>")


def numbering_xml():
    lvl = lambda i, fmt, text, left: (f'<w:lvl w:ilvl="{i}"><w:start w:val="1"/><w:numFmt w:val="{fmt}"/>'
                                      f'<w:lvlText w:val="{text}"/><w:lvlJc w:val="left"/>'
                                      f'<w:pPr><w:ind w:left="{left}" w:hanging="360"/></w:pPr></w:lvl>')
    bullets = ('<w:abstractNum w:abstractNumId="0"><w:multiLevelType w:val="hybridMultilevel"/>'
               + lvl(0, "bullet", "•", 720) + lvl(1, "bullet", "–", 1440) + "</w:abstractNum>")
    decimal = ('<w:abstractNum w:abstractNumId="1"><w:multiLevelType w:val="hybridMultilevel"/>'
               + lvl(0, "decimal", "%1.", 720) + lvl(1, "bullet", "–", 1440) + "</w:abstractNum>")
    nums = '<w:num w:numId="1"><w:abstractNumId w:val="0"/></w:num>'
    for n in NUMS:
        nums += (f'<w:num w:numId="{2 + n}"><w:abstractNumId w:val="1"/>'
                 '<w:lvlOverride w:ilvl="0"><w:startOverride w:val="1"/></w:lvlOverride></w:num>')
    return ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
            f'<w:numbering {NS}>{bullets}{decimal}{nums}</w:numbering>')


def footer_xml():
    return ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
            f'<w:ftr {NS}><w:p><w:pPr><w:jc w:val="center"/></w:pPr>'
            + field_xml("PAGE", "1") + "</w:p></w:ftr>")


def document_xml(body):
    sect = (f'<w:sectPr><w:footerReference w:type="default" r:id="rIdFooter1"/>'
            f'<w:pgSz w:w="{PAGE_W}" w:h="{PAGE_H}"/>'
            f'<w:pgMar w:top="{MARGIN}" w:right="{MARGIN}" w:bottom="{MARGIN}" w:left="{MARGIN}" '
            'w:header="708" w:footer="708" w:gutter="0"/><w:cols w:space="708"/></w:sectPr>')
    return ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
            f'<w:document {NS}><w:body>{body}{sect}</w:body></w:document>')


def zotero_custom_xml():
    """Zotero's document preferences (Word keeps them in custom document properties)."""
    sid = hashlib.sha1(TITLE.encode()).hexdigest()[:8]
    prefs = (f'<data data-version="3" zotero-version="{ZOTERO_VERSION}"><session id="{sid}"/>'
             f'<style id="http://www.zotero.org/styles/{STYLE}" locale="en-US" hasBibliography="1" '
             'bibliographyStyleHasBeenSet="1"/><prefs><pref name="fieldType" value="Field"/>'
             '<pref name="automaticJournalAbbreviations" value="true"/></prefs></data>')
    chunks = [prefs[i:i + 255] for i in range(0, len(prefs), 255)]
    props = "".join(f'<property fmtid="{{D5CDD505-2E9C-101B-9397-08002B2CF9AE}}" pid="{n + 2}" '
                    f'name="ZOTERO_PREF_{n + 1}"><vt:lpwstr>{escape(c)}</vt:lpwstr></property>'
                    for n, c in enumerate(chunks))
    return ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<Properties '
            'xmlns="http://schemas.openxmlformats.org/officeDocument/2006/custom-properties" '
            'xmlns:vt="http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes">' + props + '</Properties>')


CONTENT_TYPES = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
                 '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
                 '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
                 '<Default Extension="xml" ContentType="application/xml"/>'
                 '<Default Extension="png" ContentType="image/png"/>'
                 '<Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>'
                 '<Override PartName="/word/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml"/>'
                 '<Override PartName="/word/numbering.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.numbering+xml"/>'
                 '<Override PartName="/word/settings.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.settings+xml"/>'
                 '<Override PartName="/word/footer1.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.footer+xml"/>'
                 '<Override PartName="/docProps/core.xml" ContentType="application/vnd.openxmlformats-package.core-properties+xml"/>'
                 '<Override PartName="/docProps/app.xml" ContentType="application/vnd.openxmlformats-officedocument.extended-properties+xml"/>'
                 '<Override PartName="/docProps/custom.xml" ContentType="application/vnd.openxmlformats-officedocument.custom-properties+xml"/>'
                 '</Types>')
ROOT_RELS = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
             '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
             '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/>'
             '<Relationship Id="rId2" Type="http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties" Target="docProps/core.xml"/>'
             '<Relationship Id="rId3" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties" Target="docProps/app.xml"/>'
             '<Relationship Id="rId4" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/custom-properties" Target="docProps/custom.xml"/>'
             '</Relationships>')
SETTINGS = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
            f'<w:settings {NS}><w:defaultTabStop w:val="720"/><w:characterSpacingControl w:val="doNotCompress"/>'
            '<w:compat><w:compatSetting w:name="compatibilityMode" w:uri="http://schemas.microsoft.com/office/word" w:val="15"/></w:compat>'
            '</w:settings>')
APP = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
       '<Properties xmlns="http://schemas.openxmlformats.org/officeDocument/2006/extended-properties" '
       'xmlns:vt="http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes"><Application>Microsoft Office Word</Application></Properties>')


def core_xml():
    now = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    return ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
            '<cp:coreProperties xmlns:cp="http://schemas.openxmlformats.org/package/2006/metadata/core-properties" '
            'xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:dcterms="http://purl.org/dc/terms/" '
            'xmlns:dcmitype="http://purl.org/dc/dcmitype/" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">'
            f'<dc:title>{escape(TITLE)}</dc:title><dc:creator>{escape(AUTHOR)}</dc:creator>'
            f'<dcterms:created xsi:type="dcterms:W3CDTF">{now}</dcterms:created>'
            f'<dcterms:modified xsi:type="dcterms:W3CDTF">{now}</dcterms:modified></cp:coreProperties>')


def doc_rels():
    rels = ['<Relationship Id="rIdStyles" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/>',
            '<Relationship Id="rIdNumbering" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/numbering" Target="numbering.xml"/>',
            '<Relationship Id="rIdSettings" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/settings" Target="settings.xml"/>',
            '<Relationship Id="rIdFooter1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/footer" Target="footer1.xml"/>']
    rels += [f'<Relationship Id="{rid}" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/image" '
             f'Target="media/{os.path.basename(path)}"/>' for rid, path in FIGS]
    return ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
            '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
            + "".join(rels) + "</Relationships>")


def main():
    global ZOTERO
    if "--sync" in sys.argv:
        sync_refs()
    ZOTERO = json.load(open(REFS, encoding="utf-8"))
    body = body_xml()
    parts = {"[Content_Types].xml": CONTENT_TYPES, "_rels/.rels": ROOT_RELS, "word/document.xml": document_xml(body),
             "word/_rels/document.xml.rels": doc_rels(), "word/styles.xml": styles_xml(),
             "word/numbering.xml": numbering_xml(), "word/settings.xml": SETTINGS, "word/footer1.xml": footer_xml(),
             "docProps/core.xml": core_xml(), "docProps/app.xml": APP, "docProps/custom.xml": zotero_custom_xml()}
    for name, xml in parts.items():
        minidom.parseString(xml.encode("utf-8"))       # well-formedness check before anything is written
    with zipfile.ZipFile(OUT, "w", zipfile.ZIP_DEFLATED) as z:
        for name, xml in parts.items():
            z.writestr(name, xml)
        for _, path in FIGS:
            z.write(path, f"word/media/{os.path.basename(path)}")
    uncited = sorted(set(ZOTERO["keys"]) - set(CITES))
    print(f"wrote {os.path.relpath(OUT)}: {len(FIELDS)} citation fields, {len(CITES)} references, {len(FIGS)} figure(s)")
    if uncited:
        print("in Zotero but not cited (left out of the reference list):", ", ".join(uncited))
    if UNRESOLVED:
        print("cited but not in zotero_map.json:", sorted(UNRESOLVED))


if __name__ == "__main__":
    main()

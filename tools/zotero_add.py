"""Put the paper's references (docs/paper/refs.bib) into the user's Zotero collection "Lightbound"
through the zotero.org web API, and write docs/paper/zotero_map.json (bib key -> Zotero item key)
for the Word build.

  python3 tools/zotero_add.py --dry-run     (reads only; prints what it would do)
  python3 tools/zotero_add.py

LAB.md "References: Zotero + Word": web API with an explicit collection key (never the connector,
which saves into whatever collection the Zotero window shows); an item that is already in the
library (same DOI, else same arXiv id) is reused by adding it to the collection, never duplicated.
The API key is read from ~/.zotero-api-key and never printed or written anywhere.
Rerunning is safe: items already in the collection are left alone.
"""
import json
import os
import re
import secrets
import sys
import urllib.error
import urllib.request

USER = 5591478
COLLECTION = "Lightbound"
API = f"https://api.zotero.org/users/{USER}"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIB = os.path.join(ROOT, "docs", "paper", "refs.bib")
MAP = os.path.join(ROOT, "docs", "paper", "zotero_map.json")
MONTHS = {m: i + 1 for i, m in enumerate(
    ["jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"])}

with open(os.path.expanduser("~/.zotero-api-key")) as f:
    KEY = f.read().strip()


def call(method, path, body=None, headers=None):
    """(status, response headers, parsed JSON) for one web API request."""
    h = {"Zotero-API-Key": KEY, "Zotero-API-Version": "3"}
    if body is not None:
        h["Content-Type"] = "application/json"
    h.update(headers or {})
    req = urllib.request.Request(API + path, data=None if body is None else json.dumps(body).encode(),
                                 headers=h, method=method)
    try:
        with urllib.request.urlopen(req, timeout=60) as r:
            text = r.read().decode()
            return r.status, r.headers, json.loads(text) if text.strip() else None
    except urllib.error.HTTPError as e:
        sys.exit(f"{method} {path}: HTTP {e.code}: {e.read().decode()[:300]}")


def get_all(path):
    out, start = [], 0
    while True:
        sep = "&" if "?" in path else "?"
        _, hdr, page = call("GET", f"{path}{sep}limit=100&start={start}")
        out += page
        start += len(page)
        if not page or start >= int(hdr.get("Total-Results", 0)):
            return out


def parse_bib(text):
    """{key: (entry type, {field: value})}; values keep their inner braces stripped."""
    entries = {}
    for m in re.finditer(r"@(\w+)\{([^,]+),", text):
        i, depth = m.end(), 1
        while depth:
            depth += {"{": 1, "}": -1}.get(text[i], 0)
            i += 1
        body, fields, j = text[m.end():i - 1], {}, 0
        for f in re.finditer(r"(\w+)\s*=\s*", body):
            if f.start() < j:
                continue
            j = f.end()
            if body[j] == "{":
                depth, k = 1, j + 1
                while depth:
                    depth += {"{": 1, "}": -1}.get(body[k], 0)
                    k += 1
                value, j = body[j + 1:k - 1], k
            else:
                k = re.match(r"[^,\n]*", body[j:]).end() + j
                value, j = body[j:k].strip(), k
            fields[f.group(1).lower()] = re.sub(r"\s+", " ", value.replace("{", "").replace("}", "")).strip()
        entries[m.group(2).strip()] = (m.group(1).lower(), fields)
    return entries


def creators(authors):
    out = []
    for a in authors.split(" and "):
        a = a.strip()
        if "," in a:
            last, first = [s.strip() for s in a.split(",", 1)]
        else:
            first, _, last = a.rpartition(" ")
        out.append({"creatorType": "author", "firstName": first, "lastName": last})
    return out


def zotero_item(key, kind, f):
    date = f.get("year", "")
    month = MONTHS.get(f.get("month", "")[:3].lower())
    if month:
        date += f"-{month:02d}"
    item = {"title": f["title"], "creators": creators(f["author"]), "date": date,
            "extra": f"Citation Key: {key}", "collections": [], "tags": [], "relations": {}}
    if kind == "misc" and f.get("archiveprefix") == "arXiv":
        arxiv = f["eprint"]
        item.update(itemType="preprint", repository="arXiv", archiveID=f"arXiv:{arxiv}",
                    DOI=f"10.48550/arXiv.{arxiv}", url=f"https://arxiv.org/abs/{arxiv}")
        return item
    item.update(DOI=f["doi"], url=f"https://doi.org/{f['doi']}", pages=f.get("pages", "").replace("--", "-"),
                publisher=f.get("publisher", ""))
    if kind == "inproceedings":
        item.update(itemType="conferencePaper", proceedingsTitle=f["booktitle"], series=f.get("series", ""))
    elif kind == "article":
        item.update(itemType="journalArticle", publicationTitle=f["journal"], volume=f.get("volume", ""),
                    issue=f.get("number", ""), ISSN=f.get("issn", ""))
        del item["publisher"]  # not a journalArticle field
    else:
        sys.exit(f"{key}: unsupported entry type {kind}")
    return item


def arxiv_id(data):
    m = re.search(r"(\d{4}\.\d{4,5})", " ".join(data.get(k, "") for k in ("archiveID", "url", "extra", "DOI")
                                                if "arxiv" in data.get(k, "").lower()))
    return m.group(1) if m else None


def main():
    dry = "--dry-run" in sys.argv
    bib = parse_bib(open(BIB).read())
    wanted = {k: zotero_item(k, kind, f) for k, (kind, f) in bib.items()}
    print(f"{len(wanted)} entries in refs.bib")

    cols = [c for c in get_all("/collections") if c["data"]["name"] == COLLECTION]
    if len(cols) > 1:
        sys.exit(f'{len(cols)} collections named "{COLLECTION}"; ask the user which one is ours')
    if cols:
        ckey = cols[0]["key"]
        print(f'collection "{COLLECTION}" exists: {ckey}')
    elif dry:
        ckey = None
        print(f'collection "{COLLECTION}" does not exist yet: would create it')
    else:
        _, _, res = call("POST", "/collections", [{"name": COLLECTION}],
                         {"Zotero-Write-Token": secrets.token_hex(16)})
        if "0" not in res.get("successful", {}):
            sys.exit(f"collection not created: {res}")
        ckey = res["successful"]["0"]["key"]
        print(f'collection "{COLLECTION}" created: {ckey}')

    library = [it for it in get_all("/items") if it["data"]["itemType"] not in ("attachment", "note", "annotation")]
    by_doi, by_arxiv = {}, {}
    for it in sorted(library, key=lambda it: it["data"].get("dateAdded", "")):  # oldest copy wins
        doi = it["data"].get("DOI", "").strip().lower()
        if doi:
            by_doi.setdefault(doi, it)
        ax = arxiv_id(it["data"])
        if ax:
            by_arxiv.setdefault(ax, it)
    print(f"{len(library)} items in the library")

    mapping, new, patched, kept = {}, [], 0, 0
    for k, item in wanted.items():
        hit = by_doi.get(item["DOI"].lower()) or (by_arxiv.get(item["archiveID"][6:]) if "archiveID" in item else None)
        if not hit:
            new.append(k)
            continue
        mapping[k] = hit["key"]
        if ckey in hit["data"].get("collections", []):
            kept += 1
            continue
        print(f"  reuse {k}: {hit['key']} \"{hit['data']['title'][:60]}\" (in {len(hit['data']['collections'])} other collections)")
        if not dry:
            call("PATCH", f"/items/{hit['key']}", {"collections": hit["data"]["collections"] + [ckey]},
                 {"If-Unmodified-Since-Version": str(hit["version"])})
        patched += 1
    print(f"{kept} already in the collection, {patched} existing items added to it, {len(new)} new: {', '.join(new)}")

    if new and not dry:
        batch = [dict(wanted[k], collections=[ckey]) for k in new]
        _, _, res = call("POST", "/items", batch, {"Zotero-Write-Token": secrets.token_hex(16)})
        if res.get("failed"):
            sys.exit(f"failed: {res['failed']}")
        for i, k in enumerate(new):
            mapping[k] = res["successful"][str(i)]["key"]
        print(f"created {len(new)} items")

    if not dry:
        with open(MAP, "w") as f:
            json.dump({"zotero_user": USER, "collection": COLLECTION, "collection_key": ckey,
                       "keys": {k: [mapping[k]] for k in bib}}, f, indent=2, ensure_ascii=False)
            f.write("\n")
        print(f"wrote {os.path.relpath(MAP, ROOT)}")


if __name__ == "__main__":
    main()

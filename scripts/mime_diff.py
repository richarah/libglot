#!/usr/bin/env python3
"""Differential test: libglot's MIME parser vs Python's stdlib `email`.

A corpus run only proves "did not crash". This builds the same canonical
structure from both implementations and diffs it field by field, so every
corpus message becomes an assertion.

The libglot side is `tools/mime_dump` (see that file for the schema, which
this script mirrors exactly). The Python side is email.parser.BytesParser
with policy=email.policy.default.

Usage:
  scripts/mime_diff.py --tool build/tools/mime_dump --corpus tests/corpus/mime
  scripts/mime_diff.py ... --fail-under 1.0     # CI gate
"""

import argparse
import email
import email.parser
import email.policy
import json
import pathlib
import re
import subprocess
import sys

# ---------------------------------------------------------------------------
# Normalizations.
#
# Each one exists because the difference it hides is NOT a libglot bug. They
# are deliberately conservative: it is better to report a disagreement and
# classify it by hand than to normalize until the number looks good, which
# would defeat the entire point of this harness.
# ---------------------------------------------------------------------------

# Spelling variants of the same charset. Applied to BOTH sides, so this can
# never hide a real disagreement about which charset was selected - only
# about how it was spelled.
CHARSET_ALIASES = {
    "utf8": "utf-8",
    "ascii": "us-ascii",
    "latin1": "iso-8859-1",
    "latin-1": "iso-8859-1",
    "iso88591": "iso-8859-1",
    "cp1252": "windows-1252",
}

# Header fields compared. Anything outside this set is out of scope for the
# structural diff (libglot keeps all headers; so does Python).
SELECTED = ("from", "to", "cc", "subject", "date", "message-id",
            "content-type", "content-transfer-encoding", "content-disposition")


def norm_charset(cs):
    cs = (cs or "").strip().lower()
    return CHARSET_ALIASES.get(cs, cs)


def collapse_ws(s):
    # Unfolding a header leaves runs of whitespace; both sides collapse them.
    # This cannot hide a value difference, only a folding difference, and
    # folding is a transport detail rather than content.
    return re.sub(r"\s+", " ", s or "").strip()


def py_addresses(msg, field):
    """Mailboxes as 'Display Name <addr>' / 'addr', matching mime_dump."""
    out = []
    for raw in msg.get_all(field, []):
        try:
            for addr in getattr(raw, "addresses", []):
                disp = collapse_ws(str(addr.display_name))
                spec = str(addr.addr_spec)
                out.append(f"{disp} <{spec}>" if disp else spec)
        except Exception:
            out.append(collapse_ws(str(raw)))
    return out


def py_date(msg):
    """ISO-8601, mirroring mime_dump's rule.

    RFC 5322 4.3: '-0000' and the obsolete single-letter/military zones mean
    'offset unknown'. mime_dump omits the offset there, and Python likewise
    yields a naive datetime for '-0000', so the two line up.
    """
    raw = msg.get("date")
    if raw is None:
        return None
    dt = getattr(raw, "datetime", None)
    if dt is None:
        return collapse_ws(str(raw))
    if dt.utcoffset() is None:
        return dt.strftime("%Y-%m-%dT%H:%M:%S")
    off = dt.utcoffset()
    total = int(off.total_seconds())
    sign = "+" if total >= 0 else "-"
    total = abs(total)
    return dt.strftime("%Y-%m-%dT%H:%M:%S") + f"{sign}{total // 3600:02d}:{(total % 3600) // 60:02d}"


def py_content_type(msg):
    ct = (msg.get_content_type() or "").lower()
    cs = msg.get_content_charset() or msg.get_param("charset")
    if cs:
        return f"{ct}; charset={norm_charset(str(cs))}"
    if msg.get("content-type") is None:
        # RFC 2045 5.2: an absent Content-Type defaults to
        # "text/plain; charset=us-ascii". mime_dump applies that default, and
        # Python's get_content_charset() reports None instead, so without
        # this the two sides disagree on convention rather than on content.
        # Applied only when the header is absent entirely - a present header
        # with no charset parameter is left as-is on both sides.
        return f"{ct}; charset=us-ascii"
    return ct


def py_node(msg):
    """Build the mime_dump canonical structure from a Python message."""
    node = {"content_type": (msg.get_content_type() or "").lower()}

    headers = {}
    for field in SELECTED:
        if field in ("from", "to", "cc"):
            vals = py_addresses(msg, field)
        elif field == "subject":
            vals = [collapse_ws(str(v)) for v in msg.get_all("subject", [])]
        elif field == "date":
            d = py_date(msg)
            vals = [d] if d is not None else []
        elif field == "message-id":
            vals = [str(v).strip().strip("<>").strip()
                    for v in msg.get_all("message-id", [])]
        elif field == "content-type":
            vals = [py_content_type(msg)] if msg.get("content-type") or True else []
        elif field == "content-transfer-encoding":
            cte = msg.get("content-transfer-encoding")
            # RFC 2045 6.1 default.
            vals = [collapse_ws(str(cte)).lower()] if cte else ["7bit"]
        elif field == "content-disposition":
            disp = msg.get_content_disposition()
            vals = [disp.lower()] if disp else []
        else:
            vals = []
        if vals:
            headers[field] = vals
    node["headers"] = headers

    fname = msg.get_filename()
    if fname:
        node["filename"] = str(fname)

    if msg.is_multipart():
        node["parts"] = [py_node(p) for p in msg.iter_parts()]
    else:
        node["parts"] = []
        try:
            payload = msg.get_payload(decode=True)
        except Exception:
            payload = None
        if payload is not None:
            node["body_len"] = len(payload)
            if (msg.get_content_maintype() or "") == "text":
                cs = msg.get_content_charset() or "us-ascii"
                try:
                    node["body_text"] = payload.decode(cs)
                except Exception:
                    pass  # undecodable: mime_dump omits body_text too
    return node


def diff_nodes(a, b, path="root", out=None):
    """Field-by-field diff of two canonical nodes."""
    out = out if out is not None else []
    if a is None or b is None:
        out.append((path, repr(a), repr(b)))
        return out
    for key in ("content_type", "filename", "body_len", "body_text"):
        av, bv = a.get(key), b.get(key)
        if av != bv:
            out.append((f"{path}.{key}", repr(av), repr(bv)))
    ah, bh = a.get("headers", {}), b.get("headers", {})
    for field in sorted(set(ah) | set(bh)):
        if ah.get(field) != bh.get(field):
            out.append((f"{path}.headers.{field}",
                        repr(ah.get(field)), repr(bh.get(field))))
    ap, bp = a.get("parts", []), b.get("parts", [])
    if len(ap) != len(bp):
        out.append((f"{path}.parts.count", len(ap), len(bp)))
    for i, (x, y) in enumerate(zip(ap, bp)):
        diff_nodes(x, y, f"{path}.parts[{i}]", out)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tool", required=True)
    ap.add_argument("--corpus", required=True)
    ap.add_argument("--max-diffs", type=int, default=20)
    ap.add_argument("--fail-under", type=float, default=None)
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    # Accept any regular file: real corpora (SpamAssassin, Enron) name
    # messages by hash with no meaningful extension, so filtering on one
    # silently skips the whole corpus and reports a vacuous 0/0.
    skip_ext = {".bz2", ".gz", ".zip", ".tar"}
    skip_name = {"cmds", ".DS_Store"}
    files = sorted(p for p in pathlib.Path(args.corpus).rglob("*")
                   if p.is_file() and p.suffix not in skip_ext
                   and p.name not in skip_name)
    if not files:
        print(f"error: no messages under {args.corpus}", file=sys.stderr)
        return 2

    agree = skipped = 0
    shown = 0
    for path in files:
        proc = subprocess.run([args.tool, str(path)], capture_output=True)
        if proc.returncode != 0:
            print(f"TOOL-FAIL {path}: rc={proc.returncode}")
            continue
        lg = json.loads(proc.stdout)
        raw = path.read_bytes()
        try:
            pm = email.parser.BytesParser(policy=email.policy.default).parsebytes(raw)
        except Exception as e:
            # Python itself failed to parse the message. Not a libglot
            # disagreement, so it is excluded from the rate rather than
            # counted against either side.
            skipped += 1
            if args.verbose:
                print(f"PY-FAIL   {path}: {e}")
            continue
        # A failure below is a bug in THIS harness, not in either parser.
        # It must be loud: silently folding it into "skipped" would report
        # a comfortable agreement rate over messages nothing compared.
        py = py_node(pm)

        if lg.get("parse_error"):
            # libglot rejected the header section outright while Python
            # accepted it. Report, never hide.
            print(f"DISAGREE  {path}: libglot parse_error, Python parsed")
            continue

        diffs = diff_nodes(lg.get("root"), py)
        if not diffs:
            agree += 1
            if args.verbose:
                print(f"AGREE     {path}")
        else:
            print(f"DISAGREE  {path}")
            for field, lval, pval in diffs:
                if shown < args.max_diffs:
                    print(f"    {field}\n      libglot: {lval}\n      python : {pval}")
                    shown += 1

    compared = len(files) - skipped
    rate = agree / compared if compared else 0.0
    print(f"\n{agree}/{compared} agree ({rate:.2%})"
          + (f", {skipped} skipped (Python could not parse)" if skipped else ""))
    if args.fail_under is not None and rate < args.fail_under:
        print(f"FAIL: agreement {rate:.2%} < required {args.fail_under:.2%}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

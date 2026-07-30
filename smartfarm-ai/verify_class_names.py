#!/usr/bin/env python3
"""Validate (and repair) models/class_names.json after download.

WHY THIS EXISTS: the upstream Daksh159/plant-disease-mobilenetv2 label file is
BROKEN. It ships 37 entries for a 38-class checkpoint — index 37
("Tomato___healthy") is simply absent, and index 36 reads
"Tomato___Tomato_mosaic_Normally", a case-sensitive virus->Normally replacement
that mangled the real name. The converters used to pad the gap with a synthetic
"class_37", which is worse than it sounds: the web-server decides its headline
by matching /healthy/ against the label, so a genuinely healthy tomato — the
most likely correct answer on this farm — was reported as "Possible: class_37".

download_model.sh runs this immediately after curl, so a re-download can never
silently reintroduce that. The converters call it too (a hand-placed file gets
the same check).

Stdlib only, Python 3.6+, safe to re-run: repairing an already-repaired file is
a no-op.

    python3 verify_class_names.py [path/to/class_names.json]

Exit 0 = usable (possibly repaired, changes printed). Exit 1 = needs a human.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CANONICAL_PATH = os.path.join(HERE, "plantvillage_classes.json")
DEFAULT_TARGET = os.path.join(HERE, "models", "class_names.json")

# How much of the file must line up with the canonical list before we treat it
# as "PlantVillage, with damage" and rewrite it. Set high: repairing the wrong
# model's labels would mislabel every prediction, which is worse than stopping.
RECOGNITION_RATIO = 0.9


def canonical():
    """The 38 PlantVillage classes in torchvision ImageFolder order (plain
    alphabetical), which is the order these checkpoints are trained in."""
    with open(CANONICAL_PATH) as f:
        return json.load(f)


def parse(data):
    """Normalize the three shapes a class_names.json turns up in into an ordered
    list: a plain list, an index->name dict, or a name->index dict. Returns
    (labels, gaps) where gaps are indices missing from an index->name dict."""
    if isinstance(data, list):
        return [str(x) for x in data], []

    if not isinstance(data, dict):
        raise ValueError("unrecognized class_names.json shape: %s" % type(data).__name__)

    keys = list(data.keys())
    if keys and all(str(k).lstrip("-").isdigit() for k in keys):  # index -> name
        idx = {int(k): str(v) for k, v in data.items()}
        top = max(idx)
        gaps = [i for i in range(top + 1) if i not in idx]
        return [idx.get(i, "") for i in range(top + 1)], gaps

    try:  # name -> index
        inv = {int(v): k for k, v in data.items()}
        return [inv[i] for i in range(len(inv))], []
    except Exception:
        return [str(k) for k in keys], []


def looks_like_plantvillage(labels, canon):
    """Fraction of positions that already match the canonical list. Compared over
    the canonical length so a TRUNCATED file (the actual bug) still scores high."""
    matches = sum(1 for i, name in enumerate(labels) if i < len(canon) and name == canon[i])
    return matches / float(len(canon))


def verify(path, write=True):
    """Returns (ok, labels, notes). Repairs in place when the file is recognizably
    a damaged PlantVillage list; refuses to guess for anything else."""
    notes = []
    with open(path) as f:
        raw = json.load(f)
    labels, gaps = parse(raw)
    canon = canonical()

    if labels == canon:
        return True, labels, ["already canonical (%d classes)" % len(labels)]

    ratio = looks_like_plantvillage(labels, canon)
    if ratio < RECOGNITION_RATIO:
        notes.append(
            "only %.0f%% of labels match the canonical PlantVillage list — this looks like a "
            "DIFFERENT model. Not repairing; verify the label order by hand." % (ratio * 100)
        )
        # A custom model is fine, but a file with holes in it never is.
        if gaps or any(not name for name in labels):
            notes.append("file has missing indices %s — the label order cannot be trusted." % (gaps or "?"))
            return False, labels, notes
        return True, labels, notes

    for i, name in enumerate(canon):
        if i >= len(labels):
            notes.append("added   [%d] %s (missing upstream)" % (i, name))
        elif labels[i] != name:
            notes.append("fixed   [%d] %r -> %r" % (i, labels[i], name))
    if len(labels) > len(canon):
        notes.append("dropped %d trailing extra label(s)" % (len(labels) - len(canon)))

    if write:
        backup = path + ".orig"
        if not os.path.exists(backup):  # keep the FIRST original, not the last
            os.rename(path, backup)
            notes.append("original preserved at %s" % os.path.basename(backup))
        tmp = path + ".tmp"
        with open(tmp, "w") as f:
            json.dump(canon, f, indent=2, ensure_ascii=False)
        os.rename(tmp, path)  # atomic
    return True, canon, notes


def load_verified(path):
    """For the converters: parse + repair, or die with a clear message. Replaces
    the old 'pad the gap with class_N' behaviour that produced bogus labels."""
    ok, labels, notes = verify(path)
    for n in notes:
        print("  class_names: %s" % n)
    if not ok:
        sys.exit("ERROR: %s is not usable — see the notes above." % path)
    return labels


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_TARGET
    if not os.path.exists(path):
        sys.exit("ERROR: no such file: %s (run download_model.sh first)" % path)
    ok, labels, notes = verify(path)
    print("Checked %s — %d classes" % (path, len(labels)))
    for n in notes:
        print("  %s" % n)
    if not ok:
        return 1
    print("OK: label file is usable.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Fold probe source lines longer than 71 columns at a comma, in place
(PL/I (F) reads columns 2 to 72; submit.py refuses longer cards)."""
import sys
for path in sys.argv[1:]:
    out = []
    for line in open(path).read().split("\n"):
        while len(line) > 71:
            cut = max(line.rfind(", ", 0, 70), line.rfind("; ", 0, 70))
            if cut < 0:
                break
            out.append(line[:cut + 1])
            line = "       " + line[cut + 2:]
        out.append(line)
    open(path, "w").write("\n".join(out))

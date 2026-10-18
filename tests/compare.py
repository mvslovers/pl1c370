#!/usr/bin/env python3
"""Compare two print files (text read of a VBA data set, ASA byte first),
trailing blanks ignored.  Exit 0 only if both exist, are non-empty and agree.

The offset in a message of the system action ("IHE330I ZERODIVIDE AT OFFSET
+0006A FROM ENTRY POINT X") is where the code of each compiler raised the
condition, and two compilers' code differs: it is masked in both files,
everything else of the message is compared.  So is the six-digit offset
of IHE012I, the message of an abnormal termination ("ABENDED AT OFFSET
00009C FROM ENTRY POINT X").

With --math (a test with a <name>.math file: the mathematical built-in
functions, rt/pl1math.c), a floating-point number in E form may differ in
its last printed digits: by at most 100 units of the last digit, the
lines otherwise the same character for character.  PL1C/370's functions
are rounded more accurately than PL/I (F)'s (internals/research/pli/
tier14-measurement.md).  Such a pass prints CLOSE."""
import re, sys, difflib
OFFSET = re.compile(r"( AT OFFSET \+?)(?:[0-9A-F]{5}|(?<! \+)[0-9A-F]{6})( FROM)")
args = sys.argv[1:]
plain = "--plain" in args             # a file without control characters
math = "--math" in args               # last digits of E-form numbers may differ
args = [x for x in args if x not in ("--plain", "--math")]
FLT = re.compile(r"[+-]?(\d)\.(\d+)E([+-]\d+)")


def close(x, y):
    """two lines alike but for E-form numbers within 100 last-digit units"""
    if FLT.sub("#", x) != FLT.sub("#", y):
        return False
    for mx, my in zip(FLT.finditer(x), FLT.finditer(y)):
        if len(mx.group(2)) != len(my.group(2)):
            return False
        digits = len(mx.group(2))
        ex, ey = int(mx.group(3)), int(my.group(3))
        unit = 10.0 ** (max(ex, ey) - digits)
        if abs(float(mx.group(0)) - float(my.group(0))) > 100 * unit:
            return False
    return True
a, b = args[0], args[1]
try:
    la = [OFFSET.sub(r"\1*****\2", l.rstrip())
          for l in open(a, encoding="latin-1").read().rstrip("\n").split("\n")]
    lb = [OFFSET.sub(r"\1*****\2", l.rstrip())
          for l in open(b, encoding="latin-1").read().rstrip("\n").split("\n")]
except OSError as e:
    sys.exit(f"MISSING {e.filename}")
if (not la or la == [""] or not lb or lb == [""]) and not plain:
    sys.exit("EMPTY print file")
if la == lb:
    print(f"IDENTICAL ({len(la)} lines)")
    sys.exit(0)
if math and len(la) == len(lb) and all(close(x, y) for x, y in zip(la, lb)):
    print(f"CLOSE ({sum(x != y for x, y in zip(la, lb))} of {len(la)} lines in the last digits)")
    sys.exit(0)
sys.stdout.writelines(difflib.unified_diff([l + "\n" for l in la], [l + "\n" for l in lb], a, b))
sys.exit(1)

#!/bin/sh
# -MD / -MF: the dependency file lists the source and the %INCLUDE
# members the preprocessor read -- not one in a %IF branch not taken --
# with an empty rule for each member (#236 follow-up, for mbt).
set -e
cd "$(dirname "$0")/../.."
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
cat > "$T/cond.pli" <<'PLI'
 T: PROC OPTIONS(MAIN);
 %DCL X FIXED;
 %X = 1;
 %IF X = 2 %THEN %INCLUDE PPINC;
 %IF X = 1 %THEN %INCLUDE RXCOMM;
   PUT LIST('A');
 END T;
PLI
build/host/pl1c370 -MD -I tests/pli/inc "$T/cond.pli" -o "$T/cond.asm"
printf '%s: %s \\\n tests/pli/inc/rxcomm.pli\n\ntests/pli/inc/rxcomm.pli:\n' \
    "$T/cond.asm" "$T/cond.pli" > "$T/want"
cmp "$T/cond.d" "$T/want"
build/host/pl1c370 -MD -MF "$T/x.dep" tests/pli/t9str.pli -o "$T/n.asm"
printf '%s: tests/pli/t9str.pli\n' "$T/n.asm" | cmp "$T/x.dep" -
echo "depfile: ok"

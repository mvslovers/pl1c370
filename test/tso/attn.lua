-- test/tso/attn.lua -- ATTENTION at a real terminal (#226). TTATTN,
-- compiled with INTERRUPT from test/tso/tattn.pli and deployed by
-- tests/run_mvs.py, loops for at most a minute; PA1 must raise the
-- ATTENTION condition, whose on-unit ends the loop.
return function(t)
  t:logon()
  t:type("ALLOC F(SYSPRINT) DA(*) REUSE"):enter()
  t:type("CALL 'PL1C370.DEV.LINKLIB(TTATTN)'"):enter{ wait = false }
  t:expect("LOOPING, PRESS PA1", { timeout = 30 })
  t:pa(1)
  t:expect("ATTENTION RAISED, ONCODE", { timeout = 30 })
  t:expect("LOOP ENDED BY ATTENTION", { timeout = 30 })
  print(t:text())
end

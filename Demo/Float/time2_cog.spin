{{
  Test for timing floating point operations
}}

CON
  _clkmode = xtal1 + pll16x
  _clkfreq = 80_000_000

OBJ
  fp: "FloatMathSimple.cog"
  fds: "FullDuplexSerial"
  fstr: "FloatString"

PUB demo | x, start, end, us
  fp.__cognew
  fds.start(31, 30, 0, 115200)
  fds.str(string("floating point timing test", 13, 10))
  start := CNT
  x := series(10)
  end := CNT - start
  us := fp.FDiv(fp.FFloat(end), 80.0)
  fds.str(string("result is "))
  fds.str(fstr.FloatToString(x))
  fds.str(string(" (hex "))
  fds.hex(x, 8)
  fds.str(string(")", 13, 10))
  fds.str(string("time taken: "))
  fds.str(fstr.FloatToString(us))
  fds.str(string(" microseconds, "))
  fds.dec(end)
  fds.str(string(" cycles", 13, 10))

PUB series(n) : sum | i, term, two
  sum := 0
  two := 2.0
  term := constant(1.0)
  repeat i from 1 to n
    term := fp.FDiv(term, two)
    sum := fp.FAdd(sum, term)
  
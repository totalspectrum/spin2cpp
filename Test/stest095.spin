CON
  pin = 0

PUB start | sigmask, cntStart, cntTime, sigCnt

  repeat

    sigmask := 1<<pin

    ' set up to count positive edges on pin 0
    countera(80, 0, 0, 1, 0)
  
#ifdef NEVER
    ' Wait for a signal pulse to synchronize to the system counter
    waitpne(sigmask, sigmask, 0)
    waitpeq(sigmask, sigmask, 0)
    phsa := 0
    cntStart := cnt - 4
  
    ' count input pulses until 1.0 seconds have elapsed
    repeat
      waitpne(sigmask, sigmask, 0)
      waitpeq(sigmask, sigmask, 0)
      sigCnt := phsa
      cntTime := cnt - cntStart
    until cntTime > 80_000_000  ' 1.0 Second gate time
#endif

PRI countera(ctrmode, apin, bpin, freq, phs)
  ctra := (ctrmode<<26) | (apin) | (bpin<<9)
  frqa := freq
  phsa := phs
  
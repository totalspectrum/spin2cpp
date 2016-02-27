pub ez_pulse_in(pin) | ticks, r

  r := |<pin

  waitpne(r, r, 0)                                      ' wait for pin to be low
  waitpeq(r, r, 0)                                      ' wait for pin to go high
  ticks := -cnt                                                 ' start timing
  waitpne(r, r, 0)                                      ' wait for pin to go low
  ticks += cnt                                                  ' stop timing

  return ticks / 1_000_000                                      ' return pulse width in microseconds

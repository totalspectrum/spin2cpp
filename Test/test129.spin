pub blink(pin, n)
    repeat n
      OUTA[pin] ^= 1

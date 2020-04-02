PUB hang
  asm
    jmp $
  endasm

PUB wait(pin)
  pin := 1<<pin
  asm
    test INA, pin wc
 if_nc jmp $-1
  endasm


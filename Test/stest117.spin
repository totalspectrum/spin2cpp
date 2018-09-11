' compare u (unsigned) with s (signed)
PUB uscmp(a) : r 
  asm
       cmp a, #2 wc,wz
 if_z mov r,#1
  endasm


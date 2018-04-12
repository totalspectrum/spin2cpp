CON
  FFT_SIZE = 1024
  
VAR
  long bx[FFT_SIZE]
  byte by[FFT_SIZE]
PUB fillInput | k
  repeat k from 0 to (FFT_SIZE - 1)
    bx[k] := 13*k
    by[(FFT_SIZE-1)-k] := k*17
   
dat
   org 0
entry
   cmp par, #"ß" wz
   cmp par, #$df wz
   nop
sval byte "ß", 0

pub get
  return sval[0]


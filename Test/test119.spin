CON
  lcd_w = 96
  width = lcd_w / 32

VAR
  long array[width]
  
PUB clear | i
  repeat i from 0 to width-1
    array[i] := 0

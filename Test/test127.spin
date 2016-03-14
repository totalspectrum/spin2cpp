' check that return value processing goes through
' case statements
pub asc2val(p_str) | c
  repeat
     c := byte[p_str]
     case c
       " ":
         p_str++
       "0".."9":
         return c - "0"
       other:
         return 0


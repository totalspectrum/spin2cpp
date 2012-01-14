PUB func(a,b) : ok
  if a < b
    ok := true
  'if more digits, add current digit and prepare next
  elseif a == 0
    ok := true
  'if no more digits, add "0"
  else
    ok := false

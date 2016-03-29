var
  long x[10]
  
pub get | a[10], i, sum
  sum := 0
  repeat i from 0 to 9
    a[i] := x[i]
    sum += a[i]
  return sum


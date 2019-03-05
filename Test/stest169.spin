pub main1 | r, s
  s := 2
  repeat
    r := ||(s++)
    OUTB := r

pub main2 | r, s
  s := 2
  repeat
    r := ||(++s)
    OUTB := r
    
pub main3 | r, s
  s := 2
  repeat
    r := ||(?s)
    OUTB := r

pub main4 | r, s
  s := 2
  repeat
    r := ||(s\INB)
    OUTB := r

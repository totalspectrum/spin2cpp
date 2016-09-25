VAR
  long val
  long val2
  
obj defs : "miscdefs.spin"

PUB main | left[10]
  val := (defs.doout(defs#LEFT) * 8) / 10
  val2 := defs#CENTER
  
  
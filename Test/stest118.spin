
PUB _float_fromuns(integer) | s, x, m

''Convert integer to float    

  if m := integer               'absolutize mantissa, if 0, result 0
    s := 0                      'get sign
    x := m & $f                'get exponent
    m >>= 2
    return s | x | m             'pack result
  else
    return 0
    
PUB _float_fromint(integer) : single | negate

''Convert integer to float    
  if integer < 0
    integer := -integer
    negate := 1
  else
    negate := 0
  single := _float_fromuns(integer)
  if (negate)  
    single := _float_negate(single)
   
PRI _float_negate(singleA) : single

''Negate singleA

  return singleA ^ $8000_0000   'toggle sign bit
  


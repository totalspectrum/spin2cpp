''
'' this test uses only the Spin methods for I/O;
'' no prints here please!
''

class util
  any(t) function mymin(x as t, y as t) as t
    if x < y then
      return x
    else
      return y
    end if
  end function
end class

class FDS using "FullDuplexSerial.spin"
dim ser as FDS
dim u as util
dim msg as string

sub newline
  ser.tx(13)
  ser.tx(10)
end sub

sub doit

  dim f as sub(x as const ubyte ptr)
  dim g as sub()
  
  f = @ser.str
  g = @newline
  
  f(msg)
  f(" world!")
  g()
end sub

ser.start(31, 30, 0, 115_200)
ser.str("pointer test... ")

msg = "hi!"
doit

ser.str(u.mymin("xabc", "abc"))
newline
ser.dec(u.mymin(-4, 9))
newline
ser.str(" ...done")
newline

''
'' send the magic propload status code
''
ser.tx(255)
ser.tx(0)
ser.tx(0)

do
loop

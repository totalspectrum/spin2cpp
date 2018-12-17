''
'' this test uses only the Spin methods for I/O;
'' no prints here please!
''

class FDS using "FullDuplexSerial.spin"
dim ser as FDS
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

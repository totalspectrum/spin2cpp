class FDS using "FullDuplexSerial.spin"

dim ser as FDS
dim i as ubyte

ser.start(31, 30, 0, 115_200)
for i = 0b01 to 10
  ser.dec(i)
  ser.tx(0xd)
  ser.tx(0hA)
next i

waitcnt(0)

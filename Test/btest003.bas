class FDS input "FullDuplexSerial.spin"

dim ser as FDS
dim i as byte

ser.start(31, 30, 0, 115_200)
for i = 1 to 10
  ser.dec(i)
  ser.tx(13)
  ser.tx(10)
next i

waitcnt(0)

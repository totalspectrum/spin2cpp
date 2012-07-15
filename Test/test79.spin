CON ' i2c driver
  SCL = 28
  SDA = 29

PUB i2c_Start
  outa[SCL]~~
  dira[SCL]~~
  outa[SDA]~~
  dira[SDA]~~
  outa[SDA]~
  outa[SCL]~


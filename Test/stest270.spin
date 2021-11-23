PUB rx : rxbyte
  repeat while (rxbyte := rxcheck) < 0

PUB {++noinline} rxcheck : data
  data := outa & $FF
  if data == 0
    return -1
  return data


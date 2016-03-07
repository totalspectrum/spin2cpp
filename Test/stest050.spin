CON
  all_pin_mask = %0000_1111_1100

PUB pushData(data)
  all_pin_low
  outa |= (data << 2)

PUB all_pin_low
  outa &= !all_pin_mask

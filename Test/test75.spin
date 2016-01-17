CON
  _INIT_SIZE = 4

PUB start(code) | t, params[_INIT_SIZE]
  cognew(@entry, @params)

DAT
		org
entry
		mov DIRA, #0
		
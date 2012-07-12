CON
  _INIT_SIZE = 4

PUB start(code) | params[_INIT_SIZE]
  cognew(code, @params)

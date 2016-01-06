CON
  _INIT_SIZE = 4

PUB start(code) | t, params[_INIT_SIZE]
  cognew(code, @params)

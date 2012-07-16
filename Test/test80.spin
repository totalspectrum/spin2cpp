VAR
  byte nameBuffer[14]

OBJ
  fds: "FullDuplexSerial"

PUB init
  nameBuffer := "."  '' this really means nameBuffer[0] := '.'


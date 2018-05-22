VAR
  long count

PUB peek
  return count

PUB donext
  count := peek + 1
  return count


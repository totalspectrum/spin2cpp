VAR
  long count

PUB peek
  return count

PUB next
  count := peek + 1
  return count


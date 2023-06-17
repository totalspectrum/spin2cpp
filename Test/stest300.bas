' test for multiple lets not throwing issues
' with redeclaration
let r = 1
let r = r+1

_drvh(r)

let r = r+1
_drvh(r)

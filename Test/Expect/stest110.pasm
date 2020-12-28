pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_filllong
LR__0001
	wrlong	arg02, arg01
	add	arg01, #4
	djnz	arg03, #LR__0001
_filllong_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
arg02
	res	1
arg03
	res	1
	fit	496

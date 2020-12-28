con
	DEF1 = 2
	DEF2 = 8
pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_report
	shl	arg02, #2
	add	arg01, #4
	add	arg02, arg01
	rdlong	_var01, arg02
	add	_var01, #36
	mov	outa, _var01
_report_ret
	ret

__lockreg
	long	0
ptr___lockreg_
	long	@@@__lockreg
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
arg02
	res	1
	fit	496

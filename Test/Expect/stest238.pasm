pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_report
	shl	arg02, #2
	add	arg01, #4
	add	arg02, arg01
	rdlong	arg01, arg02
	add	arg01, #36
	mov	outa, arg01
_report_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
arg02
	res	1
	fit	496

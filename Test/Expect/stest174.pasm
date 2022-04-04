pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_main
	wrlong	fp, sp
	add	sp, #4
	mov	fp, sp
	add	sp, #8
	mov	arg01, #1
	wrlong	arg01, fp
	mov	outa, #1
	mov	arg01, fp
	rdlong	arg01, arg01
	mov	ina, arg01
	mov	sp, fp
	sub	sp, #4
	rdlong	fp, sp
_main_ret
	ret

_Proc_2
	rdlong	outa, arg01
_Proc_2_ret
	ret

_Proc_5
	rdlong	arg01, arg01
	mov	ina, arg01
_Proc_5_ret
	ret

fp
	long	0
sp
	long	@@@stackspace
COG_BSS_START
	fit	496
stackspace
	long	0[1]
	org	COG_BSS_START
arg01
	res	1
	fit	496

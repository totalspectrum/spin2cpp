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
	mov	main_tmp001_, #1
	wrlong	main_tmp001_, fp
	mov	outa, #1
	mov	ina, #1
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
	rdlong	_var01, arg01
	mov	ina, _var01
_Proc_5_ret
	ret

__lockreg
	long	0
fp
	long	0
ptr___lockreg_
	long	@@@__lockreg
sp
	long	@@@stackspace
COG_BSS_START
	fit	496
stackspace
	long	0[1]
	org	COG_BSS_START
_var01
	res	1
arg01
	res	1
main_tmp001_
	res	1
	fit	496

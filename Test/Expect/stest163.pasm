pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_main
	wrlong	fp, sp
	add	sp, #4
	mov	fp, sp
	add	sp, #104
	mov	_var01, #65
	wrbyte	_var01, fp
	mov	outa, fp
	mov	sp, fp
	sub	sp, #4
	rdlong	fp, sp
_main_ret
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
_var01
	res	1
	fit	496

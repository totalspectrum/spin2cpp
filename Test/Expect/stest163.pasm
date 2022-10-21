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
	mov	_var01, fp
	add	fp, #100
	wrlong	_var01, fp
	mov	_var02, #65
	wrbyte	_var02, _var01
	rdlong	outa, fp
	sub	fp, #100
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
_var02
	res	1
	fit	496

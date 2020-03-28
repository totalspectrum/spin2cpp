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
	mov	_tmp001_, #65
	wrbyte	_tmp001_, fp
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
_tmp001_
	res	1
	fit	496

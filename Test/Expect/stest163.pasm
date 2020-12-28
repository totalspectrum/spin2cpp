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
	mov	_tmp001_, fp
	add	fp, #100
	wrlong	_tmp001_, fp
	mov	_tmp003_, #65
	wrbyte	_tmp003_, _tmp001_
	rdlong	outa, fp
	sub	fp, #100
	mov	sp, fp
	sub	sp, #4
	rdlong	fp, sp
_main_ret
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
_tmp001_
	res	1
_tmp003_
	res	1
	fit	496

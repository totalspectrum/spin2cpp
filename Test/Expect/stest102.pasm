pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_set
	wrlong	fp, sp
	add	sp, #4
	mov	fp, sp
	add	sp, #12
	add	fp, #4
	wrlong	arg01, fp
	sub	fp, #4
	mov	_tmp001_, #0
	wrlong	_tmp001_, fp
	add	fp, #4
	rdlong	_tmp001_, fp
	sub	fp, #4
	wrlong	_tmp001_, fp
	wrlong	fp, objptr
	rdlong	result1, fp
	mov	sp, fp
	sub	sp, #4
	rdlong	fp, sp
_set_ret
	ret

_set2
	wrlong	fp, sp
	add	sp, #4
	mov	fp, sp
	add	sp, #8
	wrlong	arg01, fp
	wrlong	fp, objptr
	mov	sp, fp
	sub	sp, #4
	rdlong	fp, sp
_set2_ret
	ret

__lockreg
	long	0
fp
	long	0
objptr
	long	@@@objmem
ptr___lockreg_
	long	@@@__lockreg
result1
	long	0
sp
	long	@@@stackspace
COG_BSS_START
	fit	496
objmem
	long	0[1]
stackspace
	long	0[1]
	org	COG_BSS_START
_tmp001_
	res	1
arg01
	res	1
	fit	496

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
	mov	arg01, #0
	wrlong	arg01, fp
	add	fp, #4
	rdlong	arg01, fp
	sub	fp, #4
	wrlong	arg01, fp
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

fp
	long	0
objptr
	long	@@@objmem
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
arg01
	res	1
	fit	496

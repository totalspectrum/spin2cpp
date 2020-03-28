pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_demo
	wrlong	fp, sp
	add	sp, #4
	mov	fp, sp
	add	sp, #28
	add	fp, #16
	wrlong	arg01, fp
	add	fp, #4
	wrlong	arg02, fp
	add	fp, #4
	wrlong	arg03, fp
	sub	fp, #4
	rdlong	result1, fp
	sub	fp, #20
	mov	sp, fp
	sub	sp, #4
	rdlong	fp, sp
_demo_ret
	ret

fp
	long	0
result1
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
arg02
	res	1
arg03
	res	1
	fit	496

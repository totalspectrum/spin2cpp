pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_get
	wrlong	fp, sp
	add	sp, #4
	mov	fp, sp
	add	sp, #60
	mov	_get_sum, #0
	mov	_get__cse__0000, objptr
	mov	_get_i, #0
LR__0001
	mov	_tmp001_, _get_i
	shl	_tmp001_, #2
	add	fp, #4
	rdlong	_tmp003_, _get__cse__0000
	add	_tmp001_, fp
	wrlong	_tmp003_, _tmp001_
	mov	_tmp003_, _get_i
	shl	_tmp003_, #2
	add	_tmp003_, fp
	rdlong	_tmp003_, _tmp003_
	add	_get_sum, _tmp003_
	add	_get_i, #1
	add	_get__cse__0000, #4
	cmps	_get_i, #10 wc
	sub	fp, #4
 if_b	jmp	#LR__0001
	mov	result1, _get_sum
	mov	sp, fp
	sub	sp, #4
	rdlong	fp, sp
_get_ret
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
	long	0[10]
stackspace
	long	0[1]
	org	COG_BSS_START
_get__cse__0000
	res	1
_get_i
	res	1
_get_sum
	res	1
_tmp001_
	res	1
_tmp003_
	res	1
	fit	496

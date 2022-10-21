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
	mov	_var01, #0
	mov	_var02, objptr
	mov	_var03, #0
	add	fp, #4
LR__0001
	mov	result1, _var03
	shl	result1, #2
	rdlong	_var04, _var02
	add	result1, fp
	wrlong	_var04, result1
	mov	_var04, _var03
	shl	_var04, #2
	add	_var04, fp
	rdlong	_var04, _var04
	add	_var01, _var04
	add	_var03, #1
	add	_var02, #4
	cmps	_var03, #10 wc
 if_b	jmp	#LR__0001
	sub	fp, #4
	mov	result1, _var01
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
_var01
	res	1
_var02
	res	1
_var03
	res	1
_var04
	res	1
	fit	496

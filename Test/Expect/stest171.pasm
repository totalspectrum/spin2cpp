pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_foo
	mov	result1, ptr_L__0001_
_foo_ret
	ret

_pinstring_getstr
	mov	result1, ptr_L__0001_
_pinstring_getstr_ret
	ret

__lockreg
	long	0
objptr
	long	@@@objmem
ptr_L__0001_
	long	@@@LR__0001
ptr___lockreg_
	long	@@@__lockreg
result1
	long	0
COG_BSS_START
	fit	496

LR__0001
	byte	80
	byte	0
	byte	0
objmem
	long	0[0]
	org	COG_BSS_START
	fit	496

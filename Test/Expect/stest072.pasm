CON
	x = 13
PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_demo
	mov	result1, ptr_L__0001_
_demo_ret
	ret

ptr_L__0001_
	long	@@@L__0001
result1
	long	0
COG_BSS_START
	fit	496

L__0001
	byte	"hello"
	byte	13
	byte	0
	org	COG_BSS_START
arg1
	res	1
arg2
	res	1
arg3
	res	1
arg4
	res	1
	fit	496

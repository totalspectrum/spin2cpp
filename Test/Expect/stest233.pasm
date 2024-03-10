pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_demo
	mov	result1, arg02
_demo_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
arg01
	res	1
arg02
	res	1
	fit	496

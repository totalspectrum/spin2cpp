pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_demo
	mov	_demo_a, arg1
	mov	_demo_a + 1, arg2
	mov	_demo_a + 2, arg3
	mov	result1, _demo_a + 1
_demo_ret
	ret
wrcog
    mov    0-0, 0-0
wrcog_ret
    ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_demo_a
	res	3
_demo_x
	res	1
_demo_y
	res	1
_demo_z
	res	1
arg1
	res	1
arg2
	res	1
arg3
	res	1
	fit	496

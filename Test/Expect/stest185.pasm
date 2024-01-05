pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_getoff2
_getoff1
	mov	result1, #4
_getoff1_ret
_getoff2_ret
	ret


result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496

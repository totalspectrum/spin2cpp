pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_blah1
	mov	result1, #1
	mov	result1, #2
_blah1_ret
	ret

_blah2
	mov	result1, #2
_blah2_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496

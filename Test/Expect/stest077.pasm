PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_maskit
	and	arg1, #1
	mov	result1, arg1
_maskit_ret
	ret

_mask2
	and	arg1, arg2
	and	arg1, #255
	mov	result1, arg1
_mask2_ret
	ret

arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496

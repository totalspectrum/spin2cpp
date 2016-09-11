PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_pokeb
	wrbyte	arg2, arg1
_pokeb_ret
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

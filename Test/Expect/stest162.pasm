pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_blah
	cmp	arg01, #99 wz
 if_e	cmps	arg02, #0 wc
 if_nc_or_nz	mov	arg02, #0
	mov	result1, arg02
_blah_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
arg01
	res	1
arg02
	res	1
	fit	496

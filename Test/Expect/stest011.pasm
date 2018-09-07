PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_test
	mov	DIRA, #1
	mov	OUTA, #1
_test_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496

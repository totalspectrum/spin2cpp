PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_main
LR__0001
	xor	OUTA, #1
	jmp	#LR__0001
_main_ret
	ret

COG_BSS_START
	fit	496
	org	COG_BSS_START
	fit	496

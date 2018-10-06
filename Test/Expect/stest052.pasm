PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_foo
	mov	result1, ptr_L__0021_
_foo_ret
	ret

ptr_L__0021_
	long	@@@LR__0001
result1
	long	0
COG_BSS_START
	fit	496

LR__0001
	byte	"hello"
	byte	0
	org	COG_BSS_START
	fit	496

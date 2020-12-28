pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_main
	jmp	#LR__0002
LR__0001
	mov	outb, ptr_L__0003_
	jmp	#LR__0003
LR__0002
	mov	outb, ptr_L__0005_
	jmp	#LR__0001
LR__0003
	mov	result1, #0
_main_ret
	ret

__lockreg
	long	0
ptr_L__0003_
	long	@@@LR__0004
ptr_L__0005_
	long	@@@LR__0005
ptr___lockreg_
	long	@@@__lockreg
result1
	long	0
COG_BSS_START
	fit	496

LR__0004
	byte	"some big string",10
	byte	0
LR__0005
	byte	"some other string",10
	byte	0
	org	COG_BSS_START
	fit	496

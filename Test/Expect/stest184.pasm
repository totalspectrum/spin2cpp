pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_fetch
	add	arg01, #12
	rdlong	result1, arg01
_fetch_ret
	ret

_fetchv
	mov	arg01, sp
	add	sp, #20
	mov	arg02, ptr__dat__
	mov	arg03, #20
	mov	result1, arg01
	cmps	arg01, arg02 wc
 if_ae	jmp	#LR__0002
	mov	_var01, #20
LR__0001
	rdbyte	arg03, arg02
	wrbyte	arg03, arg01
	add	arg01, #1
	add	arg02, #1
	djnz	_var01, #LR__0001
	jmp	#LR__0005
LR__0002
	add	arg01, arg03
	add	arg02, arg03
	mov	_var02, arg03 wz
 if_e	jmp	#LR__0004
LR__0003
	sub	arg01, #1
	sub	arg02, #1
	rdbyte	_var01, arg02
	wrbyte	_var01, arg01
	djnz	_var02, #LR__0003
LR__0004
LR__0005
	add	result1, #12
	rdlong	result1, result1
_fetchv_ret
	ret

ptr__dat__
	long	@@@_dat_
result1
	long	0
sp
	long	@@@stackspace
COG_BSS_START
	fit	496
	long
_dat_
	byte	$00[20]
stackspace
	long	0[1]
	org	COG_BSS_START
_var01
	res	1
_var02
	res	1
arg01
	res	1
arg02
	res	1
arg03
	res	1
	fit	496

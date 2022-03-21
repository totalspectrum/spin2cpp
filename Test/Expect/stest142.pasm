pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_addlist
	mov	_var01, #0
LR__0001
	cmps	arg01, #1 wc
 if_b	jmp	#LR__0002
	rdlong	_var02, arg02
	add	arg02, #4
	add	_var01, _var02
	jmp	#LR__0001
LR__0002
	mov	result1, _var01
_addlist_ret
	ret

_get10
	mov	get10_tmp002_, #1
	mov	get10_tmp003_, #2
	mov	get10_tmp004_, #3
	mov	get10_tmp005_, #4
	mov	arg01, #4
	mov	arg02, sp
	wrlong	get10_tmp002_, sp
	add	sp, #4
	wrlong	get10_tmp003_, sp
	add	sp, #4
	wrlong	get10_tmp004_, sp
	add	sp, #4
	wrlong	get10_tmp005_, sp
	add	sp, #4
	call	#_addlist
	sub	sp, #16
_get10_ret
	ret

result1
	long	0
sp
	long	@@@stackspace
COG_BSS_START
	fit	496
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
get10_tmp002_
	res	1
get10_tmp003_
	res	1
get10_tmp004_
	res	1
get10_tmp005_
	res	1
	fit	496

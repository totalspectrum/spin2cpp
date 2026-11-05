pub main
  coginit(0, @entry, 0)
dat
	org	0
entry

_use
	add	arg01, arg02
	add	arg01, arg03
	mov	result1, arg01
_use_ret
	ret

_sum
	mov	sum_tmp002_, arg01
	mov	_sum_s_0001, #0
	mov	arg01, sum_tmp002_
	mov	arg02, #1
	mov	arg03, #2
	call	#_use
	add	_sum_s_0001, result1
	mov	arg01, sum_tmp002_
	mov	arg02, #3
	mov	arg03, #4
	call	#_use
	add	_sum_s_0001, result1
	mov	arg01, sum_tmp002_
	mov	arg02, #5
	mov	arg03, #6
	call	#_use
	add	_sum_s_0001, result1
	mov	arg01, sum_tmp002_
	mov	arg02, #7
	mov	arg03, #8
	call	#_use
	add	result1, _sum_s_0001
_sum_ret
	ret

result1
	long	0
COG_BSS_START
	fit	496
	org	COG_BSS_START
_sum_s_0001
	res	1
arg01
	res	1
arg02
	res	1
arg03
	res	1
sum_tmp002_
	res	1
	fit	496

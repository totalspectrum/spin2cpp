PUB main
  coginit(0, @entry, 0)
DAT
	org	0
entry

_fetchx
	add	ptr__dat__, #4
	rdlong	result1, ptr__dat__
	sub	ptr__dat__, #4
_fetchx_ret
	ret

arg1
	long	0
arg2
	long	0
arg3
	long	0
arg4
	long	0
ptr__dat__
	long	@@@_dat_
result1
	long	0
	fit	496
	long
_dat_
	long	$11223344
	long	$55667788
	long	$99aabbcc

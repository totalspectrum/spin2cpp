DAT
	org	0

_fetchx
	add	ptr__dat__, #4
	rdlong	result_, ptr__dat__
	sub	ptr__dat__, #4
_fetchx_ret
	ret

arg1_
	long	0
arg2_
	long	0
arg3_
	long	0
arg4_
	long	0
ptr__dat__
	long	@@@_dat_
result_
	long	0
	long
_dat_
	byte	$44,$33,$22,$11,$88,$77,$66,$55
	byte	$cc,$bb,$aa,$99

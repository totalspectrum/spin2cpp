DAT
	org	0

fetchx
	add	ptr_dat__, #4
	rdlong	result_, ptr_dat__
	sub	ptr_dat__, #4
fetchx_ret
	ret

ptr_dat__
	long	@@@dat_
result_
	long	0
	long
dat_
	byte	$44,$33,$22,$11,$88,$77,$66,$55
	byte	$cc,$bb,$aa,$99

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
	mov	_fetchv__cse__0001, ptr__dat__
	mov	arg01, #20
	call	#__system___gc_alloc_managed
	mov	arg01, result1
	mov	arg02, _fetchv__cse__0001
	mov	arg03, #20
	call	#__system__bytemove
	add	result1, #12
	rdlong	result1, result1
_fetchv_ret
	ret

__system___txraw
	mov	__system___txraw_val, arg01
	rdlong	__system___txraw_bitcycles, ptr___system__dat__
	or	outa, imm_1073741824_
	or	dira, imm_1073741824_
	or	__system___txraw_val, #256
	shl	__system___txraw_val, #1
	mov	__system___txraw_nextcnt, cnt
	mov	__system___txraw__idx__90001, #10
LR__0001
	add	__system___txraw_nextcnt, __system___txraw_bitcycles
	mov	arg01, __system___txraw_nextcnt
	waitcnt	arg01, #0
	shr	__system___txraw_val, #1 wc
	muxc	outa, imm_1073741824_
	djnz	__system___txraw__idx__90001, #LR__0001
	mov	result1, #1
__system___txraw_ret
	ret

__system__bytemove
	mov	_var01, arg01
	cmps	arg01, arg02 wc,wz
 if_ae	jmp	#LR__0003
	mov	_var02, arg03 wz
 if_e	jmp	#LR__0006
LR__0002
	rdbyte	_var03, arg02
	wrbyte	_var03, arg01
	add	arg01, #1
	add	arg02, #1
	djnz	_var02, #LR__0002
	jmp	#LR__0006
LR__0003
	add	arg01, arg03
	add	arg02, arg03
	mov	_var04, arg03 wz
 if_e	jmp	#LR__0005
LR__0004
	sub	arg01, #1
	sub	arg02, #1
	rdbyte	_var03, arg02
	wrbyte	_var03, arg01
	djnz	_var04, #LR__0004
LR__0005
LR__0006
	mov	result1, _var01
__system__bytemove_ret
	ret

__system___tx
	mov	__system___tx_c, arg01
	cmp	__system___tx_c, #10 wz
 if_ne	jmp	#LR__0007
	add	ptr___system__dat__, #24
	rdlong	_system___tx_tmp001_, ptr___system__dat__
	sub	ptr___system__dat__, #24
	test	_system___tx_tmp001_, #2 wz
 if_e	jmp	#LR__0007
	mov	_system___tx_tmp001_, #13
	mov	arg01, #13
	call	#__system___txraw
LR__0007
	mov	arg01, __system___tx_c
	call	#__system___txraw
__system___tx_ret
	ret

__system___gc_ptrs
	mov	_var01, __heap_ptr
	mov	_var02, _var01
	add	_var02, imm_1008_
	rdlong	_var03, _var01 wz
 if_ne	jmp	#LR__0008
	mov	_var04, _var02
	sub	_var04, _var01
	mov	_var03, #1
	wrword	_var03, _var01
	mov	_var05, _var01
	add	_var05, #2
	mov	_var03, imm_27792_
	wrword	_var03, _var05
	mov	_var06, _var01
	add	_var06, #4
	mov	_var03, #0
	wrword	_var03, _var06
	mov	_var07, _var01
	add	_var07, #6
	mov	_var03, #1
	wrword	_var03, _var07
	add	_var01, #16
	abs	_var03, _var04 wc
	shr	_var03, #4
 if_b	neg	_var03, _var03
	wrword	_var03, _var01
	mov	_var08, _var01
	add	_var08, #2
	mov	_var03, imm_27791_
	wrword	_var03, _var08
	mov	_var09, _var01
	add	_var09, #4
	mov	_var03, #0
	wrword	_var03, _var09
	mov	_var10, _var01
	add	_var10, #6
	wrword	_var03, _var10
	sub	_var01, #16
LR__0008
	mov	result2, _var02
	mov	result1, _var01
__system___gc_ptrs_ret
	ret

__system___gc_pageptr
	cmp	arg02, #0 wz
 if_e	mov	result1, #0
 if_ne	shl	arg02, #4
 if_ne	add	arg01, arg02
 if_ne	mov	result1, arg01
__system___gc_pageptr_ret
	ret

__system___gc_pageindex
	cmp	arg02, #0 wz
 if_e	mov	result1, #0
 if_ne	sub	arg02, arg01
 if_ne	shr	arg02, #4
 if_ne	mov	result1, arg02
__system___gc_pageindex_ret
	ret

__system___gc_isFree
	mov	_var01, #0
	add	arg01, #2
	rdword	_var02, arg01
	cmp	_var02, imm_27791_ wz
 if_e	neg	_var01, #1
	mov	result1, _var01
__system___gc_isFree_ret
	ret

__system___gc_nextBlockPtr
	mov	__system___gc_nextBlockPtr_ptr, arg01
	rdword	__system___gc_nextBlockPtr_t, __system___gc_nextBlockPtr_ptr wz
 if_ne	jmp	#LR__0009
	mov	arg01, ptr_L__0110_
	call	#__system___gc_errmsg
	jmp	#__system___gc_nextBlockPtr_ret
LR__0009
	shl	__system___gc_nextBlockPtr_t, #4
	add	__system___gc_nextBlockPtr_ptr, __system___gc_nextBlockPtr_t
	mov	result1, __system___gc_nextBlockPtr_ptr
__system___gc_nextBlockPtr_ret
	ret

__system___gc_tryalloc
	mov	__system___gc_tryalloc_size, arg01
	mov	__system___gc_tryalloc_reserveflag, arg02
	call	#__system___gc_ptrs
	mov	__system___gc_tryalloc_heap_base, result1
	mov	__system___gc_tryalloc_heap_end, result2
	mov	__system___gc_tryalloc_ptr, __system___gc_tryalloc_heap_base
	mov	__system___gc_tryalloc_availsize, #0
LR__0010
	mov	__system___gc_tryalloc_lastptr, __system___gc_tryalloc_ptr
	add	__system___gc_tryalloc_ptr, #6
	mov	__system___gc_tryalloc__cse__0032, __system___gc_tryalloc_ptr
	mov	_system___gc_tryalloc_tmp001_, __system___gc_tryalloc_heap_base
	rdword	_system___gc_tryalloc_tmp002_, __system___gc_tryalloc__cse__0032
	mov	arg01, _system___gc_tryalloc_tmp001_
	mov	arg02, _system___gc_tryalloc_tmp002_
	call	#__system___gc_pageptr
	mov	_system___gc_tryalloc_tmp003_, result1
	mov	__system___gc_tryalloc_ptr, _system___gc_tryalloc_tmp003_ wz
 if_ne	mov	_system___gc_tryalloc_tmp001_, __system___gc_tryalloc_ptr
 if_ne	mov	__system___gc_tryalloc__cse__0033, _system___gc_tryalloc_tmp001_
 if_ne	rdword	__system___gc_tryalloc_availsize, __system___gc_tryalloc__cse__0033
	cmp	__system___gc_tryalloc_ptr, #0 wz
 if_e	jmp	#LR__0011
	cmps	__system___gc_tryalloc_ptr, __system___gc_tryalloc_heap_end wc,wz
 if_ae	jmp	#LR__0011
	cmps	__system___gc_tryalloc_size, __system___gc_tryalloc_availsize wc,wz
 if_a	jmp	#LR__0010
LR__0011
	cmp	__system___gc_tryalloc_ptr, #0 wz
 if_e	mov	result1, __system___gc_tryalloc_ptr
 if_e	jmp	#__system___gc_tryalloc_ret
	mov	__system___gc_tryalloc__cse__0034, __system___gc_tryalloc_ptr
	add	__system___gc_tryalloc__cse__0034, #6
	rdword	__system___gc_tryalloc_linkindex, __system___gc_tryalloc__cse__0034
	cmps	__system___gc_tryalloc_size, __system___gc_tryalloc_availsize wc,wz
 if_ae	jmp	#LR__0013
	mov	__system___gc_tryalloc__cse__0035, __system___gc_tryalloc_ptr
	wrword	__system___gc_tryalloc_size, __system___gc_tryalloc__cse__0035
	mov	__system___gc_tryalloc__cse__0036, __system___gc_tryalloc_size
	shl	__system___gc_tryalloc__cse__0036, #4
	mov	__system___gc_tryalloc__cse__0037, __system___gc_tryalloc_ptr
	add	__system___gc_tryalloc__cse__0037, __system___gc_tryalloc__cse__0036
	mov	__system___gc_tryalloc__cse__0038, __system___gc_tryalloc_availsize
	sub	__system___gc_tryalloc__cse__0038, __system___gc_tryalloc_size
	mov	__system___gc_tryalloc__cse__0039, __system___gc_tryalloc__cse__0037
	wrword	__system___gc_tryalloc__cse__0038, __system___gc_tryalloc__cse__0039
	mov	__system___gc_tryalloc__cse__0040, __system___gc_tryalloc__cse__0037
	add	__system___gc_tryalloc__cse__0040, #2
	mov	_system___gc_tryalloc_tmp001_, imm_27791_
	wrword	_system___gc_tryalloc_tmp001_, __system___gc_tryalloc__cse__0040
	mov	__system___gc_tryalloc__cse__0041, __system___gc_tryalloc__cse__0037
	add	__system___gc_tryalloc__cse__0041, #4
	mov	arg01, __system___gc_tryalloc_heap_base
	mov	arg02, __system___gc_tryalloc_ptr
	call	#__system___gc_pageindex
	wrword	result1, __system___gc_tryalloc__cse__0041
	mov	__system___gc_tryalloc__cse__0042, __system___gc_tryalloc__cse__0037
	rdword	_system___gc_tryalloc_tmp001_, __system___gc_tryalloc__cse__0034
	add	__system___gc_tryalloc__cse__0042, #6
	wrword	_system___gc_tryalloc_tmp001_, __system___gc_tryalloc__cse__0042
	mov	__system___gc_tryalloc_saveptr, __system___gc_tryalloc__cse__0037
	mov	arg01, __system___gc_tryalloc_heap_base
	mov	arg02, __system___gc_tryalloc_saveptr
	call	#__system___gc_pageindex
	mov	__system___gc_tryalloc_linkindex, result1
	mov	arg01, __system___gc_tryalloc__cse__0037
	call	#__system___gc_nextBlockPtr
	mov	__system___gc_tryalloc_nextptr, result1 wz
 if_e	jmp	#LR__0012
	cmps	__system___gc_tryalloc_nextptr, __system___gc_tryalloc_heap_end wc,wz
 if_ae	jmp	#LR__0012
	mov	__system___gc_tryalloc__cse__0043, __system___gc_tryalloc_nextptr
	add	__system___gc_tryalloc__cse__0043, #4
	mov	_system___gc_tryalloc_tmp001_, __system___gc_tryalloc_heap_base
	mov	_system___gc_tryalloc_tmp002_, __system___gc_tryalloc_saveptr
	mov	arg01, _system___gc_tryalloc_tmp001_
	mov	arg02, _system___gc_tryalloc_tmp002_
	call	#__system___gc_pageindex
	mov	_system___gc_tryalloc_tmp003_, result1
	wrword	_system___gc_tryalloc_tmp003_, __system___gc_tryalloc__cse__0043
LR__0012
LR__0013
	add	__system___gc_tryalloc_lastptr, #6
	wrword	__system___gc_tryalloc_linkindex, __system___gc_tryalloc_lastptr
	mov	_system___gc_tryalloc_tmp001_, imm_27776_
	or	_system___gc_tryalloc_tmp001_, __system___gc_tryalloc_reserveflag
	cogid	result1
	or	_system___gc_tryalloc_tmp001_, result1
	mov	__system___gc_tryalloc__cse__0047, __system___gc_tryalloc_ptr
	add	__system___gc_tryalloc__cse__0047, #2
	wrword	_system___gc_tryalloc_tmp001_, __system___gc_tryalloc__cse__0047
	mov	__system___gc_tryalloc__cse__0048, __system___gc_tryalloc_heap_base
	add	__system___gc_tryalloc__cse__0048, #8
	rdword	_system___gc_tryalloc_tmp001_, __system___gc_tryalloc__cse__0048
	wrword	_system___gc_tryalloc_tmp001_, __system___gc_tryalloc__cse__0034
	mov	arg01, __system___gc_tryalloc_heap_base
	mov	arg02, __system___gc_tryalloc_ptr
	call	#__system___gc_pageindex
	wrword	result1, __system___gc_tryalloc__cse__0048
	add	__system___gc_tryalloc_ptr, #8
	or	__system___gc_tryalloc_ptr, imm_1669332992_
	mov	result1, __system___gc_tryalloc_ptr
__system___gc_tryalloc_ret
	ret

__system___gc_errmsg
	mov	__system___gc_errmsg_s, arg01
LR__0014
	rdbyte	__system___gc_errmsg_c, __system___gc_errmsg_s wz
	add	__system___gc_errmsg_s, #1
 if_e	jmp	#LR__0015
	mov	arg01, __system___gc_errmsg_c
	call	#__system___tx
	jmp	#LR__0014
LR__0015
	mov	result1, #0
__system___gc_errmsg_ret
	ret

__system___gc_alloc_managed
	mov	__system___gc_alloc_managed_size, arg01
	mov	arg02, #0
	call	#__system___gc_doalloc
	mov	__system___gc_alloc_managed_r, result1 wz
 if_ne	jmp	#LR__0016
	cmps	__system___gc_alloc_managed_size, #0 wc,wz
 if_be	jmp	#LR__0016
	mov	arg01, ptr_L__0122_
	call	#__system___gc_errmsg
	jmp	#__system___gc_alloc_managed_ret
LR__0016
	mov	result1, __system___gc_alloc_managed_r
__system___gc_alloc_managed_ret
	ret

__system___gc_doalloc
	mov	__system___gc_doalloc_size, arg01 wz
	mov	__system___gc_doalloc_reserveflag, arg02
 if_e	mov	result1, #0
 if_e	jmp	#__system___gc_doalloc_ret
	add	__system___gc_doalloc_size, #23
	andn	__system___gc_doalloc_size, #15
	shr	__system___gc_doalloc_size, #4
	mov	arg01, __system___gc_doalloc_size
	mov	arg02, __system___gc_doalloc_reserveflag
	call	#__system___gc_tryalloc
	mov	__system___gc_doalloc_ptr, result1 wz
 if_ne	jmp	#LR__0017
	call	#__system___gc_collect
	mov	arg01, __system___gc_doalloc_size
	mov	arg02, __system___gc_doalloc_reserveflag
	call	#__system___gc_tryalloc
	mov	__system___gc_doalloc_ptr, result1
LR__0017
	cmp	__system___gc_doalloc_ptr, #0 wz
 if_e	jmp	#LR__0020
	shl	__system___gc_doalloc_size, #4
	sub	__system___gc_doalloc_size, #8
	abs	_system___gc_doalloc_tmp001_, __system___gc_doalloc_size wc
	shr	_system___gc_doalloc_tmp001_, #2
 if_b	neg	_system___gc_doalloc_tmp001_, _system___gc_doalloc_tmp001_
	mov	__system___gc_doalloc__idx__90023, _system___gc_doalloc_tmp001_ wz
	mov	__system___gc_doalloc_zptr, __system___gc_doalloc_ptr
 if_e	jmp	#LR__0019
LR__0018
	mov	_system___gc_doalloc_tmp001_, #0
	wrlong	_system___gc_doalloc_tmp001_, __system___gc_doalloc_zptr
	add	__system___gc_doalloc_zptr, #4
	djnz	__system___gc_doalloc__idx__90023, #LR__0018
LR__0019
LR__0020
	mov	result1, __system___gc_doalloc_ptr
__system___gc_doalloc_ret
	ret

__system___gc_isvalidptr
	mov	_var01, arg03
	mov	_var02, _var01
	and	_var02, imm_4293918720_
	cmp	_var02, imm_1669332992_ wz
 if_ne	mov	result1, #0
 if_ne	jmp	#__system___gc_isvalidptr_ret
	sub	_var01, #8
	mov	_var03, _var01
	andn	_var03, imm_4293918720_
	cmps	_var03, arg01 wc,wz
 if_b	jmp	#LR__0021
	cmps	_var03, arg02 wc,wz
 if_b	jmp	#LR__0022
LR__0021
	mov	result1, #0
	jmp	#__system___gc_isvalidptr_ret
LR__0022
	mov	_var02, _var03
	xor	_var02, arg01
	and	_var02, #15 wz
 if_ne	mov	result1, #0
 if_ne	jmp	#__system___gc_isvalidptr_ret
	mov	_var04, _var03
	add	_var04, #2
	rdword	_var02, _var04
	and	_var02, imm_65472_
	cmp	_var02, imm_27776_ wz
 if_ne	mov	result1, #0
 if_e	mov	result1, _var03
__system___gc_isvalidptr_ret
	ret

__system___gc_dofree
	mov	__system___gc_dofree_ptr, arg01
	call	#__system___gc_ptrs
	mov	__system___gc_dofree_heapend, result2
	mov	__system___gc_dofree_heapbase, result1
	mov	__system___gc_dofree__cse__0062, __system___gc_dofree_ptr
	add	__system___gc_dofree__cse__0062, #2
	mov	_system___gc_dofree_tmp001_, imm_27791_
	wrword	_system___gc_dofree_tmp001_, __system___gc_dofree__cse__0062
	mov	__system___gc_dofree_prevptr, __system___gc_dofree_ptr
	mov	arg01, __system___gc_dofree_ptr
	call	#__system___gc_nextBlockPtr
	mov	__system___gc_dofree_nextptr, result1
LR__0023
	add	__system___gc_dofree_prevptr, #4
	mov	__system___gc_dofree__cse__0063, __system___gc_dofree_prevptr
	rdword	arg02, __system___gc_dofree__cse__0063
	mov	arg01, __system___gc_dofree_heapbase
	call	#__system___gc_pageptr
	mov	__system___gc_dofree_prevptr, result1 wz
 if_e	jmp	#LR__0024
	mov	arg01, __system___gc_dofree_prevptr
	call	#__system___gc_isFree
	mov	_system___gc_dofree_tmp002_, result1 wz
 if_e	jmp	#LR__0023
LR__0024
	cmp	__system___gc_dofree_prevptr, #0 wz
 if_e	mov	__system___gc_dofree_prevptr, __system___gc_dofree_heapbase
	mov	__system___gc_dofree__cse__0064, __system___gc_dofree_prevptr
	add	__system___gc_dofree__cse__0064, #6
	mov	__system___gc_dofree__cse__0065, __system___gc_dofree_ptr
	rdword	_system___gc_dofree_tmp001_, __system___gc_dofree__cse__0064
	add	__system___gc_dofree__cse__0065, #6
	wrword	_system___gc_dofree_tmp001_, __system___gc_dofree__cse__0065
	mov	arg01, __system___gc_dofree_heapbase
	mov	arg02, __system___gc_dofree_ptr
	call	#__system___gc_pageindex
	mov	_system___gc_dofree_tmp003_, result1
	wrword	_system___gc_dofree_tmp003_, __system___gc_dofree__cse__0064
	cmp	__system___gc_dofree_prevptr, __system___gc_dofree_heapbase wz
 if_e	jmp	#LR__0027
	mov	arg01, __system___gc_dofree_prevptr
	call	#__system___gc_nextBlockPtr
	cmp	result1, __system___gc_dofree_ptr wz
 if_ne	jmp	#LR__0026
	mov	__system___gc_dofree__cse__0066, __system___gc_dofree_prevptr
	rdword	__system___gc_dofree__cse__0068, __system___gc_dofree__cse__0066
	mov	__system___gc_dofree__cse__0067, __system___gc_dofree_ptr
	rdword	_system___gc_dofree_tmp002_, __system___gc_dofree__cse__0067
	add	__system___gc_dofree__cse__0068, _system___gc_dofree_tmp002_
	wrword	__system___gc_dofree__cse__0068, __system___gc_dofree__cse__0066
	mov	_system___gc_dofree_tmp001_, #0
	wrword	_system___gc_dofree_tmp001_, __system___gc_dofree__cse__0062
	mov	arg01, __system___gc_dofree_ptr
	call	#__system___gc_nextBlockPtr
	mov	__system___gc_dofree_nextptr, result1
	cmps	__system___gc_dofree_nextptr, __system___gc_dofree_heapend wc,wz
 if_ae	jmp	#LR__0025
	mov	__system___gc_dofree__cse__0069, __system___gc_dofree_nextptr
	add	__system___gc_dofree__cse__0069, #4
	mov	arg01, __system___gc_dofree_heapbase
	mov	arg02, __system___gc_dofree_prevptr
	call	#__system___gc_pageindex
	mov	_system___gc_dofree_tmp003_, result1
	wrword	_system___gc_dofree_tmp003_, __system___gc_dofree__cse__0069
LR__0025
	rdword	_system___gc_dofree_tmp001_, __system___gc_dofree__cse__0065
	wrword	_system___gc_dofree_tmp001_, __system___gc_dofree__cse__0064
	mov	_system___gc_dofree_tmp001_, #0
	wrword	_system___gc_dofree_tmp001_, __system___gc_dofree__cse__0065
	mov	__system___gc_dofree_ptr, __system___gc_dofree_prevptr
LR__0026
LR__0027
	mov	arg01, __system___gc_dofree_ptr
	call	#__system___gc_nextBlockPtr
	mov	__system___gc_dofree_tmpptr, result1 wz
 if_e	jmp	#LR__0029
	cmps	__system___gc_dofree_tmpptr, __system___gc_dofree_heapend wc,wz
 if_ae	jmp	#LR__0029
	mov	arg01, __system___gc_dofree_tmpptr
	call	#__system___gc_isFree
	cmp	result1, #0 wz
 if_e	jmp	#LR__0029
	mov	__system___gc_dofree_prevptr, __system___gc_dofree_ptr
	mov	__system___gc_dofree_ptr, __system___gc_dofree_tmpptr
	mov	__system___gc_dofree__cse__0070, __system___gc_dofree_prevptr
	rdword	__system___gc_dofree__cse__0072, __system___gc_dofree__cse__0070
	mov	__system___gc_dofree__cse__0071, __system___gc_dofree_ptr
	rdword	_system___gc_dofree_tmp002_, __system___gc_dofree__cse__0071
	add	__system___gc_dofree__cse__0072, _system___gc_dofree_tmp002_
	wrword	__system___gc_dofree__cse__0072, __system___gc_dofree__cse__0070
	mov	__system___gc_dofree__cse__0073, __system___gc_dofree_ptr
	add	__system___gc_dofree__cse__0073, #6
	mov	__system___gc_dofree__cse__0074, __system___gc_dofree_prevptr
	rdword	_system___gc_dofree_tmp001_, __system___gc_dofree__cse__0073
	add	__system___gc_dofree__cse__0074, #6
	wrword	_system___gc_dofree_tmp001_, __system___gc_dofree__cse__0074
	mov	__system___gc_dofree__cse__0075, __system___gc_dofree_ptr
	add	__system___gc_dofree__cse__0075, #2
	mov	_system___gc_dofree_tmp001_, #170
	wrword	_system___gc_dofree_tmp001_, __system___gc_dofree__cse__0075
	mov	_system___gc_dofree_tmp001_, #0
	wrword	_system___gc_dofree_tmp001_, __system___gc_dofree__cse__0073
	mov	_system___gc_dofree_tmp001_, __system___gc_dofree_ptr
	mov	arg01, _system___gc_dofree_tmp001_
	call	#__system___gc_nextBlockPtr
	mov	_system___gc_dofree_tmp002_, result1
	mov	__system___gc_dofree_nextptr, _system___gc_dofree_tmp002_ wz
 if_e	jmp	#LR__0028
	cmps	__system___gc_dofree_nextptr, __system___gc_dofree_heapend wc,wz
 if_ae	jmp	#LR__0028
	mov	__system___gc_dofree__cse__0076, __system___gc_dofree_nextptr
	add	__system___gc_dofree__cse__0076, #4
	mov	_system___gc_dofree_tmp001_, __system___gc_dofree_heapbase
	mov	_system___gc_dofree_tmp002_, __system___gc_dofree_prevptr
	mov	arg01, _system___gc_dofree_tmp001_
	mov	arg02, _system___gc_dofree_tmp002_
	call	#__system___gc_pageindex
	mov	_system___gc_dofree_tmp003_, result1
	wrword	_system___gc_dofree_tmp003_, __system___gc_dofree__cse__0076
LR__0028
LR__0029
	mov	result1, __system___gc_dofree_nextptr
__system___gc_dofree_ret
	ret

__system____topofstack
	wrlong	fp, sp
	add	sp, #4
	mov	fp, sp
	add	sp, #12
	add	fp, #4
	wrlong	arg01, fp
	mov	result1, fp
	sub	fp, #4
	mov	sp, fp
	sub	sp, #4
	rdlong	fp, sp
__system____topofstack_ret
	ret

__system___gc_collect
	call	#__system___gc_ptrs
	mov	__system___gc_collect_endheap, result2
	mov	__system___gc_collect_startheap, result1
	mov	arg01, __system___gc_collect_startheap
	call	#__system___gc_nextBlockPtr
	mov	__system___gc_collect_ptr, result1 wz
	cogid	result1
	mov	__system___gc_collect_ourid, result1
 if_e	jmp	#LR__0031
LR__0030
	cmps	__system___gc_collect_ptr, __system___gc_collect_endheap wc,wz
 if_ae	jmp	#LR__0031
	mov	__system___gc_collect__cse__0078, __system___gc_collect_ptr
	add	__system___gc_collect__cse__0078, #2
	rdword	__system___gc_collect__cse__0079, __system___gc_collect__cse__0078
	andn	__system___gc_collect__cse__0079, #32
	wrword	__system___gc_collect__cse__0079, __system___gc_collect__cse__0078
	mov	arg01, __system___gc_collect_ptr
	call	#__system___gc_nextBlockPtr
	mov	__system___gc_collect_ptr, result1 wz
 if_ne	jmp	#LR__0030
LR__0031
	mov	_system___gc_collect_tmp001_, #0
	mov	arg01, #0
	call	#__system____topofstack
	mov	_system___gc_collect_tmp003_, result1
	mov	arg01, _system___gc_collect_tmp001_
	mov	arg02, _system___gc_collect_tmp003_
	call	#__system___gc_markhub
	call	#__system___gc_ptrs
	mov	__system___gc_markcog_heap_base, result1
	mov	__system___gc_markcog_heap_end, result2
	mov	__system___gc_markcog_cogaddr, #0
LR__0032
	mov	_system___gc_markcog_tmp002_, #496
	sub	_system___gc_markcog_tmp002_, __system___gc_markcog_cogaddr
	mov	_system___gc_markcog_tmp001_, #496
	add	_system___gc_markcog_tmp001_, _system___gc_markcog_tmp002_
	'.live	__system___gc_markcog_ptr
	movs	wrcog, _system___gc_markcog_tmp001_
	movd	wrcog, #__system___gc_markcog_ptr
	call	#wrcog
	mov	arg02, __system___gc_markcog_heap_end
	mov	arg01, __system___gc_markcog_heap_base
	mov	arg03, __system___gc_markcog_ptr
	call	#__system___gc_isvalidptr
	mov	__system___gc_markcog_ptr, result1 wz
 if_e	jmp	#LR__0033
	mov	__system___gc_markcog__cse__0086, __system___gc_markcog_ptr
	add	__system___gc_markcog__cse__0086, #2
	rdword	__system___gc_markcog__cse__0087, __system___gc_markcog__cse__0086
	or	__system___gc_markcog__cse__0087, #32
	wrword	__system___gc_markcog__cse__0087, __system___gc_markcog__cse__0086
LR__0033
	add	__system___gc_markcog_cogaddr, #1
	cmps	__system___gc_markcog_cogaddr, #496 wc,wz
 if_b	jmp	#LR__0032
	mov	arg01, __system___gc_collect_startheap
	call	#__system___gc_nextBlockPtr
	mov	__system___gc_collect_nextptr, result1 wz
 if_ne	jmp	#LR__0034
	mov	arg01, ptr_L__0148_
	call	#__system___gc_errmsg
	jmp	#__system___gc_collect_ret
LR__0034
LR__0035
	mov	__system___gc_collect_ptr, __system___gc_collect_nextptr
	mov	arg01, __system___gc_collect_ptr
	call	#__system___gc_nextBlockPtr
	mov	__system___gc_collect_nextptr, result1
	mov	__system___gc_collect__cse__0080, __system___gc_collect_ptr
	add	__system___gc_collect__cse__0080, #2
	rdword	__system___gc_collect_flags, __system___gc_collect__cse__0080
	test	__system___gc_collect_flags, #32 wz
 if_ne	jmp	#LR__0038
	mov	_system___gc_collect_tmp002_, __system___gc_collect_flags
	and	_system___gc_collect_tmp002_, #16 wz
 if_ne	jmp	#LR__0038
	mov	_system___gc_collect_tmp001_, __system___gc_collect_flags
	and	_system___gc_collect_tmp001_, #15
	mov	__system___gc_collect_flags, _system___gc_collect_tmp001_
	cmp	__system___gc_collect_flags, __system___gc_collect_ourid wz
 if_e	jmp	#LR__0036
	cmp	__system___gc_collect_flags, #14 wz
 if_ne	jmp	#LR__0037
LR__0036
	mov	_system___gc_collect_tmp001_, __system___gc_collect_ptr
	mov	arg01, _system___gc_collect_tmp001_
	call	#__system___gc_dofree
	mov	_system___gc_collect_tmp002_, result1
	mov	__system___gc_collect_nextptr, _system___gc_collect_tmp002_
LR__0037
LR__0038
	cmp	__system___gc_collect_nextptr, #0 wz
 if_e	jmp	#LR__0039
	cmps	__system___gc_collect_nextptr, __system___gc_collect_endheap wc,wz
 if_b	jmp	#LR__0035
LR__0039
__system___gc_collect_ret
	ret

__system___gc_markhub
	mov	__system___gc_markhub_startaddr, arg01
	mov	__system___gc_markhub_endaddr, arg02
	call	#__system___gc_ptrs
	mov	__system___gc_markhub_heap_base, result1
	mov	__system___gc_markhub_heap_end, result2
LR__0040
	cmps	__system___gc_markhub_startaddr, __system___gc_markhub_endaddr wc,wz
 if_ae	jmp	#LR__0041
	rdlong	arg03, __system___gc_markhub_startaddr
	add	__system___gc_markhub_startaddr, #4
	mov	arg01, __system___gc_markhub_heap_base
	mov	arg02, __system___gc_markhub_heap_end
	call	#__system___gc_isvalidptr
	mov	__system___gc_markhub_ptr, result1 wz
 if_e	jmp	#LR__0040
	mov	arg01, __system___gc_markhub_ptr
	call	#__system___gc_isFree
	mov	_system___gc_markhub_tmp002_, result1 wz
 if_ne	jmp	#LR__0040
	mov	__system___gc_markhub__cse__0083, __system___gc_markhub_ptr
	add	__system___gc_markhub__cse__0083, #2
	rdword	__system___gc_markhub_flags, __system___gc_markhub__cse__0083
	andn	__system___gc_markhub_flags, #15
	or	__system___gc_markhub_flags, #46
	wrword	__system___gc_markhub_flags, __system___gc_markhub__cse__0083
	jmp	#LR__0040
LR__0041
__system___gc_markhub_ret
	ret
wrcog
    mov    0-0, 0-0
wrcog_ret
    ret

__heap_ptr
	long	@@@__heap_base
fp
	long	0
imm_1008_
	long	1008
imm_1073741824_
	long	1073741824
imm_1669332992_
	long	1669332992
imm_27776_
	long	27776
imm_27791_
	long	27791
imm_27792_
	long	27792
imm_4293918720_
	long	-1048576
imm_65472_
	long	65472
ptr_L__0110_
	long	@@@LR__0042
ptr_L__0122_
	long	@@@LR__0043
ptr_L__0148_
	long	@@@LR__0044
ptr___system__dat__
	long	@@@__system__dat_
ptr__dat__
	long	@@@_dat_
result1
	long	0
result2
	long	0
sp
	long	@@@stackspace
COG_BSS_START
	fit	496

LR__0042
	byte	" !!! corrupted heap??? !!! "
	byte	0
LR__0043
	byte	" !!! out of memory !!! "
	byte	0
LR__0044
	byte	" !!! corrupted heap !!! "
	byte	0
	long
_dat_
	byte	$00[20]
	long
__system__dat_
	byte	$b6, $02, $00, $00, $f0, $09, $bc, $0a, $00, $00, $68, $5c, $01, $08, $fc, $0c
	byte	$03, $08, $7c, $0c, $00, $00, $00, $00, $03, $00, $00, $00
__heap_base
	long	0[258]
stackspace
	long	0[1]
	org	COG_BSS_START
__system___gc_alloc_managed_r
	res	1
__system___gc_alloc_managed_size
	res	1
__system___gc_collect__cse__0078
	res	1
__system___gc_collect__cse__0079
	res	1
__system___gc_collect__cse__0080
	res	1
__system___gc_collect_endheap
	res	1
__system___gc_collect_flags
	res	1
__system___gc_collect_nextptr
	res	1
__system___gc_collect_ourid
	res	1
__system___gc_collect_ptr
	res	1
__system___gc_collect_startheap
	res	1
__system___gc_doalloc__idx__90023
	res	1
__system___gc_doalloc_ptr
	res	1
__system___gc_doalloc_reserveflag
	res	1
__system___gc_doalloc_size
	res	1
__system___gc_doalloc_zptr
	res	1
__system___gc_dofree__cse__0062
	res	1
__system___gc_dofree__cse__0063
	res	1
__system___gc_dofree__cse__0064
	res	1
__system___gc_dofree__cse__0065
	res	1
__system___gc_dofree__cse__0066
	res	1
__system___gc_dofree__cse__0067
	res	1
__system___gc_dofree__cse__0068
	res	1
__system___gc_dofree__cse__0069
	res	1
__system___gc_dofree__cse__0070
	res	1
__system___gc_dofree__cse__0071
	res	1
__system___gc_dofree__cse__0072
	res	1
__system___gc_dofree__cse__0073
	res	1
__system___gc_dofree__cse__0074
	res	1
__system___gc_dofree__cse__0075
	res	1
__system___gc_dofree__cse__0076
	res	1
__system___gc_dofree_heapbase
	res	1
__system___gc_dofree_heapend
	res	1
__system___gc_dofree_nextptr
	res	1
__system___gc_dofree_prevptr
	res	1
__system___gc_dofree_ptr
	res	1
__system___gc_dofree_tmpptr
	res	1
__system___gc_errmsg_c
	res	1
__system___gc_errmsg_s
	res	1
__system___gc_markcog__cse__0086
	res	1
__system___gc_markcog__cse__0087
	res	1
__system___gc_markcog_cogaddr
	res	1
__system___gc_markcog_heap_base
	res	1
__system___gc_markcog_heap_end
	res	1
__system___gc_markcog_ptr
	res	1
__system___gc_markhub__cse__0083
	res	1
__system___gc_markhub_endaddr
	res	1
__system___gc_markhub_flags
	res	1
__system___gc_markhub_heap_base
	res	1
__system___gc_markhub_heap_end
	res	1
__system___gc_markhub_ptr
	res	1
__system___gc_markhub_startaddr
	res	1
__system___gc_nextBlockPtr_ptr
	res	1
__system___gc_nextBlockPtr_t
	res	1
__system___gc_tryalloc__cse__0032
	res	1
__system___gc_tryalloc__cse__0033
	res	1
__system___gc_tryalloc__cse__0034
	res	1
__system___gc_tryalloc__cse__0035
	res	1
__system___gc_tryalloc__cse__0036
	res	1
__system___gc_tryalloc__cse__0037
	res	1
__system___gc_tryalloc__cse__0038
	res	1
__system___gc_tryalloc__cse__0039
	res	1
__system___gc_tryalloc__cse__0040
	res	1
__system___gc_tryalloc__cse__0041
	res	1
__system___gc_tryalloc__cse__0042
	res	1
__system___gc_tryalloc__cse__0043
	res	1
__system___gc_tryalloc__cse__0047
	res	1
__system___gc_tryalloc__cse__0048
	res	1
__system___gc_tryalloc_availsize
	res	1
__system___gc_tryalloc_heap_base
	res	1
__system___gc_tryalloc_heap_end
	res	1
__system___gc_tryalloc_lastptr
	res	1
__system___gc_tryalloc_linkindex
	res	1
__system___gc_tryalloc_nextptr
	res	1
__system___gc_tryalloc_ptr
	res	1
__system___gc_tryalloc_reserveflag
	res	1
__system___gc_tryalloc_saveptr
	res	1
__system___gc_tryalloc_size
	res	1
__system___tx_c
	res	1
__system___txraw__idx__90001
	res	1
__system___txraw_bitcycles
	res	1
__system___txraw_nextcnt
	res	1
__system___txraw_val
	res	1
_fetchv__cse__0001
	res	1
_system___gc_collect_tmp001_
	res	1
_system___gc_collect_tmp002_
	res	1
_system___gc_collect_tmp003_
	res	1
_system___gc_doalloc_tmp001_
	res	1
_system___gc_dofree_tmp001_
	res	1
_system___gc_dofree_tmp002_
	res	1
_system___gc_dofree_tmp003_
	res	1
_system___gc_markcog_tmp001_
	res	1
_system___gc_markcog_tmp002_
	res	1
_system___gc_markhub_tmp002_
	res	1
_system___gc_tryalloc_tmp001_
	res	1
_system___gc_tryalloc_tmp002_
	res	1
_system___gc_tryalloc_tmp003_
	res	1
_system___tx_tmp001_
	res	1
_var01
	res	1
_var02
	res	1
_var03
	res	1
_var04
	res	1
_var05
	res	1
_var06
	res	1
_var07
	res	1
_var08
	res	1
_var09
	res	1
_var10
	res	1
arg01
	res	1
arg02
	res	1
arg03
	res	1
	fit	496

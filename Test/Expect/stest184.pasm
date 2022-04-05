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
	mov	_fetchv__cse__0000, ptr__dat__
	mov	arg01, #20
	call	#__system___gc_alloc_managed
	mov	arg01, result1
	mov	arg02, _fetchv__cse__0000
	mov	arg03, #20
	call	#__system____builtin_memmove
	add	result1, #12
	rdlong	result1, result1
_fetchv_ret
	ret

__system___txraw
	mov	__system___txraw_c, arg01
	rdlong	__system___txraw_bitcycles, ptr___system__dat__ wz
 if_ne	jmp	#LR__0001
	mov	arg01, imm_115200_
	call	#__system___setbaud
	mov	__system___txraw_bitcycles, result1
LR__0001
	or	outa, imm_1073741824_
	or	dira, imm_1073741824_
	or	__system___txraw_c, #256
	shl	__system___txraw_c, #1
	mov	__system___txraw_nextcnt, cnt
	mov	__system___txraw__idx__0003, #10
LR__0002
	add	__system___txraw_nextcnt, __system___txraw_bitcycles
	mov	arg01, __system___txraw_nextcnt
	waitcnt	arg01, #0
	shr	__system___txraw_c, #1 wc
	muxc	outa, imm_1073741824_
	djnz	__system___txraw__idx__0003, #LR__0002
	add	__system___txraw_nextcnt, __system___txraw_bitcycles
	waitcnt	__system___txraw_nextcnt, #0
	or	outa, imm_1073741824_
	andn	dira, imm_1073741824_
	mov	result1, #1
__system___txraw_ret
	ret

__system___setbaud
	rdlong	muldiva_, #0
	mov	muldivb_, arg01
	call	#divide_
	wrlong	muldivb_, ptr___system__dat__
	mov	result1, muldivb_
__system___setbaud_ret
	ret

__system____builtin_memmove
	mov	_var01, arg01
	cmps	arg01, arg02 wc
 if_ae	jmp	#LR__0004
	mov	_var02, arg03 wz
 if_e	jmp	#LR__0007
LR__0003
	rdbyte	result1, arg02
	wrbyte	result1, arg01
	add	arg01, #1
	add	arg02, #1
	djnz	_var02, #LR__0003
	jmp	#LR__0007
LR__0004
	add	arg01, arg03
	add	arg02, arg03
	mov	_var03, arg03 wz
 if_e	jmp	#LR__0006
LR__0005
	sub	arg01, #1
	sub	arg02, #1
	rdbyte	_var04, arg02
	wrbyte	_var04, arg01
	djnz	_var03, #LR__0005
LR__0006
LR__0007
	mov	result1, _var01
__system____builtin_memmove_ret
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

__system___tx
	mov	__system___tx_c, arg01
	cmp	__system___tx_c, #10 wz
 if_ne	jmp	#LR__0008
	add	ptr___system__dat__, #24
	rdlong	_system___tx_tmp001_, ptr___system__dat__
	sub	ptr___system__dat__, #24
	test	_system___tx_tmp001_, #2 wz
 if_ne	mov	arg01, #13
 if_ne	call	#__system___txraw
LR__0008
	mov	arg01, __system___tx_c
	call	#__system___txraw
__system___tx_ret
	ret

__system___gc_ptrs
	mov	__system___gc_ptrs_base, __heap_ptr
	mov	__system___gc_ptrs_end, __system___gc_ptrs_base
	add	__system___gc_ptrs_end, imm_1016_
	rdlong	result2, __system___gc_ptrs_base wz
 if_ne	jmp	#LR__0009
	mov	result2, __system___gc_ptrs_end
	sub	result2, __system___gc_ptrs_base
	mov	result1, #1
	wrword	result1, __system___gc_ptrs_base
	mov	result1, __system___gc_ptrs_base
	add	result1, #2
	mov	_system___gc_ptrs_tmp001_, imm_27792_
	wrword	_system___gc_ptrs_tmp001_, result1
	mov	result1, __system___gc_ptrs_base
	add	result1, #4
	mov	_system___gc_ptrs_tmp001_, #0
	wrword	_system___gc_ptrs_tmp001_, result1
	mov	result1, __system___gc_ptrs_base
	add	result1, #6
	mov	_system___gc_ptrs_tmp001_, #1
	wrword	_system___gc_ptrs_tmp001_, result1
	add	__system___gc_ptrs_base, #16
	abs	_system___gc_ptrs_tmp001_, result2 wc
	shr	_system___gc_ptrs_tmp001_, #4
	negc	_system___gc_ptrs_tmp001_, _system___gc_ptrs_tmp001_
	wrword	_system___gc_ptrs_tmp001_, __system___gc_ptrs_base
	mov	result2, __system___gc_ptrs_base
	add	result2, #2
	mov	_system___gc_ptrs_tmp001_, imm_27791_
	wrword	_system___gc_ptrs_tmp001_, result2
	mov	result2, __system___gc_ptrs_base
	add	result2, #4
	mov	_system___gc_ptrs_tmp001_, #0
	wrword	_system___gc_ptrs_tmp001_, result2
	mov	result2, __system___gc_ptrs_base
	add	result2, #6
	wrword	_system___gc_ptrs_tmp001_, result2
	sub	__system___gc_ptrs_base, #16
LR__0009
	mov	result2, __system___gc_ptrs_end
	mov	result1, __system___gc_ptrs_base
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
	mov	result1, #0
	add	arg01, #2
	rdword	arg01, arg01
	cmp	arg01, imm_27791_ wz
 if_e	neg	result1, #1
__system___gc_isFree_ret
	ret

__system___gc_nextBlockPtr
	mov	__system___gc_nextBlockPtr_ptr, arg01
	rdword	__system___gc_nextBlockPtr_t, __system___gc_nextBlockPtr_ptr wz
 if_ne	jmp	#LR__0010
	mov	arg01, ptr_L__0031_
	call	#__system___gc_errmsg
	jmp	#__system___gc_nextBlockPtr_ret
LR__0010
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
LR__0011
	mov	__system___gc_tryalloc_lastptr, __system___gc_tryalloc_ptr
	add	__system___gc_tryalloc_ptr, #6
	mov	__system___gc_tryalloc__cse__0000, __system___gc_tryalloc_ptr
	rdword	_system___gc_tryalloc_tmp002_, __system___gc_tryalloc__cse__0000
	mov	arg01, __system___gc_tryalloc_heap_base
	mov	arg02, _system___gc_tryalloc_tmp002_
	call	#__system___gc_pageptr
	mov	_system___gc_tryalloc_tmp001_, result1
	mov	__system___gc_tryalloc_ptr, _system___gc_tryalloc_tmp001_ wz
 if_ne	mov	_system___gc_tryalloc_tmp001_, __system___gc_tryalloc_ptr
 if_ne	mov	__system___gc_tryalloc__cse__0001, _system___gc_tryalloc_tmp001_
 if_ne	rdword	__system___gc_tryalloc_availsize, __system___gc_tryalloc__cse__0001
	cmp	__system___gc_tryalloc_ptr, #0 wz
 if_ne	cmps	__system___gc_tryalloc_ptr, __system___gc_tryalloc_heap_end wc
 if_a	jmp	#LR__0012
 if_ne	cmps	__system___gc_tryalloc_size, __system___gc_tryalloc_availsize wc,wz
 if_a	jmp	#LR__0011
LR__0012
	cmp	__system___gc_tryalloc_ptr, #0 wz
 if_e	mov	result1, __system___gc_tryalloc_ptr
 if_e	jmp	#__system___gc_tryalloc_ret
	mov	__system___gc_tryalloc__cse__0002, __system___gc_tryalloc_ptr
	add	__system___gc_tryalloc__cse__0002, #6
	rdword	__system___gc_tryalloc_linkindex, __system___gc_tryalloc__cse__0002
	cmps	__system___gc_tryalloc_size, __system___gc_tryalloc_availsize wc
 if_ae	jmp	#LR__0014
	mov	__system___gc_tryalloc__cse__0003, __system___gc_tryalloc_ptr
	wrword	__system___gc_tryalloc_size, __system___gc_tryalloc__cse__0003
	mov	__system___gc_tryalloc__cse__0004, __system___gc_tryalloc_size
	shl	__system___gc_tryalloc__cse__0004, #4
	mov	__system___gc_tryalloc__cse__0005, __system___gc_tryalloc_ptr
	add	__system___gc_tryalloc__cse__0005, __system___gc_tryalloc__cse__0004
	mov	__system___gc_tryalloc__cse__0006, __system___gc_tryalloc_availsize
	sub	__system___gc_tryalloc__cse__0006, __system___gc_tryalloc_size
	mov	__system___gc_tryalloc__cse__0007, __system___gc_tryalloc__cse__0005
	wrword	__system___gc_tryalloc__cse__0006, __system___gc_tryalloc__cse__0007
	mov	__system___gc_tryalloc__cse__0008, __system___gc_tryalloc__cse__0005
	add	__system___gc_tryalloc__cse__0008, #2
	mov	_system___gc_tryalloc_tmp001_, imm_27791_
	wrword	_system___gc_tryalloc_tmp001_, __system___gc_tryalloc__cse__0008
	mov	__system___gc_tryalloc__cse__0009, __system___gc_tryalloc__cse__0005
	add	__system___gc_tryalloc__cse__0009, #4
	mov	arg01, __system___gc_tryalloc_heap_base
	mov	arg02, __system___gc_tryalloc_ptr
	call	#__system___gc_pageindex
	wrword	result1, __system___gc_tryalloc__cse__0009
	mov	__system___gc_tryalloc__cse__0010, __system___gc_tryalloc__cse__0005
	rdword	_system___gc_tryalloc_tmp001_, __system___gc_tryalloc__cse__0002
	add	__system___gc_tryalloc__cse__0010, #6
	wrword	_system___gc_tryalloc_tmp001_, __system___gc_tryalloc__cse__0010
	mov	__system___gc_tryalloc_saveptr, __system___gc_tryalloc__cse__0005
	mov	arg01, __system___gc_tryalloc_heap_base
	mov	arg02, __system___gc_tryalloc_saveptr
	call	#__system___gc_pageindex
	mov	__system___gc_tryalloc_linkindex, result1
	mov	arg01, __system___gc_tryalloc__cse__0005
	call	#__system___gc_nextBlockPtr
	mov	__system___gc_tryalloc_nextptr, result1 wz
 if_e	jmp	#LR__0013
	cmps	__system___gc_tryalloc_nextptr, __system___gc_tryalloc_heap_end wc
 if_ae	jmp	#LR__0013
	mov	__system___gc_tryalloc__cse__0011, __system___gc_tryalloc_nextptr
	add	__system___gc_tryalloc__cse__0011, #4
	mov	_system___gc_tryalloc_tmp002_, __system___gc_tryalloc_saveptr
	mov	arg01, __system___gc_tryalloc_heap_base
	mov	arg02, _system___gc_tryalloc_tmp002_
	call	#__system___gc_pageindex
	mov	_system___gc_tryalloc_tmp001_, result1
	wrword	_system___gc_tryalloc_tmp001_, __system___gc_tryalloc__cse__0011
LR__0013
LR__0014
	add	__system___gc_tryalloc_lastptr, #6
	wrword	__system___gc_tryalloc_linkindex, __system___gc_tryalloc_lastptr
	mov	_system___gc_tryalloc_tmp001_, imm_27776_
	or	_system___gc_tryalloc_tmp001_, __system___gc_tryalloc_reserveflag
	mov	__system___gc_tryalloc__cse__0011, __system___gc_tryalloc_ptr
	add	__system___gc_tryalloc__cse__0011, #2
	cogid	result1
	or	_system___gc_tryalloc_tmp001_, result1
	wrword	_system___gc_tryalloc_tmp001_, __system___gc_tryalloc__cse__0011
	mov	__system___gc_tryalloc__cse__0011, __system___gc_tryalloc_heap_base
	add	__system___gc_tryalloc__cse__0011, #8
	rdword	_system___gc_tryalloc_tmp001_, __system___gc_tryalloc__cse__0011
	wrword	_system___gc_tryalloc_tmp001_, __system___gc_tryalloc__cse__0002
	mov	arg01, __system___gc_tryalloc_heap_base
	mov	arg02, __system___gc_tryalloc_ptr
	call	#__system___gc_pageindex
	wrword	result1, __system___gc_tryalloc__cse__0011
	add	__system___gc_tryalloc_ptr, #8
	or	__system___gc_tryalloc_ptr, imm_1669332992_
	mov	result1, __system___gc_tryalloc_ptr
__system___gc_tryalloc_ret
	ret

__system___gc_errmsg
	mov	__system___gc_errmsg_s, arg01
LR__0015
	rdbyte	arg01, __system___gc_errmsg_s wz
	add	__system___gc_errmsg_s, #1
 if_e	jmp	#LR__0016
	call	#__system___tx
	jmp	#LR__0015
LR__0016
	mov	result1, #0
__system___gc_errmsg_ret
	ret

__system___gc_alloc_managed
	mov	__system___gc_alloc_managed_size, arg01
	mov	arg02, #0
	call	#__system___gc_doalloc
	mov	__system___gc_alloc_managed_r, result1 wz
 if_ne	jmp	#LR__0017
	cmps	__system___gc_alloc_managed_size, #1 wc
 if_b	jmp	#LR__0017
	mov	arg01, ptr_L__0043_
	call	#__system___gc_errmsg
	jmp	#__system___gc_alloc_managed_ret
LR__0017
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
	add	ptr___system__dat__, #36
	mov	__system___gc_doalloc__cse__0005, ptr___system__dat__
	cogid	result1
	add	result1, #256
	sub	ptr___system__dat__, #36
LR__0018
	rdlong	arg01, __system___gc_doalloc__cse__0005 wz
 if_e	wrlong	result1, __system___gc_doalloc__cse__0005
 if_e	rdlong	arg01, __system___gc_doalloc__cse__0005
 if_e	rdlong	arg01, __system___gc_doalloc__cse__0005
	cmp	arg01, result1 wz
 if_ne	jmp	#LR__0018
	mov	arg01, __system___gc_doalloc_size
	mov	arg02, __system___gc_doalloc_reserveflag
	call	#__system___gc_tryalloc
	mov	__system___gc_doalloc_ptr, result1 wz
 if_ne	jmp	#LR__0019
	call	#__system___gc_docollect
	mov	arg01, __system___gc_doalloc_size
	mov	arg02, __system___gc_doalloc_reserveflag
	call	#__system___gc_tryalloc
	mov	__system___gc_doalloc_ptr, result1
LR__0019
	mov	__system___gc_doalloc_reserveflag, #0
	wrlong	__system___gc_doalloc_reserveflag, __system___gc_doalloc__cse__0005
	cmp	__system___gc_doalloc_ptr, #0 wz
 if_e	jmp	#LR__0022
	shl	__system___gc_doalloc_size, #4
	sub	__system___gc_doalloc_size, #8
	abs	_system___gc_doalloc_tmp001_, __system___gc_doalloc_size wc
	shr	_system___gc_doalloc_tmp001_, #2
	negc	__system___gc_doalloc__idx__0000, _system___gc_doalloc_tmp001_ wz
	mov	__system___gc_doalloc_zptr, __system___gc_doalloc_ptr
 if_e	jmp	#LR__0021
LR__0020
	mov	_system___gc_doalloc_tmp001_, #0
	wrlong	_system___gc_doalloc_tmp001_, __system___gc_doalloc_zptr
	add	__system___gc_doalloc_zptr, #4
	djnz	__system___gc_doalloc__idx__0000, #LR__0020
LR__0021
LR__0022
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
	cmps	_var03, arg01 wc
 if_b	jmp	#LR__0023
	cmps	_var03, arg02 wc
 if_b	jmp	#LR__0024
LR__0023
	mov	result1, #0
	jmp	#__system___gc_isvalidptr_ret
LR__0024
	mov	_var02, _var03
	xor	_var02, arg01
	and	_var02, #15 wz
 if_ne	mov	result1, #0
 if_ne	jmp	#__system___gc_isvalidptr_ret
	mov	_var02, _var03
	add	_var02, #2
	rdword	_var02, _var02
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
	mov	__system___gc_dofree__cse__0000, __system___gc_dofree_ptr
	add	__system___gc_dofree__cse__0000, #2
	mov	result1, imm_27791_
	wrword	result1, __system___gc_dofree__cse__0000
	mov	__system___gc_dofree_prevptr, __system___gc_dofree_ptr
	mov	arg01, __system___gc_dofree_ptr
	call	#__system___gc_nextBlockPtr
	mov	__system___gc_dofree_nextptr, result1
LR__0025
	add	__system___gc_dofree_prevptr, #4
	mov	__system___gc_dofree__cse__0001, __system___gc_dofree_prevptr
	rdword	arg02, __system___gc_dofree__cse__0001
	mov	arg01, __system___gc_dofree_heapbase
	call	#__system___gc_pageptr
	mov	__system___gc_dofree_prevptr, result1 wz
 if_e	jmp	#LR__0026
	mov	arg01, __system___gc_dofree_prevptr
	call	#__system___gc_isFree
	mov	arg02, result1 wz
 if_e	jmp	#LR__0025
LR__0026
	cmp	__system___gc_dofree_prevptr, #0 wz
 if_e	mov	__system___gc_dofree_prevptr, __system___gc_dofree_heapbase
	mov	__system___gc_dofree__cse__0002, __system___gc_dofree_prevptr
	add	__system___gc_dofree__cse__0002, #6
	mov	__system___gc_dofree__cse__0003, __system___gc_dofree_ptr
	rdword	arg02, __system___gc_dofree__cse__0002
	add	__system___gc_dofree__cse__0003, #6
	wrword	arg02, __system___gc_dofree__cse__0003
	mov	arg01, __system___gc_dofree_heapbase
	mov	arg02, __system___gc_dofree_ptr
	call	#__system___gc_pageindex
	wrword	result1, __system___gc_dofree__cse__0002
	cmp	__system___gc_dofree_prevptr, __system___gc_dofree_heapbase wz
 if_e	jmp	#LR__0029
	mov	arg01, __system___gc_dofree_prevptr
	call	#__system___gc_nextBlockPtr
	cmp	result1, __system___gc_dofree_ptr wz
 if_ne	jmp	#LR__0028
	mov	__system___gc_dofree__cse__0004, __system___gc_dofree_prevptr
	rdword	__system___gc_dofree__cse__0006, __system___gc_dofree__cse__0004
	mov	__system___gc_dofree__cse__0005, __system___gc_dofree_ptr
	rdword	__system___gc_dofree_nextptr, __system___gc_dofree__cse__0005
	add	__system___gc_dofree__cse__0006, __system___gc_dofree_nextptr
	wrword	__system___gc_dofree__cse__0006, __system___gc_dofree__cse__0004
	mov	__system___gc_dofree_nextptr, #0
	wrword	__system___gc_dofree_nextptr, __system___gc_dofree__cse__0000
	mov	arg01, __system___gc_dofree_ptr
	call	#__system___gc_nextBlockPtr
	mov	__system___gc_dofree_nextptr, result1
	cmps	__system___gc_dofree_nextptr, __system___gc_dofree_heapend wc
 if_ae	jmp	#LR__0027
	mov	__system___gc_dofree__cse__0007, __system___gc_dofree_nextptr
	add	__system___gc_dofree__cse__0007, #4
	mov	arg01, __system___gc_dofree_heapbase
	mov	arg02, __system___gc_dofree_prevptr
	call	#__system___gc_pageindex
	wrword	result1, __system___gc_dofree__cse__0007
LR__0027
	rdword	__system___gc_dofree_ptr, __system___gc_dofree__cse__0003
	wrword	__system___gc_dofree_ptr, __system___gc_dofree__cse__0002
	mov	__system___gc_dofree_ptr, #0
	wrword	__system___gc_dofree_ptr, __system___gc_dofree__cse__0003
	mov	__system___gc_dofree_ptr, __system___gc_dofree_prevptr
LR__0028
LR__0029
	mov	arg01, __system___gc_dofree_ptr
	call	#__system___gc_nextBlockPtr
	mov	__system___gc_dofree_tmpptr, result1 wz
 if_e	jmp	#LR__0031
	cmps	__system___gc_dofree_tmpptr, __system___gc_dofree_heapend wc
 if_ae	jmp	#LR__0031
	mov	arg01, __system___gc_dofree_tmpptr
	call	#__system___gc_isFree
	cmp	result1, #0 wz
 if_e	jmp	#LR__0031
	mov	__system___gc_dofree_prevptr, __system___gc_dofree_ptr
	mov	__system___gc_dofree_ptr, __system___gc_dofree_tmpptr
	mov	__system___gc_dofree__cse__0008, __system___gc_dofree_prevptr
	rdword	__system___gc_dofree__cse__0010, __system___gc_dofree__cse__0008
	mov	__system___gc_dofree__cse__0009, __system___gc_dofree_ptr
	rdword	_system___gc_dofree_tmp002_, __system___gc_dofree__cse__0009
	add	__system___gc_dofree__cse__0010, _system___gc_dofree_tmp002_
	wrword	__system___gc_dofree__cse__0010, __system___gc_dofree__cse__0008
	mov	__system___gc_dofree__cse__0011, __system___gc_dofree_ptr
	add	__system___gc_dofree__cse__0011, #6
	mov	__system___gc_dofree__cse__0012, __system___gc_dofree_prevptr
	rdword	__system___gc_dofree_nextptr, __system___gc_dofree__cse__0011
	add	__system___gc_dofree__cse__0012, #6
	wrword	__system___gc_dofree_nextptr, __system___gc_dofree__cse__0012
	mov	__system___gc_dofree__cse__0013, __system___gc_dofree_ptr
	add	__system___gc_dofree__cse__0013, #2
	mov	__system___gc_dofree_nextptr, #170
	wrword	__system___gc_dofree_nextptr, __system___gc_dofree__cse__0013
	mov	__system___gc_dofree_nextptr, #0
	wrword	__system___gc_dofree_nextptr, __system___gc_dofree__cse__0011
	mov	arg01, __system___gc_dofree_ptr
	call	#__system___gc_nextBlockPtr
	mov	_system___gc_dofree_tmp001_, result1
	mov	__system___gc_dofree_nextptr, _system___gc_dofree_tmp001_ wz
 if_e	jmp	#LR__0030
	cmps	__system___gc_dofree_nextptr, __system___gc_dofree_heapend wc
 if_ae	jmp	#LR__0030
	mov	__system___gc_dofree__cse__0014, __system___gc_dofree_nextptr
	add	__system___gc_dofree__cse__0014, #4
	mov	_system___gc_dofree_tmp002_, __system___gc_dofree_prevptr
	mov	arg01, __system___gc_dofree_heapbase
	mov	arg02, _system___gc_dofree_tmp002_
	call	#__system___gc_pageindex
	mov	_system___gc_dofree_tmp001_, result1
	wrword	_system___gc_dofree_tmp001_, __system___gc_dofree__cse__0014
LR__0030
LR__0031
	mov	result1, __system___gc_dofree_nextptr
__system___gc_dofree_ret
	ret

__system___gc_docollect
	call	#__system___gc_ptrs
	mov	__system___gc_docollect_endheap, result2
	mov	__system___gc_docollect_startheap, result1
	mov	arg01, __system___gc_docollect_startheap
	call	#__system___gc_nextBlockPtr
	mov	__system___gc_docollect_ptr, result1 wz
	cogid	result1
	mov	__system___gc_docollect_ourid, result1
 if_e	jmp	#LR__0033
LR__0032
	cmps	__system___gc_docollect_ptr, __system___gc_docollect_endheap wc
 if_ae	jmp	#LR__0033
	mov	__system___gc_docollect__cse__0000, __system___gc_docollect_ptr
	add	__system___gc_docollect__cse__0000, #2
	rdword	__system___gc_docollect__cse__0001, __system___gc_docollect__cse__0000
	andn	__system___gc_docollect__cse__0001, #32
	wrword	__system___gc_docollect__cse__0001, __system___gc_docollect__cse__0000
	mov	arg01, __system___gc_docollect_ptr
	call	#__system___gc_nextBlockPtr
	mov	__system___gc_docollect_ptr, result1 wz
 if_ne	jmp	#LR__0032
LR__0033
	mov	_system___gc_docollect_tmp001_, #0
	mov	arg01, #0
	call	#__system____topofstack
	mov	arg02, result1
	mov	arg01, _system___gc_docollect_tmp001_
	call	#__system___gc_markhub
	call	#__system___gc_ptrs
	mov	__system___gc_markcog_heap_base, result1
	mov	__system___gc_markcog_heap_end, result2
	mov	__system___gc_markcog_cogaddr, #0
LR__0034
	mov	_system___gc_markcog_tmp001_, #496
	sub	_system___gc_markcog_tmp001_, __system___gc_markcog_cogaddr
	add	_system___gc_markcog_tmp001_, #496
	'.live	__system___gc_markcog_ptr
	movs	wrcog, _system___gc_markcog_tmp001_
	movd	wrcog, #__system___gc_markcog_ptr
	call	#wrcog
	mov	arg02, __system___gc_markcog_heap_end
	mov	arg01, __system___gc_markcog_heap_base
	mov	arg03, __system___gc_markcog_ptr
	call	#__system___gc_isvalidptr
	mov	__system___gc_markcog_ptr, result1 wz
 if_ne	mov	__system___gc_markcog__cse__0000, __system___gc_markcog_ptr
 if_ne	add	__system___gc_markcog__cse__0000, #2
 if_ne	rdword	__system___gc_markcog__cse__0001, __system___gc_markcog__cse__0000
 if_ne	or	__system___gc_markcog__cse__0001, #32
 if_ne	wrword	__system___gc_markcog__cse__0001, __system___gc_markcog__cse__0000
	add	__system___gc_markcog_cogaddr, #1
	cmps	__system___gc_markcog_cogaddr, #496 wc
 if_b	jmp	#LR__0034
	mov	arg01, __system___gc_docollect_startheap
	call	#__system___gc_nextBlockPtr
	mov	__system___gc_docollect_nextptr, result1 wz
 if_ne	jmp	#LR__0035
	mov	arg01, ptr_L__0069_
	call	#__system___gc_errmsg
	jmp	#__system___gc_docollect_ret
LR__0035
LR__0036
	mov	__system___gc_docollect_ptr, __system___gc_docollect_nextptr
	mov	arg01, __system___gc_docollect_ptr
	call	#__system___gc_nextBlockPtr
	mov	__system___gc_docollect_nextptr, result1
	mov	__system___gc_docollect__cse__0002, __system___gc_docollect_ptr
	add	__system___gc_docollect__cse__0002, #2
	rdword	_system___gc_docollect_tmp001_, __system___gc_docollect__cse__0002
	test	_system___gc_docollect_tmp001_, #32 wz
 if_ne	jmp	#LR__0038
	mov	_system___gc_docollect_tmp002_, _system___gc_docollect_tmp001_
	and	_system___gc_docollect_tmp002_, #16 wz
 if_ne	jmp	#LR__0038
	and	_system___gc_docollect_tmp001_, #15
	mov	__system___gc_docollect_flags, _system___gc_docollect_tmp001_
	cmp	__system___gc_docollect_flags, __system___gc_docollect_ourid wz
 if_ne	cmp	__system___gc_docollect_flags, #14 wz
 if_ne	jmp	#LR__0037
	mov	arg01, __system___gc_docollect_ptr
	call	#__system___gc_dofree
	mov	_system___gc_docollect_tmp001_, result1
	mov	__system___gc_docollect_nextptr, _system___gc_docollect_tmp001_
LR__0037
LR__0038
	cmp	__system___gc_docollect_nextptr, #0 wz
 if_ne	cmps	__system___gc_docollect_nextptr, __system___gc_docollect_endheap wc
 if_c_and_nz	jmp	#LR__0036
__system___gc_docollect_ret
	ret

__system___gc_markhub
	mov	__system___gc_markhub_startaddr, arg01
	mov	__system___gc_markhub_endaddr, arg02
	call	#__system___gc_ptrs
	mov	__system___gc_markhub_heap_base, result1
	mov	__system___gc_markhub_heap_end, result2
LR__0039
	cmps	__system___gc_markhub_startaddr, __system___gc_markhub_endaddr wc
 if_ae	jmp	#LR__0040
	rdlong	arg03, __system___gc_markhub_startaddr
	add	__system___gc_markhub_startaddr, #4
	mov	arg02, __system___gc_markhub_heap_end
	mov	arg01, __system___gc_markhub_heap_base
	call	#__system___gc_isvalidptr
	mov	__system___gc_markhub_ptr, result1 wz
 if_e	jmp	#LR__0039
	mov	arg01, __system___gc_markhub_ptr
	call	#__system___gc_isFree
	cmp	result1, #0 wz
 if_e	mov	__system___gc_markhub__cse__0001, __system___gc_markhub_ptr
 if_e	add	__system___gc_markhub__cse__0001, #2
 if_e	rdword	_system___gc_markhub_tmp001_, __system___gc_markhub__cse__0001
 if_e	andn	_system___gc_markhub_tmp001_, #15
 if_e	or	_system___gc_markhub_tmp001_, #46
 if_e	mov	__system___gc_markhub_flags, _system___gc_markhub_tmp001_
 if_e	wrword	__system___gc_markhub_flags, __system___gc_markhub__cse__0001
	jmp	#LR__0039
LR__0040
__system___gc_markhub_ret
	ret
' code originally from spin interpreter, modified slightly

unsdivide_
       mov     itmp2_,#0
       jmp     #udiv__

divide_
       abs     muldiva_,muldiva_     wc       'abs(x)
       muxc    itmp2_,divide_haxx_            'store sign of x (mov x,#1 has bits 0 and 31 set)
       abs     muldivb_,muldivb_     wc,wz    'abs(y)
 if_z  jmp     #divbyzero__
 if_c  xor     itmp2_,#1                      'store sign of y
udiv__
divide_haxx_
        mov     itmp1_,#1                    'unsigned divide (bit 0 is discarded)
        mov     DIVCNT,#32
mdiv__
        shr     muldivb_,#1        wc,wz
        rcr     itmp1_,#1
 if_nz   djnz    DIVCNT,#mdiv__
mdiv2__
        cmpsub  muldiva_,itmp1_        wc
        rcl     muldivb_,#1
        shr     itmp1_,#1
        djnz    DIVCNT,#mdiv2__
        shr     itmp2_,#31       wc,wz    'restore sign
        negnz   muldiva_,muldiva_         'remainder
        negc    muldivb_,muldivb_ wz      'division result
divbyzero__
divide__ret
unsdivide__ret
	ret
DIVCNT
	long	0
wrcog
    mov    0-0, 0-0
wrcog_ret
    ret

__heap_ptr
	long	@@@__heap_base
fp
	long	0
imm_1016_
	long	1016
imm_1073741824_
	long	1073741824
imm_115200_
	long	115200
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
itmp1_
	long	0
itmp2_
	long	0
ptr_L__0031_
	long	@@@LR__0041
ptr_L__0043_
	long	@@@LR__0042
ptr_L__0069_
	long	@@@LR__0043
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

LR__0041
	byte	" !!! corrupted heap??? !!! "
	byte	0
LR__0042
	byte	" !!! out of heap memory !!! "
	byte	0
LR__0043
	byte	" !!! corrupted heap !!! "
	byte	0
	long
_dat_
	byte	$00[20]
	long
__system__dat_
	byte	$00, $00, $00, $00, $f0, $09, $bc, $0a, $00, $00, $68, $5c, $01, $08, $fc, $0c
	byte	$03, $08, $7c, $0c, $00, $00, $00, $00, $03, $00, $00, $00, $00, $00, $00, $00
	byte	$00, $00, $00, $00, $00, $00, $00, $00
__heap_base
	long	0[258]
stackspace
	long	0[1]
	org	COG_BSS_START
__system___gc_alloc_managed_r
	res	1
__system___gc_alloc_managed_size
	res	1
__system___gc_doalloc__cse__0005
	res	1
__system___gc_doalloc__idx__0000
	res	1
__system___gc_doalloc_ptr
	res	1
__system___gc_doalloc_reserveflag
	res	1
__system___gc_doalloc_size
	res	1
__system___gc_doalloc_zptr
	res	1
__system___gc_docollect__cse__0000
	res	1
__system___gc_docollect__cse__0001
	res	1
__system___gc_docollect__cse__0002
	res	1
__system___gc_docollect_endheap
	res	1
__system___gc_docollect_flags
	res	1
__system___gc_docollect_nextptr
	res	1
__system___gc_docollect_ourid
	res	1
__system___gc_docollect_ptr
	res	1
__system___gc_docollect_startheap
	res	1
__system___gc_dofree__cse__0000
	res	1
__system___gc_dofree__cse__0001
	res	1
__system___gc_dofree__cse__0002
	res	1
__system___gc_dofree__cse__0003
	res	1
__system___gc_dofree__cse__0004
	res	1
__system___gc_dofree__cse__0005
	res	1
__system___gc_dofree__cse__0006
	res	1
__system___gc_dofree__cse__0007
	res	1
__system___gc_dofree__cse__0008
	res	1
__system___gc_dofree__cse__0009
	res	1
__system___gc_dofree__cse__0010
	res	1
__system___gc_dofree__cse__0011
	res	1
__system___gc_dofree__cse__0012
	res	1
__system___gc_dofree__cse__0013
	res	1
__system___gc_dofree__cse__0014
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
__system___gc_errmsg_s
	res	1
__system___gc_markcog__cse__0000
	res	1
__system___gc_markcog__cse__0001
	res	1
__system___gc_markcog_cogaddr
	res	1
__system___gc_markcog_heap_base
	res	1
__system___gc_markcog_heap_end
	res	1
__system___gc_markcog_ptr
	res	1
__system___gc_markhub__cse__0001
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
__system___gc_ptrs_base
	res	1
__system___gc_ptrs_end
	res	1
__system___gc_tryalloc__cse__0000
	res	1
__system___gc_tryalloc__cse__0001
	res	1
__system___gc_tryalloc__cse__0002
	res	1
__system___gc_tryalloc__cse__0003
	res	1
__system___gc_tryalloc__cse__0004
	res	1
__system___gc_tryalloc__cse__0005
	res	1
__system___gc_tryalloc__cse__0006
	res	1
__system___gc_tryalloc__cse__0007
	res	1
__system___gc_tryalloc__cse__0008
	res	1
__system___gc_tryalloc__cse__0009
	res	1
__system___gc_tryalloc__cse__0010
	res	1
__system___gc_tryalloc__cse__0011
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
__system___txraw__idx__0003
	res	1
__system___txraw_bitcycles
	res	1
__system___txraw_c
	res	1
__system___txraw_nextcnt
	res	1
_fetchv__cse__0000
	res	1
_system___gc_doalloc_tmp001_
	res	1
_system___gc_docollect_tmp001_
	res	1
_system___gc_docollect_tmp002_
	res	1
_system___gc_dofree_tmp001_
	res	1
_system___gc_dofree_tmp002_
	res	1
_system___gc_markcog_tmp001_
	res	1
_system___gc_markhub_tmp001_
	res	1
_system___gc_ptrs_tmp001_
	res	1
_system___gc_tryalloc_tmp001_
	res	1
_system___gc_tryalloc_tmp002_
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
arg01
	res	1
arg02
	res	1
arg03
	res	1
muldiva_
	res	1
muldivb_
	res	1
	fit	496

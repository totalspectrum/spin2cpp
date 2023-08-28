'**************************************
'*  Spin2/PASM Debugger - 2022.11.19  *
'**************************************
'
' Sets up debugger and then launches appended applicaion
'
CON     dat_addr        =       $FC000
        sav_addr        =       $FEA00
        dbg_addr        =       $FF1A0
        dbg_size        =       $FFC00 - dbg_addr

        overlay_end     =       $1F7
'
'
' ********************
' *  Debugger Setup  *
' ********************
'
'
' Protected hub RAM usage ($FC000..$FFFFF)
'
' $FC000 - DEBUG data                           addresses fixed in software
'
' $FEA00 - Cog N reg $010..$1F7 buffer
'
' $FF1A0 - Cog N reg $004..$1F7 debugger + overlay(s) + data
'
' $FFC00 - Cog 7 reg $000..$00F buffer          addresses fixed in silicon
' $FFC40 - Cog 7 reg $000..$00F debug ISR
'
' $FFC80 - Cog 6 reg $000..$00F buffer
' $FFCC0 - Cog 6 reg $000..$00F debug ISR
'
' $FFD00 - Cog 5 reg $000..$00F buffer
' $FFD40 - Cog 5 reg $000..$00F debug ISR
'
' $FFD80 - Cog 4 reg $000..$00F buffer
' $FFDC0 - Cog 4 reg $000..$00F debug ISR
'
' $FFE00 - Cog 3 reg $000..$00F buffer
' $FFE40 - Cog 3 reg $000..$00F debug ISR
'
' $FFE80 - Cog 2 reg $000..$00F buffer
' $FFEC0 - Cog 2 reg $000..$00F debug ISR
'
' $FFF00 - Cog 1 reg $000..$00F buffer
' $FFF40 - Cog 1 reg $000..$00F debug ISR       in each debug ISR, long[$C] = BRK condition
'
' $FFF80 - Cog 0 reg $000..$00F buffer
' $FFFC0 - Cog 0 reg $000..$00F debug ISR       in each debug ISR, long[$D] = COGBRK flag
'
'
DAT             org
'
'
' Configure the tx pin so that it stays high
'
t               wrpin   #%01_11110_0,@_txpin_/4
'
'
' Configure the rx pin as a long repository to hold the clock frequency
'
                wrpin   #%00_00001_0,@_rxpin_/4
                dirh    @_rxpin_/4
                wxpin   _clkfreq_,@_rxpin_/4
'
'
' Set clock mode
'
                hubset  _clkmode1_              'start external clock, remain in rcfast mode    (reused as t)
                waitx   ##20_000_000 / 100      'allow 10ms for external clock to stabilize
                hubset  _clkmode2_              'switch to external clock mode

                waitx   _delay_                 'do any startup delay
'
'
' Allocate lock[15]
'
                rep     #1,#16                  'allocate lock[0..15]
                locknew t

                mov     t,#14                   'return lock[14..0], leaves lock[15] allocated
.return         lockret t
                djnf    t,#.return
'
'
' Clear upper hub RAM background to clean up debugger view
'
                loc     ptra,#$FC000
                setq    ##$4000/4-1
                wrlong  #0,ptra
'
'
' Write debugger ISR to all eight cogs' ISR buffers
'
                neg     t,#$40                  'initial address of $FFFC0
                rep     #3,#8
                setq    #$00F-$000
                wrlong  @debug_isr/4,t
                sub     t,#$80
'
'
' Move debugger program into position
'
                loc     ptra,#@debugger
                loc     ptrb,#dbg_addr
                loc     pa,#@debugger_end-@debugger
                call    #move
'
'
' Move DEBUG data into position
'
                loc     ptra,#@debugger_end
                loc     ptrb,#dat_addr
                rdword  t,ptra
                callpa  t,#move
'
'
' Protect upper hub RAM and enable debugging on cogs
'
                hubset  _hubset_
'
'
' Move application code to $00000
'
                loc     ptra,#@debugger_end
                add     ptra,t
                loc     ptrb,#0
                callpa  _appsize_,#move
'
'
' Clear trailing hub RAM to clean up debugger view
'
                not     t,_appsize_
                zerox   t,#18
                shr     t,#2
                setq    t
                wrlong  #0,_appsize_
'
'
' Launch application
'
                coginit #0,#$00000              'relaunch cog 0 from $00000
'
'
' Move pa bytes from @ptra to @ptrb
'
move            add     pa,#3                   'round bytes up to long
                shr     pa,#2

.block          decod   pb,#8                   'determine number of longs in block ($100 max)
                fle     pb,pa
                sub     pa,pb                   'adjust remaining-longs count

                sub     pb,#1                   'get long count - 1

                setq2   pb                      'read block of longs
                rdlong  0,ptra++
                setq2   pb                      'write block of longs
                wrlong  0,ptrb++

        _ret_   tjnz    pa,#.block              'another block?
'
'
' Data set by compiler
'
_clkfreq_       long    0                       '@0D4: initial clock frequency
_clkmode1_      long    0                       '@0D8: initial clkmode with rcfast
_clkmode2_      long    0                       '@0DC: initial clkmode with external clock
_delay_         long    0                       '@0E0: startup delay (default is 0)
_appsize_       long    0                       '@0E4: application size in bytes
_hubset_        long    $2003_00FF              '@0E8: cog-debug-enable bits in low byte (default is $FF)
'
'
' *******************************************************
' *  Debug ISR                                          *
' *                                                     *
' *     - on debug interrupt, JMP #$1F8 to ROM          *
' *     - ROM: registers $000..$00F save to hub         *
' *     - ROM: registers $000..$00F load from hub       *
' *     - ROM: JMP #$000 executes                       *
' *     - debug_isr runs to begin debug                 *
' *     - debug saves and later restores $010..$1F7     *
' *     - JMP #$1FD to ROM                              *
' *     - ROM: registers $000..$00F restore from hub    *
' *     - ROM: RETI0 executes, returns to main code     *
' *                                                     *
' *******************************************************
'
                org
debug_isr
'
'
' Get ct and status data - routine must be located within $000..$00F
'
cth             getct   cth             wc      'get cth
ctl             getct   ctl                     'get ctl
regh            augs    #sav_addr               'get sav_addr into regh
cond            mov     regh,#sav_addr & $1FF   'cond will hold BRK condition on exit

.wait           locktry #15             wc      'wait for lock[15] so that we own the save buffer and tx/rx pins
        if_nc   jmp     #.wait

                setq    #$1F7-$010              'save registers $010..$1F7 to the save buffer
                wrlong  $010,regh

                setq    #$1F7-$004              'load debugger into registers $004..$1F7
                rdlong  $004,##dbg_addr         '(preserve cth..cond at $000..$003)

                jmp     #\debug_entry           'jump to debug_entry (jmp was already pipelined)


brk_cond        long    $00000010               '@11C: $00C, COGBRK (set later) and BRK are initially enabled
cogbrk_flag     long    0                       '      $00D, COGBRK flag is initially clear
'
'
' ***********************************************
' *  Debugger                                   *
' *                                             *
' *     - cth..cond saved at $000..$003         *
' *     - debugger loads at $004..$1F7          *
' *                                             *
' ***********************************************
'
'
                org     $004
debugger
'
'
' Debugger exit - routine must be located within $004..$00F
'
debug_exit      setq    #$1F7-$010              'restore registers $010..$1F7 to free the save buffer
                rdlong  $010,regh

                lockrel #15                     'release lock[15] so other cogs can own the save buffer and tx/rx pins

.delay  if_nz   waitx   ctl                     'if dlyms was not 0, do exit delay
        if_nz   djnz    cth,#.delay             '(start of tiny overlay area for stall)

                brk     cond                    'enable next debug interrupt

                jmp     #$1FD                   'restore $000..$00F, return from debug interrupt
'
'
' This 'stall' program gets assembled into $004..$00F
'
' $000- cth     long    0                       'old    <unused>
' $001- ctl     long    freq >> 16              'old    waitx 64ms value
' $002- regh    long    sav_addr                'old    pointer to save buffer
' $003- cond    long    0                       'old    <unused>
'
' $004-         setq    #$1F7-$010              'old    restore registers $010..$1F7 to free the save buffer
' $005-         rdlong  $010,regh               'old
'
' $006-         lockrel #15                     'old    release lock[15] so other cogs can own the save buffer and tx/rx pins
'
' $007- if_nz   waitx   ctl                     'old    wait for ~64ms (z must be clear)
'
' $008- .wait   locktry #15             wc      'new    wait for lock[15] so that we own the save buffer and tx/rx pins again
' $009- if_nc   jmp     #.wait                  'new
'
' $00A-         setq    #$1F7-$010              'new    resave registers $010..$1F7 to the save buffer
' $00B-         wrlong  $010,regh               'new
'
' $00C-         setq    #$1F7-$004              'new    load debugger into registers $004..$1F7
' $00D-         rdlong  $004,##dbg_addr         'new    (preserve cth..cond at $000..$003)
'
' $00F-         jmp     #\debug_entry           'new    jump to debug_entry (jmp is already pipelined)
'
'
' Data set by compiler
'
_txpin_         long    62                      '@140: tx pin (default is 62, msb = tx output flag)
_rxpin_         long    63                      '@144: rx pin (default is 63, msb = show timestamp)
_baud_          long    0                       '@148: baud rate

dlyms           long    0                       'exit delay in ms
'
'
' DEBUG interrupt entry or STALL return - saves cog data and determines what to do
' --------------------------------------------------------------------------------
'
'       if STALL return
'         do BREAKPOINT (updates BRK_COND, stalls or exits)
'
'       if COGINIT
'         show INIT message
'       
'       if COGBRK_FLAG set
'         clear COGBRK_FLAG
'         do BREAKPOINT (updates BRK_COND, stalls or exits)
'       
'       if COGINIT
'         if (COGINIT or MAIN) in BRK_COND
'           do BREAKPOINT (updates BRK_COND, stalls or exits)
'         else
'           exit with BRK_COND
'       
'       if (DEBUG in BRK_COND) and (BRK code <> $00)
'         do MESSAGE (exits)
'
'       do BREAKPOINT (updates BRK_COND, stalls or exits)
'
'
debug_entry

regl            brk     #0                      'in case stall entry, show debug interrupt return address at inb

cogn            cogid   cogn                    '00: get cogid (cogn = 0..7, cogn..freq form contiguous data)
brkcz           getbrk  brkcz           wcz     '01: get status wcz
brkc            getbrk  brkc            wc      '02: get status wc
brkz            getbrk  brkz            wz      '03: get status wz
cth2            getct   cth2            wc      '04: get 64-bit ct
ctl2            getct   ctl2                    '05:
stk0            pop     stk0                    '06: get stack
stk1            pop     stk1                    '07:
stk2            pop     stk2                    '08:
stk3            pop     stk3                    '09:
stk4            pop     stk4                    '0A:
stk5            pop     stk5                    '0B:
stk6            pop     stk6                    '0C:
stk7            pop     stk7                    '0D:
iret            mov     iret,inb                '0E: get debug interrupt return address
fptr            getptr  fptr                    '0F: get rdfast/wrfast pointer
ptra_           mov     ptra_,ptra              '10: get ptra
ptrb_           mov     ptrb_,ptrb              '11: get ptrb
freq            rqpin   freq,_rxpin_            '12: get clock frequency from rx pin in repository mode

t1              decod   regl,#11                'show ina/inb (must be disabled before RETI0)
t2              brk     regl

w               not     regl,cogn               'calculate register $000..$00F buffer address for cog
x               shl     regl,#7

y               sub     ctl,#62         wc      'trim 64-bit ct from initial debug_entry (not from stall)
z               subx    cth,#0

txrx            encod   x,freq                  'compute (freq / _baud_) for tx/rx configuration
ptr             encod   y,_baud_
msb             sub     x,y
dig             add     x,#1
ma              shl     _baud_,x
mb              mov     y,freq
xa              rep     #3,#32+1                'do long division to preserve any user cordic results
xb              cmpsub  y,_baud_        wc
expa            rcl     txrx,#1
expb            shl     y,#1
                subr    x,#16                   'position clocks-per-bit
                shr     txrx,x
                setbyte txrx,#8-1,#0            'set 8-N-1

.txwait         rdpin   w, _txpin_ wc           'wait for tx not busy
        if_c    jmp     #.txwait
                fltl    _txpin_                 'reset and configure tx pin
                wrpin   #%01_11110_0,_txpin_
                wxpin   txrx,_txpin_
                drvl    _txpin_

                fltl    _rxpin_                 'reset and configure rx pin
                wrpin   #%00_11111_0,_rxpin_
                wxpin   txrx,_rxpin_
                drvl    _rxpin_

                mov     ptrb,regl               'point to cog's ISR (use ptrb to preserve ptra for Spin2 DEBUG message)

                rdlong  cond,ptrb[$1C]          'if return from stall, handle breakpoint
                testb   cond,#11        wc
  if_c          jmp     #.breakpoint

                testb   brkcz,#23       wc      'if COGINIT, output message
  if_c          call    #coginit_

                rdlong  t1,ptrb[$1D]    wz      'if COGBRK, clear flag and handle breakpoint
  if_nz         wrlong  #0,ptrb[$1D]
  if_nz         jmp     #.breakpoint

  if_c          test    cond,#$101      wz      'if COGINIT, if BRK condition <> (COGINIT or MAIN), done
  if_c_and_z    jmp     #debug_done

  if_nc         test    cond,#$010      wz      'if not COGINIT, if BRK condition = DEBUG and BRK code <> 0, normal DEBUG message
  if_nc_and_nz  getbyte ptr,brkcz,#3
  if_nc_and_nz  tjnz    ptr,#debug_message

.breakpoint     loc     pb,#\dbg_addr+(@bp_handler-@debugger)   'load breakpoint handler overlay
                setq    #overlay_end-overlay_begin              'read in overlay code and execute it
                rdlong  overlay_begin,pb

                jmp     #bp_handler
'
'
' Debug done
'
debug_done      rdpin   t1,_txpin_       wc     'wait for tx not busy
        if_c    jmp     #debug_done

                wrpin   #%00_00001_0,_rxpin_    'set rx pin back to repository mode
                wxpin   freq,_rxpin_            'write freq back to rx pin repository

                fltl    _txpin_                 'disable tx pin
                fltl    _rxpin_                 'disable rx pin

                push    stk7                    'restore stack
                push    stk6
                push    stk5
                push    stk4
                push    stk3
                push    stk2
                push    stk1
                push    stk0

                mov     ptra,ptra_              'restore ptra
                mov     ptrb,ptrb_              'restore ptrb

                mov     ctl,freq                'get waitx ~1ms value into ctl
                shr     ctl,#10

                testb   cond,#11        wc      'exit or stall?

        if_nc   or      cond,#$100              'if exit, enable COGBRK in BRK condition (bit was used for COGINIT)

        if_nc   mov     cth,dlyms       wz      'if exit, get exit delay, z=1 if 0

        if_c    shl     ctl,#6          wz      'if stall, get waitx ~64ms value into ctl, z=0

        if_c    setq    #$00F-$008              'if stall, load locktry..jmp in $008..$00F
        if_c    rdlong  $008,##$FFFC0+$004*4

                jmp     #debug_exit             'exit debugger or do stall
'
'
' *****************
' *  Subroutines  *
' *****************
'
'
' Transmit byte/word/long in pa, preserves c/z
'
txlong          call    #txword                 'send lower word
                getword pa,t1,#1                'get upper word

txword          mov     t1,pa                   'pack word as %HHHHHHHH01LLLLLLLL for single transmit
                shr     pa,#8-2                 '..improves overlap of memory computations and serial transmits
                rolbyte pa,t1,#0
                bith    pa,#8
                bitl    pa,#9

                setbyte txrx,#17,#0             'ready 18-bit mode for fast word transmit

txbyte          rdpin   t2,_txpin_      wc      'wait until tx not busy
        if_c    jmp     #txbyte

                wxpin   txrx,_txpin_            'set 18-bit or 8-bit mode

                wypin   pa,_txpin_              'output byte

                setbyte txrx,#7,#0              'ready 8-bit mode

                bith    _txpin_,#31             'set flag to indicate tx output has occurred

                ret                     wcz
'
'
' Receive long into pb, preserves c/z
'
rxlong          call    #.word
.word           call    #.byte

.byte           testp   _rxpin_         wc      'wait for rx byte
        if_nc   jmp     #.byte

                rdpin   pa,_rxpin_              'get byte
                shr     pa,#32-8
                shr     pb,#8
                setbyte pb,pa,#3

                ret                     wcz
'
'
' z=0 : Read register x into x, preserves c/z
' z=1 : Write y to register x, preserves c/z
'
' $000..$00F = from @regl
' $010..$1F7 = from @regh
' $1F8..$1F9 = ptra_, ptrb_
' $1FA..$1FF = direct
' $200..$3FF = lut
'
rdreg           modz    _clr            wz      'force read

rwreg           signx   x,#9            wc      'lut $200..$3FF? (signx simplifies comparisons)
  if_c_and_nz   rdlut   x,x
  if_c_and_z    wrlut   y,x
  if_c          ret                     wcz

                cmp     x,#$010         wc      '$000..$00F ?
  if_c          shl     x,#2
  if_c          add     x,regl
  if_c          jmp     #.hub

                cmp     x,#$1F8         wc      '$010..$1F7 ?
  if_c          sub     x,#$010
  if_c          shl     x,#2
  if_c          add     x,regh
.hub
  if_c_and_nz   rdlong  x,x                     'reg $000..$1F7
  if_c_and_z    wrlong  y,x
  if_c          ret                     wcz

                cmp     x,#$1FA         wc      '$1FA..$1FF ?
  if_c          sub     x,#$1F8 - ptra_         '$1F8..$1F9 ?
  if_nz         alts    x
  if_nz         mov     x,0-0                   'reg $1F8..$1FF
  if_z          altd    x
  if_z          mov     0-0,y
                ret                     wcz
'
'
' End static code and begin overlay area
'
overlay_begin
'
'
'*****************************
'*  Message Handler Overlay  *
'*****************************
'
'
' Show COGINIT message, preserves c/z
'
coginit_        call    #cognout                'output "CogN  " with possible timestamp

                callpb  #_ini,#rstrout          'output "INIT "

                mov     x,ptrb_                 'output ptrb
                call    #hexout

                callpa  #" ",#txbyte            'output " "

                mov     x,ptra_                 'output ptra
                call    #hexout

                testb   brkcz,#0         wc     'load or jump?
        if_nc   callpb  #_lod,#rstrout          'output " load"
        if_c    callpb  #_jmp,#rstrout          'output " jump"

                ret                     wcz     'restore flags
'
'
' Normal DEBUG message
'
debug_message   shl     ptr,#1                  'compute DEBUG data address
                bith    ptr,#14 addbits 5
                rdword  ptr,ptr
                bith    ptr,#14 addbits 5
'
'
' Process DEBUG bytecodes
'
debug_byte      callpa  #z,#getdeb              'get DEBUG bytecode

                test    z,#$E0          wz      'argument command?
        if_nz   jmp     #arg_cmd

                altgb   z,#.table               'simple command
                getbyte w
                testb   z,#0            wc      'get lsb of command into c
                jmp     w-0

.table          byte    .done                   '0 = end of DEBUG commands
                byte    .asm                    '1 = set asm mode
                byte    .if                     '2 = IF(cond)
                byte    .if                     '3 = IFNOT(cond)
                byte    .cogn                   '4 = output "CogN  " with possible timestamp
                byte    .chr                    '5 = output chr
                byte    .str                    '6 = output string
                byte    .dly                    '7 = DLY(ms)
                byte    .pc                     '8 = PC_KEY(ptr)
                byte    .pc                     '9 = PC_MOUSE(ptr)

.asm            ijnz    asm,#debug_byte         'set asm mode

.if             call    #getval                 'IF/IFNOT(cond)
        if_nc   tjz     x,#.done
        if_c    tjnz    x,#.done
                jmp     #debug_byte

.cogn           call    #cognout                'output "CogN  " with possible timestamp
                jmp     #debug_byte

.chr            call    #getval                 'output chr
                callpa  x,#txbyte
                jmp     #debug_byte

.str            call    #dstrout                'output string
                jmp     #debug_byte

.dly            call    #getval                 'DLY(ms), get ms
                mov     dlyms,x
.done           testb   _txpin_,#31      wc     'if tx output has occurred, output new line
        if_c    callpb  #_lin,#rstrout
                jmp     #debug_done

.pc     if_nc   callpb  #_key,#rstrout          'if PC_KEY(ptr), output "PC_KEY"+cr+lf
        if_c    callpb  #_mou,#rstrout          'if PC_MOUSE(ptr), output "PC_MOUSE"+cr+lf
                call    #getval                 'get ptr into ptrb
                mov     ptrb,x
                call    #rxlong                 'receive long into pb
                callpa  #12,#.long              'write mouse x (or key)
        if_c    callpa  #12,#.long              'write mouse y
        if_c    callpa  #1,#.long               'write mouse wheel delta
        if_c    callpa  #0,#.long               'write mouse left button
        if_c    callpa  #0,#.long               'write mouse middle button
        if_c    callpa  #0,#.long               'write mouse right button
        if_c    call    #rxlong                 'receive long into pb
        if_c    callpa  #31,#.long              'write pixel color
                jmp     #debug_done

.long           mov     y,pb                    'pa=topbit, pb=value, ptrb=address
                signx   y,pa
                shr     pb,pa
                shr     pb,#1
                cmp     asm,#1          wz      'write to reg or hub?
        if_z    mov     x,ptrb
        if_z    add     ptrb,#1
        if_z    jmp     #rwreg                  'z=1 for write
        _ret_   wrlong  y,ptrb++                'write to hub
'
'
' Output "CogN  " with possible timestamp
'
cognout         setnib  _cog,cogn,#6            'get cog number into string
                callpb  #_cog,#rstrout          'output "CogN  "

                mov     msb,#31                 'enable 32-bit output

        _ret_   tjs     _rxpin_,#.time          'timestamp?

.time           mov     x,cth                   'output cth
                call    #hexout
                callpa  #"_",#txbyte            'output "_"
                mov     x,ctl                   'output ctl
                call    #hexout_digits
                mov     pb,#_cog+1              'output "  " (followed by rstrout)
'
'
' Register z-string output @pb
'
rstrout         shl     pb,#2                   'convert register address to byte index

.loop           altgb   pb                      'get byte
                getbyte pa

        _ret_   tjnz    pa,#.out                'if byte = 0, done

.out            call    #txbyte                 'output byte

                ijnz    pb,#.loop               'next byte, always branches
'
'
' Argument command
'
arg_cmd         testb   z,#0            wz      'if %x0, output ", "
  if_nz         callpb  #_com,#rstrout

                testb   z,#1            wz      'if %0x, output expression_string + " = "
  if_nz         call    #dstrout
  if_nz         callpb  #_equ,#rstrout

                call    #getval                 'get ptr/val argument
                mov     ptrb,x

                testb   z,#4            wz      'get len argument?
  if_z          call    #getval
  if_z          mov     y,x

                mov     pa,z                    'ZSTR/LSTR command?
                and     pa,#$EC
                cmp     pa,#$24         wz
                testb   z,#4            wc      'value or array command?
  if_nz_and_nc  jmp     #value_cmd
  if_nz_and_c   jmp     #array_cmd
'
'
' ZSTR(ptr)
' LSTR(ptr,len)
'
                testb   z,#1            wc      'if %0x, output quote
  if_nc         callpa  #$22,#txbyte

                testb   z,#4            wc      'c=0 if ZSTR, c=1 if LSTR
  if_c          tjz     y,#.done                'if LSTR and len is zero then done
.loop           rdbyte  pa,ptrb++       wz      'get next chr
  if_c_or_nz    call    #txbyte                 'if LSTR or chr <> 0 then output chr
  if_c          sub     y,#1            wz      'if LSTR, dec len
  if_nz         jmp     #.loop                  'if LSTR and len <> 0 or ZSTR and chr <> 0 then loop
.done
                testb   z,#1            wc      'if %0x, output quote
  if_nc         callpa  #$22,#txbyte

                jmp     #debug_byte
'
'
' Value command
'
value_cmd       testb   z,#3            wc      'determine msb
                testb   z,#2            wz
                mov     msb,#31                 'long/reg/minimum
  if_10         mov     msb,#15                 'word
  if_01         mov     msb,#7                  'byte

                test    z,#$C0          wz      'if fp, don't make absolute
  if_nz         testbn  z,#5            wz      'non-fp signed?
  if_z          zerox   x,msb                   'if not, zero-extend from msb (no effect on fp)
  if_nz         signx   x,msb                   'if so, sign-extend from msb
  if_nz         abs     x               wc      '..make absolute
  if_nz_and_c   callpa  #"-",#txbyte            '..if was negative then output "-"

                test    z,#$1C          wz      'minimum-sized?
  if_z          encod   msb,x

                testb   z,#7            wc      'output fp/dec/hex/bin
                testb   z,#6            wz
  if_00         mov     w,#fpout
  if_01         mov     w,#decout
  if_10         mov     w,#hexout
  if_11         mov     w,#binout
                call    w-0

                testb   z,#4            wz      'if value command then done, else continue array
  if_nz         jmp     #debug_byte             '(followed by array_loop)
'
'
' Array command
'
array_loop      djz     y,#debug_byte           'done?

                callpa  #" ",#txbyte            'output " "

array_val       testb   z,#3            wc      'read array element and advance address
                testb   z,#2            wz
        if_11   rdlong  x,ptrb++
        if_10   rdword  x,ptrb++
        if_01   rdbyte  x,ptrb++
        if_00   mov     x,ptrb
        if_00   add     ptrb,#1
        if_00   call    #rwreg                  'z=0 for read

                jmp     #value_cmd              'output value


array_cmd       tjnz    y,#array_val            'if array not empty then output values

                callpb  #_nil,#rstrout          'if array empty then output "nil"
                jmp     #debug_byte
'
'
' Get DEBUG byte - pa must hold register to receive byte
'
getdeb          altd    pa
                rdbyte  0-0,ptr
        _ret_   add     ptr,#1
'
'
' Get DEBUG value according to mode into x, preserves c/z
'
getval          tjnz    asm,#getasm

        _ret_   rdlong  x,++ptra
'
'
' Get ASM value from DEBUG data into x, preserves c/z
'
' %1000_00xx, %xxxx_xxxx                                        10-bit register, big-endian
' %00xx_xxxx, %xxxx_xxxx                                        14-bit constant, big-endian
' %0100_0000, %xxxx_xxxx, %xxxx_xxxx, %xxxx_xxxx, %xxxx_xxxx    32-bit constant, little-endian
'
getasm          rdword  x,ptr                   'get initial word
                add     ptr,#2          wz      'z=0 for rwreg

                movbyts x,#%%3201               'swap two lower bytes

                testb   x,#15           wc      '10-bit register?
        if_c    jmp     #rwreg                  'z=0 for read

                testb   x,#14           wc      '14-bit or 32-bit constant?
        if_c    sub     ptr,#1
        if_c    rdlong  x,ptr
        if_c    add     ptr,#4
                ret                     wcz
'
'
' Debug string output
'
dstrout_loop    call    #txbyte
dstrout         callpa  #pa,#getdeb
        _ret_   tjnz    pa,#dstrout_loop
'
'
' Floating-point output x
'
fpout           mov     ma,x                    'get float
                bitl    ma,#31          wcz     'make positive, negative flag in z

                cmpr    ma,##$7F800000  wc      'if NaN, output "NaN" and exit
        if_c    callpb  #_nan,#rstrout
        if_c    ret

        if_z    callpa  #"-",#txbyte2           'if negative, output "-"

                call    #.unpack                'unpack float
                mov     xb,xa
                mov     mb,ma           wz

        if_z    callpa  #"0",#txbyte2           'if zero, output "0" and exit
        if_z    ret

                mov     expa,xb                 'estimate base-10 exponent
                muls    expa,#77                '77/256 = 0.301 = log(2)
                sar     expa,#8

                mov     expb,#0                 'if base-10 exponent < -32, bias up by 13
                cmps    expa,##-32      wc
        if_nc   jmp     #.notsmall
                mov     ma,.ten13
                call    #.unpack
                call    #.mul
                mov     xb,xa
                mov     mb,ma
                mov     expb,#13
                add     expa,expb
.notsmall
                shl     expa,#2                 'determine exact base-10 exponent
.reduce         loc     pa,#dbg_addr+(@tenf-@debugger-6*4)
                add     pa,expa
                rdlong  ma,pa                   'look up power-of-ten and multiply
                call    #.unpack
                call    #.mul
                subr    xa,#29                  'round result
                shr     ma,xa           wc
                addx    ma,#0
                cmp     ma,##10_000_000 wc      'if over seven digits, reduce power-of-ten
        if_nc   add     expa,#4
        if_nc   jmp     #.reduce
                sar     expa,#2
                sub     expa,expb               'get final base-10 exponent

                mov     x,ma                    'output 7-digit mantissa
                mov     pb,#6*4         wz      'start with millions, z=0
                mov     dig,#7                  'set period to trip after first digit
                sets    doutn,#7
                setd    doutc,#"."
                call    #doutdig
                callpa  #"e",#txbyte2           'output "e"
                abs     expa            wc      'make base-10 exponent absolute
        if_nc   callpa  #"+",#txbyte2           'output "+" or "-"
        if_c    callpa  #"-",#txbyte2
                mov     x,expa                  'ready base-10 exponent
                cmp     x,x             wz      'z=1 to output leading 0's
                mov     pb,#1*4                 'start with tens
                jmp     #doutdig                'output 2-digit base-10 exponent and exit


.unpack         mov     xa,ma                   'unpack floating-point number
                shr     xa,#32-1-8      wz      'get exponent

                zerox   ma,#22                  'get mantissa

        if_nz   bith    ma,#23                  'if exponent <> 0 then insert leading one
        if_nz   shl     ma,#29-23               '...bit29-justify mantissa

        if_z    encod   xa,ma                   'if exponent = 0 then get magnitude of mantissa
        if_z    ror     ma,xa                   '...bit29-justify mantissa
        if_z    ror     ma,#32-29
        if_z    sub     xa,#22                  '...adjust exponent to -22..0

        _ret_   sub     xa,#127                 'unbias exponent


.mul            mov     pa,ma                   'multiply floating-point numbers
                mov     ma,#0

                rep     #3,#30                  'multiply mantissas
                shr     pa,#1           wc
                shr     ma,#1
        if_c    add     ma,mb

        _ret_   add     xa,xb                   'add exponents


.ten13          long    1e+13

txbyte2         jmp     #txbyte                 'fixes out-of-range callpa
'
'
' Decimal output x
'
decout          mov     pb,#9*4         wz      'start with billions, z=0
                mov     dig,#2                  'set initial underscore trip
                sets    doutn,#2                'underscore every 3 digits
                setd    doutc,#"_"              'set underscore character

doutdig         loc     pa,#dbg_addr+(@teni-@debugger)
                add     pa,pb
                rdlong  w,pa

                mov     pa,#0                   'determine decimal digit
doutsub         cmpsub  x,w             wc
  if_c          cmp     pa,pa           wz      'on first non-zero digit, z=1
  if_c          ijnz    pa,#doutsub

doutn           incmod  dig,#2          wc      'if underscore after digit, c=1

  if_nz         cmp     pb,#0           wz      'always output lowest digit, but not leading zeros

  if_z          add     pa,#"0"                 'output a digit
  if_z          call    #txbyte

  if_z          tjz     pb,#doutnext            'output an underscore?
doutc
  if_z_and_c    callpa  #"_",#txbyte2

doutnext        sub     pb,#4                   'next digit
  _ret_         tjns    pb,#doutdig
'
'
' Hex output x
'
hexout          callpa  #"$",#txbyte2           'output "$"

hexout_digits   mov     dig,msb                 'determine number of digits
                shr     dig,#2

.loop           altgn   dig,#x                  'output a hex digit
                getnib  pa
                cmp     pa,#10          wc
        if_c    add     pa,#"0"
        if_nc   add     pa,#"A"-10
                call    #txbyte
                cmp     dig,#4          wz      'output an underscore before lower group of 4 digits
        if_z    callpa  #"_",#txbyte2
        _ret_   djnf    dig,#.loop              'next digit
'
'
' Binary output x
'
binout          callpa  #"%",#txbyte2           'output "%"

                mov     dig,msb

.loop           testb   x,dig           wz      'output a binary digit
        if_nz   callpa  #"0",#txbyte2
        if_z    callpa  #"1",#txbyte2
                tjz     dig,#.skip              'output an underscore before groups of 8 digits
                test    dig,#7          wz
        if_z    callpa  #"_",#txbyte2
.skip   _ret_   djnf    dig,#.loop              'next digit
'
'
' Defined data
'
asm             long    0                       '0 = spin, 1 = asm

_cog            byte    "Cog0  ",0,0            'strings
_ini            byte    "INIT ",0,0,0
_lod            byte    " load",13,10,0
_jmp            byte    " jump",13,10,0
_com            byte    ", ",0,0
_equ            byte    " = ",0
_nil            byte    "nil",0
_nan            byte    "NaN",0
_key            byte    "PC_KEY",13,10,0,0,0,0
_mou            byte    "PC_MOUSE"
_lin            byte    13,10,0,0

                fit     overlay_end+1
'
'
'********************************
'*  Breakpoint Handler Overlay  *
'********************************
'
                org     overlay_begin
'
'
' Breakpoint handler
'
bp_handler      mov     z,#cogn                 'send cogn..freq
.packet         alts    z
                mov     pa,0-0
                call    #txlong
                incmod  z,#freq         wc
        if_nc   jmp     #.packet                '(z=0 after)

                mov     pa,cond                 'send BRK condition
                call    #txlong

.crcblock       bmask   pa,#15                  'send 64 CRC words for 16-register groups
.crclong        mov     x,z
                call    #rdreg
                setq    x
                rep     #1,#8
                crcnib  pa,crcpoly
                add     z,#1
                test    z,#$F           wz      'another register?
        if_nz   jmp     #.crclong
                call    #txword                 'send CRC word
                testb   z,#10           wz      'another block?
        if_nz   jmp     #.crcblock

                mov     ptra,#0                 'send 124 checksum words for 4KB hub blocks, from $00000..$7BFFF
                mov     x,#$1000/$40
                mov     y,#$7C000/$1000
                call    #.hubsum

                mov     z,#cmd_regs             'get command longs
.cmdlong        call    #rxlong
                altd    z
                mov     0-0,pb
                incmod  z,#cmd_brk      wc
        if_nc   jmp     #.cmdlong               '(z=0 after)

.regreq         mov     x,z                     'send requested 16-register blocks
                shr     x,#4
                altb    x,#cmd_regs
                testb   0-0,x           wc
        if_c    mov     x,z
        if_c    call    #rdreg
        if_c    mov     pa,x
        if_c    call    #txlong
                incmod  z,h3FF          wc
        if_nc   jmp     #.regreq                '(z=0 after)

.sumreq         altb    z,#cmd_sums             'send requested sub-4KB hub checksums
                testb   0-0,z           wc
        if_c    mov     ptra,z
        if_c    shl     ptra,#12
        if_c    mov     x,#$80/$40
        if_c    mov     y,#$1000/$80
        if_c    call    #.hubsum
                incmod  z,#$7C-1        wc
        if_nc   jmp     #.sumreq

                mov     z,#cmd_read             'send requested hub bytes
.hubreq         alts    z
                mov     ptra,0-0
                mov     x,ptra
                shr     x,#20           wz
.hubyte if_nz   rdbyte  pa,ptra++
        if_nz   call    #txbyte
        if_nz   djnz    x,#.hubyte              '(x=0 after)
                incmod  z,#cmd_read+4   wc      '(z=0 after)
        if_nc   jmp     #.hubreq

.rqpinread      rqpin   y,z                     'read 8 rqpin values and build pin-mask byte
                incmod  z,#$3F
                altd    x,#buff
                mov     0-0,y           wz
                bitnz   pa,x
                incmod  x,#7            wc      '(x=0 after)
        if_nc   jmp     #.rqpinread
                call    #txbyte                 'send pin-mask byte
.rqpinsend      alts    x,#buff                 'send non-zero rqpin values
                mov     pa,0-0          wz
        if_nz   call    #txlong
                incmod  x,#7            wc      '(x=0 after)
        if_nc   jmp     #.rqpinsend
                tjnz    z,#.rqpinread           '(z=0 after)

                loc     ptra,#$FFFC0+$D<<2      'perform requested COGBRKs
.cogbrk         shr     cmd_cogbrk,#1   wc      
        if_c    cogbrk  z                       'do COGBRK on targeted cog
        if_c    wrlong  #1,ptra                 'set targeted cog's COGBRK flag so it can detect COGBRK
                sub     ptra,#$20<<2
                incmod  z,#7            wc
        if_nc   jmp     #.cogbrk

                mov     cond,cmd_brk            'set and save BRK condition
                wrlong  cond,ptrb[$1C]

                jmp     #debug_done             'exit debugger or do stall


.hubsum         mov     pa,#0           wc      'new block, c=0
                rep     @.hubsum2,x             'compute and send hub checksums of y blocks of x*64 bytes @ptra
                setq    #16-1
                rdlong  buff,ptra++
                add     pa,buff+0
                add     pa,buff+1
                add     pa,buff+2
                add     pa,buff+3
                add     pa,buff+4
                add     pa,buff+5
                add     pa,buff+6
                add     pa,buff+7
                add     pa,buff+8
                add     pa,buff+9
                add     pa,buff+10
                add     pa,buff+11
                add     pa,buff+12
                add     pa,buff+13
                add     pa,buff+14
                add     pa,buff+15
                seussf  pa
.hubsum2        getword pb,pa,#1                'add upper and lower words
                add     pa,pb
                call    #txword                 'send checksum word
        _ret_   djnz    y,#.hubsum              'another block?


crcpoly         long    $8005 rev 15            'polynomial for 16-bit CRC
h3FF            long    $3FF

cmd_regs        res     2                       'command buffer
cmd_sums        res     4
cmd_read        res     5
cmd_cogbrk      res     1
cmd_brk         res     1

buff            res     16                      'data buffer

                fit     overlay_end+1
'
'
'**************
'*  Hub Data  *
'**************
'
                orgh

teni            long                1   'tens
                long               10
                long              100
                long            1_000
                long           10_000
                long          100_000
                long        1_000_000
                long       10_000_000
                long      100_000_000
                long    1_000_000_000

                long                  1e+38, 1e+37, 1e+36, 1e+35, 1e+34, 1e+33, 1e+32, 1e+31    'powers of ten
                long    1e+30, 1e+29, 1e+28, 1e+27, 1e+26, 1e+25, 1e+24, 1e+23, 1e+22, 1e+21
                long    1e+20, 1e+19, 1e+18, 1e+17, 1e+16, 1e+15, 1e+14, 1e+13, 1e+12, 1e+11
                long    1e+10, 1e+09, 1e+08, 1e+07, 1e+06, 1e+05, 1e+04, 1e+03, 1e+02, 1e+01
tenf            long    1e+00, 1e-01, 1e-02, 1e-03, 1e-04, 1e-05, 1e-06, 1e-07, 1e-08, 1e-09
                long    1e-10, 1e-11, 1e-12, 1e-13, 1e-14, 1e-15, 1e-16, 1e-17, 1e-18, 1e-19
                long    1e-20, 1e-21, 1e-22, 1e-23, 1e-24, 1e-25, 1e-26, 1e-27, 1e-28, 1e-29
                long    1e-30, 1e-31, 1e-32, 1e-33, 1e-34, 1e-35, 1e-36, 1e-37, 1e-38

debugger_end
'
'
'********************
'*  DEBUG Commands  *
'********************
'
'       00000000        end                             end of DEBUG commands
'       00000001        asm                             set asm mode
'       00000010        IF(cond)                        abort if cond = 0
'       00000011        IFNOT(cond)                     abort if cond <> 0
'       00000100        cogn                            output "CogN  " with possible timestamp
'       00000101        chr                             output chr
'       00000110        str                             output string
'       00000111        DLY(ms)                         delay for ms
'       00001000        PC_KEY(ptr)                     get PC keyboard
'       00001001        PC_MOUSE(ptr)                   get PC mouse
'
'       ______00        ', ' + zstr + ' = ' + data      specifiers for ZSTR..SBIN_LONG_ARRAY
'       ______01               zstr + ' = ' + data
'       ______10                       ', ' + data
'       ______11                              data
'
'       001000__        <empty>
'       001001__        ZSTR(ptr)                       z-string
'       001010__        <empty>
'       001011__        FDEC(val)                       floating point
'       001100__        FDEC_REG_ARRAY(ptr,len)         floating point
'       001101__        LSTR(ptr,len)                   length-string
'       001110__        <empty>
'       001111__        FDEC_ARRAY(ptr,len)             floating point
'
'       010000__        UDEC(val)                       unsigned decimal
'       010001__        UDEC_BYTE(val)
'       010010__        UDEC_WORD(val)
'       010011__        UDEC_LONG(val)
'       010100__        UDEC_REG_ARRAY(ptr,len)
'       010100__        UDEC_BYTE_ARRAY(ptr,len)
'       010110__        UDEC_WORD_ARRAY(ptr,len)
'       010111__        UDEC_LONG_ARRAY(ptr,len)
'
'       011000__        SDEC(val)                       signed decimal
'       011001__        SDEC_BYTE(val)
'       011010__        SDEC_WORD(val)
'       011011__        SDEC_LONG(val)
'       011100__        SDEC_REG_ARRAY(ptr,len)
'       011101__        SDEC_BYTE_ARRAY(ptr,len)
'       011110__        SDEC_WORD_ARRAY(ptr,len)
'       011111__        SDEC_LONG_ARRAY(ptr,len)
'
'       100000__        UHEX(val)                       unsigned hex
'       100001__        UHEX_BYTE(val)
'       100010__        UHEX_WORD(val)
'       100011__        UHEX_LONG(val)
'       100100__        UHEX_REG_ARRAY(ptr,len)
'       100101__        UHEX_BYTE_ARRAY(ptr,len)
'       100110__        UHEX_WORD_ARRAY(ptr,len)
'       100111__        UHEX_LONG_ARRAY(ptr,len)
'
'       101000__        SHEX(val)                       signed hex
'       101001__        SHEX_BYTE(val)
'       101010__        SHEX_WORD(val)
'       101011__        SHEX_LONG(val)
'       101100__        SHEX_REG_ARRAY(ptr,len)
'       101101__        SHEX_BYTE_ARRAY(ptr,len)
'       101110__        SHEX_WORD_ARRAY(ptr,len)
'       101111__        SHEX_LONG_ARRAY(ptr,len)
'
'       110000__        UBIN(val)                       unsigned binary
'       110001__        UBIN_BYTE(val)
'       110010__        UBIN_WORD(val)
'       110011__        UBIN_LONG(val)
'       110100__        UBIN_REG_ARRAY(ptr,len)
'       110101__        UBIN_BYTE_ARRAY(ptr,len)
'       110110__        UBIN_WORD_ARRAY(ptr,len)
'       110111__        UBIN_LONG_ARRAY(ptr,len)
'
'       111000__        SBIN(val)                       signed binary
'       111001__        SBIN_BYTE(val)
'       111010__        SBIN_WORD(val)
'       111011__        SBIN_LONG(val)
'       111100__        SBIN_REG_ARRAY(ptr,len)
'       111101__        SBIN_BYTE_ARRAY(ptr,len)
'       111110__        SBIN_WORD_ARRAY(ptr,len)
'       111111__        SBIN_LONG_ARRAY(ptr,len)
'
'
' For PASM mode, each argument can be register or #immediate
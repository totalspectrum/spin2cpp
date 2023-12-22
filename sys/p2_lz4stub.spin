'' LZ4 startup decompressor stub for P2
'' (C) 2023 Ada Gottensträter
DAT

              org
              ' move compressed area to end of RAM

              mov ptra,##(comprLen+1)*4
              add ptra,comprLen ' end of compressed data
              mov ptrb,ramEnd ' end of memory

              'callpa comprLen, #debug_hex

              rep @.cploop,comprLen
              rdbyte temp1,--ptra
              wrbyte temp1,--ptrb
.cploop
              'wypin #"b",#62

              wrword #0,ramEnd ' make sure stream ends in zero word
lz4_decomp
              ' Decompress LZ4 data
              mov ptra,ramEnd
              sub ptra,comprLen
              'callpa ptra, #debug_hex
              rdfast #0,ptra
              mov ptrb,#0

.next_token
              rfbyte token
              'callpa token, #debug_hex
              getnib runLen,token,#1
              tjz runLen,#.no_literal
              cmp runLen,#15 wz
        if_nz jmp #.do_literal
.extend_literal
              rfbyte temp1
              add runLen,temp1
              cmp temp1,#255 wz
        if_z  jmp #.extend_literal
.do_literal
              rep @.litlp,runLen
              rfbyte temp1
              wrbyte temp1,ptrb++
.litlp
.no_literal

              rfword ptra wz
        if_z  jmp #exec_payload ' offset zero is invalid and means we reached the end
              subr ptra,ptrb
              getnib runLen,token,#0
              add runLen,#4 ' minimum match length
              cmp runLen,#15+4 wz
        if_nz jmp #.do_match
.extend_match
              rfbyte temp1
              add runLen,temp1
              cmp temp1,#255 wz
        if_z  jmp #.extend_match
.do_match
              rep @.matlp,runLen
              rdbyte temp1,ptra++
              wrbyte temp1,ptrb++
.matlp
              jmp #.next_token


exec_payload
              coginit #0,#0
              

ramEnd        long $7C000 - 2


comprLen      res 1 ' gets autofilled with length

temp1         res 1
token         res 1
runLen        res 1



' Various Spin-isms
VAR
byte b
word w
long l

PUB looks(p) 
l := lookup(p : ina,outa,123,p,1+1,outa+1,b,w..l)
l := lookupz(p : ina..outa,-1,58)
b := lookdown(p : 0,1,2,3,ina,b,w,l)
b := lookdownz(p : 0..3,ina,vscl..outb)

PUB repeatn
repeat 5 ' No TJNZ
    ifnot outa++
        quit ' effectively terminal IF_NZ optimization grips here

repeat b ' Yes TJNZ
    ifnot outa++
        quit ' here too

repeat 0 ' Elided
    ifnot outa++
        quit

PUB repeatcount | i
repeat i from 1 to 10
    outa := w
repeat l from 1 to 10
    outa := w
repeat w from 1 to 10
    outa := w
repeat b from 1 to 10
    outa := w

PUB oddities
OUTA[0..(16+INA[5])]++
SPR[15] := SPR[0]
~~SPR[-1]

PUB cases
repeat 20
    case ina ' hope OTHER is relocated to top
        0: outa++
        5: outa--
        other: outa := 999
        123:
            if vscl[5]
            quit ' should drop a lot

vcfg++ ' foil ret optimization so drop actually happens

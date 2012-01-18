{{
        F32 Demo - Concise floating point code for the Propeller

        Copyright (c) 2010 Jonathan "lonesock" Dummer

        Released under the MIT License (see the end of this file for details)

        This demo just compares the results of F32 with Float32Full,
        sending the text back over the serial link.  You can capture
        the results and look at them in your favorite spreadsheet app.

        Updated Nov 27, 2010:
        Now the 2nd portion of this demo shows how to call the F32 routines from
        another PASM cog.
}}

' set for the standard 5MHz crystal...this must be correct for the serial code to work
CON
  _CLKMODE = XTAL1 + PLL16X 
  _XINFREQ = 5_000_000

OBJ
  term          : "FullDuplexSerial"
  f32           : "F32"
  f32_orig      : "Float32Full"
  fs            : "FloatString"

PUB demo_F32 | i1, f1, fA, fB

  ' start up the floating point objects and my serial link
  f32.start
  f32_orig.start  
  term.start( 31, 30, 0, 115_200 )

  ' let the bst Terminal go live (automatic, but takes a bit of time)      
  'waitcnt( (clkfreq >> 1) + cnt )
  ' or, if you need to manually start the parallax terminal, then press any key
  term.rx

  ' you may want to check the results in Excel
  term.str( string( "Input,F32,Float32Full", 13, 10 ) )

  ' do a quick timing check
  {
  i1 := -cnt
  f1 := f32.FMul( pi, pi )
  i1 += cnt
  term.dec( i1 - 544 )
  term.str( string( " counts for pi * pi.", 13, 13 ) )
  term.rx
  '}

  ' i1 is a 32-bit signed integer (native format), stored in a long  
  repeat i1 from -30 to 30

    ' encode an integer in float32 format (also stored in a long)
    f1 := f32.FFloat( i1 )
    ' note, if you treat f1 as a regular signed integer you'll have a weird number
    
    ' scale the range from [-30.0,30.0] to [-2*pi,2*pi]
    'f1 := f32.FMul( f1, constant( pi / 15.0 ) )
    f1 := f32.FMul( f1, constant( 0.9 / 10.0 ) )

    ' write this number, taking into account it's encoded as a float32
    ' (i.e. term.dec( f1 ) is not what you want to do)
    term.str( fs.FloatToString( f1 ) )

    ' get the F32 test value
    fA := f32.FRound( f1 )
    term.tx( "," )
    'term.str( fs.FloatToString( fA ) )
    term.dec( fA )

    ' get the Float32Full test value
    fB := f32_orig.FRound( f1 )
    term.tx( "," )
    'term.str( fs.FloatToString( fB ) )
    term.dec( fB )

    ' newline, and loop
    term.tx( 13 )
    term.tx( 10 )

  ' now call my PASM demo
  demo_F32_pasm
    
{{

  This portion of the demo shows the calling convention used by PASM.
  F32 expects a vector of 3 sequential longs in Hub RAM, call them:
      "result a b"
  F32 also has a pointer long "f32_Cmd".  The calling goes like this:

  result := the dispatch call command (e.g. cmdFAdd)
  a := the first floating point encoded parameter
  b := the second floating point encoded parameter

  Now the vector is initialized, you set "f32_Cmd" to the address of
  the start of the vector:

  f32_Cmd := @result

  Just wait until f32_Cmd equals 0, and by then the F32 object wrote
  the result of the floating point operation into "result" (the
  head of the input vector).      
}}
PUB demo_F32_pasm | timeout
  ' The PASM calling cog needs to know 2 base addresses
  F32_call_vector[0] := f32.Cmd_ptr
  F32_call_vector[1] := f32.Call_ptr
  ' start up the demo cog
  cognew( @F32_pasm_eg, @F32_call_vector )

  ' and just print some stuff
  timeout := cnt
  repeat
    ' print it out
    term.str( fs.FloatToString( F32_call_vector[2] ) )
    term.tx( 13 )
    term.tx( 10 )
    ' wait
    waitcnt( timeout += clkfreq )   
  
DAT     ' this is the F32 call vector (3 longs)
        ' Note: all 3 are initialized to 0.0 (the Spin compiler can do fp32 constants)
F32_call_vector         long    0.0[3]

ORG 0
F32_pasm_eg             ' read my pointer values in
                        mov     t1, par
                        rdlong  cmd_ptr, t1
                        add     t1, #4
                        rdlong  call_ptr, t1

                        ' initialize vector[1] (1st parameter) to 1.0
                        wrlong  increment, t1

demo_loop               ' load the dispatch call into vector[0]
                        mov     t1, #f32#offAdd
                        add     t1, call_ptr
                        rdlong  t1, t1
                        wrlong  t1, par

                        ' call the F32 routine by setting the command pointer to non-0
                        mov     t1, par
                        wrlong  t1, cmd_ptr

                        ' now wait till it's done!
:waiting_loop           rdlong  t1, cmd_ptr     wz
              if_nz     jmp     #:waiting_loop

                        ' Done!  vector[0] = vector[1] + vector[2]
                        rdlong  t1, par

                        ' update my 2nd parameter, and do this all over again
                        mov     t2, par
                        add     t2, #8
                        wrlong  t1, t2

                        jmp     #demo_loop

increment     long      1.0e6                        
cmd_ptr       res       1
call_ptr      res       1
t1            res       1
t2            res       1



{{


+------------------------------------------------------------------------------------------------------------------------------+
|                                                   TERMS OF USE: MIT License                                                  |                                                            
+------------------------------------------------------------------------------------------------------------------------------+
|Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation    | 
|files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy,    |
|modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software|
|is furnished to do so, subject to the following conditions:                                                                   |
|                                                                                                                              |
|The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.|
|                                                                                                                              |
|THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE          |
|WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR         |
|COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,   |
|ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                         |
+------------------------------------------------------------------------------------------------------------------------------+
}}
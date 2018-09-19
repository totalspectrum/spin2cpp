Fastspin
========
(Fastspin is copyright 2011-2018 Total Spectrum Software Inc.,
and is distributed under the terms of the MIT License (see the
end of this file for details)

Fastspin is a compiler from the Spin language to assembly (PASM).
Normally Spin is translated to a special kind of bytecode which
is interpreted by a program in the Propeller's ROM. This bytecode
is very compact, but it is slow to run. Fastspin produces much larger
code, but it is also much faster.

The command line options for fastspin are very similar to those of the
openspin compiler.

The basic usage is very simple:

    fastspin program.spin

will compile `program.spin` into `program.binary`, which may then be
loaded into the Propeller. There are many methods to do this; most
IDEs (including the Propeller Tool) have a way to run a binary
file. There are also command line tools (such as `propeller-load`) to
run binaries. My workflow with Spin programs typically looks like:

    emacs program.spin & # or use your favorite text editor
    fastspin program.spin
    propeller-load program.binary -r -t

## Propeller 2 Support ##

fastspin supports the Propeller 2 instruction set (the v32b FPGA images).
To compile programs for Propeller 2, you can use the -2 option. Binaries
may be downloaded to the board using Dave Hein's loadp2 program:

    fastspin -2 program.spin2
    loadp2 program.binary -t

If the fastspin program is named something that ends in "spin2" (for
example "fastspin2.exe") then it will use the -2 flag automatically. This
may be more convenient for integration with IDEs.

## Speed and Size ##

If you compile with fastspin, your binary program will be much larger
than when compiled with openspin or bstc. That's because fastspin
outputs native Propeller instructions (PASM) instead of Spin
bytecode. It also means the fastspin compiled binary is much faster.

For example, the fftbench demo program compiled with

    bstc -b -Oa fftbench.spin

is 3048 bytes long and runs in 1460 milliseconds. With

    fastspin -O fftbench.spin

it is 4968 bytes long and runs in 170 milliseconds; so it is a bit
less than twice as big and runs more than 8 times as fast.

The SPI test benchmark gives:

    openspin -u: 11732816 cycles  796 bytes
    bstc -Oa:    11699984 cycles  796 bytes
    fastspin -O:    98848 cycles 2056 bytes


Spin wrappers
-------------

The simplest way to use fastspin is just to compile a whole program
(convert everything to PASM). However, sometimes a program compiled
this way may be too big to fit in memory; or sometimes you may want
to convert some Spin module to PASM and make it easy to use in other
Spin projects (which may be compiled with openspin or bstc).

fastspin may be used to convert a Spin object into PASM code that has
Spin wrappers. This is achieved with the `-w` ("wrap") command line
flag. The output is a generic Spin module with a `.cog.spin` extension.
The wrapped Spin must fit in a single COG (so no LMM mode is used) and is
designed basically for converting device drivers from Spin to PASM
easily. All of the PUB functions of the original `.spin` will be available
in in the `.cog.spin`, but instead of running Spin bytecode they will send
a message to the PASM code (which must be running in another COG) for
execution there.  There will also be a `__cognew` method to start a COG up.
`__cognew` must be called before any other methods.

### Example

Suppose you have a file `Fibo.spin` that has:
```
PUB fibo(n)
  if (n < 2)
    return n
  return fibo(n-1) + fibo(n-2)
```

To use the Spin version of this you would do something like:
```
OBJ f : "Fibo"

PUB test
  answer1 := f.fibo(1)
  answer9 := f.fibo(9)
```

To convert this to COG PASM you would do:

    fastspin -w Fibo.spin

which would produce `Fibo.cog.spin`; and you would modify your program to
```
OBJ f : "Fibo.cog"

PUB test
  f.__cognew '' start the COG PASM running
  answer1 := f.fibo(1)
  answer9 := f.fibo(9)
```

The usage is exactly the same, except that you have to insert the call
to `__cognew` to start up the remote COG; but now the time critical
`fibo` function will actually run in the other COG, as PASM code.

Command Line Options
--------------------
There are various command line options
which may modify the compilation:
```
  [ -h ]              display this help
  [ -L or -I <path> ] add a directory to the include path
  [ -o ]             output filename
  [ -b ]             output binary file format
  [ -e ]             output eeprom file format
  [ -c ]             output only DAT sections
  [ -l ]             output a .lst listing file
  [ -f ]             output list of file names
  [ -q ]             quiet mode (suppress banner and non-error text)
  [ -p ]             disable the preprocessor
  [ -O[#] ]          set optimization level
                       -O0 disable all optimization
                       -O1 apply default optimization (same as no -O flag)
		       -O2 apply all optimization (same as -O)
  [ -D <define> ]    add a define
  [ -2 ]             compile for Prop2
  [ -w ]             produce Spin wrappers for PASM code
  [ --code=cog  ]    compile to run in COG memory instead of HUB
  [ --fcache=N  ]    set size of FCACHE space in longs (0 to disable)
  [ --fixed ]        use 16.16 fixed point instead of IEEE floating point
```
The `-2` option is new: it is for compiling for the Propeller 2 (v32
FPGA version). It is still somewhat experimental, but works well enough
to compile the Prop2 boot ROM.

`fastspin.exe` checks the name it was invoked by. If the name starts
with the string "bstc" (case matters) then its output messages mimic
that of the bstc compiler; otherwise it tries to match openspin's
messages. This is for compatibility with Propeller IDE. For example,
you can use fastspin with the PropellerIDE by renaming `bstc.exe` to
`bstc.orig.exe` and then copying `fastspin.exe` to `bstc.exe`.

Extensions
----------

fastspin supports a number of extensions to the Spin language, including a preprocessor (also found in some other Spin compilers), multiple values in return and assignments, conditional assignment, inline assembly, and abstract function pointers.

See the `spin.md` file in the `docs` directory for more details.


Limitations
-----------

Beware when compiling P1 objects that contain PASM for P2: some
instructions have changed in subtle ways.

Programs compiled with fastspin will always be larger than those
compiled with openspin or bstc. If the spin code is mostly PASM (as is
the case with, for example, PropBASIC compiler output) then the
fastspin overhead will be relatively small and probably fixed, but for
large programs with lots of Spin methods the difference could be
significant.

License
-------

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files
(the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge,
publish, distribute, sublicense, and/or sell copies of the Software,
and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

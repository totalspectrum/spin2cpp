SPIN2CPP = ../../build/spin2cpp
FASTSPIN = ../../build/fastspin
OPENSPIN = openspin -q
CC = propeller-elf-gcc
CFLAGS = -O -mfcache

default: time.elf demo_F32.elf time2.binary time2_cog.binary

demo_F32.elf: demo_F32.spin F32.spin Float32Full.spin FloatMath.spin FloatString.spin FullDuplexSerial.spin
	$(SPIN2CPP) --elf $(CFLAGS) -o $@ demo_F32.spin

time.elf: time.spin F32.spin Float32Full.spin FloatMath.spin FloatString.spin FullDuplexSerial.spin
	$(SPIN2CPP) --elf $(CFLAGS) -o $@ time.spin

time2.binary: time2.spin Float32Full.spin FloatString.spin FullDuplexSerial.spin
	$(OPENSPIN) -q time2.spin

time2_cog.binary: time2_cog.spin FloatMathSimple.cog.spin FloatString.spin FullDuplexSerial.spin
	$(OPENSPIN) -q time2_cog.spin

FloatMathSimple.cog.spin: FloatMathSimple.spin
	$(FASTSPIN) -w FloatMathSimple.spin

clean:
	rm -f *.cpp *.c *.h *.elf *.binary *.cog.spin

SPIN2CPP = ../../spin2cpp
CC = propeller-elf-gcc
CFLAGS = -O2 -mfcache

default: time.elf demo_F32.elf

demo_F32.elf: demo_F32.spin F32.spin Float32Full.spin FloatMath.spin FloatString.spin FullDuplexSerial.spin
	$(SPIN2CPP) --elf $(CFLAGS) -o $@ demo_F32.spin

time.elf: time.spin F32.spin Float32Full.spin FloatMath.spin FloatString.spin FullDuplexSerial.spin
	$(SPIN2CPP) --elf $(CFLAGS) -o $@ time.spin


clean:
	rm -f *.cpp *.h *.elf

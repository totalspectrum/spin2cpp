SPIN2CPP = ../../spin2cpp
CC = propeller-elf-gcc
CFLAGS = -O2 -mfcache

default: time.elf demo_F32.elf

demo_F32.elf: demo_F32.spin F32.spin Float32Full.spin FloatMath.spin FloatString.spin FullDuplexSerial.spin
	$(SPIN2CPP) -main demo_F32.spin
	$(CC) $(CFLAGS) -o $@ demo_F32.cpp F32.cpp Float32Full.cpp FloatMath.cpp FloatString.cpp Float32A.cpp FullDuplexSerial.cpp

time.elf: time.spin F32.spin Float32Full.spin FloatMath.spin FloatString.spin FullDuplexSerial.spin
	$(SPIN2CPP) -main time.spin
	$(SPIN2CPP) F32.spin
	$(SPIN2CPP) FloatMath.spin
	$(SPIN2CPP) Float32Full.spin
	$(CC) $(CFLAGS) -o $@ time.cpp F32.cpp Float32Full.cpp FloatMath.cpp FloatString.cpp Float32A.cpp FullDuplexSerial.cpp


clean:
	rm -f *.cpp *.h *.elf

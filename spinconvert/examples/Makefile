SPIN2CPP = ../../build/spin2cpp

default: hello.binary math.binary led.binary

%.pasm: %.spin
	$(SPIN2CPP) --asm $<

%.binary: %.pasm
	$(SPIN2CPP) --dat --binary $<

clean:
	rm -f *.pasm *.binary *.cpp *.c *.h

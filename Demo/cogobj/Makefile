FASTSPIN=../../build/fastspin -q
OPENSPIN=openspin -q -L../../Lib

all: fibodemo.binary blink1.binary blink2.binary

fibodemo.binary: fibodemo.spin Fibo.spin Fibo.cog.spin SimpleSerial.cog.spin
	$(OPENSPIN) fibodemo.spin

blink1.binary: blink1.spin
	$(OPENSPIN) blink1.spin

blink2.binary: blink2.spin blinker.cog.spin
	$(OPENSPIN) blink2.spin

Fibo.cog.spin: Fibo.spin
	$(FASTSPIN) -w Fibo.spin

SimpleSerial.cog.spin: SimpleSerial.spin
	$(FASTSPIN) -w SimpleSerial.spin

blinker.cog.spin: blinker.spin
	$(FASTSPIN) -w blinker.spin

clean:
	rm -rf *.binary *.cog.spin


# the .cog.spin files must be built with fastspin
FASTSPIN=../../build/fastspin -q -2

# regular .spin files may be built with the Spin compiler of your choice
#SPIN=openspin -q -L../../Lib
SPIN=$(FASTSPIN)

all: fibodemo.binary blink1.binary blink2.binary blink7.binary fibomulti.binary

fibodemo.binary: fibodemo.spin Fibo.spin Fibo.cog.spin SimpleSerial.cog.spin
	$(SPIN) fibodemo.spin

fibomulti.binary: fibomulti.spin Fibo.spin Fibo.cog.spin SimpleSerial.cog.spin
	$(SPIN) fibomulti.spin

blink1.binary: blink1.spin
	$(SPIN) blink1.spin

blink2.binary: blink2.spin blinker.cog.spin
	$(SPIN) blink2.spin

blink7.binary: blink7.spin blinker.cog.spin
	$(SPIN) blink7.spin

Fibo.cog.spin: Fibo.spin
	$(FASTSPIN) -w Fibo.spin

SimpleSerial.cog.spin: SimpleSerial.spin
	$(FASTSPIN) -w SimpleSerial.spin

blinker.cog.spin: blinker.spin
	$(FASTSPIN) -w blinker.spin

clean:
	rm -rf *.binary *.cog.spin


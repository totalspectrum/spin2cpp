FASTSPIN=../../build/fastspin

all: demo.binary

demo.binary: demo.spin Fibo.spin Fibo.cog.spin SimpleSerial.cog.spin
	openspin demo.spin

Fibo.cog.spin: Fibo.spin
	$(FASTSPIN) -w Fibo.spin

SimpleSerial.cog.spin: SimpleSerial.spin
	$(FASTSPIN) -w SimpleSerial.spin

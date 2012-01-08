default: demo.elf

demo.elf: Hello.spin FullDuplexSerial.spin
	../spin2cpp -main Hello.spin
	propeller-elf-gcc -o demo.elf -Os Hello.cpp FullDuplexSerial.cpp -s

clean:
	rm -f *.cpp *.h demo.elf

# Teeprom
Teensy 2.0++ EEPROM programmer/reader for the SST27SF512 parallel EEPROM. Can easily be adapted to work for other parallel EEPROMs.

Connection info:
The 8 data pins for the EEPROM are configured for pins D0-D7.
The lower 8 bits of the 16 bit address bus are configured for pins F0-F7.
The upper 8 bits of the 16 bit address bus are configured for pins C0-C7.
The CE pin is configured for pin E0, and the OE pin is configured for pin E1.
The SST27SF512 requires 12v to erase and program, to accomplish this I have attached relays to control pins E6 and E7. E6 sets the OE pin to 12v, and E7 sets the A9 pin to 12v. Before I had the relays, I manually applied the voltage. The relays just make things easier, but are not neccesary.

Program info:
I borrowed a lot of the outline for this program from the MEEPROMER project for the Arduino Uno, which used shift registers to address all the necessary pins. I reconfigured it to use the Teensy 2.0++'s port/pin interface. I removed the ability to write in smaller chunks because the SST27SF512 has to be written all at once. These functions could easily be restored with a look at the MEEPROMER source code featured here: https://github.com/mkeller0815/MEEPROMMER

Also included is a python command line client that can be used for both writing/reading the EEPROM. I borrowed the outline from the code here: https://github.com/pda/pda6502/tree/master/tools . On this too, I removed features that weren't necessary for my application, but could easily be restored if you need them. My version of the client works like this:
There are two main commands: -w for writing, and -d for dumping the EEPROM into a file. Additional arguments are:

-f: this is the file argument. For writing, the file to write is here. For dumping, the file named is written to.

-b: this is the bytes argument, which specifies the size of the EEPROM. It is in kbytes, I.E 64 = 64 kbytes, or 512 kbits.

-o: this is the offset argument. For writing, this pads a certain amount of kbytes before the file is written. For reading, this changes the address at which you begin reading. For example, if you wish to pad the first half of a 64 kb eeprom, you would use -o 32. If you wish to only read the second half of the EEPROM, you would also use -o 32.

-c: this is the com port argument. Set this to the com port that your Teensy is currently assigned to.

-s: this is the speed argument. I haven't used it, but the code is currently set to 115200, which works. Change it if you'd like, just remember it has to be changed on the teensy's end too.

An example of a command would be this: py -3 Teeprom.py -w -f yourfile.bin -c COM3
This command (executed on windows) would use Python 3, and write the file yourfile.bin to the teensy on the serial port COM3.

On the arduino side of things, for timing, I use a module called delay_x.h, which adds in more accurate delays for writing, and usb_serial.h for the serial communication. I put these in my Arduino directory under /hardware/teensy/avr/cores/teensy . This is on a Windows PC. 
Anyways, hope this can be helpful to someone else. I started this project almost a year ago with no experience in programming, and quickly realized I was in way over my head. After a year of reading programming books intermittently, I've made a program that works. I'm currently using it to program the EEPROM of my car's ECU, and the programmers reccomended cost around a hundred dollars. This is obviously a lot cheaper if you have a Teensy, and even if you don't it is. It is a little more time consuming, but I think it's worth it. If you've read this far, thanks for reading, and hopefully this can prove useful for you.

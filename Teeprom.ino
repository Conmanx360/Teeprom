//Teensy EEPROM programmer/reader. Configured by default for the 
//SST27SF512 EEPROM, but can be easily reconfigured for other 
//EEPROM's. Code outline borrowed from the MEEPROMER project
//for the Arduino uno, and ported over to the Teensy 2.0++.
//Last write on 8/22/17.

#include <avr/io.h>
#include <avr/interrupt.h>
#include "delay_x.h"
#include "usb_serial.h"

//Define the pins for the data port.
#define DATA1_PORT  PORTD
#define DATA1_PIN PIND
#define DATA1_DDR DDRD

//Define the pins for the lower address port.
//These are the first 8 bits of the address.
#define ADDR1_PORT  PORTF
#define ADDR1_PIN PINF
#define ADDR1_DDR DDRF

//Define the pins for the upper address port.
//These are the last 8 bits of the address.
#define ADDR2_PORT  PORTC
#define ADDR2_PIN PINC
#define ADDR2_DDR DDRC

//Define the pins for the control port.
//These pins are used for OE, CE, and to activate
//the relays to set OE and A9 to 12v. Currently only
//using 4 pins, so there are 4 more to use for whatever.
#define CONT_PORT PORTE
#define CONT_DDR  DDRE
#define CONT_PIN  PINE

//Setting the size for the write buffer. I have it set to 1 kilobyte,
//the Teensy has enough ram for a bigger buffer, but making it larger
//just caused timing problems.
#define BUFFERSIZE 1024
byte writebuffer[BUFFERSIZE];

//Create the buffer for the command values. Stores the various values
//used in commands.
#define COMMANDSIZE 32
char cmdbuf[COMMANDSIZE];

uint32_t EEPROMkbytes, EEPROMpadding_readoffset;

//define values for COMMANDs. These are used within the parser.
#define NOCOMMAND   0
#define READ_HEX    1
#define WRITE_HEX   2
#define ERASE       3

/*****************************************************************
 *
 *  CONTROL and DATA functions
 *
 ****************************************************************/

//Switch the data bus to an input for reading.
void      data_bus_input() {
   DDRD = 0x00;
}

//Switch the data bus to an output for writing.
void data_bus_output() {
   DDRD = 0xFF; 
}

//Read a single byte from the data bus.
byte read_data_bus()
{
   return(DATA1_PIN);
}

//Write a single byte to the data bus.
void write_data_bus(byte data) {
   DATA1_PORT = data;
}

//Splits the 16 bit address in two and sets each
//address bus to their proper values.
void set_address_bus(uint16_t address) {

   //Get the upper 8 bits of the address bus by shifting them over.
   byte hi = address >> 8;
   //get the lower 8 bits by ANDing the lower 8 bits to get rid of the upper ones.
   byte low = address & 0xFF;
   //Now set the upper 8 bits the second port.
   ADDR2_PORT = (hi);
   //Now set the lower 8 bits to the first port.
   ADDR1_PORT = (low);
}

//Set output enable to high by setting the bit on PORTE for pin 1 to 1. 
void set_oe_high () {
   PORTE |= (1<<1);
}

//Set output enable pin to low by setting the bit to 0 if it is already high.
void set_oe_low () {
   if(PINE & (1<<1)){
  PORTE &= ~(1<<1); 
  }
}

//Set chip enable to high by setting the bit on PORTE for pin 0 to 1.
void set_ce_high ()
{
  PORTE |= (1<<0);
}
//Set chip enable pin to low by setting the bit to 0 if it is already high.
void set_ce_low ()
{
  if(PINE & (1<<0)){
  PORTE &= ~(1<<0); 
  }
}

//For using relays to switch to 12v signal on a9. Used for checking chip ID
//and erasing the chip.
void set_a9_vhigh ()
{
  PORTE |= (1<<7);
}

//Set the relay to low to connect the regular address line.
void set_a9_normal ()
{
   if(PINE & (1<<7)){
  PORTE &= ~(1<<7); 
  }
}

//For using relays to switch to 12v signal on oe. Used for erasing the chip.
void set_oe_vhigh ()
{
  PORTE |= (1<<6);
}
//Set the relay to low to connect the regular output enable line.
void set_oe_normal ()
{
   if(PINE & (1<<6)){
  PORTE &= ~(1<<6); 
  }
}

//Command to read a byte from a certain address. Returns the byte that is read.
byte read_byte(uint16_t address)
{
  byte  data = 0;
  //set databus for reading 
  set_address_bus(address);
  _delay_ns(70);
  data = read_data_bus();
  return data;

}

//Command to write a byte to a certain address. Can be modified to work with other
//EEPROM's. This works specifically with the SST27SF512. Look at your specific
//EEPROM's datasheet to figure out write timings.
void fast_write(uint16_t address, byte data) {
  //Set the address bus to the address you want to write.  
  set_address_bus(address);
  //Set the data bus with the data you want to write.
  write_data_bus(data);
  //What happens here is specific to your eeprom's timings.
  set_ce_low();
  _delay_us(25);
  set_ce_high();
}
//For writing parts of the EEPROM with padding. Padding value is 0xFF, and the
//value is where you want it to stop. Could add a start address too, but it is
//not needed in my application.
void rom_padding(uint32_t address) {
   if(address == 0) {
    return;
   }
   uint32_t i;
   uint32_t fullAddress = address * 1024;   
   for(i = 0; i < fullAddress; i++) {
      fast_write(i, 0xFF);
   }
}

//This erases the SST27SF512 by setting the OE and A9 pins to 12v, and then
//toggling the ce pin from high to low.
void eeprom_erase() {
     set_oe_vhigh();
     set_a9_vhigh();
     delay(1000);
     set_ce_high();
     delay(100);
     set_ce_low();
     delay(100);
     set_ce_high();
     set_oe_normal();
     set_a9_normal();
     set_ce_low();
}

//This is for buffered writing, which is the only writing method I have included here.
//This reads data from the serial port into a 1024 byte write buffer, writes the data
//to the EEPROM, and then clears the array and repeats. Datasize is how many Kbytes
//your EEPROM is, as the buffer is in 1 kbyte chunks.
void buffered_write(uint32_t datasize, uint32_t offsetvalue) {
  uint32_t i, y, x;
  uint32_t repeatCount = (datasize-offsetvalue);
  uint32_t currentAddress = (offsetvalue * 1024); // Set the address bus to 32768 after writing the  first half of the EEPROM with padding.   
  Serial.write(45);  // Signal to begin transmission of data from PC. It is a '-' in ascii.
   
  for(x = 0; x < repeatCount; x++){ 
     for(y = 0; y < BUFFERSIZE; y++) {
        if(Serial.available() > 0) {
           writebuffer[y] = Serial.read();  
        }
     }
     for(i = 0; i < BUFFERSIZE; i++) {
        fast_write((currentAddress+i), writebuffer[i]);  
     }  
  currentAddress += BUFFERSIZE;
  memset(writebuffer, 0, sizeof(writebuffer)); 
  }
}      

//This dumps all of the data from your EEPROM to the serial port for reading.
//Currently read using Pyserial, works for me.
void eeprom_dump(uint32_t EEPROMsize, uint32_t beginAddress) {
   uint32_t i;
   uint32_t readBegin = beginAddress * 1024;
   uint32_t filesize = (EEPROMsize - beginAddress) * 1024; 
   for(i = 0; i < filesize; i++) {
      byte output = read_byte(i+readBegin);
      Serial.write(output);
   }
}


/************************************************
 *
 * COMMAND and PARSING functions
 *
 *************************************************/

//Read ascii characters from the serial input for commands. Values are stored
//into a buffer, and eventually parsed with the function parseCommand().
void readCommand() {
  //first clear command buffer
  memset(cmdbuf, 0, sizeof(cmdbuf));
  //initialize variables
  char c = ' ';
  int idx = 0;
  //now read serial data until linebreak or buffer is full
  do {
    if(Serial.available()) {
      c = Serial.read();
      cmdbuf[idx++] = c;
    }
  } 
  while (c != '\n' && idx < (COMMANDSIZE)); //save the last '\0' for string end
  //change last newline to '\0' termination
  cmdbuf[idx - 1] = 0;
}

//Parse the commands from the read command function. Each value is seperated
//by a comma, and the final terminating character is a '\n' newline character.
//The EEPROM size and padding/read begin values are sent in hex, and converted
//to regular integers through the three hex functions, hexDigit, hexByte,
//and hexWord. This allows for a maximum eeprom size way larger than can be
//addressed with a 16 bit address bus, but could prove useful for larger
//EEPROMs.
byte parseCommand() {
  //set ',' to '\0' terminator (command string has a fixed strucure)
  //First string is the command character, i.e W for write, R for dump.
  cmdbuf[1]  = 0;
  //Second string is the EEPROM size in kbytes. 
  cmdbuf[6]  = 0;
  //Third string is how far to pad from the beginning, or where to begin
  //reading from.
  cmdbuf[11] = 0;
  EEPROMkbytes=hexWord((cmdbuf+2));
  EEPROMpadding_readoffset=hexWord((cmdbuf+7));
  byte retval = 0;
  switch(cmdbuf[0]) {
  case 'R':
    retval = READ_HEX;
    break;
  case 'W':
    retval = WRITE_HEX;
    break;
  case 'E':
     retval = ERASE;
     break;
  default:
    retval = NOCOMMAND;
    break;
  }

  return retval;
}


byte hexDigit(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  } 
  else if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  } 
  else if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  } 
  else {
    return 0;   // getting here is bad: it means the character was invalid
  }
}


byte hexByte(char* a) {
  return ((hexDigit(a[0])*16) + hexDigit(a[1]));
}

uint16_t hexWord(char* data) {
  return ((hexDigit(data[0])*4096)+
    (hexDigit(data[1])*256)+
    (hexDigit(data[2])*16)+
    (hexDigit(data[3]))); 
}

/************************************************
 *
 * MAIN
 *
 *************************************************/
void setup() {

  // Set control pins to output.
  DDRF = 0xFF;
  DDRC = 0xFF;
  DDRE = 0xFF;
  PORTE = 0x00;
  PORTD = 0x00;
  DDRD = 0x00;
  set_address_bus(0);
  
  set_oe_normal();
  set_a9_normal();
  //set speed of serial connection
  Serial.begin(115200);
  
}

/**
 * main loop, that runs infinite times, parsing a given command and 
 * executing the given read or write requestes.
 **/
void loop() {
  readCommand();
  uint32_t cmd = parseCommand();
  switch(cmd) {
  
   case READ_HEX:     
      data_bus_input();
      PORTD = 0xFF;
      eeprom_dump(EEPROMkbytes, EEPROMpadding_readoffset);
      setup(); 
      break;
  case WRITE_HEX:
     {
     eeprom_erase(); //The SST27SF512 must be erased before being written.
     setup();        //may differ depending on EEPROM.     
     set_oe_vhigh();
     delay(2000);
     data_bus_output();
     PORTD = 0x00;
     set_ce_high();
     set_address_bus(0);
     rom_padding(EEPROMpadding_readoffset);
     buffered_write(EEPROMkbytes, EEPROMpadding_readoffset);
     delay(1000);
     set_oe_normal();
     setup();
     }
     break;
  case ERASE:
     eeprom_erase();
     setup();   
     break;
  default:
    break;    
  }


}


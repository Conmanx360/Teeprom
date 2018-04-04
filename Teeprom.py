import serial, sys, argparse, time
buffersize = 1024
parser = argparse.ArgumentParser(
   description='Meepromer Command Line Interface',
   epilog='Read source for further information')

task = parser.add_mutually_exclusive_group()
task.add_argument('-w', '--write', dest="cmd", action="store_const",
      const="write", help='Write file to EEPROM!')
task.add_argument('-d', '--dump', dest="cmd", action="store_const",
      const="dump", help='Dump eeprom contents to file!')

parser.add_argument('-o', '--offset', action='store', default='0',
        type=int, help='Input file offset (as hex), default 0')
parser.add_argument('-b', '--bytes', action='store', default='64',
        type=int, help='Number of kBytes to r/w, default 64')
parser.add_argument('-c', '--com', action='store', 
        default='COM3', help='Com port address')
parser.add_argument('-s', '--speed', action='store', 
        type=int, default='115200', help='Com port baud, default 115200')
parser.add_argument('-f', '--file', action='store',
      help='Name of data file')
# py -3 PyserialComTest.py -w -f h22a.bin -c COM3
def write_file():
   fi = open(args.file,'rb')
   ser.write(bytes("W "+format(args.bytes,'04x')+" "+
       format(args.offset,'04x')+
       "\n", 'ascii'))
   ser.reset_output_buffer()
   while ser.read(1) != b'-':
      print('sleeping...')
   
   
   print('begin writing...')
   ser.write(b'~')
   for i in range(args.bytes-args.offset):   
      output = fi.read(buffersize)	  
      ser.write(output)
   print('Write complete.')
   fi.close()
   ser.flushInput()
   ser.flushOutput()
   ser.reset_input_buffer()
   ser.reset_output_buffer()

   
def dump_file():
   ser.flushInput()
   ser.write(bytes("R "+format(args.bytes,'04x')+" "+
       format(args.offset,'04x')+" \n", 'ascii'))
   eeprom = ser.read((args.bytes-args.offset)*1024)
   try:
       fo = open(args.file, 'wb+')
   except OSError:
       print("Error: File cannot be opened, verify it is not in use.")
       sys.exit(1)
   time.sleep(0.1)
   fo.write(eeprom)
   fo.close()
   ser.flushInput()
   ser.flushOutput()
   ser.reset_input_buffer()
   ser.reset_output_buffer()
   
args = parser.parse_args()

class DebugSerial(serial.Serial):
    def write(self, data):
        #print("write: %s" % (data,))
        super().write(data)

SERIAL_TIMEOUT = 20 #seconds
try:
    ser = DebugSerial(args.com, args.speed, timeout=SERIAL_TIMEOUT)
except serial.serialutil.SerialException:
    print("Error: Serial port is not valid, please select a valid port")
    sys.exit(1)

# It seems necessary to wait >= ~1.8 seconds for the device to respond.
import time
time.sleep(2)

   

if args.cmd == 'write':
   write_file()
elif args.cmd == 'dump':
   dump_file()

ser.close()
sys.exit(0)
         
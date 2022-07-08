// Demo of I2C SD Card module - using TinyI2C

#include <TinyI2CMaster.h>
int address = 0x55;

void setup (void) {
  Serial.begin(9600);
  TinyI2C.init();
}
 
void loop (void) {

  Serial.println("Writing...");
  TinyI2C.start(address, 0);
  TinyI2C.write('F');
  TinyI2C.write('A');
  TinyI2C.write('1');
  TinyI2C.restart(address, 0);
  TinyI2C.write('W');
  for (int i=48; i<=90; i++) TinyI2C.write(i);
  TinyI2C.stop();

  Serial.println("Reading...");
  TinyI2C.start(address, 0);
  TinyI2C.write('F');
  TinyI2C.write('A');
  TinyI2C.write('1');
  TinyI2C.restart(address, 0);
  TinyI2C.write('S');
  TinyI2C.restart(address, 4);
  int size = 0;
  for (int i=0; i<4; i++) size = size<<8 | TinyI2C.read();
  TinyI2C.restart(address, 0);
  TinyI2C.write('R');
  TinyI2C.restart(address, size);
  for (int i=0; i<size; i++) Serial.print((char)TinyI2C.read());
  TinyI2C.stop();
  
  for(;;);
}

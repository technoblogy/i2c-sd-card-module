// Demo of I2C SD Card module - using Arduino Wire

#include <Wire.h>
int address = 0x55;

void setup (void) {
  Serial.begin(9600);
  Wire.begin();
}
 
void loop (void) {
  Serial.println("Writing...");
  Wire.beginTransmission(address);
  Wire.write('F');
  Wire.write('A');
  Wire.write('2');
  Wire.endTransmission(false);
  Wire.beginTransmission(address);
  Wire.write('W');
  for (int i=48; i<79; i++) Wire.write(i);
  Wire.endTransmission(false);
  Wire.beginTransmission(address);
  Wire.write('W');
  for (int i=79; i<91; i++) Wire.write(i);
  Wire.endTransmission();

  Serial.println("Reading...");
  Wire.beginTransmission(address);
  Wire.write('F');
  Wire.write('A');
  Wire.write('2');
  Wire.endTransmission();
  Wire.beginTransmission(address);
  Wire.write('S');
  Wire.endTransmission(false);
  Wire.requestFrom(address, 4, false);
  unsigned long size = 0;
  for (int i=0; i<4; i++) size = size<<8 | Wire.read();
  Wire.beginTransmission(address);
  Wire.write('R');
  Wire.endTransmission(false);
  while (size > 32) { 
    Wire.requestFrom(address, 32, false);
    for (int i=0; i<32; i++) Serial.print((char)Wire.read());
    size = size - 32;
  }
  Wire.requestFrom(address, size, true);
  for (int i=0; i<size; i++) Serial.print((char)Wire.read());

  for(;;);
}

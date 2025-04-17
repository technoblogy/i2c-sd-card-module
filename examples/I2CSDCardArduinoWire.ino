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
// Initialize transmission and send file command
  Wire.beginTransmission(address);
  Wire.write('F');
  Wire.write('A');
  Wire.write('2');
  Wire.endTransmission();
// Get file size
  Wire.beginTransmission(address);
  Wire.write('S');
  Wire.endTransmission(false);
  Wire.requestFrom(address, 4, false);
  uint32_t size = 0;
  for (uint8_t i = 0; i < 4; i++) {
    size = (size << 8) | Wire.read();
  }
// Read file content
  Wire.beginTransmission(address);
  Wire.write('R');
  Wire.endTransmission(false);
// Used fixed-size buffer for better memory management
  static const uint8_t BUFFER_SIZE = 32;
  char buffer[BUFFER_SIZE];
  while (size > BUFFER_SIZE) {
    Wire.requestFrom(address, BUFFER_SIZE, false);
    for (uint8_t i = 0; i < BUFFER_SIZE; i++) {
        buffer[i] = Wire.read();
    }
    Serial.write(buffer, BUFFER_SIZE);
    size -= BUFFER_SIZE;
  }
if (size > 0) {
    Wire.requestFrom(address, size, true);
    for (uint8_t i = 0; i < size; i++) {
        buffer[i] = Wire.read();
    }
    Serial.write(buffer, size);
}

  for(;;);
}

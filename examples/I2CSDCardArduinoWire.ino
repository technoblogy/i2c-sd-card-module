#include <Wire.h>

#define I2C_SLAVE_ADDR 0x6E
#define FILENAME       "test.txt"
#define DIRNAME        "mydir"

// Helper to send filename
void sendFilename(const char* name) {
  Wire.beginTransmission(I2C_SLAVE_ADDR);
  Wire.write('F');
  for (size_t i = 0; i < strlen(name); ++i) {
    Wire.write(name[i]);
  }
  Wire.endTransmission();
  delay(5);
}

// Write a byte to a file (create/truncate)
void writeFile(const char* name, uint8_t data) {
  sendFilename(name);
  Wire.beginTransmission(I2C_SLAVE_ADDR);
  Wire.write('W');
  Wire.endTransmission();
  delay(5);

  Wire.beginTransmission(I2C_SLAVE_ADDR);
  Wire.write(data);
  Wire.endTransmission();
  delay(5);
}

// Append a byte to a file
void appendFile(const char* name, uint8_t data) {
  sendFilename(name);
  Wire.beginTransmission(I2C_SLAVE_ADDR);
  Wire.write('A');
  Wire.endTransmission();
  delay(5);

  Wire.beginTransmission(I2C_SLAVE_ADDR);
  Wire.write(data);
  Wire.endTransmission();
  delay(5);
}

// Read a byte from a file
int readFile(const char* name) {
  sendFilename(name);
  Wire.beginTransmission(I2C_SLAVE_ADDR);
  Wire.write('R');
  Wire.endTransmission();
  delay(5);

  Wire.requestFrom(I2C_SLAVE_ADDR, 1);
  if (Wire.available()) {
    return Wire.read();
  }
  return -1;
}

// Check if file exists
bool fileExists(const char* name) {
  sendFilename(name);
  Wire.beginTransmission(I2C_SLAVE_ADDR);
  Wire.write('E');
  Wire.endTransmission();
  delay(5);

  Wire.requestFrom(I2C_SLAVE_ADDR, 1);
  if (Wire.available()) {
    return Wire.read() != 0;
  }
  return false;
}

// Remove a file
bool removeFile(const char* name) {
  sendFilename(name);
  Wire.beginTransmission(I2C_SLAVE_ADDR);
  Wire.write('X');
  Wire.endTransmission();
  delay(5);

  Wire.requestFrom(I2C_SLAVE_ADDR, 1);
  if (Wire.available()) {
    return Wire.read() != 0;
  }
  return false;
}

// Create a directory
bool makeDir(const char* name) {
  sendFilename(name);
  Wire.beginTransmission(I2C_SLAVE_ADDR);
  Wire.write('D');
  Wire.endTransmission();
  delay(5);

  Wire.requestFrom(I2C_SLAVE_ADDR, 1);
  if (Wire.available()) {
    return Wire.read() != 0;
  }
  return false;
}

// Get file size (returns uint32_t)
uint32_t getFileSize(const char* name) {
  sendFilename(name);
  Wire.beginTransmission(I2C_SLAVE_ADDR);
  Wire.write('S');
  Wire.endTransmission();
  delay(5);

  uint32_t size = 0;
  Wire.requestFrom(I2C_SLAVE_ADDR, 4);
  for (int i = 0; i < 4 && Wire.available(); ++i) {
    size = (size << 8) | Wire.read();
  }
  return size;
}

// Reset the ATtiny1614 remotely
void remoteReset() {
  Wire.beginTransmission(I2C_SLAVE_ADDR);
  Wire.write('Z');
  Wire.endTransmission();
  delay(5);
}

// Write a string to a file (create/truncate), chunked for I2C buffer limit
void writeFileString(const char* name, const char* text) {
  sendFilename(name);
  Wire.beginTransmission(I2C_SLAVE_ADDR);
  Wire.write('W');
  Wire.endTransmission();
  delay(5);

  size_t len = strlen(text);
  size_t sent = 0;
  while (sent < len) {
    size_t chunk = len - sent;
    if (chunk > 31) chunk = 31; // 31 bytes per I2C transmission
    Wire.beginTransmission(I2C_SLAVE_ADDR);
    for (size_t i = 0; i < chunk; ++i) {
      Wire.write(text[sent + i]);
    }
    Wire.endTransmission();
    sent += chunk;
    delay(5);
  }
}

// Append a string to a file, chunked for I2C buffer limit
void appendFileString(const char* name, const char* text) {
  sendFilename(name);
  Wire.beginTransmission(I2C_SLAVE_ADDR);
  Wire.write('A');
  Wire.endTransmission();
  delay(5);

  size_t len = strlen(text);
  size_t sent = 0;
  while (sent < len) {
    size_t chunk = len - sent;
    if (chunk > 31) chunk = 31; // 31 bytes per I2C transmission
    Wire.beginTransmission(I2C_SLAVE_ADDR);
    for (size_t i = 0; i < chunk; ++i) {
      Wire.write(text[sent + i]);
    }
    Wire.endTransmission();
    sent += chunk;
    delay(5);
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  // Demo: Write a short string to file
  writeFileString(FILENAME, "Hello from ESP!");
  Serial.println("Wrote short string to file");

  // Demo: Read back the short string
  readFileString(FILENAME);

  // Demo: Write a long string (>32 bytes) to file, chunked
  const char* longText = "This is a long string sent from ESP8266/ESP32 to the ATtiny1614 SD card module via I2C. It will be chunked!";
  writeFileString(FILENAME, longText);
  Serial.println("Wrote long string to file");

  // Demo: Read back the long string
  readFileString(FILENAME);

  // Demo: Append a string to the file
  const char* appendText = " Appending this text to the file!";
  appendFileString(FILENAME, appendText);
  Serial.println("Appended string to file");

  // Demo: Read back the file after appending
  readFileString(FILENAME);

  // Demo: Read a byte
  int val = readFile(FILENAME);
  Serial.print("Read from file: ");
  Serial.println(val, HEX);

  // Demo: Check if file exists
  bool exists = fileExists(FILENAME);
  Serial.print("File exists: ");
  Serial.println(exists);

  // Demo: Get file size
  uint32_t size = getFileSize(FILENAME);
  Serial.print("File size: ");
  Serial.println(size);

  // Demo: Remove file
  bool removed = removeFile(FILENAME);
  Serial.print("File removed: ");
  Serial.println(removed);

  // Demo: Make directory
  bool dirMade = makeDir(DIRNAME);
  Serial.print("Directory created: ");
  Serial.println(dirMade);

  // Demo: Remote reset
  // remoteReset();
  // Serial.println("Sent remote reset command");
}

void loop() {
  // Nothing here
}

/*
 * ESP32 Simple Demo - Compatible with I2C SD-Card Module
 * 
 * This demo uses bit-bang I2C to communicate with ATtiny1614 firmware
 * to overcome the 13ms clock stretch limitation of hardware I2C.
 * 
 * Commands supported by ATtiny:
 * - 'F': Set filename (up to 13 characters)
 * - 'W': Write mode (create/truncate file)
 * - 'R': Read mode
 * - 'A': Append mode
 * - 'S': Get file size
 */

#include <stdint.h>

// Bit-bang I2C pins
const int SDA_PIN = 21;
const int SCL_PIN = 22;
const uint8_t ATTINY_ADDRESS = 0x55; // ATtiny1614 I2C address (matches MyAddress in I2CSDCardModule.ino)

// I2C timing (microseconds)
// - I2C_DELAY is the base half-period used for bit-banged SCL edges.
//   One ideal SCL period ≈ 2 * I2C_DELAY, so I2C_DELAY = 5 µs → ~100 kHz nominal.
//   The real effective rate will be lower due to pinMode/digitalWrite overhead and ACK cycles.
//   Any clock stretching by the ATtiny (up to ~50 ms) further slows individual bits while SCL is held low.
//   To speed up: reduce I2C_DELAY (e.g., 3 µs ≈ 166 kHz) if your pull-ups and both ends can meet rise-time.
//   To add margin/stability on longer lines or weak pull-ups: increase I2C_DELAY.
const int I2C_DELAY = 5;
// Maximum time to wait for SCL to be released high by the slave (clock stretching tolerance).
// 100,000 µs = 100 ms; this should exceed the worst-case stretch of the ATtiny1614 firmware.
const int I2C_CLOCK_STRETCH_TIMEOUT = 100000; // 100ms timeout

void setup() {
  Serial.begin(115200);
  delay(6000); // Wait for serial monitor to initialize
  
  Serial.println("\n=== ESP32 Simple Demo ===");
  Serial.println("Compatible with ATtiny1614 SD Bridge");
  Serial.println("Using bit-bang I2C for 100ms clock stretch support\n");
  
  // Initialize bit-bang I2C
  i2c_init();
  
  delay(2000);
  
  // Run basic tests
  test_write_operations();
  test_read_operations();
  
  // New: 128-byte stream test
  test_stream_128_operations();
  
  // New: Append test
  test_append_operations();
  
  Serial.println("\n--- All tests complete ---");
  Serial.println("System running. Tests completed.");
}

void loop() {
  // Main loop - system is idle after tests
  delay(1000);
}

// Bit-bang I2C implementation
// Open-drain helpers for SDA/SCL
static inline void sda_release() { pinMode(SDA_PIN, INPUT_PULLUP); }
static inline void sda_drive_low() { pinMode(SDA_PIN, OUTPUT); digitalWrite(SDA_PIN, LOW); }
static inline void scl_release() { pinMode(SCL_PIN, INPUT_PULLUP); }
static inline void scl_drive_low() { pinMode(SCL_PIN, OUTPUT); digitalWrite(SCL_PIN, LOW); }
static inline bool scl_wait_high(uint32_t timeout_us) {
  uint32_t start = micros();
  while (digitalRead(SCL_PIN) == LOW) {
    if ((uint32_t)(micros() - start) >= timeout_us) return false;
  }
  return true;
}

void i2c_init() {
  sda_release();
  scl_release();
  delayMicroseconds(I2C_DELAY);
}

void i2c_start() {
  // Ensure bus idle (SDA high while SCL high), then pull SDA low
  sda_release();
  scl_release();
  (void)scl_wait_high(I2C_CLOCK_STRETCH_TIMEOUT);
  delayMicroseconds(I2C_DELAY);
  sda_drive_low();
  delayMicroseconds(I2C_DELAY);
  scl_drive_low();
  delayMicroseconds(I2C_DELAY);
}

void i2c_stop() {
  // Pull SDA low, release SCL and wait for it to go high, then release SDA
  sda_drive_low();
  delayMicroseconds(I2C_DELAY);
  scl_release();
  (void)scl_wait_high(I2C_CLOCK_STRETCH_TIMEOUT);
  delayMicroseconds(I2C_DELAY);
  sda_release();
  delayMicroseconds(I2C_DELAY);
}

bool i2c_write_byte(uint8_t data) {
  for (int8_t i = 7; i >= 0; --i) {
    // Prepare next bit on SDA while SCL is low
    scl_drive_low();
    if (data & (1 << i)) {
      sda_release(); // logic 1
    } else {
      sda_drive_low(); // logic 0
    }
    delayMicroseconds(I2C_DELAY);

    // Clock the bit
    scl_release();
    if (!scl_wait_high(I2C_CLOCK_STRETCH_TIMEOUT)) return false;
    delayMicroseconds(I2C_DELAY);
  }

  // ACK clock: release SDA so slave can pull it low
  scl_drive_low();
  sda_release();
  delayMicroseconds(I2C_DELAY);
  scl_release();
  if (!scl_wait_high(I2C_CLOCK_STRETCH_TIMEOUT)) return false;
  bool ack = (digitalRead(SDA_PIN) == LOW);
  delayMicroseconds(I2C_DELAY);
  scl_drive_low();
  // SDA stays released (idle high) until next bit is set
  return ack;
}

uint8_t i2c_read_byte(bool send_ack) {
  uint8_t data = 0;
  sda_release(); // let slave drive data

  for (int8_t i = 7; i >= 0; --i) {
    scl_drive_low();
    delayMicroseconds(I2C_DELAY);
    scl_release();
    if (!scl_wait_high(I2C_CLOCK_STRETCH_TIMEOUT)) {
      // If timeout, leave remaining bits as 0 and proceed to ACK/NACK phase
      break;
    }
    if (digitalRead(SDA_PIN)) {
      data |= (1 << i);
    }
    delayMicroseconds(I2C_DELAY);
  }

  // Send ACK (LOW) if more bytes expected, else NACK (HIGH)
  scl_drive_low();
  if (send_ack) {
    sda_drive_low();
  } else {
    sda_release();
  }
  delayMicroseconds(I2C_DELAY);
  scl_release();
  (void)scl_wait_high(I2C_CLOCK_STRETCH_TIMEOUT);
  delayMicroseconds(I2C_DELAY);
  scl_drive_low();
  sda_release(); // release bus after ACK/NACK

  return data;
}

// High-level I2C functions
bool send_filename(const char* filename) {
  Serial.printf("Sending filename: %s\n", filename);
  
  i2c_start();
  if (!i2c_write_byte(ATTINY_ADDRESS << 1)) {
    i2c_stop();
    Serial.println("Failed to address ATtiny");
    return false;
  }
  
  if (!i2c_write_byte('F')) {
    i2c_stop();
    Serial.println("Failed to send filename command");
    return false;
  }
  
  for (int i = 0; filename[i] && i < 13; i++) {
    if (!i2c_write_byte(filename[i])) {
      i2c_stop();
      Serial.printf("Failed to send filename byte %d\n", i);
      return false;
    }
  }
  
  i2c_stop();
  return true;
}

bool send_command(char cmd) {
  i2c_start();
  if (!i2c_write_byte(ATTINY_ADDRESS << 1)) {
    i2c_stop();
    return false;
  }
  
  if (!i2c_write_byte(cmd)) {
    i2c_stop();
    return false;
  }
  
  i2c_stop();
  return true;
}

bool write_data(const char* filename, const uint8_t* data, size_t length) {
  Serial.printf("Writing %d bytes to file: %s\n", length, filename);
  
  // Send filename first
  if (!send_filename(filename)) {
    Serial.println("ERROR: Failed to send filename for write operation");
    return false;
  }
  
  delay(100); // Give ATtiny time to process filename
  
  // Send 'W' command and data bytes in same transaction
  i2c_start();
  if (!i2c_write_byte(ATTINY_ADDRESS << 1)) {
    Serial.println("ERROR: Failed to send address for write operation");
    i2c_stop();
    return false;
  }
  
  // Send 'W' command
  if (!i2c_write_byte('W')) {
    Serial.println("ERROR: Failed to send 'W' command");
    i2c_stop();
    return false;
  }
  
  // Send data bytes in same transaction
  uint32_t start_us = micros();
  for (size_t i = 0; i < length; i++) {
    if (!i2c_write_byte(data[i])) {
      Serial.printf("ERROR: Failed to write data byte %d\n", i);
      i2c_stop();
      return false;
    }
  }
  uint32_t elapsed_us = micros() - start_us;
  if (elapsed_us == 0U) elapsed_us = 1U;
  float kbps = ((float)length * 1000000.0f) / (float)elapsed_us / 1024.0f;
  Serial.printf("Transfer speed: %.2f KB/s (write)\n", kbps);
  
  i2c_stop();
  Serial.printf("Successfully wrote %d bytes\n", length);
  return true;
}

uint32_t get_file_size(const char* filename) {
  Serial.printf("Getting file size for: %s\n", filename);

  // Always send filename first (separate transaction)
  if (!send_filename(filename)) {
    Serial.println("Failed to send filename for size query");
    return 0;
  }

  delay(150); // Allow ATtiny to process filename

  // Send size command 'S' and then perform a REPEATED START to read 4 bytes (no STOP in between)
  Serial.println("Sending size command 'S' with repeated start for read");
  i2c_start();
  if (!i2c_write_byte(ATTINY_ADDRESS << 1)) {
    i2c_stop();
    Serial.println("Failed to address ATtiny for size command");
    return 0;
  }
  if (!i2c_write_byte('S')) {
    i2c_stop();
    Serial.println("Failed to send size command");
    return 0;
  }

  // Repeated START, switch to read
  i2c_start();
  if (!i2c_write_byte((ATTINY_ADDRESS << 1) | 1)) {
    i2c_stop();
    Serial.println("Failed to address ATtiny for size read");
    return 0;
  }

  uint32_t size = 0;
  uint8_t byte0 = i2c_read_byte(true);
  uint8_t byte1 = i2c_read_byte(true);
  uint8_t byte2 = i2c_read_byte(true);
  uint8_t byte3 = i2c_read_byte(false);

  size |= ((uint32_t)byte0) << 24;
  size |= ((uint32_t)byte1) << 16;
  size |= ((uint32_t)byte2) << 8;
  size |= (uint32_t)byte3;

  Serial.printf("Raw bytes: 0x%02X 0x%02X 0x%02X 0x%02X\n", byte0, byte1, byte2, byte3);
  Serial.printf("Calculated size: %lu bytes\n", size);

  i2c_stop();
  return size;
}

bool read_file_content(const char* filename, uint32_t size) {
  Serial.printf("Reading file content for: %s (%lu bytes)\n", filename, size);

  // Send filename (separate transaction)
  if (!send_filename(filename)) {
    Serial.println("Failed to send filename");
    return false;
  }

  delay(150); // Allow ATtiny to process filename

  // Send read command 'R' then perform REPEATED START to read content (no STOP in between)
  Serial.println("Sending read command 'R' with repeated start for streaming read");
  i2c_start();
  if (!i2c_write_byte(ATTINY_ADDRESS << 1)) {
    i2c_stop();
    Serial.println("Failed to address ATtiny for read command");
    return false;
  }
  if (!i2c_write_byte('R')) {
    i2c_stop();
    Serial.println("Failed to send read command");
    return false;
  }

  // Repeated START, switch to read
  i2c_start();
  if (!i2c_write_byte((ATTINY_ADDRESS << 1) | 1)) {
    i2c_stop();
    Serial.println("Failed to address ATtiny for reading");
    return false;
  }

  Serial.println("Content:");
  uint32_t start_us = micros();
  for (uint32_t i = 0; i < size; i++) {
    uint8_t byte = i2c_read_byte(i < size - 1);
    Serial.write(byte);
  }
  uint32_t elapsed_us = micros() - start_us;
  if (elapsed_us == 0U) elapsed_us = 1U;
  float kbps = ((float)size * 1000000.0f) / (float)elapsed_us / 1024.0f;
  Serial.printf("\nTransfer speed: %.2f KB/s (read)\n", kbps);
  Serial.printf("[END] total %lu bytes\n", size);

  i2c_stop();
  return true;
}

// Test functions
void test_write_operations() {
  Serial.println("--- Testing Write Operations ---");
  
  const char* filename = "TEST.TXT";
  const char* test_data = "Hello from ESP32! This is a test message.";
  
  // Write data (filename is sent within the function)
  if (write_data(filename, (const uint8_t*)test_data, strlen(test_data))) {
    Serial.println("Write operations completed successfully!");
  } else {
    Serial.println("Write operations failed!");
  }
  
  Serial.println("Waiting for file to be properly closed and flushed...");
  delay(3000); // Extended delay to ensure file is fully closed and SD card is ready
}

void test_read_operations() {
  Serial.println(F("\n--- Testing Read Operations ---"));
  
  const char* filename = "TEST.TXT";
  
  // First try to read the file directly to see if it exists
  Serial.println("Attempting direct file read to test if file exists...");

  // Send filename for read operation
  if (!send_filename(filename)) {
    Serial.println("Failed to send filename for read test");
    return;
  }

  delay(200);

  // Open file for reading and immediately perform a REPEATED START to read a few bytes
  Serial.println("Attempting to read first few bytes with repeated start...");
  i2c_start();
  if (!i2c_write_byte(ATTINY_ADDRESS << 1)) {
    i2c_stop();
    Serial.println("Failed to address ATtiny for direct read");
    return;
  }
  if (!i2c_write_byte('R')) {
    i2c_stop();
    Serial.println("Failed to send read command for file test");
    return;
  }

  // Repeated START to switch to read
  i2c_start();
  if (!i2c_write_byte((ATTINY_ADDRESS << 1) | 1)) {
    i2c_stop();
    Serial.println("Failed to address ATtiny for direct read phase");
    return;
  }

  // Read first 10 bytes
  Serial.print("Direct read result: ");
  for (int i = 0; i < 10; i++) {
    uint8_t byte = i2c_read_byte(i < 9);
    if (byte >= 32 && byte <= 126) {
      Serial.write(byte);
    } else {
      Serial.printf("[0x%02X]", byte);
    }
  }
  Serial.println();
  i2c_stop();

  delay(500);

  // Now try the normal size operation
  uint32_t size = get_file_size(filename);
  if (size == 0U) {
    Serial.println(F("File not found or empty via size command"));
    return;
  }

  Serial.print(F("File size: "));
  Serial.print(size);
  Serial.println(F(" bytes"));

  if (!read_file_content(filename, size)) {
    Serial.println(F("Failed to read file content"));
    return;
  }

  Serial.println(F("Read operations completed!"));
}

void test_stream_128_operations() {
  Serial.println(F("\n--- Testing 128-byte Stream ---"));

  const char* filename = "STREAM.BIN"; // 8.3 filename
  uint8_t data[128];
  const char pattern[] = "0123456789ABCDEF"; // 16-byte printable pattern

  // Fill buffer with a repeating, readable pattern
  for (uint16_t i = 0; i < 128; i++) {
    data[i] = (uint8_t)pattern[i & 0x0F];
  }

  Serial.printf("Writing %d bytes to file: %s\n", 128, filename);
  if (!write_data(filename, data, 128)) {
    Serial.println("Failed to write 128-byte stream");
    return;
  }

  Serial.println("128-byte write completed, waiting for flush...");
  delay(1000);

  // Verify size
  uint32_t size = get_file_size(filename);
  if (size != 128U) {
    Serial.printf("Size mismatch: expected 128, got %lu\n", size);
    return;
  }
  Serial.println("Size verified: 128 bytes");

  // Read back content
  if (!read_file_content(filename, size)) {
    Serial.println("Failed to read back 128-byte stream");
    return;
  }

  Serial.println("128-byte stream read back successfully!");
}

void test_append_operations() {
  Serial.println(F("\n--- Testing Append Operations ---"));

  const char* filename = "APPEND.TXT"; // 8.3 filename
  const char* part1 = "Hello from ESP32! This is a test message."; // 41 bytes
  uint32_t len1 = (uint32_t)strlen(part1);

  // Initial write (create/truncate)
  if (!write_data(filename, (const uint8_t*)part1, (size_t)len1)) {
    Serial.println("Initial write failed!");
    return;
  }
  Serial.println("Initial write done, waiting for flush...");
  delay(1000);

  // Second part to append
  const char* part2 = " And then we appended more data.";
  uint32_t len2 = (uint32_t)strlen(part2);

  if (!append_data(filename, (const uint8_t*)part2, (size_t)len2)) {
    Serial.println("Append failed!");
    return;
  }
  Serial.println("Append done, waiting for flush...");
  delay(1200);

  // Verify final size
  uint32_t size = get_file_size(filename);
  uint32_t expected = len1 + len2;
  Serial.printf("Appended file size: %lu bytes (expected %lu)\n", size, expected);
  if (size != expected) {
    Serial.println("Size mismatch after append!");
    return;
  }

  // Read back the entire file
  if (!read_file_content(filename, size)) {
    Serial.println("Failed to read appended file");
    return;
  }

  Serial.println("Append operations completed successfully!");
}

bool append_data(const char* filename, const uint8_t* data, size_t length) {
  Serial.printf("Appending %d bytes to file: %s\n", length, filename);

  // Send filename first
  if (!send_filename(filename)) {
    Serial.println("ERROR: Failed to send filename for append operation");
    return false;
  }

  delay(100); // Give ATtiny time to process filename

  // Send 'A' command and data bytes in same transaction
  i2c_start();
  if (!i2c_write_byte(ATTINY_ADDRESS << 1)) {
    Serial.println("ERROR: Failed to send address for append operation");
    i2c_stop();
    return false;
  }

  // Send 'A' command
  if (!i2c_write_byte('A')) {
    Serial.println("ERROR: Failed to send 'A' command");
    i2c_stop();
    return false;
  }

  // Send data bytes in same transaction
  uint32_t start_us = micros();
  for (size_t i = 0; i < length; i++) {
    if (!i2c_write_byte(data[i])) {
      Serial.printf("ERROR: Failed to append data byte %d\n", (int)i);
      i2c_stop();
      return false;
    }
  }
  uint32_t elapsed_us = micros() - start_us;
  if (elapsed_us == 0U) elapsed_us = 1U;
  float kbps = ((float)length * 1000000.0f) / (float)elapsed_us / 1024.0f;
  Serial.printf("Transfer speed: %.2f KB/s (append)\n", kbps);

  i2c_stop();
  Serial.printf("Successfully appended %d bytes\n", length);
  return true;
}

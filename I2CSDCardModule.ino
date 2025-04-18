/* I2C SC-Card Module - see http://www.technoblogy.com/show?1LJI

   David Johnson-Davies - www.technoblogy.com - 7th July 2022
   ATtiny1614 @ 20MHz (internal clock; BOD disabled)
   
   CC BY 4.0
   Licensed under a Creative Commons Attribution 4.0 International license: 
   http://creativecommons.org/licenses/by/4.0/
*/

/* --- syntax1269 Mods and additions - April 2025
Added function to retrieve if file exists
Corrected Arduino compile warning (control reaches end of non-void function) in boolean DataHostWrite ()
Added function to remove a file from the SD card.
Changed general int to stdint.h types saving a few bytes in memory and storage
Added software reset functioon so the ATTiny can be reset from another controler.


-----
Sketch uses 11934 bytes of program storage space.
Global variables use 821 bytes of dynamic memory
-----
*/
#include <avr/wdt.h>
#include <SD.h>
 
File myFile;

// LEDs **********************************************

const uint8_t LEDoff = 0;
const uint8_t LEDgreen = 1;
const uint8_t LEDred = 2;

void LightLED (uint8_t colour) {
  digitalWrite(5, colour & 1);
  digitalWrite(4, colour>>1 & 1);
}

// I2C Interface **********************************************

const uint8_t Namelength = 13;
char Filename[Namelength];
static union { uint32_t Filesize; uint8_t Filebytes[4]; };

const uint8_t MyAddress = 0x55;

// TWI setup **********************************************

void I2CSetup () {
  TWI0.CTRLA = 0;                                        // Default timings
  TWI0.SADDR = MyAddress<<1;                             // Bottom bit is R/W bit
  // Enable address, data, and stop interrupts:
  TWI0.SCTRLA = TWI_APIEN_bm | TWI_DIEN_bm | TWI_PIEN_bm | TWI_ENABLE_bm;
}

// Functions to handle each of the cases **********************************************

uint8_t cmd = 0;                                         // Currently active command
uint8_t ch = 0, ptr = 0;                                 // Filename and size pointers
boolean checknack = false;                               // Don't check Host NACK first time

boolean AddressHostRead () {
  return true;
}

boolean AddressHostWrite () {
  cmd = 0; ch = 0; ptr = 0;                          // Reset these on writing
  return true;
}
//
void DataHostRead () {
  if (cmd == 'R') {
    TWI0.SDATA = myFile.read();                          // Host read operation
  } else if (cmd == 'E') {                           
    TWI0.SDATA = SD.exists(Filename);                // Host query if file exists (Returns: 1 if the file or directory exists, 0 if not.)
  } else if (cmd == 'Z') {                           
    _PROTECTED_WRITE(RSTCTRL.SWRR, 1); // Trigger reset
    wdt_reset(); // Call the reset function incase software reeset doesn't work, watchdog timer rest will
  } else if (cmd == 'X') {                           
    TWI0.SDATA = SD.remove(Filename);                // Remove a file from the SD card. (Returns: 1 if the removal of the file succeeded, 0 if not.)
  } else if (cmd == 'S') { 
    if (ptr < 4) {
      if (ptr == 0) Filesize = myFile.size();
      TWI0.SDATA = Filebytes[3-ptr];                     // MSB first
      ptr++;
    } else TWI0.SDATA = 0;                               // Host read too many bytes
  } else TWI0.SDATA = 0;                                 // Read in other situations
}

boolean DataHostWrite () {
  if (cmd == 0) {                                    // No command in progress
    cmd = TWI0.SDATA;
    if (!myFile && (cmd != 'F')) {
      if (cmd == 'W') {
        myFile = SD.open(Filename, O_RDWR | O_CREAT | O_TRUNC);
      } else if (cmd == 'R' || cmd == 'S') {
        myFile = SD.open(Filename, O_READ); 
      } else if (cmd == 'A') {
        myFile = SD.open(Filename, O_RDWR | O_CREAT | O_APPEND);
      }
      if (myFile) {
        LightLED(LEDgreen);                              // File opened successfully
        return true;
      } else {
        LightLED(LEDred);                                // Problem
        return false;
      }
    } else {
      return true;
    }
  } else if (cmd == 'F') {                           // Read filename
    if (ch < Namelength) {
      Filename[ch++] = TWI0.SDATA;
      Filename[ch] = 0;
      return true;
    } else {                                             // Filename too long
      return false;
    }
  } else if (cmd == 'W' || cmd == 'A') {
    myFile.write(TWI0.SDATA);                            // Write byte to file
    return true;
  } else if (cmd == 'R' || cmd == 'S') {
    return false;
  }
  return false;
}

void Stop () {
  if (cmd == 'W' || cmd == 'R' || cmd == 'A' || cmd == 'S') {
    myFile.close(); LightLED(LEDoff);                    // Close file
  }
}

void SendResponse (boolean succeed) {
  if (succeed) {
    TWI0.SCTRLB = TWI_ACKACT_ACK_gc | TWI_SCMD_RESPONSE_gc;     // Send ACK
  } else {
    TWI0.SCTRLB = TWI_ACKACT_NACK_gc | TWI_SCMD_RESPONSE_gc;    // Send NACK
  }
}

// TWI interrupt service routine **********************************************

// TWI interrupt
ISR(TWI0_TWIS_vect) { 
  boolean succeed;

  // Address interrupt:
  if ((TWI0.SSTATUS & TWI_APIF_bm) && (TWI0.SSTATUS & TWI_AP_bm)) {
    if (TWI0.SSTATUS & TWI_DIR_bm) {                     // Host reading from client
      succeed = AddressHostRead();
    } else {
      succeed = AddressHostWrite();                      // Host writing to client
    }
    SendResponse(succeed);
    return;
  }
  
  // Data interrupt:
  if (TWI0.SSTATUS & TWI_DIF_bm) {
    if (TWI0.SSTATUS & TWI_DIR_bm) {                     // Host reading from client
      if ((TWI0.SSTATUS & TWI_RXACK_bm) && checknack) {
        checknack = false;
      } else {
        DataHostRead();
        checknack = true;
      } 
      TWI0.SCTRLB = TWI_SCMD_RESPONSE_gc;                // No ACK/NACK needed
    } else {                                             // Host writing to client
      succeed = DataHostWrite();
      SendResponse(succeed);
    }
    return;
  }

  // Stop interrupt:
  if ((TWI0.SSTATUS & TWI_APIF_bm) && (!(TWI0.SSTATUS & TWI_AP_bm))) {
    Stop();
    TWI0.SCTRLB = TWI_SCMD_COMPTRANS_gc;                 // Complete transaction
    return;
  }
}

// Setup **********************************************

void setup (void) {
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  SD.begin();
  I2CSetup();
}
 
void loop (void) {
}

// Function to reset the microcontroller
void reset() {
  wdt_enable(WDTO_15MS); // Enable Watchdog Timer with a reset timeout of 15ms
  while (1); // Wait for the watchdog timer to reset the device
}

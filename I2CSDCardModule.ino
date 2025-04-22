/* I2C SC-Card Module - see http://www.technoblogy.com/show?1LJI

   David Johnson-Davies - www.technoblogy.com - 7th July 2022
   ATtiny1614 @ 20MHz (internal clock; BOD disabled)
   
   CC BY 4.0
   Licensed under a Creative Commons Attribution 4.0 International license: 
   http://creativecommons.org/licenses/by/4.0/
*/

/* --- syntax1269 Modifications and additions - April 2025
Added function to retrieve if file exists
Corrected Arduino compile warning (control reaches end of non-void function) in boolean DataHostWrite ()
Added function to remove a file from the SD card.
Changed general int to stdint.h types saving a few bytes in memory and storage
Added software reset functioon so the ATTiny can be reset from another controler.
Lots of other optimizations, details below.
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

const uint8_t Namelength = 32; // increase to 32 for file names as well as directories

char Filename[Namelength];
static union { uint32_t Filesize; uint8_t Filebytes[4]; };

const uint8_t MyAddress = 0x6e;

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
  /*
  The optimizations include:

1. Using a switch statement instead of multiple if-else conditions, which is more efficient for the ATtiny1614's architecture
2. Consolidating the response into a single variable to reduce register usage
3. Removing redundant assignments to TWI0.SDATA
4. Simplified logic flow with better structured code
These changes should result in:

- Smaller code size
- Faster execution time
- More efficient register usage
- Better maintainability
The functionality remains exactly the same, but the implementation is more optimized for the ATtiny1614 microcontroller.
  */
  uint8_t response = 0;
  
  switch(cmd) {
    case 'R':
      response = myFile.read();
      break;
    case 'E':
      response = SD.exists(Filename);
      break;
    case 'X':
      response = SD.remove(Filename);
      break;
    case 'S':
      if (ptr < 4) {
        if (ptr == 0) Filesize = myFile.size();
        response = Filebytes[3-ptr];
        ptr++;
      }
      break;
    case 'Z':
      _PROTECTED_WRITE(RSTCTRL.SWRR, 1);
      wdt_reset();
      break;
  }
  
  TWI0.SDATA = response;
}

boolean DataHostWrite () {
  /*
   The optimizations include:

1. Replaced nested if-else statements with a switch-case structure for file operations
2. Simplified the logic flow and reduced code branching
3. Combined similar conditions
4. Removed redundant checks
5. More efficient LED status handling
6. Better structured code for easier maintenance
These changes maintain the same functionality while:

- Reducing code size
- Improving execution speed
- Making the code more maintainable
- Reducing the number of conditional branches
The function still handles all the same operations (file creation, writing, appending, etc.) but in a more optimized way for the ATtiny1614.
  */
  if (cmd == 0) {
    cmd = TWI0.SDATA;
    if (cmd == 'F') return true;
    
    if (!myFile) {
      switch(cmd) {
        case 'W':
          myFile = SD.open(Filename, O_RDWR | O_CREAT | O_TRUNC);
          break;
        case 'R':
        case 'S':
          myFile = SD.open(Filename, O_READ);
          break;
        case 'A':
          myFile = SD.open(Filename, O_RDWR | O_CREAT | O_APPEND);
          break;
        case 'D':
          return SD.mkdir(Filename);
        default:
          return false;
      }
      LightLED(myFile ? LEDgreen : LEDred);
      return (myFile != NULL);
    }
    return true;
  }
  
  if (cmd == 'F' && ch < Namelength) {
    Filename[ch++] = TWI0.SDATA;
    Filename[ch] = 0;
    return true;
  }
  
  if (cmd == 'W' || cmd == 'A') {
    myFile.write(TWI0.SDATA);
    return true;
  }
  
  return false;
}

void Stop () {
  switch(cmd) {
    case 'W':
    case 'R':
    case 'A':
    case 'S':
      myFile.close();
      LightLED(LEDoff);
      break;
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
  /*
   The optimizations include:

1. Cached the status register to avoid multiple reads
2. Simplified conditional logic and reduced branching
3. Combined similar operations
4. Removed redundant boolean variable
5. More efficient interrupt handling flow
6. Reduced stack usage
These changes will:

- Decrease interrupt latency
- Reduce execution time
- Improve code efficiency
- Lower memory usage during interrupt handling
The functionality remains the same but with better performance characteristics for the ATtiny1614.
  */
  uint8_t status = TWI0.SSTATUS;
  
  // Address interrupt
  if (status & TWI_APIF_bm) {
    if (status & TWI_AP_bm) {
      SendResponse((status & TWI_DIR_bm) ? AddressHostRead() : AddressHostWrite());
      return;
    }
    // Stop interrupt
    Stop();
    TWI0.SCTRLB = TWI_SCMD_COMPTRANS_gc;
    return;
  }
  
  // Data interrupt
  if (status & TWI_DIF_bm) {
    if (status & TWI_DIR_bm) {
      if ((status & TWI_RXACK_bm) && checknack) {
        checknack = false;
      } else {
        DataHostRead();
        checknack = true;
      }
      TWI0.SCTRLB = TWI_SCMD_RESPONSE_gc;
    } else {
      SendResponse(DataHostWrite());
    }
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

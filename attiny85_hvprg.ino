/* ATTINY85 HIGH VOLTAGE SERIAL PROGRAMMING PROTOCOL

   Note: tailored specifically for the Digispark development board.
   In particular, the initiation procedure is modified to account
   for the Digispark's slow Vcc rise time. see hvpProgramMode()
   
   REFERENCE
   ATtiny25/45/85 - Complete Datasheet
   ATtiny25/V / ATtiny45/V / ATtiny85/V (Rev. 2586Q–AVR–08/2013)
   https://www.microchip.com/en-us/product/attiny85#Documentation

   Digispark Schematic, Digistump 2013
   https://s3.amazonaws.com/digistump-resources/files/97a1bb28_DigisparkSchematic.pdf

   AUTHOR
   Demitrios V. (GitHub @aegean-odyssey)

   LICENSE
   digispark-hvp, copyright © 2025 by Aegean Associates Inc., is licensed under
   Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International. To
   view a copy of this license, visit
    https://creativecommons.org/licenses/by-nc-sa/4.0/ 
*/

/* PROGRAMMER PIN ASSIGNMENTS

   The "schematic" below shows the circuit and connections between the
   Uno R3 and the Digispark's two headers. Three resistors and one NPN
   transistor are the only additional components. A 12vdc (12.5vdc)
   AC power adapter supplies power to the Uno and provides the 12v
   "high voltage" needed to drive the reset pin of the ATtiny85 on the
   Digispark board. Note: this circuit is designed specifically to
   interface to the Digispark. Though it's customary to place resistors
   in series for the connections between the programmer (Uno R3) and the
   target device (Digispark), existing circuitry on pins PB1 and PB3 of
   the Digispark may interfere with such a practice.
//
// UnoR3                      Digispark
// 
// D8  ---------------------- Vcc ( -> VCC, 5vdc)
// 
// D9  ---------------------- PB0 ( -> SDI)
// 
// D10 ---------------------- PB1 ( -> SII)
//      1.0K ohm
// D11 --/\/\/--------------- PB2 (<-> SDO)
// 
// D12 ---------------------- PB3 ( -> SCL)
//      1.0K ohm
// VIN --/\/\/-----------/--- PB5 ( -> RST, 12vdc)
//      4.7K ohm       c/
// D13 --/\/\/----b(-|<) 2N3904
//                     e\
// GND ------------------\--- GND
//
*/

#define VCC  8
#define SDI  9
#define SII  10
#define SDO  11
#define SCL  12
#define RST  13

static uint8_t _digitalRead(uint8_t pin)
{
    return (digitalRead(pin) != LOW) ? 1 : 0;
}

static void hvp_wait_on_SDO(void)
{
    // 120ms timeout
    uint32_t tm = millis() + 120;
    
    while (millis() < tm)
        if (_digitalRead(SDO))
            break;
}

static uint8_t hvp_clock_pulse(void)
{
    delayMicroseconds(2); 
    digitalWrite(SCL, HIGH);
    delayMicroseconds(2);
    // read bit from target
    uint8_t i = _digitalRead(SDO);
    digitalWrite(SCL, LOW);
    return i;
}

static uint8_t hvp_write(uint8_t sdi, uint8_t sii)
{
    hvp_wait_on_SDO();

    uint8_t i = 0;
    digitalWrite(SDI, LOW);
    digitalWrite(SII, LOW);
    for (uint8_t j = 0x80; j; j >>= 1) {
        i = i + i + hvp_clock_pulse();
        digitalWrite(SDI, (sdi & j) ? HIGH : LOW);
        digitalWrite(SII, (sii & j) ? HIGH : LOW);
    }
    hvp_clock_pulse();
    digitalWrite(SDI, LOW);
    digitalWrite(SII, LOW);
    hvp_clock_pulse();
    hvp_clock_pulse();
    return i;
}

void hvpStandbyMode(void)
{
    // leave programming mode
    pinMode(RST, OUTPUT);
    digitalWrite(RST, HIGH);
    pinMode(VCC, OUTPUT);
    pinMode(SCL, OUTPUT);
    digitalWrite(VCC, LOW);
    digitalWrite(SCL, LOW);
    // "tri-state" these
    pinMode(SDO, INPUT);
    pinMode(SDI, INPUT);
    pinMode(SII, INPUT);
}

void hvpProgramMode(void)
{
    // enter programming mode
    pinMode(VCC, OUTPUT);
    pinMode(RST, OUTPUT);
    pinMode(SCL, OUTPUT);
    digitalWrite(RST, HIGH);
    digitalWrite(VCC, LOW);
    digitalWrite(SCL, LOW);
    pinMode(SDI, OUTPUT);
    pinMode(SII, OUTPUT);
    pinMode(SDO, OUTPUT);
    digitalWrite(SDI, LOW);
    digitalWrite(SII, LOW);
    digitalWrite(SDO, LOW);
    delayMicroseconds(10);
    digitalWrite(VCC, HIGH);
    //*!* digispark's Vcc rise time is ~900us,
    //*!* delay to give Vcc time to reach ~2v
    delayMicroseconds(80);  //*!*
    digitalWrite(RST, LOW);
    delayMicroseconds(10);
    pinMode(SDO, INPUT);
    // wait to ensure Vcc in stable
    delayMicroseconds(1000);
}

void hvpChipErase(void)
{
    hvp_write(0x80, 0x4c);
    hvp_write(0x00, 0x64);
    hvp_write(0x00, 0x6c);
    delay(400);  //*!*
    hvp_wait_on_SDO();
}

//*!* attiny25/45/85 specific masks for fuse bytes
#define FUSEL_MASK  0xff
#define FUSEH_MASK  0xff
#define FUSEE_MASK  0x01

void hvpWrFuseBitsLO(uint8_t bits)
{
    hvp_write(0x40, 0x4c);
    hvp_write(bits | ~FUSEL_MASK, 0x2c);  //*!*
    hvp_write(0x00, 0x64);
    hvp_write(0x00, 0x6c);
    hvp_wait_on_SDO();
}

void hvpWrFuseBitsHI(uint8_t bits)
{
    hvp_write(0x40, 0x4c);
    hvp_write(bits | ~FUSEH_MASK, 0x2c);  //*!*
    hvp_write(0x00, 0x74);
    hvp_write(0x00, 0x7c);
    hvp_wait_on_SDO();
}

void hvpWrFuseBitsEX(uint8_t bits)
{
    hvp_write(0x40, 0x4c);
    hvp_write(bits | ~FUSEE_MASK, 0x2c);  //*!*
    hvp_write(0x00, 0x66);
    hvp_write(0x00, 0x6e);
    hvp_wait_on_SDO();
}

uint8_t hvpRdFuseBitsLO(void)
{
    hvp_write(0x04, 0x4c);
    hvp_write(0x00, 0x68);
    return hvp_write(0x00, 0x6c);
}

uint8_t hvpRdFuseBitsHI(void)
{
    hvp_write(0x04, 0x4c);
    hvp_write(0x00, 0x7a);
    return hvp_write(0x00, 0x7e);
}

uint8_t hvpRdFuseBitsEX(void)
{
    hvp_write(0x04, 0x4c);
    hvp_write(0x00, 0x6a);
    return hvp_write(0x00, 0x6e);
}

//*!* attiny25/45/85 specific mask for lock bits
#define LOCKB_MASK  0x03

void hvpWrLockBits(uint8_t bits)
{
    hvp_write(0x20, 0x4c);
    hvp_write(bits | ~LOCKB_MASK, 0x2c);  //*!*
    hvp_write(0x00, 0x64);
    hvp_write(0x00, 0x6c);
    hvp_wait_on_SDO();
}

uint8_t hvpRdLockBits(void)
{
    hvp_write(0x04, 0x4c);
    hvp_write(0x00, 0x78);
    return hvp_write(0x00, 0x7c);
}

uint8_t hvpSignatureByte0(void)
{
    hvp_write(0x08, 0x4c);
    hvp_write(0x00, 0x0c);
    hvp_write(0x00, 0x68);
    return hvp_write(0x00, 0x6c);
}

uint8_t hvpSignatureByte1(void)
{
    hvp_write(0x08, 0x4c);
    hvp_write(0x01, 0x0c);
    hvp_write(0x00, 0x68);
    return hvp_write(0x00, 0x6c);
}

uint8_t hvpSignatureByte2(void)
{
    hvp_write(0x08, 0x4c);
    hvp_write(0x02, 0x0c);
    hvp_write(0x00, 0x68);
    return hvp_write(0x00, 0x6c);
}

static int hvp_eop(uint8_t b, uint8_t m)
{   // end of page test
    return (b & m) == m;
}

//*!* hardcoded flash page size (attiny45/85)
//*!* could/should be derived from STK500 pagesize
#define FLASH_PAGE_MASK  0x1f

void hvpWrFlash(uint16_t address, uint16_t words, uint8_t * dst)
{
    hvp_write(0x10, 0x4c);
    while (words) {
        uint8_t a = (uint8_t) ((uint16_t) address >> 8);
        uint8_t b = (uint8_t) address;
        hvp_write(b, 0x0c);
        hvp_write(*dst++, 0x2c);
        hvp_write(*dst++, 0x3c);
        hvp_write(0, 0x7d);
        hvp_write(0, 0x7c);
        address++;
        words--;
        if (hvp_eop(b, FLASH_PAGE_MASK) || ! words) {
            hvp_write(a, 0x1c);
            hvp_write(0, 0x64);
            hvp_write(0, 0x6c);
            hvp_wait_on_SDO();
        }
    }
    hvp_write(0x00, 0x4c);
}

void hvpRdFlash(uint16_t address, uint16_t words, uint8_t * dst)
{
    hvp_write(0x02, 0x4c);
    while (words) {
        uint8_t a = (uint8_t) ((uint16_t) address >> 8);
        uint8_t b = (uint8_t) address;
        hvp_write(b, 0x0c);
        hvp_write(a, 0x1c);
        hvp_write(0, 0x68);
        *dst++ = hvp_write(0, 0x6c);
        hvp_write(0, 0x78);
        *dst++ = hvp_write(0, 0x7c);
        address++;
        words--;
    }
    hvp_write(0x00, 0x4c);
}

//*!* hardcoded eeprom page size (attiny25/45/85)
//*!* could/should be derived from STK500 eeprompagesize
#define EEPROM_PAGE_MASK  0x03

void hvpWrEEPROM(uint16_t address, uint16_t bytes, uint8_t * dst)
{
    hvp_write(0x11, 0x4c);
    while (bytes) {
        uint8_t a = (uint8_t) ((uint16_t) address >> 8);
        uint8_t b = (uint8_t) address;
        hvp_write(b, 0x0c);
        hvp_write(a, 0x1c);
        hvp_write(*dst++, 0x2c);
        hvp_write(0, 0x6d);
        hvp_write(0, 0x6c);
        address++;
        bytes--;
        if (hvp_eop(b, EEPROM_PAGE_MASK) || ! bytes) {
            hvp_write(0, 0x64);
            hvp_write(0, 0x6c);
            hvp_wait_on_SDO();
        }
    }
    hvp_write(0x00, 0x4c);
}

void hvpRdEEPROM(uint16_t address, uint16_t bytes, uint8_t * dst)
{
    hvp_write(0x03, 0x4c);
    while (bytes) {
        uint8_t a = (uint8_t) ((uint16_t) address >> 8);
        uint8_t b = (uint8_t) address;
        hvp_write(b, 0x0c);
        hvp_write(a, 0x1c);
        hvp_write(0, 0x68);
        *dst++ = hvp_write(0, 0x6c);
        address++;
        bytes--;
    }
    hvp_write(0x00, 0x4c);
}

void hvpStoreEEPROM(uint16_t address, uint8_t v)
{
    uint8_t a = (uint8_t) ((uint16_t) address >> 8);
    uint8_t b = (uint8_t) address;
    hvp_write(0x11, 0x4c);
    hvp_write(b, 0x0c);
    hvp_write(a, 0x1c);
    hvp_write(v, 0x2c);
    hvp_write(0, 0x6d);
    hvp_write(0, 0x64);
    hvp_write(0, 0x6c);
    hvp_wait_on_SDO();
    hvp_write(0x00, 0x4c);
}

uint8_t hvpFetchEEPROM(uint16_t address)
{
    uint8_t a = (uint8_t) ((uint16_t) address >> 8);
    uint8_t b = (uint8_t) address;
    hvp_write(3, 0x4c);
    hvp_write(b, 0x0c);
    hvp_write(a, 0x1c);
    hvp_write(0, 0x68);
    b = hvp_write(0, 0x6c);
    hvp_write(0, 0x4c);
    return b;
}

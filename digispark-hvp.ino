/* DIGISPARK HIGH VOLTAGE PROGRAMMER

   Rewritten "Arduino as ISP" sketch that can program the ATtiny85, in situ,
   on the Digispark board via its 6- and 3- pin connectors. It uses an Arduino
   Uno R3 as the programmer, and implements the ATtiny85's high-voltage serial
   programming protocol. As such, it can program devices that have disabled
   (re-assigned) the RESET pin (e.g. the Digispark board).

   This programmer includes a command to interactively erase the ATtiny85
   and program its fuses and lock bits. The command is not very sophisticated,
   but it works fairly well with the Arduino IDE's serial monitor. Entering
   'Z' in the monitor initiates this interactive programming mode.
   
   Since this program more or less emulates a STK500_v1 serial programmer,
   it can be used with the "AVR ISP" programmer as well as the "Arduino
   as ISP" programmer. Seems the main difference is that avrdude configures
   different default baud rates for the programmers (19200 for "Arduino as
   ISP" and 115200 for "AVR ISP").

   AUTHOR
   Demitrios V. (GitHub @aegean-odyssey)

   LICENSE
   digispark-hvp, copyright © 2025 by Aegean Associates Inc., is licensed under
   Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International. To
   view a copy of this license, visit
    https://creativecommons.org/licenses/by-nc-sa/4.0/ 
*/

// CONFIGURE THE SERIAL PORT TO USE
// Prefer the USB virtual serial port (aka. native USB port), if the
// Arduino has one, since it does not autoreset (except for the magic
// baud rate of 1200), and it is more reliable due to USB handshaking.
// Leonardo and similar have an USB virtual serial port: 'Serial'.
// Due and Zero have an USB virtual serial port, 'SerialUSB'. On the
// Due and Zero, 'Serial' can be used too, provided you disable the
// autoreset. To use 'Serial': #define SERIAL Serial
//
#undef  SERIAL
#ifdef  SERIAL_PORT_USBVIRTUAL
#define SERIAL SERIAL_PORT_USBVIRTUAL
#else
#define SERIAL Serial
#endif

// select a baud rate (19200, 57600, 115200, ...)
// 19200 seems to be the avrdude default for Arduino as ISP
// 115200 seems to be the avrdude default for AVR ISP
#define BAUDRATE  115200

int ch_available(void)
{
    return SERIAL.available();
}

int getch(void)
{
    // 900ms timeout
    uint32_t tm = millis() + 900;

    while (millis() < tm)
        if (SERIAL.available())
            return SERIAL.read();
    return -1;
}
 
int getbn(uint8_t * p, unsigned int n)
{
    int c = 0;
    while (n--) {
        if ((c = getch()) < 0)
            break;
        *p++ = (uint8_t) c;
    }
    return c;
}

void putch(uint8_t c)
{
    SERIAL.print((char) c);
}

void putstr(const char * s)
{
    SERIAL.print(s);
}

void putbn(uint8_t * p, unsigned int n)
{
    while (n--) putch(*p++);
}

/* PROGRAMMER, STK500_v1 PROTOCOL

   REFERENCE
   AVR061: STK500 Communication Protocol
   Application Note: AN2525 (Rev. 2525B–AVR–04/03)
   doc2525.pdf, avr061.zip (Devices.h, command.h)
   https://www.microchip.com/en-us/application-notes/an2525
 */

#define HARDWARE_VERSION  2
#define SW_MAJOR_VERSION  1
#define SW_MINOR_VERSION  20

#define STK_OK        0x10
#define STK_FAILED    0x11
#define STK_UNKNOWN   0x12
#define STK_NODEVICE  0x13
#define STK_INSYNC    0x14
#define STK_NOSYNC    0x15
#define CRC_EOP       0x20

struct __attribute__ ((__packed__)) {
    uint8_t  devicecode;
    uint8_t  revision;
    uint8_t  progtype;
    uint8_t  parmode;
    uint8_t  polling;
    uint8_t  selftimed;
    uint8_t  lockbytes;
    uint8_t  fusebytes;
    uint8_t  flashpoll1;
    uint8_t  flashpoll2;
    // 16 bits (big endian)
    uint16_t eeprompoll;
    uint16_t pagesize;
    uint16_t eepromsize;
    // 32 bits (big endian)
    uint32_t flashsize;
} device;

struct __attribute__ ((__packed__)) {
    uint8_t  commandsize;
    uint8_t  eeprompagesize;
    uint8_t  signalpagel;
    uint8_t  signalbs2;
    uint8_t  ResetDisable;
} extras;

unsigned int address;
uint8_t buffer[0x102];

uint8_t set_programming_mode(uint8_t mode)
{
    static uint8_t _mode = 0;

    if (mode < 0x80) {
        if (! mode)
            hvpStandbyMode();
        else if (! _mode)
            hvpProgramMode();
        _mode = mode;
    }
    return _mode;
}

void setup(void)
{
    SERIAL.begin(BAUDRATE);
    set_programming_mode(0);
    device.devicecode = 0;
    extras.commandsize = 0;
}

static uint8_t parameter(uint8_t id)
{
    if (id == 0x80) return HARDWARE_VERSION;
    if (id == 0x81) return SW_MAJOR_VERSION;
    if (id == 0x82) return SW_MINOR_VERSION;
    if (id == 0x93) return 'S'; // serial printer
    return 0;
}

static uint8_t universal(uint8_t * req)
{
    switch ((((uint16_t) req[0]) << 8) | req[1]) {
    case 0x5800:
        return hvpRdLockBits();
    case 0x5000:
        return hvpRdFuseBitsLO();
    case 0x5808:
        return hvpRdFuseBitsHI();
    case 0x5008:
        return hvpRdFuseBitsEX();
    case 0xace0:
        hvpWrLockBits(req[3]);
        break;
    case 0xaca0:
        hvpWrFuseBitsLO(req[3]);
        break;
    case 0xaca8:
        hvpWrFuseBitsHI(req[3]);
        break;
    case 0xaca4:
        hvpWrFuseBitsEX(req[3]);
        break;
    case 0x3000:
        if (req[2] == 0) return hvpSignatureByte0();
        if (req[2] == 1) return hvpSignatureByte1();
        if (req[2] == 2) return hvpSignatureByte2();
        break;
    case 0xac80:
        hvpChipErase();
        break;
    }
    return 0;
}

static uint8_t not_E_or_F(void)
{
    uint8_t b = buffer[0];
    return ! (b == 'E' || b == 'F');
}

static uint8_t not_set_up(void)
{
    return ! set_programming_mode(0xff);
}

void loop(void)
{
    int c, v;
    
    if (ch_available()) {
        c = getch();
        switch (c) {
        case 'Z':
            /* custom (non-spec) interactive fuse setting command */
            custom_fuse_cmd();
            set_programming_mode(0);
            break;

        case 0x52:
            /* Chip Erase (STK_CHIP_ERASE) */
            if ((c = getch()) < 0) goto __abort;
            if (c != CRC_EOP) goto __no_sync_reply;
            if (not_set_up()) goto __failure_reply;
            putch(STK_INSYNC);
            hvpChipErase();
            putch(STK_OK);
            break;
  
        case 0x75:
            /* Read Signature Bytes (STK_READ_SIGN) */
            if ((c = getch()) < 0) goto __abort;
            if (c != CRC_EOP) goto __no_sync_reply;
            if (not_set_up()) goto __failure_reply;
            putch(STK_INSYNC);
            putch(hvpSignatureByte0());
            putch(hvpSignatureByte1());
            putch(hvpSignatureByte2());
            putch(STK_OK);
            break;

#define GETN(n)  getbn(buffer, n)

        case 0x62:
            /* Program Fuse Bits (STK_PROG_FUSE) */
            if ((c = GETN(3)) < 0) goto __abort;
            if (c != CRC_EOP) goto __no_sync_reply;
            if (not_set_up()) goto __failure_reply;
            putch(STK_INSYNC);
            hvpWrFuseBitsLO(buffer[0]);
            hvpWrFuseBitsHI(buffer[1]);
            putch(STK_OK);
            break;

        case 0x65:
            /* Program Fuse Bits Extended (STK_PROG_FUSE_EXT) */
            if ((c = GETN(4)) < 0) goto __abort;
            if (c != CRC_EOP) goto __no_sync_reply;
            if (not_set_up()) goto __failure_reply;
            putch(STK_INSYNC);
            hvpWrFuseBitsLO(buffer[0]);
            hvpWrFuseBitsHI(buffer[1]);
            hvpWrFuseBitsEX(buffer[2]);
            putch(STK_OK);
            break;

        case 0x72:
            /* Read Fuse Bits (STK_READ_FUSE) */
            if ((c = getch()) < 0) goto __abort;
            if (c != CRC_EOP) goto __no_sync_reply;
            if (not_set_up()) goto __failure_reply;
            putch(STK_INSYNC);
            putch(hvpRdFuseBitsLO());
            putch(hvpRdFuseBitsHI());
            putch(STK_OK);
            break;

        case 0x77:
            /* Read Fuse Bits Extended (STK_READ_FUSE_EXT) */
            if ((c = getch()) < 0) goto __abort;
            if (c != CRC_EOP) goto __no_sync_reply;
            if (not_set_up()) goto __failure_reply;
            putch(STK_INSYNC);
            putch(hvpRdFuseBitsLO());
            putch(hvpRdFuseBitsHI());
            putch(hvpRdFuseBitsEX());
            putch(STK_OK);
            break;

        case 0x73:
            /* Read Lock Bits (STK_READ_LOCK) */
            if ((c = getch()) < 0) goto __abort;
            if (c != CRC_EOP) goto __no_sync_reply;
            if (not_set_up()) goto __failure_reply;
            putch(STK_INSYNC);
            putch(hvpRdLockBits());
            putch(STK_OK);
            break;

        case 0x63:
            /* Program Lock Bits (STK_PROG_LOCK) */
            if ((c = GETN(2)) < 0) goto __abort;
            if (c != CRC_EOP) goto __no_sync_reply;
            if (not_set_up()) goto __failure_reply;
            putch(STK_INSYNC);
            hvpWrLockBits(buffer[0]);
            putch(STK_OK);
            break;

        case 0x55:
            /* Load Address (STK_LOAD_ADDRESS) */
            if ((c = getch()) < 0) goto __abort;
            v = c;
            if ((c = getch()) < 0) goto __abort;
            v += 256 * c;
            address = v;
            goto __empty_reply;

        case 0x56:
            /* Universal Command (STK_UNIVERSAL) */
            if ((c = GETN(5)) < 0) goto __abort;
            if (c != CRC_EOP) goto __no_sync_reply;
            if (not_set_up()) goto __failure_reply;
            putch(STK_INSYNC);
            putch(universal(buffer));
            putch(STK_OK);
            break;
 
        case 0x60:
            /* Program Flash Memory (STK_PROG_FLASH) */
            if ((c = GETN(3)) < 0) goto __abort;
            if (c != CRC_EOP) goto __no_sync_reply;
            // unsupported (only page writes for flash)
            putch(STK_UNKNOWN);
            break;

        case 0x61:
            /* Program Data Memory (STK_PROG_DATA) */
            if ((c = GETN(2)) < 0) goto __abort;
            if (c != CRC_EOP) goto __no_sync_reply;
            if (not_set_up()) goto __failure_reply;
            putch(STK_INSYNC);
            hvpWrEEPROM(address, 1, buffer);
            putch(STK_OK);
            address++;
            break;
  
        case 0x70:
            /* Read Flash Memory (STK_READ_FLASH) */
            if ((c = getch()) < 0) goto __abort;
            if (c != CRC_EOP) goto __no_sync_reply;
            if (not_set_up()) goto __failure_reply;
            putch(STK_INSYNC);
            hvpRdFlash(address, 1, buffer);
            putch(buffer[0]);
            putch(buffer[1]);
            putch(STK_OK);
            address++;
            break;

        case 0x71:
            /* Read Data Memory (STK_READ_DATA) */
            if ((c = getch()) < 0) goto __abort;
            if (c != CRC_EOP) goto __no_sync_reply;
            if (not_set_up()) goto __failure_reply;
            putch(STK_INSYNC);
            hvpRdFlash(address, 1, buffer);
            putch(buffer[0]);
            putch(STK_OK);
            address++;
            break;

        case 0x64:
            /* Program Page (STK_PROG_PAGE) */

            if ((c = getch()) < 0) goto __abort;
            v = 256 * c;
            if ((c = getch()) < 0) goto __abort;
            v += c;

            // length v should never be larger than 256 bytes.
            // if it is, treat it as an out-of-sync condition.
            if (v > 256) goto __no_sync_reply;

            if ((c = GETN(v+2)) < 0) goto __abort;
            if (c != CRC_EOP) goto __no_sync_reply;
            if (not_set_up()) goto __failure_reply;
            if (not_E_or_F()) goto __failure_reply;

            putch(STK_INSYNC);
            if (buffer[0] == 'F') {
                v = (v+1)/2;
                hvpWrFlash(address, v, buffer+1);
            } else {
                hvpWrEEPROM(address, v, buffer+1);
            }
            putch(STK_OK);
            address += v;
            break;

        case 0x74:
            /* Read Page (STK_READ_PAGE) */

            if ((c = getch()) < 0) goto __abort;
            v = 256 * c;
            if ((c = getch()) < 0) goto __abort;
            v += c;

            // length v should never be larger than 256 bytes.
            // if it is, treat it as an out-of-sync condition.
            if (v > 256) goto __no_sync_reply;

            if ((c = GETN(2)) < 0) goto __abort;
            if (c != CRC_EOP) goto __no_sync_reply;
            if (not_set_up()) goto __failure_reply;
            if (not_E_or_F()) goto __failure_reply;

            putch(STK_INSYNC);
            if (buffer[0] == 'F') {
                v = (v+1)/2;
                hvpRdFlash(address, v, buffer);
                putbn(buffer, v*2);
            } else {
                hvpRdEEPROM(address, v, buffer);
                putbn(buffer, v);
            }
            putch(STK_OK);
            address += v;
            break;

        case 0x41:
            /* Get Parameter Value (STK_GET_PARAMETER) */
            if ((c = GETN(2)) < 0) goto __abort;
            if (c != CRC_EOP) goto __no_sync_reply;
            putch(STK_INSYNC);
            putch(parameter(buffer[0]));
            putch(STK_OK);
            break;

        case 0x40:
            /* Set Parameter Value (STK_SET_PARAMETER) */
            if ((c = GETN(2)) < 0) goto __abort;
            // unimplemented
            goto __empty_reply;

#define GET_STRUCTURE(name)  getbn((uint8_t *) &name, sizeof(name))

        case 0x42:
            /* Set Device Programming Parameters (STK_SET_DEVICE) */ 
            if ((c = GET_STRUCTURE(device)) < 0) {
                device.devicecode = 0;  // mark structure as unset
                goto __abort;
            }
            goto __empty_reply;

        case 0x45:
            /* Set Extended Device Programming Parameters (SET_DEVICE_EXT) */
            if ((c = GET_STRUCTURE(extras)) < 0) {
                extras.commandsize = 0;  // mark structure as unset
                goto __abort;
            }
            goto __empty_reply;

        case 0x50:
            /* Enter Programming Mode (STK_ENTER_PROGMODE) */
            if ((c = getch()) < 0) goto __abort;
            if (c != CRC_EOP) goto __no_sync_reply;
            if (! device.devicecode) {
                putch(STK_INSYNC);
                putch(STK_NODEVICE);
            } else {
                putch(STK_INSYNC);
                set_programming_mode(1);
                putch(STK_OK);
            }
            break;
            
        case 0x51:
            /* Leave Programming Mode (STK_LEAVE_PROGMODE) */
            set_programming_mode(0);
            // fall through...

        case 0x53:
            /* Check for Address Autoincrement (STK_CHECK_AUTOINC) */
            // fall through...

        case 0x30:
        __empty_reply:
            /* Get Synchronization (STK_GET_SYNC) */
            if ((c = getch()) < 0) goto __abort;
            if (c != CRC_EOP) goto __no_sync_reply;
            putch(STK_INSYNC);
            putch(STK_OK);
            break;

        case 0x31:
            /* Check if Starterkit Present (STK_GET_SIGN_ON) */
            if ((c = getch()) < 0) goto __abort;
            if (c != CRC_EOP) goto __no_sync_reply;
            putch(STK_INSYNC);
            putstr("AVR ISP");
            putch(STK_OK);
            break;

        __failure_reply:
            // not in program mode (mostly)
            putch(STK_INSYNC);
            putch(STK_FAILED);
            break;

        __abort:
            // unexpected serial timeout
            set_programming_mode(0);
            // fall through...

        case CRC_EOP:
        __no_sync_reply:
            /* Unexpected SYNC token */
            putch(STK_NOSYNC);
            break;

        default:
            /* Unknown, Unsupported Command */
            if ((c = getch()) < 0) goto __abort;
            if (c != CRC_EOP) goto __no_sync_reply;
            putch(STK_UNKNOWN);
        }
    }
}

/* INTERACTIVE COMMANDS
   read device id, chip erase, read/write fuse bytes and lock bits
   
   AUTHOR
   Demitrios V. (GitHub @aegean-odyssey)

   LICENSE
   digispark-hvp, copyright © 2025 by Aegean Associates Inc., is licensed under
   Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International. To
   view a copy of this license, visit
    https://creativecommons.org/licenses/by-nc-sa/4.0/ 
*/

extern "C" {
#include "fuse_presets.h"
}

static uint8_t cfc_getch(void)
{
    for(;;) {
        while (! SERIAL.available());
        uint8_t c = SERIAL.read();
        if (c < 0x20) continue;
        if (c > 0x7f) continue;
        return c;
    }
}

static uint32_t cfc_getlx(void)
{
    uint8_t c, n;
    uint32_t v = 0;

    n = 8;
    while(n) {
        c = cfc_getch();
        if (c == ' ') continue;
        if (c >= '0' && c <= '9')
            c = c - '0';
        if (c >= 'a' && c <= 'f')
            c = c - 'a' + 10;
        if (! (c < 16)) return 0;
        v = 16 * v + c;
        n--;
    }
    return v;
}

static void cfc_puts(const __FlashStringHelper * s)
{
    SERIAL.print(s);
}

static void cfc_putln(const __FlashStringHelper * s)
{
    SERIAL.println(s);
}

static void cfc_putx(const __FlashStringHelper * s, uint8_t b)
{
    SERIAL.print(s);
    if (b < 0x10) SERIAL.print((char) '0');
    SERIAL.print(b, HEX);
}

static void cfc_putd(uint8_t n)
{
    SERIAL.print(n, DEC);
}

struct {
    uint8_t flags;
    uint8_t devid0;
    uint8_t devid1;
    uint8_t devid2;
    cfc_t o; // old
    cfc_t n; // new
} cfc_state;

static void cfc_program_mode(void)
{
    if (! (cfc_state.flags & 0x40)) {
        cfc_state.flags |= 0x40;
        hvpProgramMode();
    }
}

static void cfc_menu(void)
{
    cfc_puts(F("\n  DEVICE "));
    cfc_putx(F(" "), cfc_state.devid0);
    cfc_putx(F(" "), cfc_state.devid1);
    cfc_putx(F(" "), cfc_state.devid2);
    cfc_putln(F(""));
    cfc_puts(F("  Fuse"));
    cfc_putx(F(" lo:"), cfc_state.o.fuselo);
    cfc_putx(F(" hi:"), cfc_state.o.fusehi);
    cfc_putx(F(" ex:"), cfc_state.o.fuseex);
    cfc_putx(F("  Lock:"), cfc_state.o.lockxx);
    cfc_putln(F(""));
    cfc_puts(F("  ----"));
    cfc_putx(F(" -- "), cfc_state.n.fuselo);
    cfc_putx(F(" -- "), cfc_state.n.fusehi);
    cfc_putx(F(" -- "), cfc_state.n.fuseex);
    cfc_putx(F("  ---- "), cfc_state.n.lockxx);
    cfc_putln(F(""));
    cfc_puts(F("(p)reset (i)nput (r)ead (w)rite (e)rase (q)uit: "));
}    
                    
void custom_fuse_cmd(void)
{
    cfc_state.flags = 0;
    while(! (cfc_state.flags & 0x80)) {

        cfc_menu();
        switch(cfc_getch()) {
        case 'r':
            cfc_puts(F("read\nReload device info, fuse and lock bits? (y|n): "));
            if (cfc_getch() != 'y') goto __cancel;
            cfc_program_mode();
            cfc_state.devid0 = hvpSignatureByte0();
            cfc_state.devid1 = hvpSignatureByte1();
            cfc_state.devid2 = hvpSignatureByte2();
            cfc_state.o.fuselo = hvpRdFuseBitsLO();
            cfc_state.o.fusehi = hvpRdFuseBitsHI();
            cfc_state.o.fuseex = hvpRdFuseBitsEX();
            cfc_state.o.lockxx = hvpRdLockBits();
            cfc_state.flags |= 0x10;
            cfc_putln(F("completed"));
            break;

        __cancel:
            cfc_putln(F("canceled"));
            break;

        case 'w':
            cfc_puts(F("write\nProgram modified fuse and lock bits? (y|n): "));
            if (cfc_getch() != 'y') goto __cancel;
            if ((cfc_state.flags & 0x18) &&
                (cfc_state.o.fuselo != cfc_state.n.fuselo)) {
                hvpWrFuseBitsLO(cfc_state.n.fuselo);
                cfc_state.o.fuselo = hvpRdFuseBitsLO();
                cfc_state.flags &= ~8;
            }
            if ((cfc_state.flags & 0x14) &&
                (cfc_state.o.fusehi != cfc_state.n.fusehi)) {
                hvpWrFuseBitsHI(cfc_state.n.fusehi);
                cfc_state.o.fusehi = hvpRdFuseBitsHI();
                cfc_state.flags &= ~4;
            }
            if ((cfc_state.flags & 0x12) &&
                (cfc_state.o.fuseex != cfc_state.n.fuseex)) {
                hvpWrFuseBitsEX(cfc_state.n.fuseex);
                cfc_state.o.fuseex = hvpRdFuseBitsEX();
                cfc_state.flags &= ~2;
            }
            if ((cfc_state.flags & 0x11) &&
                (cfc_state.o.lockxx != cfc_state.n.lockxx)) {
                hvpWrLockBits(cfc_state.n.lockxx);
                cfc_state.o.lockxx = hvpRdLockBits();
                cfc_state.flags &= ~1;
            }
            cfc_putln(F("completed"));
            break;

        case 'p':
            cfc_puts(F("preset\n"));
            cfc_t ps, *p;
            uint8_t i, c;
            for (i = 0; i < 10; i++) {
                p = (cfc_t *) pgm_read_ptr(&cfc_presets[i]);
                if (! p) break;
                memcpy_P(&ps, p, sizeof(ps));
                cfc_putd(i);
                cfc_puts(F(" Fuse"));
                cfc_putx(F(" lo:"), ps.fuselo);
                cfc_putx(F(" hi:"), ps.fusehi);
                cfc_putx(F(" ex:"), ps.fuseex);
                cfc_putx(F("  Lock:"), ps.lockxx);
                cfc_puts(F("  "));
                cfc_putln((const __FlashStringHelper *) (p->note));
            }
            cfc_putln(F("x Cancel"));
            cfc_puts(F("Preset selection? "));
            if (i > 0) {
                cfc_puts(F("(0-"));
                cfc_putd(i-1);
                cfc_puts(F("|x): "));
            } else {
                cfc_puts(F("(x) :"));
            }
            c = cfc_getch();
            if (c < '0' || c > (i + '0')) goto __cancel;
            c -= '0';
            p = (cfc_t *) pgm_read_ptr(&cfc_presets[c]);
            memcpy_P(&ps, p, sizeof(ps));
            cfc_state.n.fuselo = ps.fuselo;
            cfc_state.n.fusehi = ps.fusehi;
            cfc_state.n.fuseex = ps.fuseex;
            cfc_state.n.lockxx = ps.lockxx;
            cfc_state.flags |= 0xf;
            cfc_putln(F("completed"));
            break;
            
        case 'e':
            cfc_puts(F("erase\nErase the chip's flash and eeprom? (y|n): "));
            if (cfc_getch() != 'y') goto __cancel;
            cfc_program_mode();
            hvpChipErase();
            cfc_state.flags |= 0x20;
            cfc_putln(F("completed"));
            break;

        case 'i':
            cfc_puts(F("input\nEnter hex values (xx xx xx xx): "));
            union {uint32_t l; struct {uint8_t a, b, c, d;};} u;
            if (! ((u.l = cfc_getlx()) > 0)) goto __cancel;
            cfc_state.n.fuselo = u.d;
            cfc_state.n.fusehi = u.c;
            cfc_state.n.fuseex = u.b;
            cfc_state.n.lockxx = u.a;
            cfc_state.flags |= 0xf;
            cfc_putln(F("entered"));
            break;
            
        case 'q':
            cfc_puts(F("quit\nExit? (y|n): "));
            if (cfc_getch() != 'y') goto __cancel;
            cfc_state.flags |= 0x80;
        }
    }
    hvpStandbyMode();
    cfc_putln(F("bye"));
}

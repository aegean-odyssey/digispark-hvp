/* FUSE BYTES AND LOCK BITS PRESETS
   
   AUTHOR
   Demitrios V. (GitHub @aegean-odyssey)

   LICENSE
   digispark-hvp, copyright © 2025 by Aegean Associates Inc., is licensed under
   Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International. To
   view a copy of this license, visit
    https://creativecommons.org/licenses/by-nc-sa/4.0/ 
*/

#include "fuse_presets.h"

#define PRESET(n)  const cfc_t cfc_preset_ ## n PROGMEM

PRESET(0) = { 0x62, 0xdf, 0xff, 0xff, "factory default" };
PRESET(1) = { 0xe1, 0xdd, 0xfe, 0xff, "digispark bootloader" };
PRESET(2) = { 0xf1, 0x7c, 0xff, 0xff, "16mhz PLL, 4.3v BOD, no reset" }; 
PRESET(3) = { 0x71, 0x7d, 0xff, 0xff, "2mhz PLL, 2.7v BOD, no reset" }; 
PRESET(4) = { 0xe2, 0x7c, 0xff, 0xff, "8mhz IntRC, 4.3v BOD, no reset" }; 
PRESET(5) = { 0x62, 0x7d, 0xff, 0xff, "1mhz IntRC, 2.7v BOD, no reset" }; 
PRESET(6) = { 0xe4, 0x7e, 0xff, 0xff, "128khz WDOsc, 1.8v BOD, no reset" };
PRESET(7) = { 0x64, 0x7e, 0xff, 0xff, "16khz WDOsc, 1.8v BOD, no reset" }; 

const cfc_t * const cfc_presets[] PROGMEM = {
    &cfc_preset_0,
    &cfc_preset_1,
    &cfc_preset_2,
    &cfc_preset_3,
    &cfc_preset_4,
    &cfc_preset_5,
    &cfc_preset_6,
    &cfc_preset_7,
    (void *) 0
};

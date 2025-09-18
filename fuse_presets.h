/* FUSE BYTES AND LOCK BITS PRESETS
   
   AUTHOR
   Demitrios V. (GitHub @aegean-odyssey)

   LICENSE
   digispark-hvp, copyright © 2025 by Aegean Associates Inc., is licensed under
   Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International. To
   view a copy of this license, visit
    https://creativecommons.org/licenses/by-nc-sa/4.0/ 
*/

#include <stdint.h>
#include <avr/pgmspace.h>

typedef struct {
    uint8_t fuselo;
    uint8_t fusehi;
    uint8_t fuseex;
    uint8_t lockxx;
    char note[];
} cfc_t;

extern const cfc_t * const cfc_presets[] PROGMEM;

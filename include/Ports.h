/*

ESPeccy, a Sinclair ZX Spectrum emulator for Espressif ESP32 SoC

This project is a fork of ESPectrum.
ESPectrum is developed by Víctor Iborra [Eremus] and David Crespo [dcrespo3d]
https://github.com/EremusOne/ZX-ESPectrum-IDF

Based on previous work:
- ZX-ESPectrum-Wiimote (2020, 2022) by David Crespo [dcrespo3d]
  https://github.com/dcrespo3d/ZX-ESPectrum-Wiimote
- ZX-ESPectrum by Ramón Martinez and Jorge Fuertes
  https://github.com/rampa069/ZX-ESPectrum
- Original project by Pete Todd
  https://github.com/retrogubbins/paseVGA

Copyright (c) 2024 Juan José Ponteprino [SplinterGU]
https://github.com/SplinterGU/ESPeccy

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

*/

#ifndef Ports_h
#define Ports_h

#include <inttypes.h>
#include "ESPeccy.h"

class Ports {

public:

    static uint8_t input(uint16_t address);
    static void output(uint16_t address, uint8_t data);
    static uint8_t port[128];

    static uint8_t (*getFloatBusData)();
    static uint8_t getFloatBusData48();
    static uint8_t getFloatBusDataTK();
    static uint8_t getFloatBusData128();
    static uint8_t getFloatBusDataPentagon();
    static uint8_t getFloatBusData2A3();

    static uint8_t LastOutTo1FFD;
    static uint8_t LastOutTo7FFD;

    static int16_t in254_count;
    static bool loading;

private:

    static void ioContentionLate(bool contend);
    static uint8_t port254;
    static uint8_t speaker_values[8];

};

#endif // Ports_h

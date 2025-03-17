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

About keymap.h
Copyright (c) 2025 Antonio Tamairon [Hash6iron]
https://github.com/Hash6iron

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

#ifndef Keymap_h
#define Keymap_h

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "FileUtils.h"
#include "fabgl.h"
#include "ESPeccy.h"

using namespace std;

class Keymap
{

  public:

      // Keyboard map layout
      static fabgl::KeyboardLayout        kbdcustom;
      static char                         keymapfilename[25];
      
      // Variables
      static string                       keymapfile_path;
      static bool                         keymap_enable;

      // Methods
      static bool keymapFileExists();
      static bool getKeymapFromFile(fabgl::KeyboardLayout &kbdcustom);
      // static bool keymapInitializeFile();

      // Methods for fabgl::kbdlayout parsing and loading
      static void activeKeyboardLayout(fabgl::KeyboardLayout &kbdcustom);
      static void activeKeyboardLayoutForOSD();

};

#endif // Keymap.h
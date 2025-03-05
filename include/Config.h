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

#ifndef Config_h
#define Config_h

#include <stdio.h>
#include <inttypes.h>
#include <string>

using namespace std;

#define JOY_CURSOR 0
#define JOY_KEMPSTON 1
#define JOY_SINCLAIR1 2
#define JOY_SINCLAIR2 3
#define JOY_FULLER 4
#define JOY_CUSTOM 5
#define JOY_NONE 6

#define JOYPS2_CURSOR 0
#define JOYPS2_KEMPSTON 1
#define JOYPS2_SINCLAIR1 2
#define JOYPS2_SINCLAIR2 3
#define JOYPS2_FULLER 4
#define JOYPS2_CUSTOM 5
#define JOYPS2_NONE 6

#define CovoxNONE 0
#define CovoxMONO 1
#define CovoxSTEREO 2
#define CovoxSOUNDDRIVE1 3
#define CovoxSOUNDDRIVE2 4
class Config
{
public:

    static bool load();
    static void save();
    static void save(string value);

    static bool backupExistsOnSD();
    static bool saveToSD();
    static bool loadFromSD();

    static void requestMachine(string newArch, string newRomSet);

    static void setJoyMap(uint8_t joynum, uint8_t joy_type);

    static string   arch;
    static string   romSet;
    static string   romSet48;
    static string   romSet128;
    static string   romSet2A;
    static string   romSet3;
    static string   romSetTK90X;
    static string   romSetTK95;
    static string   pref_arch;
    static string   pref_romSet_48;
    static string   pref_romSet_128;
    static string   pref_romSet_2A;
    static string   pref_romSet_3;
    static string   pref_romSet_TK90X;
    static string   pref_romSet_TK95;
    static string   ram_file;
    static string   last_ram_file;
    static string   rom_file;
    static string   last_rom_file;
    static uint8_t  esp32rev;
    static bool     slog_on;
    static bool     aspect_16_9;
    static uint8_t  lang;
    static uint8_t  osd_LRNav;
    static uint8_t  osd_AltRot;
    static bool     AY48;
    static bool     Issue2;
    static bool     flashload;
    static bool     load_monitor;
    static bool     tape_timing_rg;
    static uint8_t  joystick1;
    static uint8_t  joystick2;
    static uint16_t joydef[24];
    static uint8_t  joyPS2;
    static uint8_t  videomode;
    static uint8_t  AluTiming;
    static uint8_t  ps2_dev2;
    static bool     CursorAsJoy;
    static int8_t   CenterH;
    static int8_t   CenterV;

    static string   SNA_Path;
    static uint16_t SNA_begin_row;
    static uint16_t SNA_focus;
    static uint8_t  SNA_fdMode;
    static string   SNA_fileSearch;

    static string   TAP_Path;
    static uint16_t TAP_begin_row;
    static uint16_t TAP_focus;
    static uint8_t  TAP_fdMode;
    static string   TAP_fileSearch;

    static string   DSK_Path;
    static uint16_t DSK_begin_row;
    static uint16_t DSK_focus;
    static uint8_t  DSK_fdMode;
    static string   DSK_fileSearch;

    static uint8_t  scanlines;
    static uint8_t  render;

    static bool     TABasfire1;

    static bool     StartMsg;

    static uint8_t  port254default; // For TK90X v1 ROM -> 0xbf: Spanish, 0x3f: Portuguese

    static uint8_t  ALUTK; // TK ALU -> 0 -> Ferranti, 1 -> Microdigital 50hz, 2 -> Microdigital 60hz

    static uint8_t  DiskCtrl; // 0 -> None, 1 -> Betadisk

    static bool     TimeMachine;

    static int8_t   volume;

    static bool     TapeAutoload;

    static bool     thumbsEnabled;
    static bool     instantPreview;

    static uint8_t  Covox;

    static uint8_t  mousesamplerate;
    static uint8_t  mousedpi;
    static uint8_t  mousescaling;

    static bool     keymap_enable;
    static uint8_t  pathforkeymapfile_pos;
    static string   pathforkeymapfile;

    static bool     keymap_enable;
    static uint8_t  pathforkeymapfile_pos;
    static string   pathforkeymapfile;

    static uint8_t  realtape_mode;
    static uint8_t  realtape_gpio_num;

    static uint32_t psramsize;

};

#endif // Config.h

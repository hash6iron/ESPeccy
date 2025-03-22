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

#ifndef ESPECCY_MESSAGES_EN_h
#define ESPECCY_MESSAGES_EN_h

#define ERR_FS_EXT_FAIL_EN "Cannot mount external storage!"

#define OSD_MSGDIALOG_YES_EN " Yes  "
#define OSD_MSGDIALOG_NO_EN "  No  "

#define OSD_PAUSE_EN "--=[ PAUSED ]=--"

#define POKE_ERR_ADDR1_EN "Address should be between 16384 and 65535"
#define POKE_ERR_ADDR2_EN "Address should be lower than 16384"
#define POKE_ERR_VALUE_EN "Value should be lower than 256"

#define OSD_INVALIDCHAR_EN "Invalid character"

#define OSD_TAPE_SAVE_EN "SAVE command"

#define OSD_TAPE_SAVE_EXIST_EN "File exists. Overwrite?"

#define OSD_PSNA_SAVE_EN "Save snapshot"

#define OSD_PSNA_EXISTS_EN "Overwrite slot?"

#define OSD_TAPE_SELECT_ERR_EN "No tape file selected"

#define OSD_FILE_INDEXING_EN "Indexing"

#define OSD_FILE_INDEXING_EN_1 "  Sorting   "

#define OSD_FILE_INDEXING_EN_2 "Saving index"

#define OSD_FILE_INDEXING_EN_3 "  Cleaning  "

#define OSD_FIRMW_UPDATE_EN "Firmware update"

#define OSD_DLG_SURE_EN "Are you sure?"

#define OSD_DLG_JOYSAVE_EN "Save changes?"

#define OSD_DLG_JOYDISCARD_EN "Discard changes?"

#define OSD_DLG_SETJOYMAPDEFAULTS_EN "Load joy type default map?"

#define OSD_FIRMW_EN "Updating firmware"

#define OSD_FIRMW_BEGIN_EN "Erasing destination partition."

#define OSD_FIRMW_WRITE_EN "    Flashing new firmware.    "

#define OSD_FIRMW_END_EN "Flashing complete. Rebooting."

#define OSD_NOFIRMW_ERR_EN "No firmware file found."

#define OSD_FIRMW_ERR_EN "Problem updating firmware."

#define MENU_ROM_TITLE_EN "Select ROM"

#define OSD_ROM_ERR_EN "Problem flashing ROM."

#define OSD_NOROMFILE_ERR_EN "No custom ROM file found."

#define OSD_ROM_EN "Flash Custom ROM"

#define OSD_ROM_BEGIN_EN "   Preparing flash space.   "

#define OSD_ROM_WRITE_EN "    Flashing custom ROM.    "

#define MENU_SNA_TITLE_EN "Select Snapshot"

#define MENU_SAVE_SNA_TITLE_EN "Save Snapshot"

#define MENU_TAP_TITLE_EN "Select tape"

#define MENU_DSK_TITLE_EN "Select disk"

#define MENU_CHT_TITLE_EN "Select cheats file"

#define MENU_SCR_TITLE_EN "Select .SCR"

#define MENU_DELETE_TAP_BLOCKS_EN "Delete selection"

#define MENU_DELETE_CURRENT_TAP_BLOCK_EN "Delete block"

#define OSD_BLOCK_SELECT_ERR_EN "No blocks selected"

#define OSD_BLOCK_TYPE_ERR_EN "Invalid block type"

#define MENU_DELETE_CURRENT_FILE_EN "Delete file"

#define OSD_READONLY_FILE_WARN_EN "Read only file"

#define OSD_TAPE_FLASHLOAD_EN "Flash loading tape file"
#define OSD_TAPE_INSERT_EN "Tape inserted"
#define OSD_TAPE_EJECT_EN "Tape ejected"

#define OSD_ROM_EJECT_EN "Cartridge ejected"
#define OSD_ROM_INSERT_ERR_EN "No cartridge inserted"

#define MENU_DELETE_SNA_EN "Delete snapshot"

#define TRDOS_RESET_ERR_EN "Can't reset to TR-DOS. Enable Betadisk."

#define MENU_SNA_EN \
    "Snapshots\n"\
    "Load\t>\n"\
    "Save\t>\n"\
    "Load in slot \t>\n"\
    "Save in slot \t>\n"

#define MENU_TAPE_EN \
    "Tapes\n"\
    "Select (TAP,TZX)\t>\n"\
    "Eject Tape\n"\
    "Tape browser\t>\n"\
    "LOAD monitor\t>\n"\
    "Real tape\t>\n"

#define MENU_REALTAPE_EN "Real tape load\n"\
    "Auto\t[0]\n"\
    "Force LOAD\t[1]\n"\
    "Force SAVE\t[2]\n"

#define MENU_BETADISK_EN \
    "Drives\n"\
    "Drive A\t>\n"\
    "Drive B\t>\n"\
    "Drive C\t>\n"\
    "Drive D\t>\n"

#define MENU_BETADRIVE_EN \
    "Drive#\n"\
    "Insert disk\t>\n"\
    "Eject disk\n"

#define MENU_ROM_CART_EN \
    "Cartridges\n"\
    "Insert\t>\n"\
    "Eject\n"

#define MENU_MAIN_EN \
    "Snapshots\t>\n"\
    "Tapes\t>\n"\
    "Betadisk\t>\n"\
    "Cartridges\t>\n"\
    "Machine\t>\n"\
    "Reset\t>\n"\
    "Options\t>\n"\
    "Update\t>\n"\
    "Keyboard help\t \n"\
    "About\n"

#define MENU_OPTIONS_EN \
    "Options\n"\
    "Storage\t>\n"\
    "Preferred Machine\t>\n"\
    "Preferred ROM\t>\n"\
    "Joystick\t>\n"\
    "Joystick emulation\t>\n"\
    "Display\t>\n"\
    "Mouse\t>\n"\
    "Sound\t>\n"\
    "Other\t>\n"\
    "Language\t>\n"\
    "User Interface\t>\n"\
    "EAR config\t>\n"

#define MENU_UPDATE_EN \
    "Update\n"\
    "Firmware\n"\
    "Custom ROM 48K\n"\
    "Custom ROM 128k\n"\
    "Custom ROM +2A\n"\
    "Custom ROM TK\n"

#define MENU_VIDEO_EN \
    "Display\n"\
    "Render type\t>\n"\
    "Resolution\t>\n"\
    "Scanlines\t>\n"

#define MENU_RENDER_EN \
    "Render type\n"\
    "Standard\t[S]\n"\
    "Snow effect\t[A]\n"

#define MENU_ASPECT_EN \
    "Resolution\n"\
    "320x240 (4:3)\t[4]\n"\
    "360x200 (16:9)\t[1]\n"

#define MENU_RESET_EN \
    "Reset\n"\
    "Soft reset\n"\
    "TR-DOS reset \t(CF11)\n"\
    "Hard reset\t(F11)\n"\
    "ESP32 reset\t(F12)\n"

#define MENU_PERSIST_SAVE_EN \
    "Save snapshot\n"

#define MENU_PERSIST_LOAD_EN \
    "Load snapshot\n"

#define MENU_STORAGE_EN "Storage\n"\
    "Betadisk\t>\n"\
    "Auto tape load\t>\n"\
    "Flash tape load\t>\n"\
    "R.G. ROM timings\t>\n"

#define MENU_YESNO_EN "Yes\t[Y]\n"\
    "No\t[N]\n"

#define MENU_SOUND_EN "Sound\n"\
    "AY on 48K\t>\n"\
    "Covox\t>\n"

#define MENU_MOUSE_EN "Mouse\n"\
    "Sample Rate\t>\n"\
    "Resolution\t>\n"\
    "Scaling\t>\n"

#define MENU_OTHER_EN "Other\n"\
    "ULA Timing\t>\n"\
    "48K Issue 2\t>\n"\
    "TK ULA\t>\n"\
    "Second PS/2 device\t>\n"

#define MENU_KBD2NDPS2_EN "Device     \n"\
    "None\t[N]\n"\
    "Kbd / DB9 adapter\t[K]\n"\
    "Mouse\t[M]\n"

#define MENU_ALUTIMING_EN "ULA Timing\n"\
    "Early\t[E]\n"\
    "Late\t[L]\n"

#define MENU_ARCH_EN "Select machine\n"

#define MENU_ROMS48_EN "Select ROM\n"\
    "48K\n"\
    "48K Spanish\n"\
    "Custom\n"

#define MENU_ROMS128_EN "Select ROM\n"\
    "128K\n"\
    "128K Spanish\n"\
    "+2\n"\
    "+2 Spanish\n"\
    "+2 French\n"\
    "ZX81+\n"\
    "Custom\n"

#define MENU_ROMS2A_3_EN "Select ROM\n"\
    "v4.0 +2a/+3\n"\
    "v4.0 +2a/+3 Spanish\n"\
    "v4.1 +2a/+3\n"\
    "v4.1 +2a/+3 Spanish\n"\
    "Custom\n"

#define MENU_ROMSTK_EN "Select ROM\n"\
    "v1 Spanish\n"\
    "v1 Portuguese\n"\
    "v2 Spanish\n"\
    "v2 Portuguese\n"\
    "v3 Spanish (R.G.)\n"\
    "v3 Portuguese (R.G.)\n"\
    "v3 English (R.G.)\n"\
    "Custom\n"

#define MENU_ROMSTK95_EN "Select ROM\n"\
    "Spanish\n"\
    "Portuguese\n"

#define MENU_ROMS48_PREF_EN	"48K\t[48K]\n"\
    "48K Spanish\t[48Kes]\n"\
    "Custom\t[48Kcs]\n"

#define MENU_ROMSTK90X_PREF_EN "v1 Spanish\t[v1es ]\n"\
    "v1 Portuguese\t[v1pt ]\n"\
	"v2 Spanish\t[v2es ]\n"\
    "v2 Portuguese\t[v2pt ]\n"\
	"v3 Spanish (R.G.)\t[v3es ]\n"\
    "v3 Portuguese (R.G.)\t[v3pt ]\n"\
    "v3 English (R.G.)\t[v3en ]\n"\
    "Custom\t[TKcs ]\n"

#define MENU_ROMSTK95_PREF_EN "Spanish\t[95es ]\n"\
    "Portuguese\t[95pt ]\n"

#define MENU_ROMS128_PREF_EN "128K\t[128K]\n"\
    "128K Spanish\t[128Kes]\n"\
	"+2\t[+2]\n"\
    "+2 Spanish\t[+2es]\n"\
    "+2 French\t[+2fr]\n"\
    "ZX81+\t[ZX81+]\n"\
    "Custom\t[128Kcs]\n"

#define MENU_ROMS2A_3_PREF_EN "v4.0 +2a/+3\t[+2A]\n"\
    "v4.0 +2a/+3 Spanish\t[+2Aes]\n"\
    "v4.1 +2a/+3\t[+2A41]\n"\
    "v4.1 +2a/+3 Spanish\t[+2A41es]\n"\
    "Custom\t[+2Acs]\n"

#define MENU_INTERFACE_LANG_EN "Language\n"\
    "English\t[E]\n"\
    "Spanish\t[S]\n"\
    "Portuguese\t[P]\n"

#define MENU_JOY_EN "Joystick\n"

#define MENU_DEFJOY_EN "Assign keys\n"

#define MENU_JOYPS2_EN "Joystick emulation\n" "Joy type\t>\n" "Cursor Keys as Joy\t>\n" "TAB as fire 1\t>\n"

#define DLG_TITLE_INPUTPOK_EN "Input Poke"

#define MENU_UI_EN "User Interface\n"\
    "Menu with cursors\t>\n"\
    "Text scroll\t>\n"\
    "Thumbnails\t>\n"\
    "Instant preview\t>\n"

#endif // ESPECCY_MESSAGES_EN_h

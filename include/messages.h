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

#ifndef ESPECTRUM_MESSAGES_h
#define ESPECTRUM_MESSAGES_h

#define EMU_VERSION "       v1.3 "

// Language files
#include "messages_en.h"
#include "messages_es.h"
#include "messages_pt.h"

// General
#define NLANGS 3

static const int LANGCODEPAGE[NLANGS] = { 437, 437, 860 };

#define MSG_LOADING_SNA "Loading SNA file"
#define MSG_LOADING_Z80 "Loading Z80 file"
#define MSG_SAVE_CONFIG "Saving config file"
#define MSG_VGA_INIT "Initializing VGA"

// Error
#define ERROR_TITLE "  !!!   ERROR - CLIVE MEDITATION   !!!  "
#define ERROR_BOTTOM "  Sir Clive is smoking in the Rolls...  "
#define ERR_READ_FILE "Cannot read file!"
#define ERR_BANK_FAIL "Failed to allocate RAM bank"
#define ERR_FS_INT_FAIL "Cannot mount internal storage!"

static const char *ERR_FS_EXT_FAIL[NLANGS] = { ERR_FS_EXT_FAIL_EN, ERR_FS_EXT_FAIL_ES, ERR_FS_EXT_FAIL_PT };

#define ERR_DIR_OPEN "Cannot open directory!"

// OSD
#define OSD_TITLE  " ESPeccy - The ESP32 powered emulator         "
//#define OSD_BOTTOM " zxespectrum.speccy.org     " EMU_VERSION
#define OSD_BOTTOM " github.com/SplinterGU/ESPeccy "

static const char *OSD_MSGDIALOG_YES[NLANGS] = { OSD_MSGDIALOG_YES_EN, OSD_MSGDIALOG_YES_ES, OSD_MSGDIALOG_YES_PT };
static const char *OSD_MSGDIALOG_NO[NLANGS] = { OSD_MSGDIALOG_NO_EN, OSD_MSGDIALOG_NO_ES, OSD_MSGDIALOG_NO_PT };

static const char *OSD_PAUSE[NLANGS] = { OSD_PAUSE_EN,OSD_PAUSE_ES,OSD_PAUSE_PT };

#define OSD_PSNA_NOT_AVAIL "No Snapshot Available"
#define OSD_PSNA_LOADING "Loading Snapshot"
#define OSD_PSNA_SAVING  "Saving Snapshot"
#define OSD_PSNA_SAVE_WARN "Disk error. Trying slow mode, be patient"
#define OSD_PSNA_SAVE_ERR "ERROR Saving Snapshot"
#define OSD_PSNA_LOADED  "Snapshot Loaded"
#define OSD_PSNA_LOAD_ERR "ERROR Loading Snapshot"
#define OSD_PSNA_SAVED  "Snapshot Saved"
#define OSD_TAPE_LOAD_ERR "ERROR Loading tape file"
#define OSD_TAPE_SAVE_ERR "ERROR Saving tape file"
#define OSD_BETADISK_LOAD_ERR "ERROR Loading Disk file"
#define OSD_ROM_LOAD_ERR "ERROR Loading ROM file"

#define OSD_PLEASE_WAIT "Please Wait..."

static const char *OSD_READONLY_FILE_WARN[NLANGS] = { OSD_READONLY_FILE_WARN_EN, OSD_READONLY_FILE_WARN_ES, OSD_READONLY_FILE_WARN_PT };

static const char *OSD_TAPE_FLASHLOAD[NLANGS] = { OSD_TAPE_FLASHLOAD_EN, OSD_TAPE_FLASHLOAD_ES, OSD_TAPE_FLASHLOAD_PT };

static const char *OSD_TAPE_INSERT[NLANGS] = { OSD_TAPE_INSERT_EN, OSD_TAPE_INSERT_ES, OSD_TAPE_INSERT_PT };

static const char *POKE_ERR_ADDR1[NLANGS] = { POKE_ERR_ADDR1_EN, POKE_ERR_ADDR1_ES, POKE_ERR_ADDR1_PT };

static const char *POKE_ERR_ADDR2[NLANGS] = { POKE_ERR_ADDR2_EN, POKE_ERR_ADDR2_ES, POKE_ERR_ADDR2_PT };

static const char *POKE_ERR_VALUE[NLANGS] = { POKE_ERR_VALUE_EN, POKE_ERR_VALUE_ES, POKE_ERR_VALUE_PT };

static const char *OSD_INVALIDCHAR[NLANGS] = { OSD_INVALIDCHAR_EN, OSD_INVALIDCHAR_ES, OSD_INVALIDCHAR_PT };

static const char *OSD_TAPE_SAVE[NLANGS] = { OSD_TAPE_SAVE_EN, OSD_TAPE_SAVE_ES, OSD_TAPE_SAVE_PT };

static const char *OSD_TAPE_SAVE_EXIST[NLANGS] = { OSD_TAPE_SAVE_EXIST_EN, OSD_TAPE_SAVE_EXIST_ES, OSD_TAPE_SAVE_EXIST_PT };

static const char *OSD_PSNA_SAVE[NLANGS] = { OSD_PSNA_SAVE_EN, OSD_PSNA_SAVE_ES, OSD_PSNA_SAVE_PT };

static const char *OSD_PSNA_EXISTS[NLANGS] = { OSD_PSNA_EXISTS_EN, OSD_PSNA_EXISTS_ES, OSD_PSNA_EXISTS_PT };

static const char *OSD_TAPE_SELECT_ERR[NLANGS] = { OSD_TAPE_SELECT_ERR_EN,OSD_TAPE_SELECT_ERR_ES, OSD_TAPE_SELECT_ERR_PT };

static const char *OSD_ROM_INSERT_ERR[NLANGS] = { OSD_ROM_INSERT_ERR_EN, OSD_ROM_INSERT_ERR_ES, OSD_ROM_INSERT_ERR_PT };

static const char *OSD_FILE_INDEXING[NLANGS] = { OSD_FILE_INDEXING_EN, OSD_FILE_INDEXING_ES, OSD_FILE_INDEXING_PT };

static const char *OSD_FILE_INDEXING_1[NLANGS] = { OSD_FILE_INDEXING_EN_1, OSD_FILE_INDEXING_ES_1, OSD_FILE_INDEXING_PT_1 };

static const char *OSD_FILE_INDEXING_2[NLANGS] = { OSD_FILE_INDEXING_EN_2, OSD_FILE_INDEXING_ES_2, OSD_FILE_INDEXING_PT_2 };

static const char *OSD_FILE_INDEXING_3[NLANGS] = { OSD_FILE_INDEXING_EN_3, OSD_FILE_INDEXING_ES_3, OSD_FILE_INDEXING_PT_3 };

static const char *OSD_FIRMW_UPDATE[NLANGS] = { OSD_FIRMW_UPDATE_EN,OSD_FIRMW_UPDATE_ES, OSD_FIRMW_UPDATE_PT };

static const char *OSD_DLG_SURE[NLANGS] = { OSD_DLG_SURE_EN, OSD_DLG_SURE_ES, OSD_DLG_SURE_PT };

static const char *OSD_DLG_JOYSAVE[NLANGS] = { OSD_DLG_JOYSAVE_EN, OSD_DLG_JOYSAVE_ES, OSD_DLG_JOYSAVE_PT };

static const char *OSD_DLG_JOYDISCARD[NLANGS] = { OSD_DLG_JOYDISCARD_EN, OSD_DLG_JOYDISCARD_ES, OSD_DLG_JOYDISCARD_PT };

static const char *OSD_DLG_SETJOYMAPDEFAULTS[NLANGS] = { OSD_DLG_SETJOYMAPDEFAULTS_EN, OSD_DLG_SETJOYMAPDEFAULTS_ES, OSD_DLG_SETJOYMAPDEFAULTS_PT };

static const char *OSD_FIRMW[NLANGS] = { OSD_FIRMW_EN,OSD_FIRMW_ES, OSD_FIRMW_PT };

static const char *OSD_FIRMW_BEGIN[NLANGS] = { OSD_FIRMW_BEGIN_EN,OSD_FIRMW_BEGIN_ES, OSD_FIRMW_BEGIN_PT };

static const char *OSD_FIRMW_WRITE[NLANGS] = { OSD_FIRMW_WRITE_EN,OSD_FIRMW_WRITE_ES, OSD_FIRMW_WRITE_PT };

static const char *OSD_FIRMW_END[NLANGS] = { OSD_FIRMW_END_EN,OSD_FIRMW_END_ES, OSD_FIRMW_END_PT };

static const char *OSD_NOFIRMW_ERR[NLANGS] = { OSD_NOFIRMW_ERR_EN,OSD_NOFIRMW_ERR_ES, OSD_NOFIRMW_ERR_PT };

static const char *OSD_FIRMW_ERR[NLANGS] = { OSD_FIRMW_ERR_EN,OSD_FIRMW_ERR_ES, OSD_FIRMW_ERR_PT };

static const char *MENU_ROM_TITLE[NLANGS] = { MENU_ROM_TITLE_EN,MENU_ROM_TITLE_ES,MENU_ROM_TITLE_PT };

static const char *OSD_ROM_ERR[NLANGS] = { OSD_ROM_ERR_EN,OSD_ROM_ERR_ES,OSD_ROM_ERR_PT };

static const char *OSD_NOROMFILE_ERR[NLANGS] = { OSD_NOROMFILE_ERR_EN,OSD_NOROMFILE_ERR_ES,OSD_NOROMFILE_ERR_PT };

static const char *OSD_ROM[NLANGS] = { OSD_ROM_EN,OSD_ROM_ES,OSD_ROM_PT };

static const char *OSD_ROM_BEGIN[NLANGS] = { OSD_ROM_BEGIN_EN,OSD_ROM_BEGIN_ES,OSD_ROM_BEGIN_PT };

static const char *OSD_ROM_WRITE[NLANGS] = { OSD_ROM_WRITE_EN,OSD_ROM_WRITE_ES,OSD_ROM_WRITE_PT };

static const char *MENU_SNA_TITLE[NLANGS] = { MENU_SNA_TITLE_EN,MENU_SNA_TITLE_ES,MENU_SNA_TITLE_PT };

static const char *MENU_SAVE_SNA_TITLE[NLANGS] = { MENU_SAVE_SNA_TITLE_EN,MENU_SAVE_SNA_TITLE_ES,MENU_SAVE_SNA_TITLE_PT };

static const char *MENU_TAP_TITLE[NLANGS] = { MENU_TAP_TITLE_EN,MENU_TAP_TITLE_ES,MENU_TAP_TITLE_PT };

static const char *MENU_DSK_TITLE[NLANGS] = { MENU_DSK_TITLE_EN,MENU_DSK_TITLE_ES,MENU_DSK_TITLE_PT };

static const char *MENU_CHT_TITLE[NLANGS] = { MENU_CHT_TITLE_EN,MENU_CHT_TITLE_ES,MENU_CHT_TITLE_PT };

static const char *MENU_SCR_TITLE[NLANGS] = { MENU_SCR_TITLE_EN,MENU_SCR_TITLE_ES,MENU_SCR_TITLE_PT };

static const char *MENU_DELETE_TAP_BLOCKS[NLANGS] = { MENU_DELETE_TAP_BLOCKS_EN,MENU_DELETE_TAP_BLOCKS_ES,MENU_DELETE_TAP_BLOCKS_PT };

static const char *MENU_DELETE_CURRENT_TAP_BLOCK[NLANGS] = { MENU_DELETE_CURRENT_TAP_BLOCK_EN,MENU_DELETE_CURRENT_TAP_BLOCK_ES,MENU_DELETE_CURRENT_TAP_BLOCK_PT };

static const char *OSD_BLOCK_SELECT_ERR[NLANGS] = { OSD_BLOCK_SELECT_ERR_EN,OSD_BLOCK_SELECT_ERR_ES,OSD_BLOCK_SELECT_ERR_PT };

static const char *OSD_BLOCK_TYPE_ERR[NLANGS] = { OSD_BLOCK_TYPE_ERR_EN,OSD_BLOCK_TYPE_ERR_ES,OSD_BLOCK_TYPE_ERR_PT };

static const char *MENU_DELETE_CURRENT_FILE[NLANGS] = { MENU_DELETE_CURRENT_FILE_EN,MENU_DELETE_CURRENT_FILE_ES,MENU_DELETE_CURRENT_FILE_PT };

static const char *OSD_TAPE_EJECT[NLANGS] = { OSD_TAPE_EJECT_EN,OSD_TAPE_EJECT_ES,OSD_TAPE_EJECT_PT };

static const char *OSD_ROM_EJECT[NLANGS] = { OSD_ROM_EJECT_EN,OSD_ROM_EJECT_ES,OSD_ROM_EJECT_PT };

static const char *MENU_DELETE_SNA[NLANGS] = { MENU_DELETE_SNA_EN,MENU_DELETE_SNA_ES,MENU_DELETE_SNA_PT };

static const char *TRDOS_RESET_ERR[NLANGS] = { TRDOS_RESET_ERR_EN,TRDOS_RESET_ERR_ES,TRDOS_RESET_ERR_PT };

static const char *MENU_SNA[NLANGS] = { MENU_SNA_EN,MENU_SNA_ES,MENU_SNA_PT };

static const char *MENU_TAPE[NLANGS] = { MENU_TAPE_EN,MENU_TAPE_ES,MENU_TAPE_PT };

static const char *MENU_TAPEPLAYER[NLANGS] = { "Player mode\n", "Modo reproductor\n", "Modo reprodutor\n" };

static const char *MENU_BETADISK[NLANGS] = { MENU_BETADISK_EN,MENU_BETADISK_ES,MENU_BETADISK_PT };

static const char *MENU_BETADRIVE[NLANGS] = { MENU_BETADRIVE_EN,MENU_BETADRIVE_ES,MENU_BETADRIVE_PT };

static const char *MENU_ROM_CART[NLANGS] = { MENU_ROM_CART_EN, MENU_ROM_CART_ES, MENU_ROM_CART_PT };

static const char *MENU_MAIN[3] = { MENU_MAIN_EN,MENU_MAIN_ES,MENU_MAIN_PT };

static const char *MENU_OPTIONS[NLANGS] = { MENU_OPTIONS_EN,MENU_OPTIONS_ES,MENU_OPTIONS_PT };

static const char *MENU_UPDATE_FW[NLANGS] = { MENU_UPDATE_EN,MENU_UPDATE_ES,MENU_UPDATE_PT };

static const char *MENU_VIDEO[NLANGS] = { MENU_VIDEO_EN, MENU_VIDEO_ES, MENU_VIDEO_PT };

static const char *MENU_RENDER[NLANGS] = { MENU_RENDER_EN, MENU_RENDER_ES, MENU_RENDER_PT };

static const char *MENU_ASPECT[NLANGS] = { MENU_ASPECT_EN, MENU_ASPECT_ES, MENU_ASPECT_PT };

static const char *MENU_SCANLINES[NLANGS] = { "Scanlines\n", "Scanlines\n", "Scanlines\n" };

static const char *MENU_RESET[NLANGS] = { MENU_RESET_EN, MENU_RESET_ES, MENU_RESET_PT };

static const char *MENU_PERSIST_SAVE[NLANGS] = { MENU_PERSIST_SAVE_EN, MENU_PERSIST_SAVE_ES, MENU_PERSIST_SAVE_PT };

static const char *MENU_PERSIST_LOAD[NLANGS] = { MENU_PERSIST_LOAD_EN, MENU_PERSIST_LOAD_ES, MENU_PERSIST_LOAD_PT };

static const char *MENU_STORAGE[NLANGS] = { MENU_STORAGE_EN, MENU_STORAGE_ES, MENU_STORAGE_PT };

static const char *MENU_YESNO[NLANGS] = { MENU_YESNO_EN, MENU_YESNO_ES, MENU_YESNO_PT };

static const char *MENU_DISKCTRL[NLANGS] = { "Betadisk\n" , "Betadisk\n", "Betadisk\n" };

static const char *MENU_AUTOLOAD[NLANGS] = { "Auto load\n" , "Carga autom\xA0tica\n", "Carga autom\xA0tica\n" };

static const char *MENU_FLASHLOAD[NLANGS] = { "Flash load\n" , "Carga r\xA0pida\n", "Carga r\xA0pida\n" };

static const char *MENU_RGTIMINGS[NLANGS] = { "R.G. Timings\n" , "Timings R.G.\n", "Timings R.G.\n" };

static const char *MENU_OTHER[NLANGS] = { MENU_OTHER_EN, MENU_OTHER_ES, MENU_OTHER_PT };

static const char *MENU_AY48[NLANGS] = { "AY on 48K \n" , "AY en 48K \n" , "AY em 48K \n" };

static const char *SECOND_PS2_DEVICE[NLANGS][4] = {
    { "2nd PS/2 device: ", "None" , "Kbd / DB9 adapter" , "Mouse"},
    { "Segundo disp. PS/2: ", "Nada" , "Teclado / Adapt. DB9" , "Rat\xA2n"},
    { "Segundo disp. PS/2: ", "Nada" , "Teclado / Adapt. DB9" , "Mouse"}
};

#define MENU_COVOXS "Covox Mono\t[M]\n"\
    "Covox Stereo\t[S]\n"\
    "SoundDrive 1.05 mode 1\t[1]\n"\
    "SoundDrive 1.05 mode 2\t[2]\n"

static const char *MENU_COVOX[NLANGS] = { "Covox\n" "None   \t[N]\n" MENU_COVOXS, "Covox\n" "Ninguno\t[N]\n" MENU_COVOXS, "Covox\n" "Nenhum \t[N]\n" MENU_COVOXS};

#define MENU_ALUTK "Ferranti\t[F]\n"\
    "Microdigital 50hz\t[5]\n"\
    "Microdigital 60hz\t[6]\n"

static const char *MENU_ALUTK_PREF[NLANGS] = { "TK ULA\n" MENU_ALUTK, "ULA TK\n" MENU_ALUTK, "ULA TK\n" MENU_ALUTK };

static const char *MENU_KBD2NDPS2[NLANGS] = { MENU_KBD2NDPS2_EN, MENU_KBD2NDPS2_ES, MENU_KBD2NDPS2_PT };

static const char *MENU_ALUTIMING[NLANGS] = { MENU_ALUTIMING_EN, MENU_ALUTIMING_ES, MENU_ALUTIMING_PT };

static const char *MENU_ISSUE2[NLANGS] = { "48K Issue 2\n", "48K Issue 2\n", "48K Issue 2\n" };

#define MENU_ARCHS "Spectrum 48K\t>\n"\
    "Spectrum 128K\t>\n"\
    "Spectrum +2A\t>\n"\
    "Pentagon 128K\n"\
    "TK90X\t>\n"\
    "TK95\t>\n"

static const char *MENU_ARCH[NLANGS] = { MENU_ARCH_EN MENU_ARCHS, MENU_ARCH_ES MENU_ARCHS, MENU_ARCH_PT MENU_ARCHS };

static const char *MENU_ROMS48[NLANGS] = { MENU_ROMS48_EN, MENU_ROMS48_ES, MENU_ROMS48_PT };

static const char *MENU_ROMS128[NLANGS] = { MENU_ROMS128_EN, MENU_ROMS128_ES, MENU_ROMS128_PT };

static const char *MENU_ROMS2A_3[NLANGS] = { MENU_ROMS2A_3_EN, MENU_ROMS2A_3_ES, MENU_ROMS2A_3_PT };

static const char *MENU_ROMSTK[NLANGS] = { MENU_ROMSTK_EN, MENU_ROMSTK_ES, MENU_ROMSTK_PT };

static const char *MENU_ROMSTK95[NLANGS] = { MENU_ROMSTK95_EN, MENU_ROMSTK95_ES, MENU_ROMSTK95_PT };

#define MENU_ARCHS_PREF "Spectrum 48K\t[4]\n"\
    "Spectrum 128K\t[1]\n"\
    "Spectrum +2A\t[2]\n"\
    "Pentagon 128K\t[P]\n"\
    "TK90X\t[T]\n"\
    "TK95\t[9]\n"

static const char *MENU_ARCH_PREF[NLANGS] = {
    "Preferred machine\n" MENU_ARCHS_PREF "Last used\t[L]\n",
    "Modelo preferido\n" MENU_ARCHS_PREF "Ultimo utilizado\t[L]\n",
    "Hardware favorito\n" MENU_ARCHS_PREF "Usado por \xA3ltimo\t[L]\n"
};

#define MENU_ROMS_PREF "Spectrum 48K\t>\n"\
    "Spectrum 128K\t>\n"\
    "Spectrum +2A\t>\n"\
    "TK90X\t>\n"\
    "TK95\t>\n"

static const char *MENU_ROM_PREF[NLANGS] = { "Preferred ROM\n" MENU_ROMS_PREF, "ROM preferida\n" MENU_ROMS_PREF, "ROM favorita\n" MENU_ROMS_PREF };

static const char *MENU_ROM_PREF_48[NLANGS] = {
    "Select ROM\n" MENU_ROMS48_PREF_EN "Last used\t[Last]\n",
    "Elija ROM\n" MENU_ROMS48_PREF_ES "Ultima usada\t[Last]\n",
    "Escolha ROM\n" MENU_ROMS48_PREF_PT "Usada por \xA3ltimo\t[Last]\n"
    };

static const char *MENU_ROM_PREF_TK90X[NLANGS] = {
    "Select ROM\n" MENU_ROMSTK90X_PREF_EN "Last used\t[Last ]\n",
    "Elija ROM\n" MENU_ROMSTK90X_PREF_ES "Ultima usada\t[Last ]\n",
    "Escolha ROM\n" MENU_ROMSTK90X_PREF_PT "Usada por \xA3ltimo\t[Last ]\n"
};

static const char *MENU_ROM_PREF_128[NLANGS] = {
    "Select ROM\n" MENU_ROMS128_PREF_EN "Last used\t[Last]\n",
    "Elija ROM\n" MENU_ROMS128_PREF_ES "Ultima usada\t[Last]\n",
    "Escolha ROM\n" MENU_ROMS128_PREF_PT "Usada por \xA3ltimo\t[Last]\n"
};

static const char *MENU_ROMS2A_3_PREF[NLANGS] = {
    "Select ROM\n" MENU_ROMS2A_3_PREF_EN "Last used\t[Last]\n",
    "Elija ROM\n" MENU_ROMS2A_3_PREF_ES "Ultima usada\t[Last]\n",
    "Escolha ROM\n" MENU_ROMS2A_3_PREF_PT "Usada por \xA3ltimo\t[Last]\n"
};

static const char *MENU_ROM_PREF_TK95[NLANGS] = {
    "Select ROM\n" MENU_ROMSTK95_PREF_EN"Last used\t[Last ]\n",
    "Elija ROM\n" MENU_ROMSTK95_PREF_ES "Ultima usada\t[Last ]\n",
    "Escolha ROM\n" MENU_ROMSTK95_PREF_PT "Usada por \xA3ltimo\t[Last ]\n"
};

static const char *MENU_INTERFACE_LANG[3] = { MENU_INTERFACE_LANG_EN, MENU_INTERFACE_LANG_ES, MENU_INTERFACE_LANG_PT };

#define MENU_JOYS "Joystick 1\n"\
    "Joystick 2\n"

static const char *MENU_JOY[NLANGS] = { MENU_JOY_EN MENU_JOYS, MENU_JOY_ES MENU_JOYS, MENU_JOY_PT MENU_JOYS };

#define MENU_DEFJOY_TITLE "Joystick#\n"\

#define MENU_DEFJOYS "Cursor\t[0]\n"\
    "Kempston\t[1]\n"\
    "Sinclair 1\t[2]\n"\
    "Sinclair 2\t[3]\n"\
    "Fuller\t[4]\n"

static const char *MENU_DEFJOY[NLANGS] = { MENU_DEFJOY_TITLE MENU_DEFJOYS MENU_DEFJOY_EN, MENU_DEFJOY_TITLE MENU_DEFJOYS MENU_DEFJOY_ES, MENU_DEFJOY_TITLE MENU_DEFJOYS MENU_DEFJOY_PT };

static const char *MENU_JOYPS2[NLANGS] = { MENU_JOYPS2_EN, MENU_JOYPS2_ES, MENU_JOYPS2_PT };

static const char *MENU_PS2JOYTYPE[NLANGS] = { "Joy type\n" MENU_DEFJOYS, "Tipo joystick\n" MENU_DEFJOYS, "Tipo joystick\n" MENU_DEFJOYS };

static const char *MENU_CURSORJOY[NLANGS] = { "Cursor as Joy\n" , "Joy en Cursor\n" , "Joy no Cursor\n" };

static const char *MENU_TABASFIRE[NLANGS] = { "TAB as fire 1\n" , "TAB disparo 1\n" , "TAB fire 1\n" };

static const char *DLG_TITLE_INPUTPOK[NLANGS] = { DLG_TITLE_INPUTPOK_EN, DLG_TITLE_INPUTPOK_ES, DLG_TITLE_INPUTPOK_PT };

static const char *POKE_BANK_MENU[NLANGS] = { " Bank  \n" , " Banco \n" , " Banco \n" };

static const char *MENU_MOUSE[NLANGS] = { MENU_MOUSE_EN, MENU_MOUSE_ES, MENU_MOUSE_PT};

static const char *MENU_SOUND[NLANGS] = { MENU_SOUND_EN, MENU_SOUND_ES, MENU_SOUND_PT};

static const char *MENU_UI[NLANGS] = { MENU_UI_EN, MENU_UI_ES, MENU_UI_PT };

static const char *MENU_UI_OPT[NLANGS] = {"Option \n","Opci\xA2n \n","Opo \n" };

#define MENU_UI_TEXT_SCROLL "Normal\t[N]\n"\
    "Ping-Pong\t[P]\n"

#define MENU_MOUSE_SAMPLE_RATE_ITEMS "10\t[10]\n"\
    "20\t[20]\n"\
    "40\t[40]\n"\
    "60\t[60]\n"\
    "80\t[80]\n"\
    "100\t[100]\n"\
    "200\t[200]\n"

#define MENU_MOUSE_DPI_ITEMS "25 dpi\t[0]\n"\
    "50 dpi\t[1]\n"\
    "100 dpi\t[2]\n"\
    "200 dpi\t[3]\n"

#define MENU_MOUSE_SCALING_ITEMS "1:1\t[1]\n"\
    "1:2\t[2]\n"

static const char *MENU_MOUSE_SAMPLE_RATE[NLANGS] = { "Sample Rate\n" MENU_MOUSE_SAMPLE_RATE_ITEMS, "Tasa de muestreo\n" MENU_MOUSE_SAMPLE_RATE_ITEMS, "Taxa de amostragem\n" MENU_MOUSE_SAMPLE_RATE_ITEMS };

static const char *MENU_MOUSE_DPI[NLANGS] = { "Resolution\n" MENU_MOUSE_DPI_ITEMS, "Resoluci\xA2n\n" MENU_MOUSE_DPI_ITEMS, "Resolu\x87\x84o\n" MENU_MOUSE_DPI_ITEMS };

static const char *MENU_MOUSE_SCALING[NLANGS] = { "Scaling\n" MENU_MOUSE_SCALING_ITEMS, "Escalado\n" MENU_MOUSE_SCALING_ITEMS, "Escala\n" MENU_MOUSE_SCALING_ITEMS };

static const char *AboutMsg[NLANGS][5] = {
	// English
	{
	"\nF1(C)2024 Juan Jos\x82 Ponteprino \"SplinterGU\"\r"
	"\r"
	"\nA1ESP\nF1eccy is a fork of \nA1ESP\nF1ectrum.\r"
	"\r"
	"\r"
	"\n71(C)2023-2024 V\xA1" "ctor Iborra \"Eremus\"\r"
	"(C)2023      David Crespo  \"dcrespo3d\""
	,
	"\n21Based on ZX-ESPectrum-Wiimote\r"
	"(C)2020-2023 David Crespo\r"
	"\r"
	"\n31Inspired by previous projects\r"
	"from Pete Todd and Rampa & Queru\r"
	"\r"
	"\n41Z80 emulation by J.L. S\xA0nchez\r"
	"\n51VGA driver by BitLuni\r"
	"\n61AY-3-8912 library by A. Sashnov\r"
	"\n21PS2 driver by Fabrizio di Vittorio"
	,
	"\n71Collaborators:\r"
	"\r"
	"\n21ackerman            \n71Code & ideas\r"
	"\n31Armand              \n71Testing & broadcasting\r"
	"\n41azesmbog            \n71Testing & ideas\r"
	"\n51Carlo Brini         \n71Our UK guy ;)\r"
	"\n61David Carri\xA2n       \n71H/W code, ZX kbd\r"
	"\n21Rodolfo Guerra      \n71Our LATAM guy ;)\r"
	"\n31Ram\xA2n Mart\xA1nez      \n71AY emul. improvements\r"
	"\n41Ron                 \n71Testing & broadcasting\r"
	"\n51J.L. S\xA0nchez        \n71Z80 core improvements\r"
	"\n61Antonio Villena     \n71Hardware support\r"
	"\n21ZjoyKiLer           \n71Testing & ideas"
 	,
	"\n71Thanks for help and donations to:\r"
	"\r"
	"\n21Abel Bayon @Baycorps    \n71Amstrad Eterno\r"
	"\n31Pablo Forcen Soler      \n71AUA\r"
	"\n41Jordi Ramos Montes\r"
	"\n51Tsvetan Usunov          \n71Olimex Ltd.\r"
	"\r"
	"\n71ZX81+ ROM included courtesy of:\r"
	"\r"
	"\n21Paul Farrow"
	,
	"\n71Thanks also to:\r"
	"\r"
	"\n21Retrowiki.es \n71and its great community\r"
	"\n31Ron \n71for his cool RetroCrypta\r"
	"\n41Viejoven FX, \n51J.Ortiz \"El Spectrumero\"\r"
	"\n61J.C. Gonz\xA0lez Amestoy \n71for RVM\r"
	"\n21VidaExtraRetro, \n31C\x82sar Nicol\xA0s-Gonz\xA0lez\r"
	"\n41Rodolfo Guerra, \n51All creators in\r"
	"ZX Spectrum server at Discord\r"
	"\r"
	"\n71and, of course, to:\r"
	"\r"
	"\n51Sir Clive Sinclair \n71& \n21M\n61a\n41t\n51t\n31h\n21e\n61w \n41S\n51m\n31i\n21t\n61h"
	},
	// Español
	{
	"\nF1(C)2024 Juan Jos\x82 Ponteprino \"SplinterGU\"\r"
	"\r"
	"\nA1ESP\nF1eccy es un fork de \nA1ESP\nF1ectrum.\r"
	"\r"
	"\r"
	"\n71(C)2023-2024 V\xA1" "ctor Iborra \"Eremus\"\r"
	"(C)2023      David Crespo  \"dcrespo3d\""
	,
	"\n21Basado en ZX-ESPectrum-Wiimote\r"
	"(C)2020-2023 David Crespo\r"
	"\r"
	"\n31Inspirado en proyectos anteriores\r"
	"de Pete Todd y Rampa & Queru\r"
	"\r"
	"\n41Emulaci\xA2n Z80 por J.L. S\xA0nchez\r"
	"\n51Driver VGA por BitLuni\r"
	"\n61Librer\xA1" "a AY-3-8912 por A. Sashnov\r"
	"\n21Driver PS2 por Fabrizio di Vittorio"
	,
	"\n71Colaboradores:\r"
	"\r"
	"\n21ackerman            \n71C\xA2" "digo e ideas\r"
	"\n31Armand              \n71Testing y difusi\xA2n\r"
	"\n41azesmbog            \n71Testing e ideas\r"
	"\n51Carlo Brini         \n71ESPectrum en UK ;)\r"
	"\n61David Carri\xA2n       \n71C\xA2" "digo h/w, teclado ZX\r"
	"\n21Rodolfo Guerra      \n71ESPectrum en LATAM ;)\r"
	"\n31Ram\xA2n Mart\xA1nez      \n71Mejoras emulaci\xA2n AY\r"
	"\n41Ron                 \n71Testing y difusi\xA2n\r"
	"\n51J.L. S\xA0nchez        \n71Mejoras core Z80\r"
	"\n61Antonio Villena     \n71Soporte hardware\r"
	"\n21ZjoyKiLer           \n71Testing e ideas"
	,
	"\n71Gracias por su ayuda y donaciones a:\r"
	"\r"
	"\n21Abel Bayon @Baycorps    \n71Amstrad Eterno\r"
	"\n31Pablo Forcen Soler      \n71AUA\r"
	"\n41Jordi Ramos Montes\r"
	"\n51Tsvetan Usunov          \n71Olimex Ltd.\r"
	"\r"
	"\n71ZX81+ ROM incluida por cortes\xA1" "a de:\r"
	"\r"
	"\n21Paul Farrow"\
	,
	"\n71Gracias tambi\x82n a:\r"
	"\r"
	"\n21Retrowiki.es \n71y su magn\xA1" "fica comunidad\r"
	"\n31Ron \n71por su genial RetroCrypta\r"
	"\n41Viejoven FX, \n51J.Ortiz \"El Spectrumero\"\r"
	"\n61J.C. Gonz\xA0lez Amestoy \n71por RVM\r"
	"\n21VidaExtraRetro, \n31C\x82sar Nicol\xA0s-Gonz\xA0lez\r"
	"\n41Rodolfo Guerra, \n51Todos los creadores en\r"
	"el servidor ZXSpectrum en Discord\r"
	"\r"
	"\n71y, por supuesto, a:\r"
	"\r"
	"\n51Sir Clive Sinclair \n71& \n21M\n61a\n41t\n51t\n31h\n21e\n61w \n41S\n51m\n31i\n21t\n61h"
	},
	// Portugues
	{
	"\nF1(C)2024 Juan Jos\x82 Ponteprino \"SplinterGU\"\r"
	"\r" \
	"\nA1ESP\nF1eccy \x82 um fork do \nA1ESP\nF1ectrum.\r"
	"\r"
	"\r"
	"\n71(C)2023-2024 V\xA1" "ctor Iborra \"Eremus\"\r"
	"(C)2023      David Crespo  \"dcrespo3d\""
	,
	"\n21Baseado no ZX-ESPectrum-Wiimote\r"
	"(C)2020-2023 David Crespo\r"\
	"\r"
	"\n31Inspirado em projetos anteriores\r"
	"de Pete Todd e Rampa & Queru\r"
	"\r"
	"\n41Emula\x87\x84o Z80 por J.L. S\xA0nchez\r"
	"\n51Driver VGA por BitLuni\r"
	"\n61Livraria AY-3-8912 por A. Sashnov\r"
	"\n21Driver PS2 por Fabrizio di Vittorio"
	,
	"\n71Colaboradores:\r"
	"\r"
	"\n21ackerman            \n71C\xA2" "digo e ideias\r"
	"\n31Armand              \n71Teste e difus\x84o\r"
	"\n41azesmbog            \n71Teste e ideias\r"
	"\n51Carlo Brini         \n71ESPectrum em UK ;)\r"
	"\n61David Carri\xA2n       \n71C\xA2" "digo h/w, teclado ZX\r"
	"\n21Rodolfo Guerra      \n71ESPectrum na LATAM ;)\r"
	"\n31Ram\xA2n Mart\xA1nez      \n71Melhorias na emula\x87\x84o AY\r"
	"\n41Ron                 \n71Teste e difus\x84o\r"
	"\n51J.L. S\xA0nchez        \n71Melhorias no core Z80\r"
	"\n61Antonio Villena     \n71Suporte do hardware\r"
	"\n21ZjoyKiLer           \n71Teste e ideias"
	,
	"\n71Obrigado pela sua ajuda e doa\x87\x94" "es a:\r"
	"\r"
	"\n21Abel Bayon @Baycorps    \n71Amstrad Eterno\r"
	"\n31Pablo Forcen Soler      \n71AUA\r"
	"\n41Jordi Ramos Montes\r"
	"\n51Tsvetan Usunov          \n71Olimex Ltd.\r"
	"\r"
	"\n71ZX81+ ROM inclu\xA1" "da como cortesia de:\r"
	"\r"
	"\n21Paul Farrow"
	,
	"\n71Obrigado tamb\x82m a:\r"
	"\r"
	"\n21Retrowiki.es \n71e sua magn\xA1" "fica comunidade\r"
	"\n31Ron \n71por sua brilhante RetroCrypta\r"
	"\n41Viejoven FX, \n51J.Ortiz \"El Spectrumero\"\r"
	"\n61J.C. Gonz\xA0lez Amestoy \n71por RVM\r"
	"\n21VidaExtraRetro, \n31C\x82sar Nicol\xA0s-Gonz\xA0lez\r"
	"\n41Rodolfo Guerra, \n51Todos os criadores no\r"
	"servidor ZXSpectrum no Discord\r"
	"\r"
	"\n71e, \x82 claro, a:\r"
	"\r"
	"\n51Sir Clive Sinclair \n71& \n21M\n61a\n41t\n51t\n31h\n21e\n61w \n41S\n51m\n31i\n21t\n61h"
	}
 };

static const char *StartMsg[NLANGS] = {
	// English
    "Hi! Thanks for choosing \nA1ESP\nF1eccy!\r"
	"(https://github.com/SplinterGU/ESPeccy)\r"
    "\r"
    "\nA1ESP\nF1eccy is open source software licensed\r"
    "under GPL v3. You can use, modify,\r"
    "and share it for free.\r"
    "\r"
	"Powered by \nA1ESP\nF1ectrum\r"\
    "(https://zxespectrum.speccy.org/)\r"
	,
	// Spanish
	"\xAD" "Hola! \xADGracias por elegir \nA1ESP\nF1eccy!\r"
	"(https://github.com/SplinterGU/ESPeccy)\r"
	"\r"
	"\nA1ESP\nF1eccy es software de c\xA2" "digo abierto bajo\r"
	"licencia GPL v3, puedes usarlo modificarlo\r"
	"y compartirlo gratis.\r"
    "\r"
	"Powered by \nA1ESP\nF1ectrum\r"\
    "(https://zxespectrum.speccy.org/)\r"
	,
	// Portugues
	"Oi! Obrigado por escolher \nA1ESP\nF1eccy!\r"
	"(https://github.com/SplinterGU/ESPeccy)\r"
	"\r"
	"\nA1ESP\nF1eccy \x82 um software de c" "\xA2" "digo aberto sob\r"
	"licen\x87" "a GPL v3, voc\x88 pode us\xa0-lo,\r"
	"modific\xa0-lo e compartilh\xa0-lo gr\xa0tis.\r"
    "\r"
	"Powered by \nA1ESP\nF1ectrum\r"\
    "(https://zxespectrum.speccy.org/)\r"
 };

static const char *STARTMSG_CLOSE[NLANGS] = { "This message will close in %02ds",
											  "Este mensaje se cerrar\xA0 en %02ds",
											  "Esta mensagem ser\xA0 fechada em %02ds"
											};

#include "images.h"

#endif // ESPECTRUM_MESSAGES_h

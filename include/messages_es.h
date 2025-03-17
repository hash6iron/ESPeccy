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

#ifndef ESPECCY_MESSAGES_ES_h
#define ESPECCY_MESSAGES_ES_h

#define ERR_FS_EXT_FAIL_ES "\xAD" "Almacenamiento externo no disponible!"

#define OSD_MSGDIALOG_YES_ES "  S\xA1  "
#define OSD_MSGDIALOG_NO_ES "  No  "

#define OSD_PAUSE_ES "--=[EN PAUSA]=--"

#define POKE_ERR_ADDR1_ES "Direcci\xA2n debe estar entre 16384 y 65535"
#define POKE_ERR_ADDR2_ES "Direcci\xA2n debe ser menor que 16384"
#define POKE_ERR_VALUE_ES "Valor debe ser menor que 256"

#define OSD_INVALIDCHAR_ES "Car\xA0cter no v\xA0lido"

#define OSD_TAPE_SAVE_ES "Comando SAVE"

#define OSD_TAPE_SAVE_EXIST_ES "El fichero ya existe \xA8Sobreescribir?"

#define OSD_PSNA_SAVE_ES "Guardar snapshot"

#define OSD_PSNA_EXISTS_ES "\xA8Sobreescribir ranura?"

#define OSD_TAPE_SELECT_ERR_ES "Fichero de cinta no seleccionado"

#define OSD_FILE_INDEXING_ES "Indexando"
#define OSD_FILE_INDEXING_ES_1 "   Ordenando   "
#define OSD_FILE_INDEXING_ES_2 "Grabando \xA1ndice"
#define OSD_FILE_INDEXING_ES_3 "   Limpiando   "

#define OSD_FIRMW_UPDATE_ES "Actualizar firmware"

#define OSD_DLG_SURE_ES "\xA8" "Desea continuar?"

#define OSD_DLG_JOYSAVE_ES "\xA8Guardar cambios?"

#define OSD_DLG_JOYDISCARD_ES "\xA8" "Descartar cambios?"

#define OSD_DLG_SETJOYMAPDEFAULTS_ES "\xA8" "Cargar mapeo por defecto?"

#define OSD_FIRMW_ES "Actualizando firmware"

#define OSD_FIRMW_BEGIN_ES "Borrando partici\xA2n de destino."

#define OSD_FIRMW_WRITE_ES "   Grabando nuevo firmware.   "

#define OSD_FIRMW_END_ES "  Completado. Reiniciando.   "

#define OSD_NOFIRMW_ERR_ES "Firmware no encontrado."

#define OSD_FIRMW_ERR_ES "Error actualizando firmware."

#define MENU_ROM_TITLE_ES "Elija ROM"

#define OSD_ROM_ERR_ES "Error flasheando ROM."

#define OSD_NOROMFILE_ERR_ES "Custom ROM no encontrada."

#define OSD_ROM_ES "Flashear ROM Custom"

#define OSD_ROM_BEGIN_ES "Preparando espacio en flash."

#define OSD_ROM_WRITE_ES "    Grabando ROM custom.    "

#define MENU_SNA_TITLE_ES "Elija snapshot"

#define MENU_SAVE_SNA_TITLE_ES "Guardar snapshot"

#define MENU_TAP_TITLE_ES "Elija cinta"

#define MENU_DSK_TITLE_ES "Elija disco"

#define MENU_CHT_TITLE_ES "Elija fichero de cheats"

#define MENU_SCR_TITLE_ES "Elija fichero .SCR"

#define MENU_DELETE_TAP_BLOCKS_ES "Borrar selecci\xA2n"

#define MENU_DELETE_CURRENT_TAP_BLOCK_ES "Borrar bloque"

#define OSD_BLOCK_SELECT_ERR_ES "Ning\xA3n bloque seleccionado"

#define OSD_BLOCK_TYPE_ERR_ES "Tipo de bloque inv\xA0lido"

#define MENU_DELETE_CURRENT_FILE_ES "Borrar archivo"

#define OSD_READONLY_FILE_WARN_ES "Archivo de solo lectura"

#define OSD_TAPE_FLASHLOAD_ES "Carga r\xA0pida de cinta"
#define OSD_TAPE_INSERT_ES "Cinta insertada"
#define OSD_TAPE_EJECT_ES "Cinta expulsada"

#define OSD_ROM_EJECT_ES "Cartucho expulsado"
#define OSD_ROM_INSERT_ERR_ES "Cartucho no insertado"

#define MENU_DELETE_SNA_ES "Borrar snapshot"

#define TRDOS_RESET_ERR_ES "Error en reset a TR-DOS. Active Betadisk."

#define MENU_SNA_ES \
    "Snapshots\n"\
    "Cargar\t>\n"\
    "Guardar\t>\n"\
    "Cargar en slot\t>\n"\
    "Guardar en slot\t>\n"

#define MENU_TAPE_ES \
    "Cintas\n"\
    "Elegir (TAP,TZX)\t>\n"\
    "Play/Stop\n"\
    "Expulsar cinta\n"\
    "Navegador cinta\t>\n"\
	"Monitor LOAD\t>\n"\
    "Cinta real\t>\n"

#define MENU_REALTAPE_ES "Carga de cinta real\n"\
    "Auto\t[0]\n"\
    "Forzar LOAD\t[1]\n"\
    "Forzar SAVE\t[2]\n"

#define MENU_BETADISK_ES \
    "Unidades\n"\
    "Unidad A\t>\n"\
    "Unidad B\t>\n"\
    "Unidad C\t>\n"\
    "Unidad D\t>\n"

#define MENU_BETADRIVE_ES \
    "Unidad#\n"\
    "Insertar disco\t>\n"\
    "Expulsar disco\n"

#define MENU_ROM_CART_ES \
    "Cartuchos\n"\
    "Insertar\t>\n"\
    "Expulsar\n"

#define MENU_MAIN_ES \
    "Snapshots\t>\n"\
    "Cintas\t>\n"\
    "Betadisk\t>\n"\
    "Cartuchos\t>\n"\
    "Modelo\t>\n"\
    "Resetear\t>\n"\
    "Opciones\t>\n"\
    "Actualizar\t>\n"\
    "Ayuda de teclado\t \n"\
    "Acerca de\n"

#define MENU_OPTIONS_ES \
    "Opciones\n"\
    "Almacenamiento\t>\n"\
    "Modelo preferido\t>\n"\
    "ROM preferida\t>\n"\
    "Joystick\t>\n"\
    "Emulaci\xA2n joystick\t>\n"\
    "Pantalla\t>\n"\
    "Rat\xA2n\t>\n"\
    "Sonido\t>\n"\
    "Otros\t>\n"\
    "Idioma\t>\n"\
    "Interfaz de Usuario\t>\n"\
    "EAR config\t>\n"\
    "Cinta\t>\n"    

#define MENU_UPDATE_ES \
    "Actualizar\n"\
    "Firmware\n"\
    "ROM Custom 48K\n"\
    "ROM Custom 128k\n"\
    "ROM Custom +2A\n"\
    "ROM Custom TK\n"

#define MENU_VIDEO_ES \
    "Pantalla\n"\
    "Tipo render\t>\n"\
    "Resoluci\xA2n\t>\n"\
    "Scanlines\t>\n"

#define MENU_RENDER_ES \
    "Tipo render\n"\
    "Est\xA0ndar\t[S]\n"\
    "Efecto nieve\t[A]\n"

#define MENU_ASPECT_ES \
    "Resoluci\xA2n\n"\
    "320x240 (4:3)\t[4]\n"\
    "360x200 (16:9)\t[1]\n"

#define MENU_RESET_ES \
    "Resetear\n"\
    "Reset parcial\n"\
    "Reset a TR-DOS \t(CF11)\n"\
    "Reset completo\t(F11)\n"\
    "Resetear ESP32\t(F12)\n"

#define MENU_PERSIST_SAVE_ES \
    "Guardar snapshot\n"

#define MENU_PERSIST_LOAD_ES \
    "Cargar snapshot\n"

#define MENU_STORAGE_ES "Almacenamiento\n"\
    "Betadisk\t>\n"\
    "Timings ROM R.G.\t>\n"

#define MENU_CAS_ES "Cintas\n"\
    "Auto carga de cinta\t>\n"\
    "Carga r\xA0pida cinta\t>\n"

#define MENU_YESNO_ES "S\xA1\t[Y]\n"\
    "No\t[N]\n"

#define MENU_MOUSE_ES "Rat\xA2n\n"\
    "Tasa de muestreo\t>\n"\
    "Resoluci\xA2n\t>\n"\
    "Escalado\t>\n"

#define MENU_SOUND_ES "Sonido\n"\
    "AY en 48K\t>\n"\
    "Covox\t>\n"

#define MENU_OTHER_ES "Otros\n"\
    "Timing ULA\t>\n"\
    "48K Issue 2\t>\n"\
    "ULA TK\t>\n"\
    "Segundo disp. PS/2\t>\n"

#define MENU_KBD2NDPS2_ES "Dispositivo\n"\
    "Nada\t[N]\n"\
    "Teclado / Adapt. DB9\t[K]\n"\
    "Rat\xA2n\t[M]\n"

#define MENU_ALUTIMING_ES "Timing ULA\n"\
    "Early\t[E]\n"\
    "Late\t[L]\n"

#define MENU_ARCH_ES "Elija modelo\n"

#define MENU_ROMS48_ES "Elija ROM\n"\
    "48K\n"\
    "48K Espa\xA4ol\n"\
    "Custom\n"

#define MENU_ROMS128_ES "Elija ROM\n"\
    "128K\n"\
    "128K Espa\xA4ol\n"\
    "+2\n"\
    "+2 Espa\xA4ol\n"\
    "+2 Franc\x82s\n"\
    "ZX81+\n"\
    "Custom\n"

#define MENU_ROMS2A_3_ES "Elija ROM\n"\
    "v4.0 +2a/+3\n"\
    "v4.0 +2a/+3 Espa\xA4ol\n"\
    "v4.1 +2a/+3\n"\
    "v4.1 +2a/+3 Espa\xA4ol\n"\
    "Custom\n"

#define MENU_ROMSTK_ES "Elija ROM\n"\
    "v1 Espa\xA4ol\n"\
    "v1 Portugu\x82s\n"\
    "v2 Espa\xA4ol\n"\
    "v2 Portugu\x82s\n"\
    "v3 Espa\xA4ol (R.G.)\n"\
    "v3 Portugu\x82s (R.G.)\n"\
    "v3 Ingl\x82s (R.G.)\n"\
    "Custom\n"

#define MENU_ROMSTK95_ES "Elija ROM\n"\
    "Espa\xA4ol\n"\
    "Portugu\x82s\n"

#define MENU_ROMS48_PREF_ES	"48K\t[48K]\n"\
    "48K Espa\xA4ol\t[48Kes]\n"\
    "Custom\t[48Kcs]\n"

#define MENU_ROMSTK90X_PREF_ES "v1 Espa\xA4ol\t[v1es ]\n"\
    "v1 Portugu\x82s\t[v1pt ]\n"\
    "v2 Espa\xA4ol\t[v2es ]\n"\
    "v2 Portugu\x82s\t[v2pt ]\n"\
    "v3 Espa\xA4ol (R.G.)\t[v3es ]\n"\
    "v3 Portugu\x82s (R.G.)\t[v3pt ]\n"\
    "v3 Ingl\x82s (R.G.)\t[v3en ]\n"\
    "Custom\t[TKcs ]\n"

#define MENU_ROMSTK95_PREF_ES "Espa\xA4ol\t[95es ]\n"\
    "Portugu\x82s\t[95pt ]\n"

#define MENU_ROMS128_PREF_ES "128K\t[128K]\n"\
    "128K Espa\xA4ol\t[128Kes]\n"\
    "+2\t[+2]\n"\
    "+2 Espa\xA4ol\t[+2es]\n"\
    "+2 Franc\x82s\t[+2fr]\n"\
    "ZX81+\t[ZX81+]\n"\
    "Custom\t[128Kcs]\n"

#define MENU_ROMS2A_3_PREF_ES "v4.0 +2a/+3\t[+2A]\n"\
    "v4.0 +2a/+3 Espa\xA4ol\t[+2Aes]\n"\
    "v4.1 +2a/+3\t[+2A41]\n"\
    "v4.1 +2a/+3 Espa\xA4ol\t[+2A41es]\n"\
    "Custom\t[+2Acs]\n"

#define MENU_INTERFACE_LANG_ES "Idioma\n"\
    "Ingl\x82s\t[E]\n"\
    "Espa\xA4ol\t[S]\n"\
    "Portugu\x82s\t[P]\n"

#define MENU_JOY_ES "Joystick\n"

#define MENU_DEFJOY_ES "Definir\n"

#define MENU_JOYPS2_ES "Emulaci\xA2n Joystick\n" "Tipo joystick\t>\n" "Joy en teclas de cursor\t>\n" "TAB como disparo 1\t>\n"

#define DLG_TITLE_INPUTPOK_ES "A\xA4" "adir Poke"

#define MENU_UI_ES "Interfaz de Usuario\n"\
    "Men\xA3 con cursores\t>\n"\
    "Rotaci\xA2n de texto\t>\n"\
    "Miniaturas\t>\n"\
    "Vista instant\xA0nea\t>\n"

#endif // ESPECCY_MESSAGES_ES_h

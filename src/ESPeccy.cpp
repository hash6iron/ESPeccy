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

#include <stdio.h>
#include <string>
#include <functional>
#include <dirent.h>

#include "nvs_flash.h"
#include "nvs.h"
#include "nvs_handle.hpp"

#include "ESPeccy.h"
#include "Snapshot.h"
#include "Config.h"
#include "FileUtils.h"
#include "OSDMain.h"
#include "Ports.h"
#include "MemESP.h"
#include "cpuESP.h"
#include "Video.h"
#include "messages.h"
#include "AySound.h"
#include "Tape.h"
#include "Z80_JLS/z80.h"
#include "pwm_audio.h"
#include "fabgl.h"
#include "wd1793.h"

#include "ROMLoad.h"

#include "RealTape.h"

#include "ZXKeyb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/timer.h"
#include "soc/timer_group_struct.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_efuse.h"
#include "soc/efuse_reg.h"
#include "esp_ota_ops.h"

#include "CommitDate.h"
#include "Keymap.h"

//=======================================================================================
// KEYBOARD
//=======================================================================================
fabgl::PS2Controller ESPeccy::PS2Controller;
bool ESPeccy::ps2kbd = false;
bool ESPeccy::ps2kbd2 = false;
bool ESPeccy::ps2mouse = false;

//=======================================================================================
// AUDIO
//=======================================================================================
uint8_t ESPeccy::audioBuffer[ESP_AUDIO_SAMPLES_PENTAGON] = { 0 };
uint8_t ESPeccy::SamplebufCOVOX[ESP_AUDIO_SAMPLES_PENTAGON] = { 0 };
uint32_t ESPeccy::audbufcntCOVOX = 0;
uint32_t ESPeccy::faudbufcntCOVOX = 0;
uint8_t ESPeccy::covoxData[4];
uint16_t ESPeccy::covoxMix;
uint16_t ESPeccy::fcovoxMix;
uint32_t* ESPeccy::overSamplebuf;
signed char ESPeccy::aud_volume = ESP_VOLUME_DEFAULT;
uint32_t ESPeccy::audbufcnt = 0;
uint32_t ESPeccy::audbufcntover = 0;
// uint32_t ESPeccy::faudbufcnt = 0;
uint32_t ESPeccy::faudbufcntover = 0;
uint32_t ESPeccy::audbufcntAY = 0;
uint32_t ESPeccy::faudbufcntAY = 0;
int ESPeccy::lastaudioBit = 0;
int ESPeccy::flastaudioBit = 0;
static int faudioBitBuf = 0;
static unsigned char faudioBitbufCount = 0;
//int ESPeccy::faudioBit = 0;
int ESPeccy::samplesPerFrame;
bool ESPeccy::AY_emu = false;
int ESPeccy::Audio_freq[4];
unsigned char ESPeccy::audioSampleDivider;
unsigned char ESPeccy::audioAYDivider;
unsigned char ESPeccy::audioCOVOXDivider;
unsigned char ESPeccy::audioOverSampleDivider;
int ESPeccy::audioBitBuf = 0;
unsigned char ESPeccy::audioBitbufCount = 0;
QueueHandle_t audioTaskQueue;
TaskHandle_t ESPeccy::audioTaskHandle;
uint8_t param;

int runBios = 0;

uint8_t ESPeccy::aud_active_sources = 0;

bool booting = false;

//=======================================================================================
// TAPE OSD
//=======================================================================================

int ESPeccy::TapeNameScroller = 0;

//=======================================================================================
// BETADISK
//=======================================================================================

bool ESPeccy::trdos = false;
WD1793 ESPeccy::Betadisk;

//=======================================================================================
// RealTape
//=======================================================================================

RealTapeParams ESP_RT_PARMS;
bool ESPeccy::sync_realtape = false;

//=======================================================================================
// ARDUINO FUNCTIONS
//=======================================================================================

#define NOP() asm volatile ("nop")

IRAM_ATTR unsigned long millis()
{
    return (unsigned long) (esp_timer_get_time() / 1000ULL);
}

IRAM_ATTR void delayMicroseconds(int64_t us)
{
    int64_t m = esp_timer_get_time();
    if(us){
        int64_t e = (m + us);
        if(m > e){ //overflow
            while(esp_timer_get_time() > e){
                NOP();
            }
        }
        while(esp_timer_get_time() < e){
            NOP();
        }
    }
}

//=======================================================================================
// TIMING / SYNC
//=======================================================================================

double ESPeccy::totalseconds = 0;
double ESPeccy::totalsecondsnodelay = 0;
int64_t ESPeccy::target[4];
int ESPeccy::sync_cnt = 0;
volatile bool ESPeccy::vsync = false;
int64_t ESPeccy::ts_start;
int64_t ESPeccy::elapsed;
int64_t ESPeccy::idle;
uint8_t ESPeccy::ESP_delay = 1; // EMULATION SPEED: 0-> MAX. SPEED (NO SOUND), 1-> 100% SPEED, 2-> 125% SPEED, 3-> 150% SPEED
int ESPeccy::ESPoffset = 0;

//=======================================================================================
// LOGGING / TESTING
//=======================================================================================

int ESPeccy::ESPtestvar = 0;
int ESPeccy::ESPtestvar1 = 0;
int ESPeccy::ESPtestvar2 = 0;

#define START_MSG_DURATION 20

void ShowStartMsg() {

    VIDEO::vga.clear(zxColor(7,0));

    OSD::drawOSD(false);

    VIDEO::vga.fillRect(OSD::osdInsideX(), OSD::osdInsideY(), OSD_COLS * OSD_FONT_W, 50, zxColor(0, 0));

    // Decode Logo in EBF8 format
    int logo_w = (ESPeccy_logo[5] << 8) + ESPeccy_logo[4]; // Get Width
    int logo_h = (ESPeccy_logo[7] << 8) + ESPeccy_logo[6]; // Get Height
    int pos_x = OSD::osdInsideX() + ( OSD_COLS * OSD_FONT_W - logo_w ) / 2;
    int pos_y = OSD::osdInsideY() + ( 50 - logo_h ) / 2;

    OSD::drawCompressedBMP(pos_x, pos_y, ESPeccy_logo);

    OSD::osdAt(7, 1);
    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(1, 0));

    char nextChar;
    const char *text = StartMsg[Config::lang];

    int osdCol = 0, osdRow = 0;

    for ( int i = 0; i < strlen(text); ++i ) {
        nextChar = text[i];
        if (nextChar != 13) {
            if (nextChar == 10) {
                char fore = text[++i];
                char back = text[++i];
                int foreint = (fore >= 'A') ? (fore - 'A' + 10) : (fore - '0');
                int backint = (back >= 'A') ? (back - 'A' + 10) : (back - '0');
                VIDEO::vga.setTextColor(zxColor(foreint & 0x7, foreint >> 3), zxColor(backint & 0x7, backint >> 3));
                continue;
            }
            VIDEO::vga.print((const char)nextChar);
        } else {
            VIDEO::vga.print("\n");
        }
    }

    char msg[38];
    bool quit = false;
    for (int i=START_MSG_DURATION; i >= 0 && !quit; i--) {
        OSD::osdAt(20, 1);
        sprintf(msg,STARTMSG_CLOSE[Config::lang],i);
        VIDEO::vga.setTextColor(zxColor(7, 0), zxColor(1, 0));
        VIDEO::vga.print(msg);

        for (int j = 0; j < 200; j++) {
            if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead(KBDREAD_MODEDIALOG);

            ESPeccy::readKbdJoy();

            if (ESPeccy::PS2Controller.keyboard()->virtualKeyAvailable()) {
                fabgl::VirtualKeyItem Nextkey;
                ESPeccy::readKbd(&Nextkey, KBDREAD_MODEDIALOG);
                if(!Nextkey.down) continue;
                if (Nextkey.vk == fabgl::VK_ESCAPE || Nextkey.vk == fabgl::VK_RETURN || Nextkey.vk == fabgl::VK_JOY1A || Nextkey.vk == fabgl::VK_JOY1B || Nextkey.vk == fabgl::VK_JOY2A || Nextkey.vk == fabgl::VK_JOY2B) {
                    quit = true;
                    break;
                }
            }
            vTaskDelay(5 / portTICK_PERIOD_MS);
        }
    }

    VIDEO::vga.clear(zxColor(7,0));

    // Disable StartMsg
    Config::StartMsg = false;

    // Save all keys after new flash or update
    Config::save();

}

void ESPeccy::showMemInfo(const char* caption) {

    string textout;

    multi_heap_info_t info;

    if (caption) printf("=== %s ===\n", caption);

    heap_caps_get_info(&info, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); // internal RAM, memory capable to store data or to create new task
    textout = " Total free bytes         : " + to_string(info.total_free_bytes) + "\n";
    printf(textout.c_str());

    textout = " Minimum free ever        : " + to_string(info.minimum_free_bytes) + "\n";
    printf(textout.c_str());

    printf("CAPS\n", __FUNCTION__);
    printf("    MALLOC_CAP_8BIT      free:%9lu min:%9lu largest free:%9lu\n", heap_caps_get_free_size(MALLOC_CAP_8BIT), heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    printf("    MALLOC_CAP_DMA       free:%9lu min:%9lu largest free:%9lu\n", heap_caps_get_free_size(MALLOC_CAP_DMA), heap_caps_get_minimum_free_size(MALLOC_CAP_DMA), heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
    printf("    MALLOC_CAP_SPIRAM    free:%9lu min:%9lu largest free:%9lu\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM), heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM), heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
    printf("    MALLOC_CAP_INTERNAL  free:%9lu min:%9lu largest free:%9lu\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL), heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL), heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    printf("    MALLOC_CAP_DEFAULT   free:%9lu min:%9lu largest free:%9lu\n", heap_caps_get_free_size(MALLOC_CAP_DEFAULT), heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT), heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
    printf("    MALLOC_CAP_IRAM_8BIT free:%9lu min:%9lu largest free:%9lu\n", heap_caps_get_free_size(MALLOC_CAP_IRAM_8BIT), heap_caps_get_minimum_free_size(MALLOC_CAP_IRAM_8BIT), heap_caps_get_largest_free_block(MALLOC_CAP_IRAM_8BIT));
    printf("    MALLOC_CAP_RETENTION free:%9lu min:%9lu largest free:%9lu\n", heap_caps_get_free_size(MALLOC_CAP_RETENTION), heap_caps_get_minimum_free_size(MALLOC_CAP_RETENTION), heap_caps_get_largest_free_block(MALLOC_CAP_RETENTION));

}

string ESPeccy::getHardwareInfo() {
    // Get chip information
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    // Chip models for ESP32
    string textout = " Chip model    : ";
    uint32_t chip_ver = esp_efuse_get_pkg_ver();
    uint32_t pkg_ver = chip_ver & 0x7;
    switch (pkg_ver) {
        case EFUSE_RD_CHIP_VER_PKG_ESP32D0WDQ6 :
            textout += (chip_info.revision == 3) ? "ESP32-D0WDQ6-V3" : "ESP32-D0WDQ6";
            break;
        case EFUSE_RD_CHIP_VER_PKG_ESP32D0WDQ5 :
            textout += (chip_info.revision == 3) ? "ESP32-D0WD-V3" : "ESP32-D0WD";
            break;
        case EFUSE_RD_CHIP_VER_PKG_ESP32D2WDQ5 :
            textout += "ESP32-D2WD";
            break;
        case EFUSE_RD_CHIP_VER_PKG_ESP32PICOD2 :
            textout += "ESP32-PICO-D2";
            break;
        case EFUSE_RD_CHIP_VER_PKG_ESP32PICOD4 :
            textout += "ESP32-PICO-D4";
            break;
        case EFUSE_RD_CHIP_VER_PKG_ESP32PICOV302 :
            textout += "ESP32-PICO-V3-02";
            break;
        case EFUSE_RD_CHIP_VER_PKG_ESP32D0WDR2V3 :
            textout += "ESP32-D0WDR2-V3";
            break;
        default:
            textout += "Unknown";
    }
    textout += "\n";

    // Continuar obteniendo información del hardware

    textout += " Chip cores    : " + to_string(chip_info.cores) + "\n";
    textout += " Chip revision : " + to_string(chip_info.revision) + "\n";
    textout += " Flash size    : " + to_string(spi_flash_get_chip_size() / (1024 * 1024)) + (chip_info.features & CHIP_FEATURE_EMB_FLASH ? "MB embedded" : "MB external") + "\n";
    multi_heap_info_t info; heap_caps_get_info(&info, MALLOC_CAP_SPIRAM);
    Config::psramsize = (info.total_free_bytes + info.total_allocated_bytes) >> 10;
    textout += " PSRAM size    : " + (Config::psramsize == 0 ? "N/A or disabled" : to_string(Config::psramsize) + " MB") + "\n";
    textout += " IDF Version   : " + (string)(esp_get_idf_version()) + "\n";

    return textout;
};

//=======================================================================================
// BOOT KEYBOARD
//=======================================================================================
void ESPeccy::bootKeyboard() {

    // printf("Boot kbd!\n");

    bool biosButton = false;

    for (int i = 0; i < 200; i++) {

        if (ZXKeyb::Exists) {
            // Process physical keyboard
            ZXKeyb::process();

            // Detect and process physical kbd menu key combinations
            if (ZXKBD_2) { // 2
                runBios = 1;

            } else
            if (ZXKBD_3) { // 3
                runBios = 3;
            }

        }
        else
        if (!gpio_get_level((gpio_num_t)GPIO_NUM_36)) {
            biosButton = true;
            break;
        }

	    if (ps2kbd) {
            auto Kbd = PS2Controller.keyboard();
            fabgl::VirtualKeyItem NextKey;

            while (Kbd->virtualKeyAvailable()) {

                bool r = Kbd->getNextVirtualKey(&NextKey);

                if (r && NextKey.down) {

                    // Check keyboard status
                    switch (NextKey.vk) {
                        case fabgl::VK_F2:
                        case fabgl::VK_DELETE:  // For Special BIOS key in Spectrum+ 3D model.
                        case fabgl::VK_2:
                            runBios = 1;
                            break;

                        case fabgl::VK_F3:
                        case fabgl::VK_3:
                            runBios = 3;
                            break;
                    }

                }
            }
        }

        if (runBios) break;

        delayMicroseconds(1000);

    }

    if (biosButton) {
        runBios = 1;
        /* continue reading for 11 seconds max */
        for (int i = 0; i < 11000 && !gpio_get_level((gpio_num_t)GPIO_NUM_36); i++) {
            if (i >= 10000) { /* 10 seconds, then CRT BIOS */
                runBios = 3;
                break;
            }
            delayMicroseconds(1000);
        }
    }


    if (runBios) {
        Config::videomode = runBios - 1;
        Config::aspect_16_9 = false;
        Config::scanlines = false;
    }

}

#ifndef FWBUFFSIZE
#define FWBUFFSIZE 512 /* 4096 */
#endif


void ESPeccy::showBIOS() {

    Config::load(); // Restore original config values

    auto Kbd = PS2Controller.keyboard();
    fabgl::VirtualKeyItem NextKey;

    VIDEO::vga.clear(zxColor(7, 0));

    int base_row = OSD_FONT_H * 4;
    int base_col = OSD_FONT_W * 4;
    int total_rows = OSD::scrH / OSD_FONT_H - 8;
    int total_cols = OSD::scrW / OSD_FONT_W - 8;

    #define PRINT_FILLED_CENTERED(text)  VIDEO::vga.print(string((total_cols - strlen(text)) / 2, ' ').c_str()); VIDEO::vga.print(text); VIDEO::vga.print(string(total_cols - strlen(text) - (total_cols - strlen(text)) / 2, ' ').c_str())
    #define PRINT_FILLED_ROW(text)  VIDEO::vga.print(text); VIDEO::vga.print(string(total_cols - strlen(text), ' ').c_str())
    #define PRINT_FILLED_ROW_ALIGN_RIGHT(text)  VIDEO::vga.print(string(total_cols - strlen(text), ' ').c_str()); VIDEO::vga.print(text)
    #define SET_CURSOR(col,row) VIDEO::vga.setCursor(base_col + (col) * OSD_FONT_W, base_row + (row) * OSD_FONT_H)

    // Opciones del menú
    const char* menuOptions[] = {"Main", "Video", "Keyboard", "Config", "Update", "Exit"};
    const int menuCount = sizeof(menuOptions)/sizeof(menuOptions[0]);

    const char* menuOptionsLily[] = {"Main", "Video", "Keyboard", "Others", "Config", "Update", "Exit"};
    const int menuCountLily = sizeof(menuOptionsLily)/sizeof(menuOptionsLily[0]);

    const char* menuVideo[] = {"Resolution", "Frequency", "Scanlines"};
    const int menuVideoCount = sizeof(menuVideo) / sizeof(menuVideo[0]);

    const char* menuKeyboard[] = {"ZXUnoPS2 (.ZXPure)","Enable custom layout", "Select custom layout"};
    const int menuKeyboardCount = sizeof(menuKeyboard) / sizeof(menuKeyboard[0]);

    const char* menuOthers[] = {"IO36 Button"};
    const int menuOthersCount = sizeof(menuOthers) / sizeof(menuOthers[0]);

    const char* menuConfig[] = {"Backup Settings", "Restore Settings", "Reset Settings"};
    const int menuConfigCount = sizeof(menuConfig) / sizeof(menuConfig[0]);

    // const char* menuKeyMap[] = {"Enable custom layout", "Select custom layout"};
    // const int menuKeyMapCount = sizeof(menuKeyMap) / sizeof(menuKeyMap[0]);

    const char* menuExit[] = {"Save Changes & Exit", "Discard Changes & Exit"};
    const int menuExitCount = sizeof(menuExit) / sizeof(menuExit[0]);

    const char* menuOptionsResolution[] = {"320x240 (4:3)", "360x200 (16:9)"};

    const char* menuOptionsFrequency[] = {"60Hz (VGA)", "50Hz (VGA)", "15kHz (CRT)"};

    const char* menuYesNo[] = {"No", "Yes"};
    //const int menuYesNoCount = sizeof(menuYesNo) / sizeof(menuYesNo[0]);
    
    const char* menuOthersIO36Button[] = {"RESET", "NMI", "CHEATS", "POKE", "STATS", "MENU"};

    const char* menuUpdate[] = {"Firmware Update"};
    const int menuUpdateCount = sizeof(menuUpdate) / sizeof(menuUpdate[0]);

    // Keymap menu variables
    const char* menuOptionsEnableKeyMap[] = {"No", "Yes"};
    string menuOptionsSelectKeyMap[5];
    uint8_t maxKeyMapFilesFound = 0;
    uint8_t itemskeymap = 0;

    //
    int selectedOption = 0;

    auto remountSD = [&]() {
        if ( FileUtils::SDReady && !FileUtils::isMountedSDCard() ) FileUtils::unmountSDCard();
        if ( !FileUtils::SDReady ) FileUtils::initFileSystem();
    };

    int selectedAdvancedOption = 0;
    int selectedConfigOption = 0;
    int selectedExitOption = 0;
    int selectedKeyMapOption = 0;

    // Load key.map files found
    auto getKeyMapFile = [&menuOptionsSelectKeyMap](uint8_t &maxKeyMapFilesFound)
    {
        
        printf("\n");
        printf("\n");
        printf("Open directory\n");
        printf("\n");
        printf("\n");

        char dpath[50];
        strcpy(dpath,MOUNT_POINT_SD);
        strcat(dpath,"/");
    
        struct dirent *entry;
        DIR *dir = opendir(dpath);
        if (dir == NULL) {
            return;
        }

        int items = 0;

        while ((entry = readdir(dir)) != NULL) 
        {
            string filename = entry->d_name;
            printf("file name: %s\n",entry->d_name);
            int pos = filename.find(".");

            if (pos == string::npos)
            {continue;}

            string ext;
            ext = (filename.substr(pos+1));
            
            printf("file ext: %s\n",ext.c_str());

            if (ext=="map")
            {   
                menuOptionsSelectKeyMap[items] = filename;
                printf("[%02u]Keymap found: %s\n",items,filename.c_str());
                maxKeyMapFilesFound = items;
                items++;
            }

            // Only first five items are loaded
            if (items > 4)
            {break;}

        }

        closedir(dir);
        // if (FileUtils::isSDReady())
        // {
        //     std::string fkeymapd_path_str = Keymap::keymapfile_path + Keymap::keymapfilename;
        //     //fkeymapd = fopen(fkeymapd_path_str.c_str(),"w+");            
        // }        
    };    

    // Renderizar el menú inicial
    auto renderMenu = [&](int highlight) {
        SET_CURSOR(0, 1);
        int len = 0;
        int limit = ZXKeyb::Exists ? menuCount : menuCountLily;
        for (int i = 0; i < limit; ++i) {
            VIDEO::vga.setTextColor(i == highlight ? zxColor(1, 1) : zxColor(7, 1), i == highlight ? zxColor(7, 1) : zxColor(1, 0));
            VIDEO::vga.print(" ");
            VIDEO::vga.print(ZXKeyb::Exists ? menuOptions[i] : menuOptionsLily[i]);
            VIDEO::vga.print(" ");
            len += strlen(ZXKeyb::Exists ? menuOptions[i] : menuOptionsLily[i]) + 2;
        }
        VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(1, 0));
        VIDEO::vga.print(string(total_cols - len, ' ').c_str());
    };

    auto renderOptions = [&](const char *options[], const char *values[], const int optionsCount, int highlight) {
        SET_CURSOR(1, 3); // Ajustar la posición para el submenú
        for (int i = 0; i < optionsCount; ++i) {
            // Color del texto, resaltado para el elemento seleccionado
            VIDEO::vga.setTextColor(i == highlight ? zxColor(7, 1) : zxColor(1, 0),
                                    i == highlight ? zxColor(0, 0) : zxColor(7, 0));

            // Imprimir el nombre de la opción
            VIDEO::vga.print(" ");
            VIDEO::vga.print(options[i]);

            // Calcular espacios en blanco para alinear valores
            int padding = total_cols - 19 /* Help Column */ - 2 - strlen(options[i]) -
                          (values && values[i] ? strlen(values[i]) : 0) - 2; // espacio antes del valor si existe

            // Añadir los espacios para alineación
            VIDEO::vga.print(string(padding, ' ').c_str());

            // Si hay valores, imprimir el valor alineado a la derecha
            if (values && values[i]) {
                VIDEO::vga.print(values[i]);
            }

            VIDEO::vga.print(" \n");
        }
    };

    auto screen_clear = [&](bool fullwidth = false) {
        int color = zxColor(7, 0);
        for (int y = base_row + OSD_FONT_H * 3; y < base_row + ( total_rows - 2 ) * OSD_FONT_H - OSD_FONT_H / 2; y++)
            for (int x = base_col + OSD_FONT_W - OSD_FONT_W / 2; x < base_col + ( total_cols - ( fullwidth ? 0 : 20 )) * OSD_FONT_W + OSD_FONT_W / 2; x++)
                VIDEO::vga.dotFast(x, y, color);

        const int top = base_row + OSD_FONT_H * 2 + OSD_FONT_H / 2;
        const int buttom = base_row + ( total_rows - 1 ) * OSD_FONT_H - OSD_FONT_H / 2;
        const int left = base_col + OSD_FONT_W / 2;
        const int right = base_col + ( total_cols - 1 ) * OSD_FONT_W + OSD_FONT_W / 2;

        VIDEO::vga.line(  left,    top, right,    top, zxColor(1, 0));
        VIDEO::vga.line(  left, buttom, right, buttom, zxColor(1, 0));
        VIDEO::vga.line(  left,    top,  left, buttom, zxColor(1, 0));
        VIDEO::vga.line( right,    top, right, buttom, zxColor(1, 0));

        VIDEO::vga.line( right - 19 * OSD_FONT_W, top                    , right - 19 * OSD_FONT_W, buttom                 , zxColor(1, 0));
        VIDEO::vga.line( right - 19 * OSD_FONT_W, buttom - 6 * OSD_FONT_H, right                  , buttom - 6 * OSD_FONT_H, zxColor(1, 0));

        SET_CURSOR(total_cols - 19, total_rows - 7);
        VIDEO::vga.setTextColor(zxColor(1, 0), zxColor(7, 0));
        VIDEO::vga.print("\x1A \x1B Select Screen\n");
        VIDEO::vga.print("\x18 \x19 Select Item\n");
        VIDEO::vga.print("Enter: Select/Chg.\n");
        if (ZXKeyb::Exists || Config::zxunops2) {
            VIDEO::vga.print("\x06+S: Save & Exit\n");
            VIDEO::vga.print("BREAK: Exit\n");
        } else {
            VIDEO::vga.print("F10: Save & Exit\n");
            VIDEO::vga.print("ESC: Exit  \n");
        }

    };

    #define BIOS_DLG_ALERT      0
    #define BIOS_DLG_CONFIRM    1
    #define BIOS_DLG_MSG        2

    auto msg_dialog = [&](const char *title, const char *message, int type = BIOS_DLG_ALERT) {
        // Calcular el ancho del título
        int title_length = strlen(title);

        // Inicializar el ancho del diálogo con el ancho del título
        int dialog_width = title_length;

        // Calcular la altura y el ancho del mensaje
        int message_height = 0; // Contador de líneas
        const char *p = message, *pi = message;
        while(*p) {
            if (*p == '\n') {
                if (p - pi > dialog_width) dialog_width = p - pi;
                pi = p + 1;
                message_height++;
            }
            ++p;
        }

        if (pi < p && p - pi + 1 > dialog_width) dialog_width = p - pi + 1;

        dialog_width += 4; // 2 caracteres de margen a cada lado

        // Ajustar el alto total del diálogo
        int dialog_height = message_height + 3 + ((type != BIOS_DLG_MSG) ? 2 : 0); // Incluye el título, márgenes y botones

        int left = (total_cols - dialog_width) / 2;
        int right = left + dialog_width;
        int top = (total_rows - dialog_height) / 2;
        int bottom = top + 2 + dialog_height;

        // Limpiar el área del diálogo
        for (int y = base_row + top * OSD_FONT_H; y < base_row + bottom * OSD_FONT_H - OSD_FONT_H / 2; y++) {
            for (int x = base_col + left * OSD_FONT_W - OSD_FONT_W / 2; x < base_col + right * OSD_FONT_W + OSD_FONT_W / 2; x++) {
                VIDEO::vga.dotFast(x, y, zxColor(7, 0));
            }
        }

        // Dibujar el borde del diálogo
        VIDEO::vga.line(base_col +  left * OSD_FONT_W - OSD_FONT_W / 2, base_row +    top * OSD_FONT_H + OSD_FONT_H / 2, base_col + right * OSD_FONT_W + OSD_FONT_W / 2, base_row +    top * OSD_FONT_H + OSD_FONT_H / 2, zxColor(1, 0));
        VIDEO::vga.line(base_col +  left * OSD_FONT_W - OSD_FONT_W / 2, base_row + bottom * OSD_FONT_H - OSD_FONT_H / 2, base_col + right * OSD_FONT_W + OSD_FONT_W / 2, base_row + bottom * OSD_FONT_H - OSD_FONT_H / 2, zxColor(1, 0));
        VIDEO::vga.line(base_col +  left * OSD_FONT_W - OSD_FONT_W / 2, base_row +    top * OSD_FONT_H + OSD_FONT_H / 2, base_col +  left * OSD_FONT_W - OSD_FONT_W / 2, base_row + bottom * OSD_FONT_H - OSD_FONT_H / 2, zxColor(1, 0));
        VIDEO::vga.line(base_col + right * OSD_FONT_W + OSD_FONT_W / 2, base_row +    top * OSD_FONT_H + OSD_FONT_H / 2, base_col + right * OSD_FONT_W + OSD_FONT_W / 2, base_row + bottom * OSD_FONT_H - OSD_FONT_H / 2, zxColor(1, 0));

        VIDEO::vga.fillRect(base_col + left * OSD_FONT_W, base_row + ( top + 1 ) * OSD_FONT_H, dialog_width * OSD_FONT_W, dialog_height * OSD_FONT_H, zxColor(1,0));

        // Mostrar el título en la primera línea dentro del cuadro
        SET_CURSOR(left + dialog_width / 2 - strlen(title) / 2, top);
        VIDEO::vga.setTextColor(zxColor(1, 0), zxColor(7, 0));
        VIDEO::vga.print(title);

        if ( message_height > 0 ) {
            // Mostrar el mensaje en la tercera línea
            SET_CURSOR(left + 1, top + 2);
            VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(1, 0));
            VIDEO::vga.print(message);
        } else {
            // Mostrar el mensaje en la tercera línea
            SET_CURSOR(left + 1, top + 2);

            int current_line = 0;
            pi = p = message;
            while(*p) {
                if (*p == '\n') {
                    SET_CURSOR(left + dialog_width / 2 - (p - pi) / 2, top + 2 + current_line); // +2 por la línea del título y el margen superior
                    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(1, 0));
                    for(; pi < p - 1; ++pi) VIDEO::vga.print(*pi);
                    ++current_line;
                }
                ++p;
            }

            if (pi < p) {
                SET_CURSOR(left + dialog_width / 2 - (p - pi) / 2, top + 2 + current_line); // +2 por la línea del título y el margen superior
                VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(1, 0));
                for(; pi < p; ++pi) VIDEO::vga.print(*pi);
            }
        }


        // Mostrar los botones "OK" y "Cancel" en la quinta línea
        int selectedButton = 0; // 0 = OK, 1 = Cancel
        auto renderButtons = [&](int type = BIOS_DLG_ALERT) {
            SET_CURSOR(left + dialog_width / 2 - ( ( type == BIOS_DLG_CONFIRM ) ? 11 : 5 ), top + 4 + message_height);
            VIDEO::vga.setTextColor(selectedButton == 0 ? zxColor(1, 1) : zxColor(7, 0), selectedButton == 0 ? zxColor(7, 1) : zxColor(1, 0));
            VIDEO::vga.print("[   OK   ]");

            if ( type == BIOS_DLG_CONFIRM ) {
                VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(1, 0));
                VIDEO::vga.print("  ");

                VIDEO::vga.setTextColor(selectedButton == 1 ? zxColor(1, 1) : zxColor(7, 0), selectedButton == 1 ? zxColor(7, 1) : zxColor(1, 0));
                VIDEO::vga.print("[ CANCEL ]");
            }
        };

        if (type != BIOS_DLG_MSG) {
            renderButtons(type);
        }

        // No unir este if con el de arriba, si se une, desconozco el motivo, pero el BIOS_DLG_MSG espera por las key...
        if (type != BIOS_DLG_MSG) {
            // Esperar la selección del usuario
            while (true) {
                if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead(KBDREAD_MODEBIOS);
                while (Kbd->virtualKeyAvailable()) {
                    fabgl::VirtualKeyItem NextKey;
                    if (readKbd(&NextKey, KBDREAD_MODEBIOS) && NextKey.down) {
                        switch (NextKey.vk) {
                            case fabgl::VK_LEFT:
                            case fabgl::VK_RIGHT:
                                if (type == BIOS_DLG_CONFIRM) {
                                    selectedButton = 1 - selectedButton; // Cambiar entre 0 y 1
                                    renderButtons(BIOS_DLG_CONFIRM);
                                }
                                break;
                            case fabgl::VK_RETURN:
                            case fabgl::VK_SPACE:
                                return selectedButton == 0; // Retorna true si seleccionó OK, false si seleccionó Cancel
                            case fabgl::VK_ESCAPE:
                                return false;
                        }
                    }
                }
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }
        }
    };

    // Mostrar información de hardware
    auto showHardwareInfo = [&]() {
        screen_clear();

        // Mostrar información de chip
        SET_CURSOR(1, 3);
        VIDEO::vga.setTextColor(zxColor(1, 0), zxColor(7, 0));

        VIDEO::vga.print(ESPeccy::getHardwareInfo().c_str());
    };

    // Iniciar el menú
    getKeyMapFile(maxKeyMapFilesFound);

    for(int i = 0;i<5;i++)
    {
        printf("Item %01u : %s\n",i,menuOptionsSelectKeyMap[i].c_str());
    }

    // Iniciar el menú
    renderMenu(selectedOption);
    showHardwareInfo();

    // Special position
    //VIDEO::vga.setCursor(base_col, base_row - 1); // 0, 0 - 1px
    SET_CURSOR(0,0);
    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(1, 1));
    PRINT_FILLED_CENTERED("ESPeccy Bios Setup Utility");

    SET_CURSOR(0, total_rows - 1);
    const char* commitDate = getShortCommitDate();
    string footer = "BIOS Date: 20"
                    + std::string(1, commitDate[0]) + commitDate[1] + "/"
                    + std::string(1, commitDate[2]) + commitDate[3] + "/"
                    + std::string(1, commitDate[4]) + commitDate[5] + " "
                    + std::string(1, commitDate[6]) + commitDate[7] + ":"
                    + std::string(1, commitDate[8]) + commitDate[9] + " ";
    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(1, 0));
    PRINT_FILLED_ROW_ALIGN_RIGHT(footer.c_str());

    // Lógica de navegación del menú
    bool exitMenu = false;

    bool exit_to_main = false;

    auto mainMenuNav = [&](const std::function<void()>& escCancel, const std::function<void()>& f10Cancel) {
        switch (NextKey.vk) {
            case fabgl::VK_RIGHT:
                selectedOption = (selectedOption + 1) % (ZXKeyb::Exists ? menuCount : menuCountLily);
                renderMenu(selectedOption);
                exit_to_main = true;
                break;

            case fabgl::VK_LEFT:
                selectedOption = (selectedOption - 1 + (ZXKeyb::Exists ? menuCount : menuCountLily)) % (ZXKeyb::Exists ? menuCount : menuCountLily);
                renderMenu(selectedOption);
                exit_to_main = true;
                break;

            case fabgl::VK_ESCAPE:
                if ( msg_dialog("Exit BIOS Setup", "Are you sure you want to exit?\nUnsaved changes will be lost.", BIOS_DLG_CONFIRM) ) {
                    OSD::esp_hard_reset();
                } else {
                    screen_clear(true);
                    escCancel();
                }
                break;

            case fabgl::VK_s:
            case fabgl::VK_S:
                if (!ZXKeyb::Exists) break;

            case fabgl::VK_F10:
                if ( msg_dialog("Confirm Save & Exit", "Are you sure you want to save\nchanges and exit?", BIOS_DLG_CONFIRM) ) {
                    Config::save();
                    OSD::esp_hard_reset();
                } else {
                    screen_clear(true);
                    f10Cancel();
                }
                break;
        }
    };


    auto optionsNav = [&](int &selectedOption, int menuCount, const std::function<void()> &renderMenu) {
        switch (NextKey.vk) {
            case fabgl::VK_DOWN:
                selectedOption = (selectedOption + 1) % menuCount;
                screen_clear();
                renderMenu();
                break;

            case fabgl::VK_UP:
                selectedOption = (selectedOption - 1 + menuCount) % menuCount;
                screen_clear();
                renderMenu();
                break;
        }
    };

    while (!exitMenu) {
        int oldSelectedOptions = selectedOption;

        if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead(KBDREAD_MODEBIOS);
        while (Kbd->virtualKeyAvailable()) {
            bool r = readKbd(&NextKey, KBDREAD_MODEBIOS);
            if (r && NextKey.down) mainMenuNav([&showHardwareInfo](){showHardwareInfo();}, [&showHardwareInfo](){showHardwareInfo();});
        }

        if (selectedOption != oldSelectedOptions || exit_to_main ) {
            exit_to_main = false;
            // Acción según la opción seleccionada
            if (selectedOption == 0 ) { // Acción para MAIN
                screen_clear();
                showHardwareInfo();
            }
            else if (selectedOption == 1) { // Acción para Video
                int selectedVideoOption = 0;

                auto renderVideoOptions = [&]() {
                    const char *valuesVideo[] = { menuOptionsResolution[Config::aspect_16_9], menuOptionsFrequency[Config::videomode], menuYesNo[Config::scanlines] };
                    renderOptions(menuVideo, valuesVideo, menuVideoCount, selectedVideoOption);
                };

                // Renderizar menú video
                screen_clear();
                renderVideoOptions();

                // Lógica para el menú video
                while (true) {
                    if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead(KBDREAD_MODEBIOS);
                    while (Kbd->virtualKeyAvailable()) {
                        bool r = readKbd(&NextKey, KBDREAD_MODEBIOS);
                        if (r && NextKey.down) {

                            mainMenuNav([&renderVideoOptions](){renderVideoOptions();}, [&renderVideoOptions](){renderVideoOptions();});
                            optionsNav(selectedVideoOption, menuVideoCount, [&renderVideoOptions](){renderVideoOptions();});

                            switch (NextKey.vk) {
                                case fabgl::VK_RETURN:
                                case fabgl::VK_SPACE:
                                    switch (selectedVideoOption) {
                                        case 0: // Acción para RESOLUTION
                                            Config::aspect_16_9 = (Config::aspect_16_9 + 1) % 2;
                                            break;
                                        case 1: // Acción para FREQUENCY
                                            Config::videomode = (Config::videomode + 1) % 3;
                                            break;
                                        case 2: // Acción para SCANLINES
                                            Config::scanlines = (Config::scanlines + 1) % 2;
                                            break;
                                    }

                                    screen_clear();
                                    renderVideoOptions();
                                    break;
                            }
                        }
                    }

                    if (exit_to_main) break;

                    vTaskDelay(100 / portTICK_PERIOD_MS);

                }

                if (!exit_to_main) {
                    screen_clear();
                    renderVideoOptions();
                }
            }
            else if (selectedOption == 2) { // Acción para Keyboard
                int selectedKeyboardOption = 0;

                auto renderKeyboardOptions = [&]() {
                    // Cut name for no more than 10 chars (textbox limitation)
                    string shrtname = (menuOptionsSelectKeyMap[Config::pathforkeymapfile_pos]);
                    shrtname = shrtname.length() < 10 ? shrtname : shrtname.substr(0,9);
                    const char *valuesKeyboard[] = { menuYesNo[Config::zxunops2], menuYesNo[Config::keymap_enable], shrtname.c_str()};
                    renderOptions(menuKeyboard, valuesKeyboard, menuKeyboardCount, selectedKeyboardOption);
                };

                // Renderizar menú keyboard
                screen_clear();
                renderKeyboardOptions();

                // Lógica para el menú keyboard
                while (true) {
                    if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead(KBDREAD_MODEBIOS);
                    while (Kbd->virtualKeyAvailable()) {
                        bool r = readKbd(&NextKey, KBDREAD_MODEBIOS);
                        if (r && NextKey.down) {

                            mainMenuNav([&renderKeyboardOptions](){renderKeyboardOptions();}, [&renderKeyboardOptions](){renderKeyboardOptions();});
                            optionsNav(selectedKeyboardOption, menuKeyboardCount, [&renderKeyboardOptions](){renderKeyboardOptions();});

                            switch (NextKey.vk) {
                                case fabgl::VK_RETURN:
                                case fabgl::VK_SPACE:
                                    switch (selectedKeyboardOption) {
                                        case 0: // Acción para ZXUnoPS2
                                            Config::zxunops2 = !Config::zxunops2;
                                            break;
                                        case 1: // Acción para Enable key.map
                                            Config::keymap_enable = (Config::keymap_enable + 1) %2;
                                            printf("BIOS: enable keymap:[%s] \n", Config::keymap_enable ? "yes" : "no");
                                            break;
                                        case 2: // Acción para Select key.map
                                            // Get next .map file found.
                                            Config::pathforkeymapfile = "/" + menuOptionsSelectKeyMap[itemskeymap];
                                            Config::pathforkeymapfile_pos = itemskeymap;
                                            itemskeymap++;                        
                                            itemskeymap = itemskeymap > maxKeyMapFilesFound ? 0 : itemskeymap;

                                            printf("BIOS: path for keymap:[%s] \n", Config::pathforkeymapfile.c_str());
                                            break;                                             
                                    }

                                    screen_clear();
                                    renderKeyboardOptions();
                                    break;
                            }
                        }
                    }

                    if (exit_to_main) break;

                    vTaskDelay(100 / portTICK_PERIOD_MS);

                }

                if (!exit_to_main) {
                    screen_clear();
                    renderKeyboardOptions();
                }
            }
            else if (!ZXKeyb::Exists && selectedOption == 3) { // Acción para Others
                int selectedOthersOption = 0;

                auto renderOthersOptions = [&]() {
                    const char *valuesOthes[] = { menuOthersIO36Button[Config::io36button] };
                    renderOptions(menuOthers, valuesOthes, menuOthersCount, selectedOthersOption);
                };

                // Renderizar menú otros
                screen_clear();
                renderOthersOptions();

                // Lógica para el menú otros
                while (true) {
                    if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead(KBDREAD_MODEBIOS);
                    while (Kbd->virtualKeyAvailable()) {
                        bool r = readKbd(&NextKey, KBDREAD_MODEBIOS);
                        if (r && NextKey.down) {

                            mainMenuNav([&renderOthersOptions](){renderOthersOptions();}, [&renderOthersOptions](){renderOthersOptions();});
                            optionsNav(selectedOthersOption, menuOthersCount, [&renderOthersOptions](){renderOthersOptions();});

                            switch (NextKey.vk) {
                                case fabgl::VK_RETURN:
                                case fabgl::VK_SPACE:
                                    switch (selectedOthersOption) {
                                        case 0: // Acción para IO36 Button
                                            Config::io36button = (Config::io36button + 1) % (sizeof(menuOthersIO36Button)/sizeof(menuOthersIO36Button[0]));
                                            break;
                                    }

                                    screen_clear();
                                    renderOthersOptions();
                                    break;
                            }
                        }
                    }

                    if (exit_to_main) break;

                    vTaskDelay(100 / portTICK_PERIOD_MS);

                }

                if (!exit_to_main) {
                    screen_clear();
                    renderOthersOptions();
                }
            }
            else if (selectedOption == (ZXKeyb::Exists ? 3 : 4)) { // Acción para CONFIG
                int selectedConfigOption = 0;
                // Renderizar menú de configuración
                screen_clear();
                renderOptions(menuConfig, NULL, menuConfigCount, selectedConfigOption);

                while (true) {
                    if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead(KBDREAD_MODEBIOS);
                    while (Kbd->virtualKeyAvailable()) {
                        bool r = readKbd(&NextKey, KBDREAD_MODEBIOS);
                        if (r && NextKey.down) {

                            mainMenuNav([&renderOptions, &menuConfig, &menuConfigCount, &selectedConfigOption](){renderOptions(menuConfig, NULL, menuConfigCount, selectedConfigOption);},
                                        [&renderOptions, &menuConfig, &menuConfigCount, &selectedConfigOption](){renderOptions(menuConfig, NULL, menuConfigCount, selectedConfigOption);});
                            optionsNav(selectedConfigOption, menuConfigCount, [&renderOptions, &menuConfig, &menuConfigCount, &selectedConfigOption](){renderOptions(menuConfig, NULL, menuConfigCount, selectedConfigOption);});

                            switch (NextKey.vk) {
                                case fabgl::VK_RETURN:
                                case fabgl::VK_SPACE:
                                    switch (selectedConfigOption) {
                                        case 0: // Acción para BACKUP
                                        {
                                            if (msg_dialog("Confirm Backup", "Insert a valid SD card.\nPress OK to save BIOS settings,\nor Cancel to abort.", BIOS_DLG_CONFIRM)) {
                                                screen_clear(true);
                                                renderOptions(menuConfig, NULL, menuConfigCount, selectedConfigOption);
                                                remountSD();
                                                bool status = !FileUtils::SDReady || Config::saveToSD();
                                                if (status) msg_dialog("Backup Error", "Failed to write backup.\nPlease check SD card and try again.");
                                                else msg_dialog("Backup Completed", "BIOS settings successfully saved to SD card.");
                                            }
                                            screen_clear(true);
                                            renderOptions(menuConfig, NULL, menuConfigCount, selectedConfigOption);
                                            break;
                                        }
                                        case 1: // Acción para RESTORE
                                        {
                                            if (msg_dialog("Confirm Restore", "Insert the SD card with the backup.\nPress OK to restore settings,\nor Cancel to abort.", BIOS_DLG_CONFIRM)) {
                                                screen_clear(true);
                                                renderOptions(menuConfig, NULL, menuConfigCount, selectedConfigOption);
                                                remountSD();
                                                bool status = !FileUtils::SDReady || Config::loadFromSD();
                                                if (status) msg_dialog("Restore Error", "Failed to restore settings.\nPlease verify SD card and try again.");
                                                else msg_dialog("Restore Completed", "BIOS settings successfully restored from SD card.");
                                            }
                                            screen_clear(true);
                                            renderOptions(menuConfig, NULL, menuConfigCount, selectedConfigOption);
                                            break;
                                        }
                                        case 2: // Acción para RESET
                                            if (msg_dialog("Reset Configuration & Reboot", "Do you really want to reset all settings?", BIOS_DLG_CONFIRM)) {
                                                nvs_flash_erase();
                                                OSD::esp_hard_reset();
                                            } else {
                                                screen_clear(true);
                                                renderOptions(menuConfig, NULL, menuConfigCount, selectedConfigOption);
                                            }
                                            break;
                                    }
                                    break;
                            }
                        }
                    }

                    if (exit_to_main) break;

                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }

                if (!exit_to_main) {
                    screen_clear();
                    renderOptions(menuConfig, NULL, menuConfigCount, selectedConfigOption);
                }
            }
            else if (selectedOption == (ZXKeyb::Exists ? 4 : 5)) { // Acción para Update
                int selectedUpdateOption = 0;
                // Renderizar menú de actualización
                screen_clear();
                renderOptions(menuUpdate, NULL, menuUpdateCount, selectedUpdateOption);

                while (true) {
                    if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead(KBDREAD_MODEBIOS);
                    while (Kbd->virtualKeyAvailable()) {
                        bool r = readKbd(&NextKey, KBDREAD_MODEBIOS);
                        if (r && NextKey.down) {

                            mainMenuNav([&renderOptions, &menuUpdate, &menuUpdateCount, &selectedUpdateOption](){renderOptions(menuUpdate, NULL, menuUpdateCount, selectedUpdateOption);},
                                        [&renderOptions, &menuUpdate, &menuUpdateCount, &selectedUpdateOption](){renderOptions(menuUpdate, NULL, menuUpdateCount, selectedUpdateOption);});
                            optionsNav(selectedUpdateOption, menuUpdateCount, [&renderOptions, &menuUpdate, &menuUpdateCount, &selectedUpdateOption](){renderOptions(menuUpdate, NULL, menuUpdateCount, selectedUpdateOption);});

                            switch (NextKey.vk) {
                                case fabgl::VK_RETURN:
                                case fabgl::VK_SPACE:
                                    switch (selectedUpdateOption) {
                                        case 0: // Acción para Save Changes & Update
                                            if ( msg_dialog("Confim update firmware", "Are you sure you want to perform\na firmware update?", BIOS_DLG_CONFIRM) ) {

                                                remountSD();

                                                FILE *firmware = fopen("/sd/firmware.upg", "rb");
                                                if (firmware) {

                                                    screen_clear(true);
                                                    renderOptions(menuUpdate, NULL, menuUpdateCount, selectedUpdateOption);
                                                    msg_dialog("Firmware Update", "Update... 0% completed", BIOS_DLG_MSG);

                                                    char ota_write_data[FWBUFFSIZE + 1] = { 0 };

                                                    // get the currently running partition
                                                    const esp_partition_t *partition = esp_ota_get_running_partition();
                                                    if (partition) {
                                                        // Grab next update target
                                                        // const esp_partition_t *target = esp_ota_get_next_update_partition(NULL);
                                                        string splabel;
                                                        if (strcmp(partition->label,"esp0")==0) splabel = "esp1"; else splabel= "esp0";

                                                        const esp_partition_t *target = esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_ANY,splabel.c_str());
                                                        if (target) {

                                                            esp_ota_handle_t ota_handle;
                                                            esp_err_t result = esp_ota_begin(target, OTA_SIZE_UNKNOWN, &ota_handle);
                                                            if (result == ESP_OK) {

                                                                size_t bytesread;
                                                                uint32_t byteswritten = 0;

                                                                // Get firmware size
                                                                fseek(firmware, 0, SEEK_END);
                                                                long bytesfirmware = ftell(firmware);
                                                                rewind(firmware);

                                                                while (1) {
                                                                    bytesread = fread(ota_write_data, 1, FWBUFFSIZE, firmware);
                                                                    result = esp_ota_write(ota_handle,(const void *) ota_write_data, bytesread);
                                                                    if (result != ESP_OK) break;

                                                                    byteswritten += bytesread;

                                                                    int percent = (float) 100 / ((float) bytesfirmware / (float) byteswritten);
                                                                    if (percent%10 == 0) {
                                                                        char percent_str[32];
                                                                        sprintf(percent_str, "Update... %d%% completed", percent);
                                                                        msg_dialog("Firmware Update", percent_str, BIOS_DLG_MSG);
                                                                    }

                                                                    // printf("Bytes written: %d\n",byteswritten);
                                                                    if (feof(firmware)) break;
                                                                }

                                                                if (result == ESP_OK) {
                                                                    result = esp_ota_end(ota_handle);
                                                                    if (result == ESP_OK) {
                                                                        result = esp_ota_set_boot_partition(target);
                                                                        if (result == ESP_OK) {

                                                                            screen_clear(true);
                                                                            renderOptions(menuUpdate, NULL, menuUpdateCount, selectedUpdateOption);
                                                                            msg_dialog("Firmware Update", "Update 100% ""completed\nRestarting...", BIOS_DLG_MSG);

                                                                            // Enable StartMsg
                                                                            Config::StartMsg = true;
                                                                            Config::save("StartMsg");

                                                                            delay(2000);

                                                                            // Firmware written: reboot
                                                                            OSD::esp_hard_reset();
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }

                                                screen_clear(true);
                                                renderOptions(menuUpdate, NULL, menuUpdateCount, selectedUpdateOption);
                                                msg_dialog("Firmware Update", "Firmware Update Failed!");

                                            }

                                            screen_clear(true);
                                            renderOptions(menuUpdate, NULL, menuUpdateCount, selectedUpdateOption);
                                            break;
                                    }
                                    break;
                            }
                        }
                    }

                    if (exit_to_main) break;

                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }

                if (!exit_to_main) {
                    screen_clear();
                    renderOptions(menuUpdate, NULL, menuUpdateCount, selectedUpdateOption);
                }

            }
            else if (selectedOption == (ZXKeyb::Exists ? 5 : 6)) { // Acción para EXIT
                int selectedExitOption = 0;
                // Renderizar menú de salida
                screen_clear();
                renderOptions(menuExit, NULL, menuExitCount, selectedExitOption);

                while (true) {
                    if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead(KBDREAD_MODEBIOS);
                    while (Kbd->virtualKeyAvailable()) {
                        bool r = readKbd(&NextKey, KBDREAD_MODEBIOS);
                        if (r && NextKey.down) {

                            mainMenuNav([&renderOptions, &menuExit, &menuExitCount, &selectedExitOption](){renderOptions(menuExit, NULL, menuExitCount, selectedExitOption);},
                                        [&renderOptions, &menuExit, &menuExitCount, &selectedExitOption](){renderOptions(menuExit, NULL, menuExitCount, selectedExitOption);});
                            optionsNav(selectedExitOption, menuExitCount, [&renderOptions, &menuExit, &menuExitCount, &selectedExitOption](){renderOptions(menuExit, NULL, menuExitCount, selectedExitOption);});

                            switch (NextKey.vk) {
                                case fabgl::VK_RETURN:
                                case fabgl::VK_SPACE:
                                    switch (selectedExitOption) {
                                        case 0: // Acción para Save Changes & Exit
                                            if ( msg_dialog("Confim Save & Exit", "Are you sure you want to save\nchanges and exit?", BIOS_DLG_CONFIRM) ) {
                                                Config::save();
                                                OSD::esp_hard_reset();
                                            } else {
                                                screen_clear(true);
                                                renderOptions(menuExit, NULL, menuExitCount, selectedExitOption);
                                            }
                                            break;
                                        case 1: // Acción para Discard Changes & Exit
                                            if ( msg_dialog("Exit BIOS Setup", "Are you sure you want to exit?\nUnsaved changes will be lost.", BIOS_DLG_CONFIRM) ) {
                                                OSD::esp_hard_reset();
                                            } else {
                                                screen_clear(true);
                                                renderOptions(menuExit, NULL, menuExitCount, selectedExitOption);
                                            }
                                            break;
                                    }
                                    break;
                            }
                        }
                    }

                    if (exit_to_main) break;

                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }

                if (!exit_to_main) {
                    screen_clear();
                    renderOptions(menuExit, NULL, menuExitCount, selectedExitOption);
                }

            }

        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    VIDEO::vga.clear(zxColor(7, 0));
}

//=======================================================================================
// SETUP
//=======================================================================================

void ESPeccy::setup()
{

    booting = true;

    // force get psram size
    ESPeccy::getHardwareInfo();

    if (Config::slog_on) {
        printf("------------------------------------\n");
        printf("| ESPeccy: booting                 |\n");
        printf("------------------------------------\n");
        showMemInfo();
    }

    //=======================================================================================
    // PHYSICAL KEYBOARD (SINCLAIR 8 + 5 MEMBRANE KEYBOARD)
    //=======================================================================================

    ZXKeyb::setup();

    //=======================================================================================
    // PHYSICAL KEYBOARD THROUGH MCP23017
    //=======================================================================================

    // if (!ZXKeyb::Exists) MCPKeyb::setup();

    //=======================================================================================
    // LOAD CONFIG
    //=======================================================================================

    bool config_load_failed = Config::load();

    if (Config::StartMsg) Config::save(); // Firmware updated or reflashed: save all config data

    // printf("---------------------------------\n");
    // printf("Ram file: %s\n",Config::ram_file.c_str());
    // printf("Arch: %s\n",Config::arch.c_str());
    // printf("pref Arch: %s\n",Config::pref_arch.c_str());
    // printf("romSet: %s\n",Config::romSet.c_str());
    // printf("romSet48: %s\n",Config::romSet48.c_str());
    // printf("romSet128: %s\n",Config::romSet128.c_str());
    // printf("romSetTK90X: %s\n",Config::romSetTK90X.c_str());
    // printf("romSetTK95: %s\n",Config::romSetTK95.c_str());
    // printf("pref_romSet_48: %s\n",Config::pref_romSet_48.c_str());
    // printf("pref_romSet_128: %s\n",Config::pref_romSet_128.c_str());
    // printf("pref_romSet_TK90X: %s\n",Config::pref_romSet_TK90X.c_str());
    // printf("pref_romSet_TK95: %s\n",Config::pref_romSet_TK95.c_str());

    bool was_vreset = false;

    // Set arch if there's no snapshot to load
    if (Config::ram_file == NO_RAM_FILE) {

        if (Config::pref_arch.substr(Config::pref_arch.length()-1) == "R") {

            Config::pref_arch.pop_back();
            Config::save("pref_arch");

            was_vreset = true;

        } else {

            if (Config::pref_arch != "Last") Config::arch = Config::pref_arch;

            if (Config::arch == "48K") {
                if (Config::pref_romSet_48 != "Last")
                    Config::romSet = Config::pref_romSet_48;
                else
                    Config::romSet = Config::romSet48;
            } else if (Config::arch == "128K") {
                if (Config::pref_romSet_128 != "Last")
                    Config::romSet = Config::pref_romSet_128;
                else
                    Config::romSet = Config::romSet128;
            } else if (Config::arch == "+2A") {
                if (Config::pref_romSet_2A != "Last")
                    Config::romSet = Config::pref_romSet_2A;
                else
                    Config::romSet = Config::romSet2A;
            } else if (Config::arch == "TK90X") {
                if (Config::pref_romSet_TK90X != "Last")
                    Config::romSet = Config::pref_romSet_TK90X;
                else
                    Config::romSet = Config::romSetTK90X;
            } else if (Config::arch == "TK95") {
                if (Config::pref_romSet_TK95 != "Last")
                    Config::romSet = Config::pref_romSet_TK95;
                else
                    Config::romSet = Config::romSetTK95;
            } else Config::romSet = "Pentagon";

        }

    }

    printf("Arch: %s, Romset: %s\n",Config::arch.c_str(), Config::romSet.c_str());

    // printf("---------------------------------\n");
    // printf("Ram file: %s\n",Config::ram_file.c_str());
    // printf("Arch: %s\n",Config::arch.c_str());
    // printf("pref Arch: %s\n",Config::pref_arch.c_str());
    // printf("romSet: %s\n",Config::romSet.c_str());
    // printf("romSet48: %s\n",Config::romSet48.c_str());
    // printf("romSet128: %s\n",Config::romSet128.c_str());
    // printf("pref_romSet_48: %s\n",Config::pref_romSet_48.c_str());
    // printf("pref_romSet_128: %s\n",Config::pref_romSet_128.c_str());

    //=======================================================================================
    // INIT PS/2 KEYBOARD
    //=======================================================================================

    // Config::ps2_dev2 = 0; // Force 0 for testing

    if (ZXKeyb::Exists) {

        switch (Config::ps2_dev2) {
            case 0:
                PS2Controller.begin(PS2Preset::zxKeyb, KbdMode::CreateVirtualKeysQueue);
                break;
            case 1:
                PS2Controller.begin(PS2Preset::KeyboardPort0, KbdMode::CreateVirtualKeysQueue);
                // if (PS2Controller.keyboard()->isKeyboardAvailable() == false) {
                //     printf("2nd PS/2 device (Kbd/ESPjoy) not detected\n");
                //     Config::ps2_dev2 = 3;
                //     Config::save("PS2Dev2");
                //     OSD::esp_hard_reset();
                // }
                ps2kbd = true;
                break;
            case 2:
                PS2Controller.begin(PS2Preset::MousePort0, KbdMode::CreateVirtualKeysQueue);
                ps2mouse = PS2Controller.mouse() && PS2Controller.mouse()->isMouseAvailable();
                if (!ps2mouse) {
                    printf("2nd PS/2 device (Kbd/ESPjoy) not detected\n");
                    Config::ps2_dev2 = 4;
                    Config::save("PS2Dev2");
                    OSD::esp_hard_reset();
                }
                break;
            case 3:
            case 4:
                PS2Controller.begin(PS2Preset::zxKeyb, KbdMode::CreateVirtualKeysQueue);
                Config::ps2_dev2 -= 2;
                Config::save("PS2Dev2");
                break;
        }

    } else {

        switch (Config::ps2_dev2) {
            case 0:
                PS2Controller.begin(PS2Preset::KeyboardPort0, KbdMode::CreateVirtualKeysQueue);
                ps2kbd = true;
                break;
            case 1:
                PS2Controller.begin(PS2Preset::KeyboardPort0_KeybJoystickPort1, KbdMode::CreateVirtualKeysQueue);
                ps2kbd  = true;
                ps2kbd2 = true;
                // ps2kbd2 = PS2Controller.keybjoystick()->isKeyboardAvailable();
                // if (!ps2kbd2) {
                //     printf("2nd PS/2 device (Kbd/ESPjoy) not detected\n");
                //     Config::ps2_dev2 = 3;
                //     Config::save("PS2Dev2");
                //     OSD::esp_hard_reset();
                // }
                break;
            case 2:
                PS2Controller.begin(PS2Preset::KeyboardPort0_MousePort1, KbdMode::CreateVirtualKeysQueue);
                ps2kbd = true;
                ps2mouse = PS2Controller.mouse() && PS2Controller.mouse()->isMouseAvailable();
                if (!ps2mouse) {
                    printf("2nd PS/2 device (mouse) not detected\n");
                    Config::ps2_dev2 = 4;
                    Config::save("PS2Dev2");
                    OSD::esp_hard_reset();
                }
                break;
            case 3:
            case 4:
                PS2Controller.begin(PS2Preset::KeyboardPort0, KbdMode::CreateVirtualKeysQueue);
                ps2kbd = true;
                Config::ps2_dev2 -= 2;
                Config::save("PS2Dev2");
                break;
        }

    }

    if (ps2mouse) {
        printf("Mouse detected\n");
        PS2Controller.mouse()->setSampleRate(Config::mousesamplerate);
        PS2Controller.mouse()->setResolution(Config::mousedpi);
        PS2Controller.mouse()->setScaling(Config::mousescaling);
    }

    printf("Waiting boot keys\n");
    bootKeyboard(); // BOOTKEYS: Read keyboard for 200 ms. checking boot keys
    printf("End Waiting boot keys\n");

    // Set Scroll Lock Led as current CursorAsJoy value
    if(ps2kbd)  PS2Controller.keyboard()->setLEDs(false, false, Config::CursorAsJoy);
    if(ps2kbd2) PS2Controller.keybjoystick()->setLEDs(false, false, Config::CursorAsJoy);

    // Set TAB and GRAVEACCENT behaviour
    if (Config::TABasfire1) {
        ESPeccy::VK_ESPECCY_FIRE1 = fabgl::VK_TAB;
        ESPeccy::VK_ESPECCY_FIRE2 = fabgl::VK_GRAVEACCENT;
        ESPeccy::VK_ESPECCY_TAB = fabgl::VK_NONE;
        ESPeccy::VK_ESPECCY_GRAVEACCENT = fabgl::VK_NONE;
    } else {
        ESPeccy::VK_ESPECCY_FIRE1 = fabgl::VK_NONE;
        ESPeccy::VK_ESPECCY_FIRE2 = fabgl::VK_NONE;
        ESPeccy::VK_ESPECCY_TAB = fabgl::VK_TAB;
        ESPeccy::VK_ESPECCY_GRAVEACCENT = fabgl::VK_GRAVEACCENT;
    }

    if (Config::slog_on) {
        showMemInfo("Keyboard started");
    }

    // Get chip information
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    Config::esp32rev = chip_info.revision;

    if (Config::slog_on) {

        printf("\n");
        printf("This is %s chip with %d CPU core(s), WiFi%s%s, ",
                CONFIG_IDF_TARGET,
                chip_info.cores,
                (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
                (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
        printf("silicon revision %d, ", chip_info.revision);
        printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
                (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
        printf("IDF Version: %s\n",esp_get_idf_version());
        printf("\n");

        if (Config::slog_on) printf("Executing on core: %u\n", xPortGetCoreID());

        showMemInfo();

    }

    //=======================================================================================
    // BOOTKEYS: Read keyboard for 200 ms. checking boot keys
    //=======================================================================================

    // printf("Waiting boot keys\n");
    bootKeyboard();
    // printf("End Waiting boot keys\n");

    //=======================================================================================
    // MEMORY SETUP
    //=======================================================================================

    // Load romset
    Config::requestMachine(Config::arch, Config::romSet);

    if (Config::slog_on) showMemInfo("ROMSET loaded");

    MemESP::Init();

    if (Config::slog_on) showMemInfo("MEM Allocated");

    MemESP::Reset();

    if (Config::slog_on) showMemInfo("RAM Initialized");

    //=======================================================================================
    // VIDEO
    //=======================================================================================

    VIDEO::Init();

    if (Config::slog_on) showMemInfo("VIDEO Initialized");

    VIDEO::Reset();

    if (Config::slog_on) showMemInfo("VGA started");

    //=======================================================================================
    // INIT FILESYSTEM
    //=======================================================================================

    FileUtils::initFileSystem();

    if (Config::slog_on) showMemInfo("File system started");

    //=======================================================================================
    // Start Message
    //=======================================================================================

    if (Config::StartMsg) {
        ShowStartMsg(); // Show welcome message
        if (config_load_failed) {
            if (FileUtils::SDReady && !FileUtils::isMountedSDCard()) FileUtils::unmountSDCard();
            if (!FileUtils::SDReady) FileUtils::initFileSystem();
            if (FileUtils::SDReady && Config::backupExistsOnSD()) {
                uint8_t res = OSD::msgDialog("Config backup found on SD.", "Restore it?");
                if (res == DLG_YES) {
                    Config::loadFromSD();
                    Config::save();
                }
            }
        }
    }

    //
    //
    //
    
    Keymap::keymap_enable = Config::keymap_enable;
    strcpy(Keymap::keymapfilename,Config::pathforkeymapfile.c_str());
    
    if (Config::keymap_enable)
    {

        printf("------------------------------------\n");
        printf("- KEYMAP setting                   -\n");
        printf("------------------------------------\n");
        printf("\n");        
        
        if (!Keymap::keymapFileExists())
        {
            // Back to come into BIOS to define path for key.map or disable custom keymap
            VIDEO::vga.setTextColor(zxColor(7,1),zxColor(2,0));
            VIDEO::vga.print(" ");
            VIDEO::vga.print("Layout file not found. Disable or select other.");
            delay(750);
            VIDEO::vga.setTextColor(zxColor(7,1),zxColor(0,0));
            VIDEO::vga.print(" ");
            VIDEO::vga.print("Back to BIOS menu.");
            delay(1000);

            showBIOS();
        }
        else
        {
            // Keymap::regexTest();
            Keymap::getKeymapFromFile(Keymap::kbdcustom);
            Keymap::activeKeyboardLayout(Keymap::kbdcustom);
            printf("> [%s] Keyboard layout applied.\n",Keymap::keymapfile_path.c_str());                
        } 

        printf("\n");
        printf("------------------------------------\n");

    }


    //=======================================================================================
    // BIOS
    //=======================================================================================

    if (runBios) showBIOS();

    //=======================================================================================
    // AUDIO
    //=======================================================================================

    overSamplebuf = (uint32_t *) heap_caps_calloc(ESP_AUDIO_SAMPLES_PENTAGON, sizeof(uint32_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_32BIT);
    if (overSamplebuf == NULL) printf("Can't allocate oversamplebuffer\n");

    // Create Audio task
    audioTaskQueue = xQueueCreate(1, sizeof(uint8_t));
    // Latest parameter = Core. In ESPIF, main task runs on core 0 by default. In Arduino, loop() runs on core 1.
    xTaskCreatePinnedToCore(&ESPeccy::audioTask, "audioTask", 2048 /* 1024 /* 1536 */, NULL, configMAX_PRIORITIES - 1, &audioTaskHandle, 1);

    // Set samples per frame and AY_emu flag depending on arch
    if (Config::arch == "48K") {
        samplesPerFrame=ESP_AUDIO_SAMPLES_48;
        audioOverSampleDivider = ESP_AUDIO_OVERSAMPLES_DIV_48;
        audioAYDivider = ESP_AUDIO_AY_DIV_48;
        audioSampleDivider = ESP_AUDIO_SAMPLES_DIV_48;
        AY_emu = Config::AY48;
        Audio_freq[0] = ESP_AUDIO_FREQ_48;
        Audio_freq[1] = ESP_AUDIO_FREQ_48;
        Audio_freq[2] = ESP_AUDIO_FREQ_48_125SPEED;
        Audio_freq[3] = ESP_AUDIO_FREQ_48_150SPEED;
    } else if (Config::arch == "TK90X" || Config::arch == "TK95") {

        switch (Config::ALUTK) {
        case 0:
            samplesPerFrame=ESP_AUDIO_SAMPLES_48;
            audioOverSampleDivider = ESP_AUDIO_OVERSAMPLES_DIV_48;
            audioAYDivider = ESP_AUDIO_AY_DIV_48;
            audioSampleDivider = ESP_AUDIO_SAMPLES_DIV_48;
            Audio_freq[0] = ESP_AUDIO_FREQ_48;
            Audio_freq[1] = ESP_AUDIO_FREQ_48;
            Audio_freq[2] = ESP_AUDIO_FREQ_48_125SPEED;
            Audio_freq[3] = ESP_AUDIO_FREQ_48_150SPEED;
            break;
        case 1:
            samplesPerFrame=ESP_AUDIO_SAMPLES_TK_50;
            audioOverSampleDivider = ESP_AUDIO_OVERSAMPLES_DIV_TK_50;
            audioAYDivider = ESP_AUDIO_AY_DIV_TK_50;
            audioSampleDivider = ESP_AUDIO_SAMPLES_DIV_TK_50;
            Audio_freq[0] = ESP_AUDIO_FREQ_TK_50;
            Audio_freq[1] = ESP_AUDIO_FREQ_TK_50;
            Audio_freq[2] = ESP_AUDIO_FREQ_TK_50_125SPEED;
            Audio_freq[3] = ESP_AUDIO_FREQ_TK_50_150SPEED;
            break;
        case 2:
            samplesPerFrame=ESP_AUDIO_SAMPLES_TK_60;
            audioOverSampleDivider = ESP_AUDIO_OVERSAMPLES_DIV_TK_60;
            audioAYDivider = ESP_AUDIO_AY_DIV_TK_60;
            audioSampleDivider = ESP_AUDIO_SAMPLES_DIV_TK_60;
            Audio_freq[0] = ESP_AUDIO_FREQ_TK_60;
            Audio_freq[1] = ESP_AUDIO_FREQ_TK_60;
            Audio_freq[2] = ESP_AUDIO_FREQ_TK_60_125SPEED;
            Audio_freq[3] = ESP_AUDIO_FREQ_TK_60_150SPEED;
        }

        AY_emu = Config::AY48;

    } else if (Config::arch == "128K" || Config::arch == "+2A") {
        samplesPerFrame=ESP_AUDIO_SAMPLES_128;
        audioOverSampleDivider = ESP_AUDIO_OVERSAMPLES_DIV_128;
        audioAYDivider = ESP_AUDIO_AY_DIV_128;
        audioSampleDivider = ESP_AUDIO_SAMPLES_DIV_128;
        AY_emu = true;
        Audio_freq[0] = ESP_AUDIO_FREQ_128;
        Audio_freq[1] = ESP_AUDIO_FREQ_128;
        Audio_freq[2] = ESP_AUDIO_FREQ_128_125SPEED;
        Audio_freq[3] = ESP_AUDIO_FREQ_128_150SPEED;
    } else if (Config::arch == "Pentagon") {
        samplesPerFrame=ESP_AUDIO_SAMPLES_PENTAGON;
        audioOverSampleDivider = ESP_AUDIO_OVERSAMPLES_DIV_PENTAGON;
        audioAYDivider = ESP_AUDIO_AY_DIV_PENTAGON;
        audioSampleDivider = ESP_AUDIO_SAMPLES_DIV_PENTAGON;
        AY_emu = true;
        Audio_freq[0] = ESP_AUDIO_FREQ_PENTAGON;
        Audio_freq[1] = ESP_AUDIO_FREQ_PENTAGON;
        Audio_freq[2] = ESP_AUDIO_FREQ_PENTAGON_125SPEED;
        Audio_freq[3] = ESP_AUDIO_FREQ_PENTAGON_150SPEED;
    }

    audioCOVOXDivider = audioAYDivider;
    covoxData[0] = covoxData[1] = covoxData[2] = covoxData[3] = 0;

    if (Config::load_monitor) {
        AY_emu = false; // Disable AY emulation if tape load monitor mode is set
        ESPeccy::aud_volume = ESP_VOLUME_MAX;
    } else
        ESPeccy::aud_volume = Config::volume;

    ESPoffset = 0;

    // AY Sound
    AySound::init();
    AySound::set_sound_format(Audio_freq[ESP_delay],1,8);
    AySound::set_stereo(AYEMU_MONO,NULL);
    AySound::reset();

    aud_active_sources = (Config::Covox & 0x01) | (AY_emu << 1);

    // Init tape
    Tape::Init();
    Tape::tapeFileName = "none";
    Tape::tapeStatus = TAPE_STOPPED;
    Tape::SaveStatus = SAVE_STOPPED;
    Tape::romLoading = false;

    if (Z80Ops::is128 || Z80Ops::is2a3) { // Apply pulse length compensation for 128K
        Tape::tapeCompensation = FACTOR128K;
    } else if ((Config::arch=="TK90X" || Config::arch == "TK95") && Config::ALUTK > 0) { // Apply pulse length compensation for Microdigital ALU
        Tape::tapeCompensation = FACTORALUTK;
    } else
        Tape::tapeCompensation = 1;

    // Init CPU
    Z80::create();

    // Set Ports starting values
    for (int i = 0; i < 128; i++) Ports::port[i] = 0xBF;
    Ports::LastOutTo1FFD = 0;
    if (Config::joystick1 == JOY_KEMPSTON || Config::joystick2 == JOY_KEMPSTON || Config::joyPS2 == JOYPS2_KEMPSTON) Ports::port[0x1f] = 0; // Kempston
    if (Config::joystick1 == JOY_FULLER || Config::joystick2 == JOY_FULLER || Config::joyPS2 == JOYPS2_FULLER) Ports::port[0x7f] = 0xff; // Fuller

    // Read joystick default definition
    for (int n = 0; n < 24; n++)
        ESPeccy::JoyVKTranslation[n] = (fabgl::VirtualKey) Config::joydef[n];

    // Init disk controller
    Betadisk.Init();

    // rvmWD1793Reset(&fdd);

    // Reset cpu
    CPU::reset();

    // Clear Cheat data
    CheatMngr::closeCheatFile();

    Config::last_rom_file = Config::rom_file;

    // Load snapshot if present in Config::ram_file
    if (Config::ram_file != NO_RAM_FILE) {
        FileUtils::SNA_Path = Config::SNA_Path;
        FileUtils::fileTypes[DISK_SNAFILE].begin_row = Config::SNA_begin_row;
        FileUtils::fileTypes[DISK_SNAFILE].focus = Config::SNA_focus;
        FileUtils::fileTypes[DISK_SNAFILE].fdMode = Config::SNA_fdMode;
        FileUtils::fileTypes[DISK_SNAFILE].fileSearch = Config::SNA_fileSearch;

        FileUtils::TAP_Path = Config::TAP_Path;
        FileUtils::fileTypes[DISK_TAPFILE].begin_row = Config::TAP_begin_row;
        FileUtils::fileTypes[DISK_TAPFILE].focus = Config::TAP_focus;
        FileUtils::fileTypes[DISK_TAPFILE].fdMode = Config::TAP_fdMode;
        FileUtils::fileTypes[DISK_TAPFILE].fileSearch = Config::TAP_fileSearch;

        FileUtils::DSK_Path = Config::DSK_Path;
        FileUtils::fileTypes[DISK_DSKFILE].begin_row = Config::DSK_begin_row;
        FileUtils::fileTypes[DISK_DSKFILE].focus = Config::DSK_focus;
        FileUtils::fileTypes[DISK_DSKFILE].fdMode = Config::DSK_fdMode;
        FileUtils::fileTypes[DISK_DSKFILE].fileSearch = Config::DSK_fileSearch;

        LoadSnapshot(Config::ram_file,"","",0xff);
        OSD::LoadCheatFile(Config::ram_file);
        Config::ram_file = NO_RAM_FILE;
        Config::save("ram");

    } else {
        // Load ROM if present in Config::rom_file
        if (!was_vreset && Config::rom_file != NO_ROM_FILE) {
            ROMLoad::load(Config::rom_file);
            Config::rom_file = NO_ROM_FILE;
            Config::save("rom");
        }
    }

    // Configura carga de cinta real
    RealTape_realloc_buffers(ESPeccy::Audio_freq[1], ESPeccy::samplesPerFrame, CPU::statesInFrame);

    ESP_RT_PARMS.gpio_num = &Config::realtape_gpio_num;
    ESP_RT_PARMS.global_tstates = &CPU::global_tstates;
    ESP_RT_PARMS.tstates = &CPU::tstates;
    ESP_RT_PARMS.zxkeyb_exists = ZXKeyb::Exists;

    RealTape_init(&ESP_RT_PARMS);

    if (Config::slog_on) showMemInfo("Setup finished.");

    booting = false;

}

//=======================================================================================
// RESET
//=======================================================================================

void ESPeccy::reset()
{
    // Load romset
    Config::requestMachine(Config::arch, Config::romSet);

    // Ports
    for (int i = 0; i < 128; i++) Ports::port[i] = 0xBF;
    Ports::LastOutTo1FFD = 0;
    if (Config::joystick1 == JOY_KEMPSTON || Config::joystick2 == JOY_KEMPSTON || Config::joyPS2 == JOYPS2_KEMPSTON) Ports::port[0x1f] = 0; // Kempston
    if (Config::joystick1 == JOY_FULLER || Config::joystick2 == JOY_FULLER || Config::joyPS2 == JOYPS2_FULLER) Ports::port[0x7f] = 0xff; // Fuller

    // Read joystick default definition
    for (int n = 0; n < 24; n++)
        ESPeccy::JoyVKTranslation[n] = (fabgl::VirtualKey) Config::joydef[n];

    MemESP::Reset(); // Reset Memory

    VIDEO::Reset();

    // Reinit disk controller
    if (Config::DiskCtrl == 1 || Z80Ops::isPentagon) {
        // Betadisk.ShutDown();
        // Betadisk.Init();
        Betadisk.EnterIdle();
    }

    Tape::tapeStatus = TAPE_STOPPED;
    Tape::tapePhase = TAPE_PHASE_STOPPED;
    Tape::SaveStatus = SAVE_STOPPED;
    Tape::romLoading = false;

    if (Z80Ops::is128 || Z80Ops::is2a3) { // Apply pulse length compensation for 128K
        Tape::tapeCompensation = FACTOR128K;
    } else if ((Config::arch=="TK90X" || Config::arch == "TK95") && Config::ALUTK > 0) { // Apply pulse length compensation for Microdigital ALU
        Tape::tapeCompensation = FACTORALUTK;
    } else
        Tape::tapeCompensation = 1;

    // Set block timings if there's a tape loaded and is a .tap
    if (Tape::tape != NULL && Tape::tapeFileType == TAPE_FTYPE_TAP) {
        Tape::TAP_setBlockTimings();
    }

    // Empty audio buffers
    for (int i=0;i<ESP_AUDIO_SAMPLES_PENTAGON;i++) {
        overSamplebuf[i]=0;
        audioBuffer[i]=0;
        AySound::SamplebufAY[i]=0;
    }
    lastaudioBit=0;

    // Set samples per frame and AY_emu flag depending on arch
    int prevAudio_freq = Audio_freq[ESP_delay];
    if (Config::arch == "48K") {
        samplesPerFrame=ESP_AUDIO_SAMPLES_48;
        audioOverSampleDivider = ESP_AUDIO_OVERSAMPLES_DIV_48;
        audioAYDivider = ESP_AUDIO_AY_DIV_48;
        audioSampleDivider = ESP_AUDIO_SAMPLES_DIV_48;
        AY_emu = Config::AY48;
        Audio_freq[0] = ESP_AUDIO_FREQ_48;
        Audio_freq[1] = ESP_AUDIO_FREQ_48;
        Audio_freq[2] = ESP_AUDIO_FREQ_48_125SPEED;
        Audio_freq[3] = ESP_AUDIO_FREQ_48_150SPEED;
    } else if (Config::arch == "TK90X" || Config::arch == "TK95") {

        switch (Config::ALUTK) {
        case 0:
            samplesPerFrame=ESP_AUDIO_SAMPLES_48;
            audioOverSampleDivider = ESP_AUDIO_OVERSAMPLES_DIV_48;
            audioAYDivider = ESP_AUDIO_AY_DIV_48;
            audioSampleDivider = ESP_AUDIO_SAMPLES_DIV_48;
            Audio_freq[0] = ESP_AUDIO_FREQ_48;
            Audio_freq[1] = ESP_AUDIO_FREQ_48;
            Audio_freq[2] = ESP_AUDIO_FREQ_48_125SPEED;
            Audio_freq[3] = ESP_AUDIO_FREQ_48_150SPEED;
            break;
        case 1:
            samplesPerFrame=ESP_AUDIO_SAMPLES_TK_50;
            audioOverSampleDivider = ESP_AUDIO_OVERSAMPLES_DIV_TK_50;
            audioAYDivider = ESP_AUDIO_AY_DIV_TK_50;
            audioSampleDivider = ESP_AUDIO_SAMPLES_DIV_TK_50;
            Audio_freq[0] = ESP_AUDIO_FREQ_TK_50;
            Audio_freq[1] = ESP_AUDIO_FREQ_TK_50;
            Audio_freq[2] = ESP_AUDIO_FREQ_TK_50_125SPEED;
            Audio_freq[3] = ESP_AUDIO_FREQ_TK_50_150SPEED;
            break;
        case 2:
            samplesPerFrame=ESP_AUDIO_SAMPLES_TK_60;
            audioOverSampleDivider = ESP_AUDIO_OVERSAMPLES_DIV_TK_60;
            audioAYDivider = ESP_AUDIO_AY_DIV_TK_60;
            audioSampleDivider = ESP_AUDIO_SAMPLES_DIV_TK_60;
            Audio_freq[0] = ESP_AUDIO_FREQ_TK_60;
            Audio_freq[1] = ESP_AUDIO_FREQ_TK_60;
            Audio_freq[2] = ESP_AUDIO_FREQ_TK_60_125SPEED;
            Audio_freq[3] = ESP_AUDIO_FREQ_TK_60_150SPEED;
        }

        AY_emu = Config::AY48;

    } else if (Config::arch == "128K" || Config::arch == "+2A") {
        samplesPerFrame=ESP_AUDIO_SAMPLES_128;
        audioOverSampleDivider = ESP_AUDIO_OVERSAMPLES_DIV_128;
        audioAYDivider = ESP_AUDIO_AY_DIV_128;
        audioSampleDivider = ESP_AUDIO_SAMPLES_DIV_128;
        AY_emu = true;
        Audio_freq[0] = ESP_AUDIO_FREQ_128;
        Audio_freq[1] = ESP_AUDIO_FREQ_128;
        Audio_freq[2] = ESP_AUDIO_FREQ_128_125SPEED;
        Audio_freq[3] = ESP_AUDIO_FREQ_128_150SPEED;
    } else if (Config::arch == "Pentagon") {
        samplesPerFrame=ESP_AUDIO_SAMPLES_PENTAGON;
        audioOverSampleDivider = ESP_AUDIO_OVERSAMPLES_DIV_PENTAGON;
        audioAYDivider = ESP_AUDIO_AY_DIV_PENTAGON;
        audioSampleDivider = ESP_AUDIO_SAMPLES_DIV_PENTAGON;
        AY_emu = true;
        Audio_freq[0] = ESP_AUDIO_FREQ_PENTAGON;
        Audio_freq[1] = ESP_AUDIO_FREQ_PENTAGON;
        Audio_freq[2] = ESP_AUDIO_FREQ_PENTAGON_125SPEED;
        Audio_freq[3] = ESP_AUDIO_FREQ_PENTAGON_150SPEED;
    }

    audioCOVOXDivider = audioAYDivider;
    covoxData[0] = covoxData[1] = covoxData[2] = covoxData[3] = 0;

    if (Config::load_monitor) AY_emu = false; // Disable AY emulation if tape load monitor mode is set

    ESPoffset = 0;

    // Readjust output pwmaudio frequency if needed
    if (prevAudio_freq != Audio_freq[ESP_delay]) {
        // printf("Resetting pwmaudio to freq: %d\n",Audio_freq);
        esp_err_t res;
        res = pwm_audio_set_sample_rate(Audio_freq[ESP_delay]);
        if (res != ESP_OK) {
            printf("Can't set sample rate\n");
        }
    }

    // Reset AY emulation
    AySound::init();
    AySound::set_sound_format(Audio_freq[ESP_delay],1,8);
    AySound::set_stereo(AYEMU_MONO,NULL);
    AySound::reset();

    aud_active_sources = (Config::Covox & 0x01) | (AY_emu << 1);

    CPU::reset();

    // Reconfigura carga de cinta real
    RealTape_realloc_buffers(ESPeccy::Audio_freq[1], ESPeccy::samplesPerFrame, CPU::statesInFrame);

}

//=======================================================================================
// KEYBOARD / KEMPSTON
//=======================================================================================
IRAM_ATTR bool ESPeccy::readKbd(fabgl::VirtualKeyItem *NextKey, uint8_t mode) {

    bool r = PS2Controller.keyboard()->getNextVirtualKey(NextKey);

    // Global keys
    if (NextKey->down) {

        if (ps2kbd) {

            // Start ZXUNOPS2
            if (Config::zxunops2 && !ZXKeyb::Exists) {
                fabgl::VirtualKeyItem akey;
                akey.vk = fabgl::VK_NONE;
                akey.CTRL = false;
                akey.SHIFT = false;

                switch(mode) {
                    case KBDREAD_MODENORMAL:
                        // Detect and process physical kbd menu key combinations
                        // CS+SS+<1..0> -> F1..F10 Keys, CS+SS+Q -> F11, CS+SS+W -> F12, CS+SS+S -> Capture screen
                        if (NextKey->SHIFT && NextKey->CTRL) {
                                 if (NextKey->vk == fabgl::VK_1)                               { akey.SHIFT = false; akey.CTRL = false; akey.vk = fabgl::VK_F1; }
                            else if (NextKey->vk == fabgl::VK_2)                               { akey.SHIFT = false; akey.CTRL = false; akey.vk = fabgl::VK_F2; }
                            else if (NextKey->vk == fabgl::VK_3)                               { akey.SHIFT = false; akey.CTRL = false; akey.vk = fabgl::VK_F3; }
                            else if (NextKey->vk == fabgl::VK_4)                               { akey.SHIFT = false; akey.CTRL = false; akey.vk = fabgl::VK_F4; }
                            else if (NextKey->vk == fabgl::VK_5)                               { akey.SHIFT = false; akey.CTRL = false; akey.vk = fabgl::VK_F5; }
                            else if (NextKey->vk == fabgl::VK_6)                               { akey.SHIFT = false; akey.CTRL = false; akey.vk = fabgl::VK_F6; }
                            else if (NextKey->vk == fabgl::VK_7)                               { akey.SHIFT = false; akey.CTRL = false; akey.vk = fabgl::VK_F7; }
                            else if (NextKey->vk == fabgl::VK_8)                               { akey.SHIFT = false; akey.CTRL = false; akey.vk = fabgl::VK_F8; }
                            else if (NextKey->vk == fabgl::VK_9)                               { akey.SHIFT = false; akey.CTRL = false; akey.vk = fabgl::VK_F9; }
                            else if (NextKey->vk == fabgl::VK_0)                               { akey.SHIFT = false; akey.CTRL = false; akey.vk = fabgl::VK_F10; }
                            else if (NextKey->vk == fabgl::VK_Q || NextKey->vk == fabgl::VK_q) { akey.SHIFT = false; akey.CTRL = false; akey.vk = fabgl::VK_F11; }
                            else if (NextKey->vk == fabgl::VK_W || NextKey->vk == fabgl::VK_w) { akey.SHIFT = false; akey.CTRL = false; akey.vk = fabgl::VK_F12; }
                            else if (NextKey->vk == fabgl::VK_P || NextKey->vk == fabgl::VK_p) { akey.SHIFT = false; akey.CTRL = false; akey.vk = fabgl::VK_PAUSE; }         // P -> Pause
                            else if (NextKey->vk == fabgl::VK_I || NextKey->vk == fabgl::VK_i) { akey.SHIFT = true;  akey.CTRL = false; akey.vk = fabgl::VK_F1; }            // I -> INFO
                            else if (NextKey->vk == fabgl::VK_E || NextKey->vk == fabgl::VK_e) { akey.SHIFT = true;  akey.CTRL = false; akey.vk = fabgl::VK_F6; }            // E -> Eject tape
                            else if (NextKey->vk == fabgl::VK_R || NextKey->vk == fabgl::VK_r) { akey.SHIFT = false; akey.CTRL = true;  akey.vk = fabgl::VK_F11; }           // R -> Reset to TR-DOS
                            else if (NextKey->vk == fabgl::VK_T || NextKey->vk == fabgl::VK_t) { akey.SHIFT = false; akey.CTRL = true;  akey.vk = fabgl::VK_F2; }            // T -> Turbo
                            else if (NextKey->vk == fabgl::VK_B || NextKey->vk == fabgl::VK_b) { akey.SHIFT = true;  akey.CTRL = false; akey.vk = fabgl::VK_PRINTSCREEN; }   // B -> BMP capture
                            else if (NextKey->vk == fabgl::VK_O || NextKey->vk == fabgl::VK_o) { akey.SHIFT = false; akey.CTRL = true;  akey.vk = fabgl::VK_F9; }            // O -> Poke
                            else if (NextKey->vk == fabgl::VK_Y || NextKey->vk == fabgl::VK_y) { akey.SHIFT = true;  akey.CTRL = false; akey.vk = fabgl::VK_F3; }            // Y -> Cartridge
                            else if (NextKey->vk == fabgl::VK_U || NextKey->vk == fabgl::VK_u) { akey.SHIFT = true;  akey.CTRL = false; akey.vk = fabgl::VK_F9; }            // U -> Cheats
                            else if (NextKey->vk == fabgl::VK_N || NextKey->vk == fabgl::VK_n) { akey.SHIFT = false; akey.CTRL = true;  akey.vk = fabgl::VK_F10; }           // N -> NMI
                            else if (NextKey->vk == fabgl::VK_K || NextKey->vk == fabgl::VK_k) { akey.SHIFT = false; akey.CTRL = true;  akey.vk = fabgl::VK_F1; }            // K -> Help / Kbd layout
                            else if (NextKey->vk == fabgl::VK_S || NextKey->vk == fabgl::VK_s) { akey.SHIFT = true;  akey.CTRL = false; akey.vk = fabgl::VK_F2; }            // S -> Save snapshot
                            else if (NextKey->vk == fabgl::VK_D || NextKey->vk == fabgl::VK_d) { akey.SHIFT = true;  akey.CTRL = false; akey.vk = fabgl::VK_F5; }            // D -> Load .SCR
                            else if (NextKey->vk == fabgl::VK_G || NextKey->vk == fabgl::VK_g) { akey.SHIFT = false; akey.CTRL = false; akey.vk = fabgl::VK_PRINTSCREEN; }   // G -> Capture SCR
                            else if (NextKey->vk == fabgl::VK_Z || NextKey->vk == fabgl::VK_z) { akey.SHIFT = false; akey.CTRL = true;  akey.vk = fabgl::VK_F5; }            // Z -> CenterH
                            else if (NextKey->vk == fabgl::VK_X || NextKey->vk == fabgl::VK_x) { akey.SHIFT = false; akey.CTRL = true;  akey.vk = fabgl::VK_F6; }            // X -> CenterH
                            else if (NextKey->vk == fabgl::VK_C || NextKey->vk == fabgl::VK_c) { akey.SHIFT = false; akey.CTRL = true;  akey.vk = fabgl::VK_F7; }            // C -> CenterV
                            else if (NextKey->vk == fabgl::VK_V || NextKey->vk == fabgl::VK_v) { akey.SHIFT = false; akey.CTRL = true;  akey.vk = fabgl::VK_F8; }            // V -> CenterV
                        }
                        break;

                    case KBDREAD_MODEFILEBROWSER:
                        if (NextKey->SHIFT && !NextKey->CTRL) { // CS + !SS
                                 if (NextKey->vk == fabgl::VK_RETURN)                               { akey.vk = fabgl::VK_JOY1C; } // CS + ENTER -> SPACE / SELECT
                            else if (NextKey->vk == fabgl::VK_SPACE)                                { akey.vk = fabgl::VK_ESCAPE; } // BREAK -> ESCAPE
                            else if (NextKey->vk == fabgl::VK_RIGHTPAREN)                           { akey.vk = fabgl::VK_BACKSPACE; } // CS + 0 -> BACKSPACE
                            else if (NextKey->vk == fabgl::VK_AMPERSAND)                            { akey.vk = fabgl::VK_UP; } // 7 -> VK_UP
                            else if (NextKey->vk == fabgl::VK_CARET)                                { akey.vk = fabgl::VK_DOWN; } // 6 -> VK_DOWN
                            else if (NextKey->vk == fabgl::VK_PERCENT)                              { akey.vk = fabgl::VK_LEFT; } // 5 -> VK_LEFT
                            else if (NextKey->vk == fabgl::VK_ASTERISK)                             { akey.vk = fabgl::VK_RIGHT; } // 8 -> VK_RIGHT
                            // invalid shift keys for filebrowser
                            else if (NextKey->vk == fabgl::VK_EXCLAIM || // 1
                                     NextKey->vk == fabgl::VK_AT || // 2
                                     NextKey->vk == fabgl::VK_HASH || // 3
                                     NextKey->vk == fabgl::VK_DOLLAR || // 4
                                     NextKey->vk == fabgl::VK_LEFTPAREN // 9
                                    )
                                    NextKey->vk = fabgl::VK_NONE;
                        }
                        else
                        if (NextKey->SHIFT && NextKey->CTRL) {
                                 if (NextKey->vk == fabgl::VK_7)                                    { akey.vk = fabgl::VK_PAGEUP; } // 7 -> VK_PAGEUP
                            else if (NextKey->vk == fabgl::VK_6)                                    { akey.vk = fabgl::VK_PAGEDOWN; } // 6 -> VK_PAGEDOWN
                            else if (NextKey->vk == fabgl::VK_5)                                    { akey.SHIFT = true; akey.vk = fabgl::VK_LEFT; } // 5 -> SS + VK_LEFT
                            else if (NextKey->vk == fabgl::VK_8)                                    { akey.SHIFT = true; akey.vk = fabgl::VK_RIGHT; } // 8 -> SS + VK_RIGHT
                            else if (NextKey->vk == fabgl::VK_G || NextKey->vk == fabgl::VK_g)      { akey.vk = fabgl::VK_PRINTSCREEN; } // G -> USE THIS SCR
                            else if (NextKey->vk == fabgl::VK_N || NextKey->vk == fabgl::VK_n)      { akey.vk = fabgl::VK_F2; } // N -> NUEVO / RENOMBRAR
                            else if (NextKey->vk == fabgl::VK_R || NextKey->vk == fabgl::VK_r)      { akey.SHIFT = true; akey.vk = fabgl::VK_F2; } // R -> NUEVO Con ROM
                            else if (NextKey->vk == fabgl::VK_M || NextKey->vk == fabgl::VK_m)      { akey.vk = fabgl::VK_F6; } // M -> MOVE / MOVER
                            else if (NextKey->vk == fabgl::VK_D || NextKey->vk == fabgl::VK_d)      { akey.vk = fabgl::VK_F8; } // D -> DELETE / BORRAR
                            else if (NextKey->vk == fabgl::VK_F || NextKey->vk == fabgl::VK_f)      { akey.vk = fabgl::VK_F3; } // F -> FIND / BUSQUEDA
                            else if (NextKey->vk == fabgl::VK_Z || NextKey->vk == fabgl::VK_z)      { akey.CTRL = true; akey.vk = fabgl::VK_LEFT; } // Z -> CTRL + LEFT
                            else if (NextKey->vk == fabgl::VK_X || NextKey->vk == fabgl::VK_x)      { akey.CTRL = true; akey.vk = fabgl::VK_RIGHT; } // X -> CTRL + RIGHT
                            else if (NextKey->vk == fabgl::VK_C || NextKey->vk == fabgl::VK_c)      { akey.CTRL = true; akey.vk = fabgl::VK_UP; } // C -> CTRL + UP
                            else if (NextKey->vk == fabgl::VK_V || NextKey->vk == fabgl::VK_v)      { akey.CTRL = true; akey.vk = fabgl::VK_DOWN; } // V -> CTRL + DOWN
                        }
                        break;

                    case KBDREAD_MODEINPUT:
                        //akey.CTRL = NextKey->CTRL;
                        //akey.SHIFT = NextKey->SHIFT;

                        if (NextKey->SHIFT && !NextKey->CTRL) { // CS
                                 if (NextKey->vk == fabgl::VK_AMPERSAND)                { akey.vk = fabgl::VK_END; } // 7 -> END
                            else if (NextKey->vk == fabgl::VK_CARET)                    { akey.vk = fabgl::VK_HOME; } // 6 -> HOME
                            else if (NextKey->vk == fabgl::VK_PERCENT)                  { akey.vk = fabgl::VK_LEFT; } // 5 -> LEFT
                            else if (NextKey->vk == fabgl::VK_ASTERISK)                 { akey.vk = fabgl::VK_RIGHT; } // 8 -> RIGHT
                            else if (NextKey->vk == fabgl::VK_RIGHTPAREN)               { akey.vk = fabgl::VK_BACKSPACE; } // CS + 0 -> BACKSPACE
                            else if (NextKey->vk == fabgl::VK_SPACE)                    { akey.vk = fabgl::VK_ESCAPE; } // CS + SPACE && !SS -> ESCAPE
                            // invalid shift keys for input
                            else if (NextKey->vk == fabgl::VK_EXCLAIM ||
                                     NextKey->vk == fabgl::VK_AT ||
                                     NextKey->vk == fabgl::VK_HASH ||
                                     NextKey->vk == fabgl::VK_DOLLAR ||
                                     NextKey->vk == fabgl::VK_LEFTPAREN
                                    )
                                    NextKey->vk = fabgl::VK_NONE;
                        }
                        break;

                    case KBDREAD_MODEINPUTMULTI:
                        if (NextKey->SHIFT && !NextKey->CTRL) { // CS
                                 if (NextKey->vk == fabgl::VK_AMPERSAND)                { akey.vk = fabgl::VK_UP; } // 7 -> VK_UP
                            else if (NextKey->vk == fabgl::VK_CARET)                    { akey.vk = fabgl::VK_DOWN; } // 6 -> VK_DOWN
                            else if (NextKey->vk == fabgl::VK_PERCENT)                  { akey.vk = fabgl::VK_LEFT; } // 5 -> LEFT
                            else if (NextKey->vk == fabgl::VK_ASTERISK)                 { akey.vk = fabgl::VK_RIGHT; } // 8 -> RIGHT
                            else if (NextKey->vk == fabgl::VK_RIGHTPAREN)               { akey.vk = fabgl::VK_BACKSPACE; } // CS + 0 -> BACKSPACE
                            else if (NextKey->vk == fabgl::VK_SPACE)                    { akey.vk = fabgl::VK_ESCAPE; } // CS + SPACE && !SS -> ESCAPE
                            // invalid shift keys for input
                            else if (NextKey->vk == fabgl::VK_EXCLAIM ||
                                     NextKey->vk == fabgl::VK_AT ||
                                     NextKey->vk == fabgl::VK_HASH ||
                                     NextKey->vk == fabgl::VK_DOLLAR ||
                                     NextKey->vk == fabgl::VK_LEFTPAREN
                                    )
                                    NextKey->vk = fabgl::VK_NONE;
                        }
                        break;

                    case KBDREAD_MODEKBDLAYOUT:
                             if (NextKey->vk == fabgl::VK_SPACE)                            { akey.vk = fabgl::VK_ESCAPE; } // SPACE -> ESCAPE
                        else if (NextKey->vk == fabgl::VK_RETURN)                           { akey.vk = fabgl::VK_ESCAPE; } // RETURN -> ESCAPE
                        else if (NextKey->SHIFT && NextKey->CTRL) { // CS + SS
                            if (NextKey->vk == fabgl::VK_K || NextKey->vk == fabgl::VK_k)   { akey.vk = fabgl::VK_ESCAPE; }
                        }
                        break;

                    case KBDREAD_MODEDIALOG:
                        if (NextKey->SHIFT && !NextKey->CTRL) { // CS
                                 if (NextKey->vk == fabgl::VK_PERCENT)                  { akey.vk = fabgl::VK_LEFT; } // 5 -> LEFT
                            else if (NextKey->vk == fabgl::VK_ASTERISK)                 { akey.vk = fabgl::VK_RIGHT; } // 8 -> RIGHT
                            else if (NextKey->vk == fabgl::VK_SPACE)                    { akey.vk = fabgl::VK_ESCAPE; } // CS + SPACE && !SS -> ESCAPE
                        }
                        else if (NextKey->SHIFT && NextKey->CTRL) { // CS + SS
                            if (NextKey->vk == fabgl::VK_I || NextKey->vk == fabgl::VK_i)   { akey.vk = fabgl::VK_F1; } // For exit from HWInfo
                        }
                        break;

                }

                if (akey.vk != fabgl::VK_NONE) {
                    NextKey->vk = akey.vk;
                    NextKey->CTRL = akey.CTRL;
                    NextKey->SHIFT = akey.SHIFT;
                }

            }
            // End ZXUNOPS2

            if (mode == KBDREAD_MODEBIOS) {
                if (NextKey->SHIFT && !NextKey->CTRL) { // CS
                         if (NextKey->vk == fabgl::VK_AMPERSAND)                { NextKey->SHIFT = false; NextKey->vk = fabgl::VK_UP; } // 7 -> VK_UP
                    else if (NextKey->vk == fabgl::VK_CARET)                    { NextKey->SHIFT = false; NextKey->vk = fabgl::VK_DOWN; } // 6 -> VK_DOWN
                    else if (NextKey->vk == fabgl::VK_PERCENT)                  { NextKey->SHIFT = false; NextKey->vk = fabgl::VK_LEFT; } // 5 -> LEFT
                    else if (NextKey->vk == fabgl::VK_ASTERISK)                 { NextKey->SHIFT = false; NextKey->vk = fabgl::VK_RIGHT; } // 8 -> RIGHT
                    else if (NextKey->vk == fabgl::VK_RIGHTPAREN)               { NextKey->SHIFT = false; NextKey->vk = fabgl::VK_BACKSPACE; } // CS + 0 -> BACKSPACE
                    else if (NextKey->vk == fabgl::VK_SPACE)                    { NextKey->SHIFT = false; NextKey->vk = fabgl::VK_ESCAPE; } // CS + SPACE && !SS -> ESCAPE
                    // invalid shift keys for input
                    else if (NextKey->vk == fabgl::VK_EXCLAIM ||
                             NextKey->vk == fabgl::VK_AT ||
                             NextKey->vk == fabgl::VK_HASH ||
                             NextKey->vk == fabgl::VK_DOLLAR ||
                             NextKey->vk == fabgl::VK_LEFTPAREN
                            )
                            NextKey->vk = fabgl::VK_NONE;
                }
                else
                if (!NextKey->SHIFT && NextKey->CTRL) { // SS
                    if (NextKey->vk == fabgl::VK_S || NextKey->vk == fabgl::VK_s) { NextKey->CTRL = false; NextKey->vk = fabgl::VK_F10; }
                }
            }
        }

        if (NextKey->SHIFT && !NextKey->CTRL && NextKey->vk == fabgl::VK_PRINTSCREEN) { // Capture framebuffer to BMP file in SD Card (thx @dcrespo3d!)
            CaptureToBmp();
            r = false;
        }

        if (NextKey->vk == fabgl::VK_SCROLLLOCK) { // Change CursorAsJoy setting
            Config::CursorAsJoy = !Config::CursorAsJoy;
            if(ps2kbd)
                PS2Controller.keyboard()->setLEDs(false,false,Config::CursorAsJoy);
            if(ps2kbd2)
                PS2Controller.keybjoystick()->setLEDs(false, false, Config::CursorAsJoy);
            Config::save("CursorAsJoy");
            r = false;
        }

    }

    return r;
}

//
// Read second ps/2 port and inject on first queue
//
IRAM_ATTR void ESPeccy::readKbdJoy() {

    if (ps2kbd2) {

        fabgl::VirtualKeyItem NextKey;
        auto KbdJoy = PS2Controller.keybjoystick();

        while (KbdJoy->virtualKeyAvailable()) {
            PS2Controller.keybjoystick()->getNextVirtualKey(&NextKey);
            ESPeccy::PS2Controller.keyboard()->injectVirtualKey(NextKey.vk, NextKey.down, false);
        }

    }

}

fabgl::VirtualKey ESPeccy::JoyVKTranslation[24];
//     fabgl::VK_FULLER_LEFT, // Left
//     fabgl::VK_FULLER_RIGHT, // Right
//     fabgl::VK_FULLER_UP, // Up
//     fabgl::VK_FULLER_DOWN, // Down
//     fabgl::VK_S, // Start
//     fabgl::VK_M, // Mode
//     fabgl::VK_FULLER_FIRE, // A
//     fabgl::VK_9, // B
//     fabgl::VK_SPACE, // C
//     fabgl::VK_X, // X
//     fabgl::VK_Y, // Y
//     fabgl::VK_Z, // Z

fabgl::VirtualKey ESPeccy::VK_ESPECCY_FIRE1 = fabgl::VK_NONE;
fabgl::VirtualKey ESPeccy::VK_ESPECCY_FIRE2 = fabgl::VK_NONE;
fabgl::VirtualKey ESPeccy::VK_ESPECCY_TAB = fabgl::VK_TAB;
fabgl::VirtualKey ESPeccy::VK_ESPECCY_GRAVEACCENT = fabgl::VK_GRAVEACCENT;

int ESPeccy::zxDelay = 0;

int32_t ESPeccy::mouseX = 0;
int32_t ESPeccy::mouseY = 0;
bool ESPeccy::mouseButtonL = 0;
bool ESPeccy::mouseButtonR = 0;

static uint8_t PS2cols[8] = { 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf };

IRAM_ATTR void ESPeccy::processKeyboard() {

    bool r = false;

    ESPeccy::sync_realtape = false;

    readKbdJoy();

    if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead();
    else
    if (!booting && !gpio_get_level((gpio_num_t)GPIO_NUM_36)) { // Se deshabilita durante el booteo por cualquier posible futuro conflicto

        bool shift_down = false, ctrl_down = false;

        fabgl::VirtualKey injectKey = fabgl::VK_NONE;

        switch(Config::io36button) {
            case BTN_ASSIGN_RESET:
                injectKey = fabgl::VK_F11;
                break;

            case BTN_ASSIGN_NMI:
                ctrl_down = true;
                injectKey = fabgl::VK_F10;
                break;

            case BTN_ASSIGN_CHEATS:
                shift_down = true;
                injectKey = fabgl::VK_F9;
                break;

            case BTN_ASSIGN_POKE:
                ctrl_down = true;
                injectKey = fabgl::VK_F9;
                break;

            case BTN_ASSIGN_STATS:
                injectKey = fabgl::VK_F8;
                break;

            case BTN_ASSIGN_MENU:
                injectKey = fabgl::VK_F1;
                break;

        }

        if (injectKey != fabgl::VK_NONE) {
            if (shift_down||ctrl_down) {
                VirtualKeyItem vki;
                vki.vk = injectKey;
                vki.down = true;
                vki.SHIFT = shift_down;
                vki.CTRL = ctrl_down;
                ESPeccy::PS2Controller.keyboard()->injectVirtualKey(vki, false);
                vki.down = false;
                ESPeccy::PS2Controller.keyboard()->injectVirtualKey(vki, false);
            } else {
                ESPeccy::PS2Controller.keyboard()->injectVirtualKey(injectKey, true, false);
                ESPeccy::PS2Controller.keyboard()->injectVirtualKey(injectKey, false, false);
            }
        }
    }

    auto Kbd = PS2Controller.keyboard();
    fabgl::VirtualKeyItem NextKey;
    fabgl::VirtualKey KeytoESP;
    bool j[10] = { true, true, true, true, true, true, true, true, true, true };
    bool jShift = true;

    while (Kbd->virtualKeyAvailable()) {

        r = readKbd(&NextKey);

        if (r) {

            if (!NextKey.SHIFT && !NextKey.CTRL && NextKey.vk == fabgl::VK_PRINTSCREEN) {
                if (Tape::tapeSaveName=="none") {
                    OSD::osdCenteredMsg(OSD_TAPE_SELECT_ERR[Config::lang], LEVEL_WARN);
                } else {
                    OSD::saveSCR(Tape::tapeSaveName, (uint32_t *)(MemESP::videoLatch ? MemESP::ram[7] : MemESP::ram[5]));
                }
                continue;
            }

            KeytoESP = NextKey.vk;

            if (KeytoESP >= fabgl::VK_JOY1LEFT && KeytoESP <= fabgl::VK_JOY2Z) {
                // printf("KeytoESP: %d\n",KeytoESP);
                ESPeccy::PS2Controller.keyboard()->injectVirtualKey(JoyVKTranslation[KeytoESP - 248], NextKey.down, false);
                continue;
            }

            if (NextKey.down && ((KeytoESP >= fabgl::VK_F1 && KeytoESP <= fabgl::VK_F12) || KeytoESP == fabgl::VK_PAUSE || KeytoESP == fabgl::VK_VOLUMEUP || KeytoESP == fabgl::VK_VOLUMEDOWN || KeytoESP == fabgl::VK_VOLUMEMUTE)) {

                int64_t osd_start = esp_timer_get_time();

                OSD::do_OSD(KeytoESP, NextKey.CTRL, NextKey.SHIFT);

                // sync real tape is needed
                if (ESPeccy::sync_realtape && RealTape_enabled) {
                    ESPeccy::sync_realtape = false;
                    RealTape_pause();
                    RealTape_start();
                }

                Kbd->emptyVirtualKeyQueue();

                // Set all zx keys as not pressed
                for (uint8_t i = 0; i < 8; i++) ZXKeyb::ZXcols[i] = 0xbf;
                zxDelay = 15;

                // totalseconds = 0;
                // totalsecondsnodelay = 0;
                // VIDEO::framecnt = 0;

                // Refresh border
                VIDEO::brdnextframe = true;

                ESPeccy::ts_start += esp_timer_get_time() - osd_start;

                return;

            }

            // Reset keys
            if (NextKey.down && NextKey.LALT) {
                if (NextKey.CTRL) {
                    if (KeytoESP == fabgl::VK_DELETE) {
                        // printf("Ctrl + Alt + Supr!\n");
                        // ESP host reset
                        Config::rom_file = NO_ROM_FILE;
                        Config::save("rom");

                        Config::ram_file = NO_RAM_FILE;
                        Config::save("ram");
                        OSD::esp_hard_reset();
                    } else if (KeytoESP == fabgl::VK_BACKSPACE) {
                        // printf("Ctrl + Alt + backSpace!\n");
                        // Hard
                        Config::ram_file = NO_RAM_FILE;
                        Config::last_ram_file = NO_RAM_FILE;

                        if (Config::last_rom_file != NO_ROM_FILE) {
                            if ( FileUtils::isSDReady() ) ROMLoad::load(Config::last_rom_file);
                            Config::rom_file = Config::last_rom_file;
                        } else
                            ESPeccy::reset();
                        return;
                    }
                } else if (KeytoESP == fabgl::VK_BACKSPACE) {
                    // printf("Alt + backSpace!\n");
                    // Soft reset
                    if (Config::last_ram_file != NO_RAM_FILE) {
                        LoadSnapshot(Config::last_ram_file,"","",0xff);
                        OSD::LoadCheatFile(Config::last_ram_file);
                        Config::ram_file = Config::last_ram_file;

                    } else {
                        // Clear Cheat data
                        CheatMngr::closeCheatFile();
                        if (Config::last_rom_file != NO_ROM_FILE) {
                            if ( FileUtils::isSDReady() ) ROMLoad::load(Config::last_rom_file);
                            Config::rom_file = Config::last_rom_file;
                        } else
                            ESPeccy::reset();
                    }
                    return;
                }
            }

            if (Config::joystick1 == JOY_KEMPSTON || Config::joystick2 == JOY_KEMPSTON || Config::joyPS2 == JOYPS2_KEMPSTON) Ports::port[0x1f] = 0;
            if (Config::joystick1 == JOY_FULLER || Config::joystick2 == JOY_FULLER || Config::joyPS2 == JOYPS2_FULLER) Ports::port[0x7f] = 0xff;

            if (Config::joystick1 == JOY_KEMPSTON || Config::joystick2 == JOY_KEMPSTON) {

                for (int i = fabgl::VK_KEMPSTON_RIGHT; i <= fabgl::VK_KEMPSTON_ALTFIRE; i++)
                    if (Kbd->isVKDown((fabgl::VirtualKey) i))
                        bitWrite(Ports::port[0x1f], i - fabgl::VK_KEMPSTON_RIGHT, 1);

            }

            if (Config::joystick1 == JOY_FULLER || Config::joystick2 == JOY_FULLER) {

                // Fuller
                if (Kbd->isVKDown(fabgl::VK_FULLER_RIGHT)) {
                    bitWrite(Ports::port[0x7f], 3, 0);
                }

                if (Kbd->isVKDown(fabgl::VK_FULLER_LEFT)) {
                    bitWrite(Ports::port[0x7f], 2, 0);
                }

                if (Kbd->isVKDown(fabgl::VK_FULLER_DOWN)) {
                    bitWrite(Ports::port[0x7f], 1, 0);
                }

                if (Kbd->isVKDown(fabgl::VK_FULLER_UP)) {
                    bitWrite(Ports::port[0x7f], 0, 0);
                }

                if (Kbd->isVKDown(fabgl::VK_FULLER_FIRE)) {
                    bitWrite(Ports::port[0x7f], 7, 0);
                }

            }

            jShift = !NextKey.SHIFT;

            if (Config::CursorAsJoy) {

                // Kempston Joystick emulation
                if (Config::joyPS2 == JOYPS2_KEMPSTON) {

                    if (Kbd->isVKDown(fabgl::VK_RIGHT)) {
                        j[8] = jShift;
                        bitWrite(Ports::port[0x1f], 0, j[8]);
                    }

                    if (Kbd->isVKDown(fabgl::VK_LEFT)) {
                        j[5] = jShift;
                        bitWrite(Ports::port[0x1f], 1, j[5]);
                    }

                    if (Kbd->isVKDown(fabgl::VK_DOWN)) {
                        j[6] = jShift;
                        bitWrite(Ports::port[0x1f], 2, j[6]);
                    }

                    if (Kbd->isVKDown(fabgl::VK_UP)) {
                        j[7] = jShift;
                        bitWrite(Ports::port[0x1f], 3, j[7]);
                    }

                // Fuller Joystick emulation
                } else if (Config::joyPS2 == JOYPS2_FULLER) {

                    if (Kbd->isVKDown(fabgl::VK_RIGHT)) {
                        j[8] = jShift;
                        bitWrite(Ports::port[0x7f], 3, !j[8]);
                    }

                    if (Kbd->isVKDown(fabgl::VK_LEFT)) {
                        j[5] = jShift;
                        bitWrite(Ports::port[0x7f], 2, !j[5]);
                    }

                    if (Kbd->isVKDown(fabgl::VK_DOWN)) {
                        j[6] = jShift;
                        bitWrite(Ports::port[0x7f], 1, !j[6]);
                    }

                    if (Kbd->isVKDown(fabgl::VK_UP)) {
                        j[7] = jShift;
                        bitWrite(Ports::port[0x7f], 0, !j[7]);
                    }

                } else if (Config::joyPS2 == JOYPS2_CURSOR) {

                    j[5] =  !Kbd->isVKDown(fabgl::VK_LEFT);
                    j[8] =  !Kbd->isVKDown(fabgl::VK_RIGHT);
                    j[7] =  !Kbd->isVKDown(fabgl::VK_UP);
                    j[6] =  !Kbd->isVKDown(fabgl::VK_DOWN);

                } else if (Config::joyPS2 == JOYPS2_SINCLAIR1) { // Right Sinclair

                    if (jShift) {
                        j[9] =  !Kbd->isVKDown(fabgl::VK_UP);
                        j[8] =  !Kbd->isVKDown(fabgl::VK_DOWN);
                        j[7] =  !Kbd->isVKDown(fabgl::VK_RIGHT);
                        j[6] =  !Kbd->isVKDown(fabgl::VK_LEFT);
                    } else {
                        j[5] =  !Kbd->isVKDown(fabgl::VK_LEFT);
                        j[8] =  !Kbd->isVKDown(fabgl::VK_RIGHT);
                        j[7] =  !Kbd->isVKDown(fabgl::VK_UP);
                        j[6] =  !Kbd->isVKDown(fabgl::VK_DOWN);
                    }

                } else if (Config::joyPS2 == JOYPS2_SINCLAIR2) { // Left Sinclair

                    if (jShift) {
                        j[4] =  !Kbd->isVKDown(fabgl::VK_UP);
                        j[3] =  !Kbd->isVKDown(fabgl::VK_DOWN);
                        j[2] =  !Kbd->isVKDown(fabgl::VK_RIGHT);
                        j[1] =  !Kbd->isVKDown(fabgl::VK_LEFT);
                    } else {
                        j[5] =  !Kbd->isVKDown(fabgl::VK_LEFT);
                        j[8] =  !Kbd->isVKDown(fabgl::VK_RIGHT);
                        j[7] =  !Kbd->isVKDown(fabgl::VK_UP);
                        j[6] =  !Kbd->isVKDown(fabgl::VK_DOWN);
                    }

                }

            } else {

                // Cursor Keys
                if (Kbd->isVKDown(fabgl::VK_RIGHT)) {
                    jShift = false;
                    j[8] = jShift;
                }

                if (Kbd->isVKDown(fabgl::VK_LEFT)) {
                    jShift = false;
                    j[5] = jShift;
                }

                if (Kbd->isVKDown(fabgl::VK_DOWN)) {
                    jShift = false;
                    j[6] = jShift;
                }

                if (Kbd->isVKDown(fabgl::VK_UP)) {
                    jShift = false;
                    j[7] = jShift;
                }

            }

            // Keypad PS/2 Joystick emulation
            if (Config::joyPS2 == JOYPS2_KEMPSTON) {

                if (Kbd->isVKDown(fabgl::VK_KP_RIGHT)) {
                    bitWrite(Ports::port[0x1f], 0, 1);
                }

                if (Kbd->isVKDown(fabgl::VK_KP_LEFT)) {
                    bitWrite(Ports::port[0x1f], 1, 1);
                }

                if (Kbd->isVKDown(fabgl::VK_KP_DOWN) || Kbd->isVKDown(fabgl::VK_KP_CENTER)) {
                    bitWrite(Ports::port[0x1f], 2, 1);
                }

                if (Kbd->isVKDown(fabgl::VK_KP_UP)) {
                    bitWrite(Ports::port[0x1f], 3, 1);
                }

                if (Kbd->isVKDown(fabgl::VK_RALT) || Kbd->isVKDown(VK_ESPECCY_FIRE1)) {
                    bitWrite(Ports::port[0x1f], 4, 1);
                }

                if (Kbd->isVKDown(fabgl::VK_SLASH) || /*Kbd->isVKDown(fabgl::VK_QUESTION) ||*/Kbd->isVKDown(fabgl::VK_RGUI) || Kbd->isVKDown(fabgl::VK_APPLICATION) || Kbd->isVKDown(VK_ESPECCY_FIRE2)) {
                    bitWrite(Ports::port[0x1f], 5, 1);
                }

            } else if (Config::joyPS2 == JOYPS2_FULLER) {

                if (Kbd->isVKDown(fabgl::VK_KP_RIGHT)) {
                    bitWrite(Ports::port[0x7f], 3, 0);
                }

                if (Kbd->isVKDown(fabgl::VK_KP_LEFT)) {
                    bitWrite(Ports::port[0x7f], 2, 0);
                }

                if (Kbd->isVKDown(fabgl::VK_KP_DOWN) || Kbd->isVKDown(fabgl::VK_KP_CENTER)) {
                    bitWrite(Ports::port[0x7f], 1, 0);
                }

                if (Kbd->isVKDown(fabgl::VK_KP_UP)) {
                    bitWrite(Ports::port[0x7f], 0, 0);
                }

                if (Kbd->isVKDown(fabgl::VK_RALT) || Kbd->isVKDown(VK_ESPECCY_FIRE1)) {
                    bitWrite(Ports::port[0x7f], 7, 0);
                }

            } else if (Config::joyPS2 == JOYPS2_CURSOR) {

                if (Kbd->isVKDown(fabgl::VK_KP_LEFT)) {
                    jShift = true;
                    j[5] = false;
                };

                if (Kbd->isVKDown(fabgl::VK_KP_RIGHT)) {
                    jShift = true;
                    j[8] = false;
                };

                if (Kbd->isVKDown(fabgl::VK_KP_UP)) {
                    jShift = true;
                    j[7] = false;
                };

                if (Kbd->isVKDown(fabgl::VK_KP_DOWN) || Kbd->isVKDown(fabgl::VK_KP_CENTER)) {
                    jShift = true;
                    j[6] = false;
                };

                if (Kbd->isVKDown(fabgl::VK_RALT) || Kbd->isVKDown(VK_ESPECCY_FIRE1)) {
                    jShift = true;
                    j[0] = false;
                };

            } else if (Config::joyPS2 == JOYPS2_SINCLAIR1) { // Right Sinclair

                if (Kbd->isVKDown(fabgl::VK_KP_LEFT)) {
                    jShift = true;
                    j[6] = false;
                };

                if (Kbd->isVKDown(fabgl::VK_KP_RIGHT)) {
                    jShift = true;
                    j[7] = false;
                };

                if (Kbd->isVKDown(fabgl::VK_KP_UP)) {
                    jShift = true;
                    j[9] = false;
                };

                if (Kbd->isVKDown(fabgl::VK_KP_DOWN) || Kbd->isVKDown(fabgl::VK_KP_CENTER)) {
                    jShift = true;
                    j[8] = false;
                };

                if (Kbd->isVKDown(fabgl::VK_RALT) || Kbd->isVKDown(VK_ESPECCY_FIRE1)) {
                    jShift = true;
                    j[0] = false;
                };

            } else if (Config::joyPS2 == JOYPS2_SINCLAIR2) { // Left Sinclair

                if (Kbd->isVKDown(fabgl::VK_KP_LEFT)) {
                    jShift = true;
                    j[1] = false;
                };

                if (Kbd->isVKDown(fabgl::VK_KP_RIGHT)) {
                    jShift = true;
                    j[2] = false;
                };

                if (Kbd->isVKDown(fabgl::VK_KP_UP)) {
                    jShift = true;
                    j[4] = false;
                };

                if (Kbd->isVKDown(fabgl::VK_KP_DOWN) || Kbd->isVKDown(fabgl::VK_KP_CENTER)) {
                    jShift = true;
                    j[3] = false;
                };

                if (Kbd->isVKDown(fabgl::VK_RALT) || Kbd->isVKDown(VK_ESPECCY_FIRE1)) {
                    jShift = true;
                    j[5] = false;
                };

            }

            // Check keyboard status and map it to Spectrum Ports

            bitWrite(PS2cols[0], 0, (jShift)
                & (!Kbd->isVKDown(fabgl::VK_BACKSPACE))
                & (!Kbd->isVKDown(fabgl::VK_CAPSLOCK)) // Caps lock
                &   (!Kbd->isVKDown(VK_ESPECCY_GRAVEACCENT)) // Edit
                &   (!Kbd->isVKDown(VK_ESPECCY_TAB)) // Extended mode
                &   (!Kbd->isVKDown(fabgl::VK_ESCAPE)) // Break
                ); // CAPS SHIFT
            bitWrite(PS2cols[0], 1, (!Kbd->isVKDown(fabgl::VK_Z)) & (!Kbd->isVKDown(fabgl::VK_z)));
            bitWrite(PS2cols[0], 2, (!Kbd->isVKDown(fabgl::VK_X)) & (!Kbd->isVKDown(fabgl::VK_x)));
            bitWrite(PS2cols[0], 3, (!Kbd->isVKDown(fabgl::VK_C)) & (!Kbd->isVKDown(fabgl::VK_c)));
            bitWrite(PS2cols[0], 4, (!Kbd->isVKDown(fabgl::VK_V)) & (!Kbd->isVKDown(fabgl::VK_v)));

            bitWrite(PS2cols[1], 0, (!Kbd->isVKDown(fabgl::VK_A)) & (!Kbd->isVKDown(fabgl::VK_a)));
            bitWrite(PS2cols[1], 1, (!Kbd->isVKDown(fabgl::VK_S)) & (!Kbd->isVKDown(fabgl::VK_s)));
            bitWrite(PS2cols[1], 2, (!Kbd->isVKDown(fabgl::VK_D)) & (!Kbd->isVKDown(fabgl::VK_d)));
            bitWrite(PS2cols[1], 3, (!Kbd->isVKDown(fabgl::VK_F)) & (!Kbd->isVKDown(fabgl::VK_f)));
            bitWrite(PS2cols[1], 4, (!Kbd->isVKDown(fabgl::VK_G)) & (!Kbd->isVKDown(fabgl::VK_g)));

            bitWrite(PS2cols[2], 0, (!Kbd->isVKDown(fabgl::VK_Q)) & (!Kbd->isVKDown(fabgl::VK_q)));
            bitWrite(PS2cols[2], 1, (!Kbd->isVKDown(fabgl::VK_W)) & (!Kbd->isVKDown(fabgl::VK_w)));
            bitWrite(PS2cols[2], 2, (!Kbd->isVKDown(fabgl::VK_E)) & (!Kbd->isVKDown(fabgl::VK_e)));
            bitWrite(PS2cols[2], 3, (!Kbd->isVKDown(fabgl::VK_R)) & (!Kbd->isVKDown(fabgl::VK_r)));
            bitWrite(PS2cols[2], 4, (!Kbd->isVKDown(fabgl::VK_T)) & (!Kbd->isVKDown(fabgl::VK_t)));

            bitWrite(PS2cols[3], 0, (!Kbd->isVKDown(fabgl::VK_1)) & (!Kbd->isVKDown(fabgl::VK_EXCLAIM))
                                &   (!Kbd->isVKDown(VK_ESPECCY_GRAVEACCENT)) // Edit
                                & (j[1]));
            bitWrite(PS2cols[3], 1, (!Kbd->isVKDown(fabgl::VK_2)) & (!Kbd->isVKDown(fabgl::VK_AT))
                                &   (!Kbd->isVKDown(fabgl::VK_CAPSLOCK)) // Caps lock
                                & (j[2])
                                );
            bitWrite(PS2cols[3], 2, (!Kbd->isVKDown(fabgl::VK_3)) & (!Kbd->isVKDown(fabgl::VK_HASH)) & (j[3]));
            bitWrite(PS2cols[3], 3, (!Kbd->isVKDown(fabgl::VK_4)) & (!Kbd->isVKDown(fabgl::VK_DOLLAR)) & (j[4]));
            bitWrite(PS2cols[3], 4, (!Kbd->isVKDown(fabgl::VK_5)) & (!Kbd->isVKDown(fabgl::VK_PERCENT)) & (j[5]));

            bitWrite(PS2cols[4], 0, (!Kbd->isVKDown(fabgl::VK_0)) & (!Kbd->isVKDown(fabgl::VK_RIGHTPAREN)) & (!Kbd->isVKDown(fabgl::VK_BACKSPACE)) & (j[0]));
            bitWrite(PS2cols[4], 1, !Kbd->isVKDown(fabgl::VK_9) & (!Kbd->isVKDown(fabgl::VK_LEFTPAREN)) & (j[9]));
            bitWrite(PS2cols[4], 2, (!Kbd->isVKDown(fabgl::VK_8)) & (!Kbd->isVKDown(fabgl::VK_ASTERISK)) & (j[8]));
            bitWrite(PS2cols[4], 3, (!Kbd->isVKDown(fabgl::VK_7)) & (!Kbd->isVKDown(fabgl::VK_AMPERSAND)) & (j[7]));
            bitWrite(PS2cols[4], 4, (!Kbd->isVKDown(fabgl::VK_6)) & (!Kbd->isVKDown(fabgl::VK_CARET)) & (j[6]));

            bitWrite(PS2cols[5], 0, (!Kbd->isVKDown(fabgl::VK_P)) & (!Kbd->isVKDown(fabgl::VK_p))
                                &   (!Kbd->isVKDown(fabgl::VK_QUOTE)) // Double quote
                                );
            bitWrite(PS2cols[5], 1, (!Kbd->isVKDown(fabgl::VK_O)) & (!Kbd->isVKDown(fabgl::VK_o))
                                &   (!Kbd->isVKDown(fabgl::VK_SEMICOLON)) // Semicolon
                                );
            bitWrite(PS2cols[5], 2, (!Kbd->isVKDown(fabgl::VK_I)) & (!Kbd->isVKDown(fabgl::VK_i)));
            bitWrite(PS2cols[5], 3, (!Kbd->isVKDown(fabgl::VK_U)) & (!Kbd->isVKDown(fabgl::VK_u)));
            bitWrite(PS2cols[5], 4, (!Kbd->isVKDown(fabgl::VK_Y)) & (!Kbd->isVKDown(fabgl::VK_y)));

            bitWrite(PS2cols[6], 0, !Kbd->isVKDown(fabgl::VK_RETURN));
            bitWrite(PS2cols[6], 1, (!Kbd->isVKDown(fabgl::VK_L)) & (!Kbd->isVKDown(fabgl::VK_l)));
            bitWrite(PS2cols[6], 2, (!Kbd->isVKDown(fabgl::VK_K)) & (!Kbd->isVKDown(fabgl::VK_k)));
            bitWrite(PS2cols[6], 3, (!Kbd->isVKDown(fabgl::VK_J)) & (!Kbd->isVKDown(fabgl::VK_j)));
            bitWrite(PS2cols[6], 4, (!Kbd->isVKDown(fabgl::VK_H)) & (!Kbd->isVKDown(fabgl::VK_h)));

            bitWrite(PS2cols[7], 0, !Kbd->isVKDown(fabgl::VK_SPACE)
                            &   (!Kbd->isVKDown(fabgl::VK_ESCAPE)) // Break
            );
            bitWrite(PS2cols[7], 1, (!NextKey.CTRL)
                                &   (!Kbd->isVKDown(fabgl::VK_COMMA)) // Comma
                                &   (!Kbd->isVKDown(fabgl::VK_PERIOD)) // Period
                                &   (!Kbd->isVKDown(fabgl::VK_SEMICOLON)) // Semicolon
                                &   (!Kbd->isVKDown(fabgl::VK_QUOTE)) // Double quote
                                &   (!Kbd->isVKDown(VK_ESPECCY_TAB)) // Extended mode
                                ); // SYMBOL SHIFT
            bitWrite(PS2cols[7], 2, (!Kbd->isVKDown(fabgl::VK_M)) & (!Kbd->isVKDown(fabgl::VK_m))
                                &   (!Kbd->isVKDown(fabgl::VK_PERIOD)) // Period
                                );
            bitWrite(PS2cols[7], 3, (!Kbd->isVKDown(fabgl::VK_N)) & (!Kbd->isVKDown(fabgl::VK_n))
                                &   (!Kbd->isVKDown(fabgl::VK_COMMA)) // Comma
                                );
            bitWrite(PS2cols[7], 4, (!Kbd->isVKDown(fabgl::VK_B)) & (!Kbd->isVKDown(fabgl::VK_b)));

        }

    }

    if (ZXKeyb::Exists) { // START - ZXKeyb Exists
        if (Config::realtape_gpio_num == KM_COL_0) {
            if (ZXKBD_SS) { // SS
                if (ZXKBD_CS) { // CS
                    if (ZXKBD_J) { // j
                        // enabled REAL TAPE LOAD mode
                        OSD::osdCenteredMsg("REAL TAPE LOAD ENABLED", LEVEL_INFO, 1000);
                        RealTape_start();
                    }
                } else {
                    if (RealTape_enabled && ZXKBD_S) { // s
                        // disable REAL TAPE LOAD mode (STOP LOADING)
                        OSD::osdCenteredMsg("REAL TAPE LOAD DISABLED", LEVEL_INFO, 1000);
                        RealTape_pause();
                    }
                }
            }
        }
    }

    if (r) {
        for (uint8_t rowidx = 0; rowidx < 8; rowidx++) {
            Ports::port[rowidx] = PS2cols[rowidx];
        }
    }
}


uint8_t ESPeccy::TurboModeSet(int8_t esp_delay) {

    uint8_t current_esp_delay = ESPeccy::ESP_delay;

    if (esp_delay != -1) ESPeccy::ESP_delay = (uint8_t) esp_delay;

    if (ESPeccy::ESP_delay) {

        // Empty audio buffers
        for (int i=0;i<ESP_AUDIO_SAMPLES_PENTAGON;i++) {
            ESPeccy::overSamplebuf[i]=0;
            ESPeccy::audioBuffer[i]=0;
            AySound::SamplebufAY[i]=0;
        }
        ESPeccy::lastaudioBit=0;

        ESPeccy::ESPoffset = 0;

        // printf("Resetting pwmaudio to freq: %d\n",Audio_freq);
        esp_err_t res;
        res = pwm_audio_set_sample_rate(ESPeccy::Audio_freq[ESPeccy::ESP_delay]);
        if (res != ESP_OK) {
            printf("Can't set sample rate\n");
        }

        // Reset AY emulation
        //AySound::init();
        AySound::set_sound_format(ESPeccy::Audio_freq[ESPeccy::ESP_delay],1,8);
        //AySound::set_stereo(AYEMU_MONO,NULL);
        //AySound::reset();
        AySound::prepare_generation();

    }

    return current_esp_delay;

}

//=======================================================================================
// AUDIO
//=======================================================================================

IRAM_ATTR void ESPeccy::audioTask(void *unused) {

    size_t written;

    // // MCP23017 Init
    // mcp23017_test_init();
    // printf("Init done!\n");

    // PWM Audio Init
    pwm_audio_config_t pac;
    pac.duty_resolution    = LEDC_TIMER_8_BIT;
    pac.gpio_num_left      = SPEAKER_PIN;
    pac.ledc_channel_left  = LEDC_CHANNEL_0;
    pac.gpio_num_right     = -1;
    pac.ledc_channel_right = LEDC_CHANNEL_1;
    pac.ledc_timer_sel     = LEDC_TIMER_0;
    pac.tg_num             = TIMER_GROUP_0;
    pac.timer_num          = TIMER_0;
    pac.ringbuf_len        = 3072; /* 4096; */

    pwm_audio_init(&pac);
    pwm_audio_set_param(Audio_freq[ESP_delay],LEDC_TIMER_8_BIT,1);
    pwm_audio_start();
    pwm_audio_set_volume(aud_volume);

    for (;;) {

        xQueueReceive(audioTaskQueue, &param, portMAX_DELAY);

        if (ESP_delay) pwm_audio_write(audioBuffer, samplesPerFrame, &written, 5 / portTICK_PERIOD_MS);

        // task2start = esp_timer_get_time();

        // Process ZX Keyboard
        if (ZXKeyb::Exists) {

            if (zxDelay > 0)

                zxDelay--;

            else {

                // Process physical keyboard
                ZXKeyb::process();

                // Combine both keyboards
                for (uint8_t rowidx = 0; rowidx < 8; rowidx++) {
                    Ports::port[rowidx] = PS2cols[rowidx] & ZXKeyb::ZXcols[rowidx];
                }

            }

        }

        // task2elapsed = esp_timer_get_time() - task2start;

        xQueueReceive(audioTaskQueue, &param, portMAX_DELAY);

        // task2start = esp_timer_get_time();

        if (ESP_delay) {

            // Finish fill of beeper oversampled audio buffers
            if (faudbufcntover < samplesPerFrame)
                overSamplebuf[faudbufcntover++] = faudioBitBuf + (flastaudioBit * (audioSampleDivider - faudioBitbufCount));
            faudioBitBuf = flastaudioBit * audioSampleDivider;
            for(;faudbufcntover < samplesPerFrame;)
                overSamplebuf[faudbufcntover++] = faudioBitBuf;

            switch (aud_active_sources) {
            case 0:
                // Beeper only
                for (int i = 0; i < samplesPerFrame; i++) {
                    int beeper = overSamplebuf[i] / audioSampleDivider;
                    audioBuffer[i] = beeper > 255 ? 255 : beeper;
                }
                break;
            case 1:
                // Beeper + Covox
                for (;faudbufcntCOVOX < samplesPerFrame;)
                    SamplebufCOVOX[faudbufcntCOVOX++] = fcovoxMix;

                for (int i = 0; i < samplesPerFrame; i++) {
                    int beeper = (overSamplebuf[i] / audioSampleDivider) + SamplebufCOVOX[i];
                    audioBuffer[i] = beeper > 255 ? 255 : beeper; // Clamp
                }

                break;
            case 2:
                // Beeper + AY
                if (faudbufcntAY < samplesPerFrame)
                    AySound::gen_sound(samplesPerFrame - faudbufcntAY , faudbufcntAY);

                for (int i = 0; i < samplesPerFrame; i++) {
                    int beeper = (overSamplebuf[i] / audioSampleDivider) + AySound::SamplebufAY[i];
                    audioBuffer[i] = beeper > 255 ? 255 : beeper; // Clamp
                }

                break;
            case 3:
                // Beeper + AY + Covox
                for (;faudbufcntCOVOX < samplesPerFrame;)
                    SamplebufCOVOX[faudbufcntCOVOX++] = fcovoxMix;

                if (faudbufcntAY < samplesPerFrame)
                    AySound::gen_sound(samplesPerFrame - faudbufcntAY , faudbufcntAY);

                for (int i = 0; i < samplesPerFrame; i++) {
                    int beeper = (overSamplebuf[i] / audioSampleDivider) + AySound::SamplebufAY[i] + SamplebufCOVOX[i];
                    audioBuffer[i] = beeper > 255 ? 255 : beeper; // Clamp
                }

                break;
            }

            // if (Config::Covox) {
            //     for (;faudbufcntCOVOX < samplesPerFrame;)
            //         SamplebufCOVOX[faudbufcntCOVOX++] = fcovoxMix;
            // }

            // if (AY_emu) {

            //     if (faudbufcntAY < samplesPerFrame)
            //         AySound::gen_sound(samplesPerFrame - faudbufcntAY , faudbufcntAY);

            //     for (int i = 0; i < samplesPerFrame; i++) {
            //         // uint16_t beeper = (overSamplebuf[i] / audioSampleDivider) + AySound::SamplebufAY[i] + SamplebufCOVOX[i];
            //         // audioBuffer[i] = beeper & 0x00ff; // Clamp
            //         int beeper = (overSamplebuf[i] / audioSampleDivider) + AySound::SamplebufAY[i] + SamplebufCOVOX[i];
            //         audioBuffer[i] = beeper > 255 ? 255 : beeper; // Clamp
            //     }

            // } else

            //     for (int i = 0; i < samplesPerFrame; i++) {
            //         // uint16_t beeper = (overSamplebuf[i] / audioSampleDivider) + SamplebufCOVOX[i];
            //         // audioBuffer[i] = beeper & 0x00ff; // Clamp
            //         int beeper = (overSamplebuf[i] / audioSampleDivider) + SamplebufCOVOX[i];
            //         audioBuffer[i] = beeper > 255 ? 255 : beeper; // Clamp
            //     }

        }

        // task2elapsed = esp_timer_get_time() - task2start;

    }
}

//=======================================================================================
// Beeper audiobuffer generation (oversample)
//=======================================================================================
IRAM_ATTR void ESPeccy::BeeperGetSample() {
    uint32_t audbufpos = CPU::tstates / audioOverSampleDivider;
    if ( audbufcntover < samplesPerFrame ) { // <-- this don't must be needed
        for (;audbufcnt < audbufpos; audbufcnt++) {
            audioBitBuf += lastaudioBit;
            if (++audioBitbufCount == audioSampleDivider) {
                overSamplebuf[audbufcntover++] = audioBitBuf;
                audioBitBuf = 0;
                audioBitbufCount = 0;
                if (audbufcntover == samplesPerFrame) break; // <-- this don't must be needed
            }
        }
    }
}

//=======================================================================================
// AY audiobuffer generation (oversample)
//=======================================================================================
IRAM_ATTR void ESPeccy::AYGetSample() {
    uint32_t audbufpos = CPU::tstates / audioAYDivider;
    if (audbufpos > audbufcntAY) {
        AySound::gen_sound(audbufpos - audbufcntAY, audbufcntAY);
        audbufcntAY = audbufpos;
    }
}

//=======================================================================================
// COVOX audiobuffer generation
//=======================================================================================
IRAM_ATTR void ESPeccy::COVOXGetSample() {
    uint32_t audbufpos = CPU::tstates / audioCOVOXDivider;
    if (audbufpos > audbufcntCOVOX) {
        covoxMix = (covoxData[0] + covoxData[1] + covoxData[2] + covoxData[3]) >> 2;
        for (int i=audbufcntCOVOX;i<audbufpos;i++)
            SamplebufCOVOX[i] = covoxMix;
        audbufcntCOVOX = audbufpos;
    }
}

//=======================================================================================
// MAIN LOOP
//=======================================================================================

IRAM_ATTR void ESPeccy::loop() {

    int tapeCurBlock = 0;

    for(;;) {

        ts_start = esp_timer_get_time();

        // Send audioBuffer to pwmaudio

        xQueueSend(audioTaskQueue, &param, portMAX_DELAY);

        RealTape_prepare_frame();

        // ===== Start detect loading
        // TODO: Move this to a function in Tape.cpp (maybe)
        Ports::loading = Ports::in254_count > 40;

        // Start / Stop .tap reproduction
        if (Tape::tapeFileType != TAPE_FTYPE_EMPTY && (!RealTape_enabled || Config::realtape_mode != REALTAPE_FORCE_LOAD)) {
            if (Ports::loading) {
                if (Tape::tapeStatus == TAPE_STOPPED) {
                    tapeCurBlock = Tape::tapeCurBlock;
                    Tape::Play();
                }
            } else {
                if (Tape::tapeStatus == TAPE_LOADING &&
                    tapeCurBlock != Tape::tapeCurBlock /*&&
                    Tape::tapePhase != TAPE_PHASE_PAUSE_GDB &&
                    Tape::tapePhase != TAPE_PHASE_PAUSE */)
                {
                    tapeCurBlock = Tape::tapeCurBlock;
                    Tape::Stop();
                }
            }
        }

        //printf("loading %s %d %s\n", Ports::loading ? "true" : "false", Ports::in254_count, Tape::tapeStatus == TAPE_STOPPED ? "STOPPED" : "LOADING");

        Ports::in254_count = 0;
        // ===== End detect loading


        CPU::loop();

        // Copy audio buffer vars for finish buffer code
        faudbufcntover = audbufcntover;
        faudioBitBuf = audioBitBuf;
        faudioBitbufCount = audioBitbufCount;
        faudbufcntAY = audbufcntAY;
        faudbufcntCOVOX = audbufcntCOVOX;
        flastaudioBit = lastaudioBit;
        fcovoxMix = covoxMix;

        xQueueSend(audioTaskQueue, &param, portMAX_DELAY);

        processKeyboard();

        // Update stats every 50 frames
        if (VIDEO::OSD && VIDEO::framecnt >= 50) {

            if (VIDEO::OSD & 0x04) {

                // printf("Vol. OSD out -> Framecnt: %d\n", VIDEO::framecnt);

                if (VIDEO::framecnt >= 100) {

                    // Save selected volume if not in tape load monitor mode
                    if (!Config::load_monitor) {
                        Config::volume = aud_volume;
                        Config::save("volume");
                    }

                    VIDEO::OSD &= 0xfb;

                    if (VIDEO::OSD == 0) {

                        if (Config::aspect_16_9)
                            VIDEO::Draw_OSD169 = Z80Ops::is2a3 ? VIDEO::MainScreen_2A3 : VIDEO::MainScreen;
                        else
                            VIDEO::Draw_OSD43 = Z80Ops::isPentagon ? VIDEO::BottomBorder_Pentagon :  VIDEO::BottomBorder;

                        VIDEO::brdnextframe = true;

                    }

                }

            }

            if ((VIDEO::OSD & 0x04) == 0) {

                if (VIDEO::OSD == 1 && Tape::tapeStatus==TAPE_LOADING) {

                    snprintf(OSD::stats_lin1, sizeof(OSD::stats_lin1), " %-12s %04d/%04d ", Tape::tapeFileName.substr(0 + ESPeccy::TapeNameScroller, 12).c_str(), Tape::tapeCurBlock + 1, Tape::tapeNumBlocks);

                    float percent = (float)((Tape::tapebufByteCount + Tape::tapePlayOffset) * 100) / (float)Tape::tapeFileSize;
                    snprintf(OSD::stats_lin2, sizeof(OSD::stats_lin2), " %05.2f%% %07d%s%07d ", percent, Tape::tapebufByteCount + Tape::tapePlayOffset, "/" , Tape::tapeFileSize);

                    if ((++ESPeccy::TapeNameScroller + 12) > Tape::tapeFileName.length()) ESPeccy::TapeNameScroller = 0;

                    OSD::drawStats();

                } else if (VIDEO::OSD == 2) {

                    snprintf(OSD::stats_lin1, sizeof(OSD::stats_lin1), "CPU: %05d / IDL: %05d ", (int)(ESPeccy::elapsed), (int)(ESPeccy::idle));
                    snprintf(OSD::stats_lin2, sizeof(OSD::stats_lin2), "FPS:%6.2f / FND:%6.2f ", VIDEO::framecnt / (ESPeccy::totalseconds / 1000000), VIDEO::framecnt / (ESPeccy::totalsecondsnodelay / 1000000));

                    OSD::drawStats();

                }

                totalseconds = 0;
                totalsecondsnodelay = 0;
                VIDEO::framecnt = 0;

            }

        }

        // Flashing flag change
        if (!(VIDEO::flash_ctr++ & 0x0f)) VIDEO::flashing ^= 0x80;

        #ifdef TIME_MACHINE_ENABLED
        // Time machine
        if (Config::TimeMachine) MemESP::Tm_DoTimeMachine();
        #endif

        elapsed = esp_timer_get_time() - ts_start;
        idle = target[ESP_delay] - elapsed - ESPoffset; // 100% No turbo
        // idle = 15974 - elapsed - ESPoffset; // +125% 48k
        // idle = 13312 - elapsed - ESPoffset; // +150% 48K
        // idle = 11138 - elapsed - ESPoffset; // +150% TK60

        totalsecondsnodelay += elapsed;

        if (!ESP_delay) {
            totalseconds += elapsed;
            continue;
        }

        if(Config::videomode && !Config::load_monitor && ESP_delay == 1) {

            if (sync_cnt++ == 0) {

                if (idle > 0)
                    delayMicroseconds(idle);

            } else {

                // Audio sync (once every 128 frames ~ 2,5 seconds)
                if (sync_cnt & 0x80) {
                    ESPoffset = 128 - pwm_audio_rbstats();
                    sync_cnt = 0;
                }

                // Wait for vertical sync
                for (;;) {
                    if (vsync) break;
                }

                // printf("Vsync!\n");

            }

        } else {

            if (idle > 0)
                delayMicroseconds(idle);
            // else
            //     printf("Elapsed: %d\n",(int)elapsed);

            // Audio sync
            if (++sync_cnt & 0x10) {
                ESPoffset = 128 - pwm_audio_rbstats();
                sync_cnt = 0;
            }

        }

        totalseconds += esp_timer_get_time() - ts_start;
        // printf("Totalsecond: %f\n",double(esp_timer_get_time() - ts_start));

    }

}

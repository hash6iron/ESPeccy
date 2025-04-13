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

#include "OSD.h"
#include "FileUtils.h"
#include "cpuESP.h"
#include "Video.h"
#include "ESPeccy.h"
#include "messages.h"
#include "Config.h"
#include "Snapshot.h"
#include "MemESP.h"
#include "Tape.h"
#include "ZXKeyb.h"
#include "AySound.h"
#include "pwm_audio.h"
#include "Z80_JLS/z80.h"
#include "roms.h"
#include "CommitDate.h"
#include "ROMLoad.h"

#include "RealTape.h"

#include "Cheat.h"

#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_efuse.h"
#include "soc/efuse_reg.h"

#include "fabgl.h"

#include "soc/rtc_wdt.h"
#include "esp_int_wdt.h"
#include "esp_task_wdt.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <sys/types.h>
#include <dirent.h>

// #include "driver/uart.h"
// #include "esp_log.h"
// #include "esp_err.h"

#include <string>

#include "tzx_headers.h"

using namespace std;

#define MENU_REDRAW true
#define MENU_UPDATE false
#define OSD_ERROR true
#define OSD_NORMAL false

#ifdef USE_FONT_6x8
#define OSD_W 248
#else
#define OSD_W (OSD_FONT_W * OSD_COLS + 8)
#endif
#define OSD_H 192
#define OSD_MARGIN 4

extern Font SystemFont;

uint8_t OSD::cols;                     // Maximum columns
uint8_t OSD::mf_rows;                  // File menu maximum rows
unsigned short OSD::real_rows;      // Real row count
uint8_t OSD::virtual_rows;             // Virtual maximum rows on screen
uint16_t OSD::w;                        // Width in pixels
uint16_t OSD::h;                        // Height in pixels
uint16_t OSD::x;                        // X vertical position
uint16_t OSD::y;                        // Y horizontal position
uint16_t OSD::prev_y[5];                // Y prev. position
unsigned short OSD::menu_prevopt = 1;
string OSD::menu;                   // Menu string
unsigned short OSD::begin_row = 1;      // First real displayed row
bool OSD::use_current_menu_state = false;
uint8_t OSD::focus = 1;                    // Focused virtual row
uint8_t OSD::last_focus = 0;               // To check for changes
unsigned short OSD::last_begin_row = 0; // To check for changes

uint8_t OSD::menu_level = 0;
bool OSD::menu_saverect = false;
unsigned short OSD::menu_curopt = 1;
unsigned int OSD::SaveRectpos = 0;

unsigned short OSD::scrW = 320;
unsigned short OSD::scrH = 240;

char OSD::stats_lin1[25]; // "CPU: 00000 / IDL: 00000 ";
char OSD::stats_lin2[25]; // "FPS:000.00 / FND:000.00 ";

// X origin to center an element with pixel_width
unsigned short OSD::scrAlignCenterX(unsigned short pixel_width) { return (scrW / 2) - (pixel_width / 2); }

// Y origin to center an element with pixel_height
unsigned short OSD::scrAlignCenterY(unsigned short pixel_height) { return (scrH / 2) - (pixel_height / 2); }

uint8_t OSD::osdMaxRows() { return (OSD_H - (OSD_MARGIN * 2)) / OSD_FONT_H; }
uint8_t OSD::osdMaxCols() { return (OSD_W - (OSD_MARGIN * 2)) / OSD_FONT_W; }
unsigned short OSD::osdInsideX() { return scrAlignCenterX(OSD_W) + OSD_MARGIN; }
unsigned short OSD::osdInsideY() { return scrAlignCenterY(OSD_H) + OSD_MARGIN; }

static const uint8_t click48[12]={ 0,8,32,32,32,32,32,32,32,32,8,0 };

static const uint8_t click128[116] = {   0,8,32,32,32,32,32,32,32,32,32,32,32,32,32,32,
                                        32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,
                                        32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,
                                        32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,
                                        32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,
                                        32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,
                                        32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,
                                        32,32,8,0
                                    };

/* IRAM_ATTR */ void OSD::click() {
    if (Config::load_monitor) return; // Disable interface click on tape load monitor mode
    pwm_audio_set_volume(ESP_VOLUME_MAX);
    size_t written;
    if (Z80Ops::is48)   pwm_audio_write((uint8_t *) click48, sizeof(click48), &written,  5 / portTICK_PERIOD_MS);
    else                pwm_audio_write((uint8_t *) click128, sizeof(click128), &written, 5 / portTICK_PERIOD_MS);
    pwm_audio_set_volume(ESPeccy::aud_volume);
}

void OSD::esp_hard_reset() {
    // RESTART ESP32 (This is the most similar way to hard resetting it)
    rtc_wdt_protect_off();
    rtc_wdt_set_stage(RTC_WDT_STAGE0, RTC_WDT_STAGE_ACTION_RESET_RTC);
    rtc_wdt_set_time(RTC_WDT_STAGE0, 100);
    rtc_wdt_enable();
    rtc_wdt_protect_on();
    while (true);
}

void OSD::restoreBackbufferData(bool force) {
    if ( !SaveRectpos ) return;
    if (menu_saverect || force) {
        printf("--- OSD::restoreBackbufferData %d 0x%x\n", SaveRectpos, SaveRectpos * 4);

        uint16_t w = VIDEO::SaveRect[--SaveRectpos] >> 16;
        uint16_t h = VIDEO::SaveRect[SaveRectpos] & 0xffff;

        uint16_t x = VIDEO::SaveRect[--SaveRectpos] >> 16;
        uint16_t y = VIDEO::SaveRect[SaveRectpos] & 0xffff;

        SaveRectpos -= ( ( ( ( x + w ) >> 2 ) + 1 ) - ( x >> 2 ) ) * h;

        uint32_t j = SaveRectpos;

        printf("OSD::restoreBackbufferData x=%hd y=%hd w=%hd h=%hd\n", x, y, w, h );

        for (uint32_t m = y; m < y + h; m++) {
            uint32_t *backbuffer32 = (uint32_t *)(VIDEO::vga.frameBuffer[m]);
            for (uint32_t n = x >> 2; n < ( ( x + w ) >> 2 ) + 1; n++) {
                backbuffer32[n] = VIDEO::SaveRect[j++];
            }
        }
        printf("OSD::restoreBackbufferData exit %d 0x%x j:%d 0x%x\n", SaveRectpos, SaveRectpos * 4, j, j * 4);
//        if ( !force )
        menu_saverect = false;
    }
}

void OSD::saveBackbufferData(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool force) {

    if ( force || menu_saverect ) {
        printf("OSD::saveBackbufferData x=%hd y=%hd w=%hd h=%hd pos=%d 0x%x\n", x, y, w, h, SaveRectpos, SaveRectpos * 4);

        for (uint32_t m = y; m < y + h; m++) {
            uint32_t *backbuffer32 = (uint32_t *)(VIDEO::vga.frameBuffer[m]);
            for (uint32_t n = x >> 2; n < ( ( x + w ) >> 2 ) + 1; n++) {
                VIDEO::SaveRect[SaveRectpos++] = backbuffer32[n];
            }
        }

        VIDEO::SaveRect[SaveRectpos++] = ( x << 16 ) | y;
        VIDEO::SaveRect[SaveRectpos++] = ( w << 16 ) | h;

        printf("OSD::saveBackbufferData exit %d 0x%x\n", SaveRectpos, SaveRectpos * 4);
    }
}

void OSD::saveBackbufferData(bool force) {
    OSD::saveBackbufferData(x, y, w, h, force);
}

void OSD::flushBackbufferData() {
    while(SaveRectpos) {
        OSD::restoreBackbufferData(true);
    }
}


// // Cursor to OSD first row,col
void OSD::osdHome() { VIDEO::vga.setCursor(osdInsideX(), osdInsideY()); }

// // Cursor positioning
void OSD::osdAt(uint8_t row, uint8_t col) {
    if (row > osdMaxRows() - 1)
        row = 0;
    if (col > osdMaxCols() - 1)
        col = 0;
    unsigned short y = (row * OSD_FONT_H) + osdInsideY();
    unsigned short x = (col * OSD_FONT_W) + osdInsideX();
    VIDEO::vga.setCursor(x, y);
}

void OSD::drawWindow(uint16_t width, uint16_t height, string top, string bottom, bool clear) {

    unsigned short x = scrAlignCenterX(width);
    unsigned short y = scrAlignCenterY(height);
    if (clear) VIDEO::vga.fillRect(x, y, width, height, zxColor(0, 0));
    VIDEO::vga.rect(x, y, width, height, zxColor(0, 0));
    VIDEO::vga.rect(x + 1, y + 1, width - 2, height - 2, zxColor(7, 0));

    if (top != "") {
        VIDEO::vga.rect(x + 3, y + 3, width - 6, 9, zxColor(5, 0));
        VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
        VIDEO::vga.setFont(SystemFont);
        VIDEO::vga.setCursor(x + 3, y + 4);
        VIDEO::vga.print(top.c_str());
    }

    if (bottom != "") {
        VIDEO::vga.rect(x + 3, y + height - 12, width - 6, 9, zxColor(5, 0));
        VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
        VIDEO::vga.setFont(SystemFont);
        VIDEO::vga.setCursor(x + 3, y + height - 11);
        VIDEO::vga.print(bottom.c_str()); 
    }

}

// Shows a red panel with error text
void OSD::errorPanel(string errormsg) {
    unsigned short x = scrAlignCenterX(OSD_W);
    unsigned short y = scrAlignCenterY(OSD_H);

    if (Config::slog_on)
        printf((errormsg + "\n").c_str());

    VIDEO::vga.fillRect(x, y, OSD_W, OSD_H, zxColor(0, 0));
    VIDEO::vga.rect(x, y, OSD_W, OSD_H, zxColor(7, 0));
    VIDEO::vga.rect(x + 1, y + 1, OSD_W - 2, OSD_H - 2, zxColor(2, 1));
    VIDEO::vga.setFont(SystemFont);
    osdHome();
    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(2, 1));
    VIDEO::vga.print(ERROR_TITLE);
    osdAt(2, 0);
    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(0, 0));
    VIDEO::vga.println(errormsg.c_str());
    osdAt(17, 0);
    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(2, 1));
    VIDEO::vga.print(ERROR_BOTTOM);
}

// Error panel and infinite loop
void OSD::errorHalt(string errormsg) {
    errorPanel(errormsg);
    while (1) {
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}

// Centered message
void OSD::osdCenteredMsg(string msg, uint8_t warn_level) {
    osdCenteredMsg(msg,warn_level,1000);
}

void OSD::osdCenteredMsg(string msg, uint8_t warn_level, uint16_t millispause) {

    const unsigned short h = OSD_FONT_H * 3;
    const unsigned short y = scrAlignCenterY(h);
    unsigned short paper;
    unsigned short ink;
    unsigned int j;

    if (msg.length() > (scrW / 6) - 10) msg = msg.substr(0,(scrW / 6) - 10);

    const unsigned short w = (msg.length() + 2) * OSD_FONT_W;
    const unsigned short x = scrAlignCenterX(w);

    switch (warn_level) {
    case LEVEL_OK:
        ink = zxColor(7, 1);
        paper = zxColor(4, 0);
        break;
    case LEVEL_ERROR:
        ink = zxColor(7, 1);
        paper = zxColor(2, 0);
        break;
    case LEVEL_WARN:
        ink = zxColor(0, 0);
        paper = zxColor(6, 0);
        break;
    default:
        ink = zxColor(7, 0);
        paper = zxColor(1, 0);
    }

    if (millispause > 0) {
        // Save backbuffer data
        OSD::saveBackbufferData(x,y,w,h,true);
    }

    VIDEO::vga.fillRect(x, y, w, h, paper);
    // VIDEO::vga.rect(x - 1, y - 1, w + 2, h + 2, ink);
    VIDEO::vga.setTextColor(ink, paper);
    VIDEO::vga.setFont(SystemFont);
    VIDEO::vga.setCursor(x + OSD_FONT_W, y + OSD_FONT_H);
    VIDEO::vga.print(msg.c_str());

    if (millispause > 0) {
        vTaskDelay(millispause/portTICK_PERIOD_MS); // Pause if needed
        OSD::restoreBackbufferData(true);
    }
}

// // Count NL chars inside a string, useful to count menu rows
unsigned short OSD::rowCount(string& menu) {
    unsigned short count = 0;
    for (unsigned short i = 0; i < menu.length(); i++) {
        if (menu.at(i) == ASCII_NL) {
            count++;
        }
    }
    return count;
}

// // Get a row text
string OSD::rowGet(string& menu, unsigned short row) {
    unsigned short count = 0;
    unsigned short last = 0;
    for (unsigned short i = 0; i < menu.length(); i++) {
        if (menu.at(i) == ASCII_NL) {
            if (count == row) {
                return menu.substr(last,i - last);
            }
            count++;
            last = i + 1;
        }
    }
    return "<Unknown menu row>";
}

string OSD::rowReplace(string& menu, unsigned short row, const string& newRowContent) {
    unsigned short count = 0;
    unsigned short last = 0;
    string newMenu;
    bool rowReplaced = false;

    for (unsigned short i = 0; i <= menu.length(); i++) {
        if (i == menu.length() || menu.at(i) == ASCII_NL) {
            if (count == row) {
                newMenu += newRowContent + "\n";
                rowReplaced = true;
            } else {
                newMenu += menu.substr(last, i - last) + "\n";
            }
            count++;
            last = i + 1;
        }
    }

    // Si la fila especificada no existe, simplemente devuelve el menú original
    if (!rowReplaced) {
        return menu;
    }

    return newMenu;
}

#define SCREEN_WIDTH  256
#define SCREEN_HEIGHT 192
#define SCRLEN 6912
#define WORDS_IN_SCREEN (SCRLEN / 4) // Número de palabras de 32 bits en la pantalla

// Paleta de colores del ZX Spectrum
static const uint8_t ZX_PALETTE[16][3] = {
    {0, 0, 0}, {0, 0, 128}, {128, 0, 0}, {128, 0, 128},
    {0, 128, 0}, {0, 128, 128}, {128, 128, 0}, {128, 128, 128},
    {0, 0, 0}, {0, 0, 255}, {255, 0, 0}, {255, 0, 255},
    {0, 255, 0}, {0, 255, 255}, {255, 255, 0}, {255, 255, 255}
};

// Función común para construir el path en la subcarpeta SCRSHOT
std::string buildSCRFilePath(const std::string& absolutePath, std::string& scrDir) {
    size_t lastSlashPos = absolutePath.find_last_of("/\\");
    if (lastSlashPos == std::string::npos) {
        return ""; // Ruta inválida
    }

    // Obtén el directorio base y el nombre del archivo sin extensión
    std::string baseDir = absolutePath.substr(0, lastSlashPos);
    std::string fileName = absolutePath.substr(lastSlashPos + 1);
    size_t dotPos = fileName.find_last_of('.');
    if (dotPos != std::string::npos) {
        fileName = fileName.substr(0, dotPos);
    }

    // Construye los paths necesarios
    scrDir = baseDir + "/SCRSHOT";
    return scrDir + "/" + fileName + ".scr";
}

// Verifica si un archivo SCR existe
std::string fileExistsWithScrExtension(const std::string& absolutePath) {
    std::string scrDir;
    std::string scrFilePath = buildSCRFilePath(absolutePath, scrDir);
    if (scrFilePath.empty()) {
        return absolutePath;
    }

    // Verifica si el archivo existe
    if (access(scrFilePath.c_str(), F_OK) == 0) {
        return scrFilePath;
    }

    return absolutePath;
}

// Guarda el contenido del bitmap como archivo SCR
void OSD::saveSCR(const std::string& absolutePath, const uint32_t *bitmap) {
    uint32_t pos = OSD::SaveRectpos;

    if ( FileUtils::isSDReady() ) {
        std::string scrDir;
        std::string scrFilePath = buildSCRFilePath(absolutePath, scrDir);
        if (scrFilePath.empty()) {
            return; // Ruta inválida
        }

        // Crea la carpeta SCRSHOT si no existe
        struct stat st;
        if (stat(scrDir.c_str(), &st) != 0) {
            if (mkdir(scrDir.c_str(), 0755) != 0) {
                perror("Failed to create SCRSHOT directory");
                SaveRectpos = pos; // must be 0
                return;
            }
        }

        // Verifica si el archivo existe
        if (access(scrFilePath.c_str(), F_OK) == 0) {
            SaveRectpos = WORDS_IN_SCREEN;

            uint8_t res = OSD::msgDialog(OSD_TAPE_SAVE_EXIST[Config::lang],OSD_DLG_SURE[Config::lang]);
            if (res != DLG_YES) {
                SaveRectpos = pos; // must be 0
                return;
            }

        }

        if ( FileUtils::isSDReady() ) {
            // Abre el archivo para escritura en modo binario
            FILE *file = fopen(scrFilePath.c_str(), "wb");
            if (!file) {
                perror("Failed to open .scr file for writing");
                SaveRectpos = pos; // must be 0
                return;
            }

            // Escribe los primeros 6912 bytes del bitmap en palabras de 32 bits
            for(int i = 0; i < WORDS_IN_SCREEN; ++i) {
                uint32_t word = bitmap[i];
                fwrite(&word, sizeof(uint32_t), 1, file);
            }

            // Cierra el archivo
            fclose(file);
        }
    }

    SaveRectpos = pos; // must be 0
}

#if 0
void OSD::renderScreenNormal(int x0, int y0, const uint32_t *bitmap, bool monocrome) {

    const uint32_t *attributes = bitmap + 0x1800 / 4; // Offset en palabras de 32 bits

    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            int r = 0, g = 0, b = 0, count = 0;

            // Calcular offset en pantalla original
            int char_col = x / 8;
            int bit = 7 - (x % 8);

            // Leer atributos
            uint8_t attr = (monocrome)
                           ? 0x38
                           : ((attributes[(y / 8) * 8 + char_col / 4] >> ((char_col % 4) * 8)) & 0xFF);

            // Obtener colores según el atributo
            int ink = attr & 0x07;          // INK (color del pixel encendido)
            int paper = (attr >> 3) & 0x07; // PAPER (color del pixel apagado)
            int bright = (attr & 0x40) ? 8 : 0; // BRIGHT

            int address = (((y & 0xC0) >> 6) << 11) |
                          ((y & 0x07) << 8) |
                          (((y & 0x38) >> 3) << 5);

            // Leer palabra alineada de 32 bits
            uint32_t word = bitmap[(address / 4) + char_col / 4];
            uint8_t databyte = (word >> ((char_col % 4) * 8)) & 0xFF;

            // Determinar el color del pixel actual
            uint8_t color_index = (databyte & (1 << bit))
                                  ? ink + bright
                                  : paper + bright;
            const uint8_t *rgb = ZX_PALETTE[color_index];

            // Sumar valores RGB para el promedio
            r = rgb[0];
            g = rgb[1];
            b = rgb[2];

            // Promediar colores del bloque y guardar en el buffer reducido
            uint8_t color =  (rgb[0] >> 6) |
                            ((rgb[1] >> 6) << 2) |
                            ((rgb[2] >> 6) << 4);

            VIDEO::vga.dotFast(x0 + x, y0 + y, color);
    }
}
#endif

void OSD::renderScreenScaled(int x0, int y0, const uint32_t *bitmap, int divisor, bool monocrome) {
    if (divisor <= 0) divisor = 1; // Evitar divisores inválidos
    int scaled_width = SCREEN_WIDTH / divisor;
    int scaled_height = SCREEN_HEIGHT / divisor;

    const uint32_t *attributes = bitmap + 0x1800 / 4; // Offset en palabras de 32 bits

    for (int y = 0; y < scaled_height; y++) {
        for (int x = 0; x < scaled_width; x++) {
            int r = 0, g = 0, b = 0, count = 0;

            // Procesar bloques según el divisor
            for (int j = 0; j < divisor; j++) {
                for (int i = 0; i < divisor; i++) {
                    int src_x = x * divisor + i;
                    int src_y = y * divisor + j;

                    // Calcular offset en pantalla original
                    int char_col = src_x / 8;
                    int bit = 7 - (src_x % 8);

                    // Leer atributos
                    uint8_t attr = (monocrome)
                                   ? 0x38
                                   : ((attributes[(src_y / 8) * 8 + char_col / 4] >> ((char_col % 4) * 8)) & 0xFF);

                    // Obtener colores según el atributo
                    int ink = attr & 0x07;          // INK (color del pixel encendido)
                    int paper = (attr >> 3) & 0x07; // PAPER (color del pixel apagado)
                    int bright = (attr & 0x40) ? 8 : 0; // BRIGHT

                    int address = (((src_y & 0xC0) >> 6) << 11) |
                                  ((src_y & 0x07) << 8) |
                                  (((src_y & 0x38) >> 3) << 5);

                    // Leer palabra alineada de 32 bits
                    uint32_t word = bitmap[(address / 4) + char_col / 4];
                    uint8_t databyte = (word >> ((char_col % 4) * 8)) & 0xFF;

                    // Determinar el color del pixel actual
                    uint8_t color_index = (databyte & (1 << bit))
                                          ? ink + bright
                                          : paper + bright;
                    const uint8_t *rgb = ZX_PALETTE[color_index];

                    // Sumar valores RGB para el promedio
                    r += rgb[0];
                    g += rgb[1];
                    b += rgb[2];
                    count++;
                }
            }

            // Promediar colores del bloque y guardar en el buffer reducido
            uint8_t color =  ((r / count) >> 6) |
                            (((g / count) >> 6) << 2) |
                            (((b / count) >> 6) << 4);

            VIDEO::vga.dotFast(x0 + x, y0 + y, color);
        }
    }
}

void OSD::loadCompressedScreen(FILE *f, uint32_t *buffer) {
    uint8_t ed_cnt = 0;
    uint8_t repcnt = 0;
    uint8_t repval = 0;
    uint32_t memidx = 0;

    uint8_t local_buffer[4]; // Buffer local para almacenar 4 bytes antes de escribir
    uint8_t local_idx = 0;   // Índice dentro del buffer local

    // Escribe el contenido del local_buffer al buffer de destino si está lleno
    #define WRITE_BUFFER(val)                       \
        local_buffer[local_idx++] = val;            \
        if (local_idx == 4) {                       \
            *buffer++ = (local_buffer[0] << 0) |    \
                        (local_buffer[1] << 8) |    \
                        (local_buffer[2] << 16) |   \
                        (local_buffer[3] << 24);    \
            local_idx = 0;                          \
        }

    while (memidx < SCRLEN) {
        uint8_t databyte;
        fread(&databyte, sizeof(uint8_t), 1, f);

        if (ed_cnt == 0) {
            if (databyte != 0xED) {
                WRITE_BUFFER(databyte);
                memidx++;
            } else {
                ed_cnt++;
            }
        } else if (ed_cnt == 1) {
            if (databyte != 0xED) {
                WRITE_BUFFER(0xED);
                WRITE_BUFFER(databyte);
                memidx += 2;
                ed_cnt = 0;
            } else {
                ed_cnt++;
            }
        } else if (ed_cnt == 2) {
            repcnt = databyte;
            ed_cnt++;
        } else if (ed_cnt == 3) {
            repval = databyte;
            if (memidx + repcnt > SCRLEN) repcnt = SCRLEN - memidx;
            while (repcnt--) {
                WRITE_BUFFER(repval);
                memidx++;
            }
            ed_cnt = 0;
        }
    }

    // Si quedaron datos residuales en el buffer local, completarlos y escribir
    if (local_idx > 0) {
        while (local_idx < 4) {
            local_buffer[local_idx++] = 0; // Completar con ceros
        }
        *buffer = (local_buffer[0] << 0) |
                  (local_buffer[1] << 8) |
                  (local_buffer[2] << 16) |
                  (local_buffer[3] << 24); // Última palabra
    }
}

unsigned char aux_buff[128];

int check_screen_relocator(unsigned char *buffer) {
    // Buscar desde la posición 255 hacia atrás
    for (int i = sizeof(aux_buff) - 1; i >= 9; --i) {
        // Seq1: 237,176,201
        if (buffer[i - 2] == 237 && buffer[i - 1] == 176 && buffer[i] == 201) {
            return i + 1; // Retornar posición final + 1
        }
        // Seq2: 237,176,251,201
        if (buffer[i - 3] == 237 && buffer[i - 2] == 176 && buffer[i - 1] == 251 && buffer[i] == 201) {
            return i + 1; // Retornar posición final + 1
        }
        // Seq3: 237,176,195,*,*
        if (buffer[i - 4] == 237 && buffer[i - 3] == 176 && buffer[i - 2] == 195) {
            return i + 1; // Retornar posición final + 1
        }
        // Seq4: 237,176,251,195,*,*
        if (buffer[i - 5] == 237 && buffer[i - 4] == 176 && buffer[i - 3] == 251 && buffer[i - 2] == 195) {
            return i + 1; // Retornar posición final + 1
        }
        // Seq5: 237,176,205,*,*,201
        if (buffer[i - 5] == 237 && buffer[i - 4] == 176 && buffer[i - 3] == 205 && buffer[i] == 201) {
            return i + 1; // Retornar posición final + 1
        }
        // Seq6: 237,176,205,*,*,195,*,*
        if (buffer[i - 7] == 237 && buffer[i - 6] == 176 && buffer[i - 5] == 205 && buffer[i - 2] == 195) {
            return i + 1; // Retornar posición final + 1
        }
    }
    // Si no se encuentra ninguna coincidencia
    return 0;
}

// CAUTION: Use this funcion only if menu_level = 0

int OSD::renderScreen(int x0, int y0, const char* filename, int screen_number, off_t* screen_offset) {

    int ret = RENDER_PREVIEW_OK;

    bool monocrome = false;

    uint32_t * snapshot = (uint32_t *) VIDEO::SaveRect;

    string fname = fileExistsWithScrExtension(string(filename));

    FILE *file = fopen(fname.c_str(), "rb");
    if (!file) {
        perror("Error opening file");
        return RENDER_PREVIEW_ERROR;
    }

    size_t filesize = FileUtils::fileSize(fname.c_str());

    if (FileUtils::hasExtension(fname.c_str(), "tzx")) {

        // Leer y validar el encabezado
        st_head_tzx header;
        if (fread(&header, sizeof(st_head_tzx), 1, file) != 1 || strncmp(header.zx_tape, "ZXTape!", 7) != 0 || header._1a != 0x1A) {
            fprintf(stderr, "Error: Invalid TZX file format\n");
            fclose(file);
            return RENDER_PREVIEW_ERROR;
        }

        unsigned screen_count = 0;
        unsigned pos = sizeof(st_head_tzx);
        unsigned char b; //, lb;

        // Procesar los bloques
        while (pos < filesize && screen_count <= screen_number + 1) {
            // Calcular el tamaño del bloque
            unsigned long block_length = 0;
            unsigned long data_length = 0;

            unsigned char block_id;

            fseek(file, pos, SEEK_SET);
            if (fread(&block_id, 1, 1, file) != 1) break;

            switch (block_id) {
                case 0x10: // Standard Speed Data Block
                    fseek(file, pos + 3, SEEK_SET);
                    fread(&b, 1, 1, file);
                    data_length = b;
                    fread(&b, 1, 1, file);
                    data_length |= b << 8;
                    block_length = 5 + data_length;

                    if ((data_length >= 6912 && data_length <= 7200) ||
                        (data_length >= 49152 - 6912)) {

                        if (screen_count == screen_number) {
                            off_t seek_pos_add = 2;

                            if (data_length - 2 < 6912) {
                                seek_pos_add = 6912 - (data_length - 2);
                            }
                            else if (data_length > 6914) {
                                fseek(file, pos + 4 + 2, SEEK_SET);
                                fread(aux_buff, 1, sizeof(aux_buff), file);
                                seek_pos_add = check_screen_relocator(aux_buff) + 2;
                            }

                            off_t off = seek_pos_add;
                            if (screen_offset) {
                                off += *screen_offset;
                                if (pos + 4 + off + 6912 > filesize) off = filesize - 6912 - ( pos + 4 );
                                if (seek_pos_add + *screen_offset < 0) off = 0;
                                *screen_offset = off - seek_pos_add;
                            }

                            fseek(file, pos + 4 + off, SEEK_SET);
                            for(int i = 0; i < WORDS_IN_SCREEN; ++i) {
                                uint32_t word;
                                fread(&word, sizeof(uint32_t), 1, file);
                                snapshot[i] = word;
                            }
                        }
                        screen_count++;
                    }
                    break;
                case 0x11: // Turbo Speed Data Block
                    fseek(file, pos + 16, SEEK_SET);
                    fread(&b, 1, 1, file);
                    data_length = b;
                    fread(&b, 1, 1, file);
                    data_length |= b << 8;
                    fread(&b, 1, 1, file);
                    data_length |= b << 16;
                    block_length = 19 + data_length;

                    if ((data_length >= 6912 && data_length <= 7200) ||
                        (data_length >= 49152 - 6912)) {

                        if (screen_count == screen_number) {
                            off_t seek_pos_add = 2;

                            if (data_length - 2 < 6912) {
                                seek_pos_add = 6912 - (data_length - 2);
                            }
                            else if (data_length > 6914) {
                                fseek(file, pos + 4 + 2, SEEK_SET);
                                fread(aux_buff, 1, sizeof(aux_buff), file);
                                seek_pos_add = check_screen_relocator(aux_buff) + 2;
                            }

                            off_t off = seek_pos_add;
                            if (screen_offset) {
                                off += *screen_offset;
                                if (pos + 18 + off + 6912 > filesize) off = filesize - 6912 - ( pos + 18 );
                                if (seek_pos_add + *screen_offset < 0) off = 0;
                                *screen_offset = off - seek_pos_add;
                            }

                            fseek(file, pos + 18 + off, SEEK_SET);
                            for(int i = 0; i < WORDS_IN_SCREEN; ++i) {
                                uint32_t word;
                                fread(&word, sizeof(uint32_t), 1, file);
                                snapshot[i] = word;
                            }
                        }
                        screen_count++;
                    }
                    break;
                case 0x12: // Pure Tone
                    block_length = 5;
                    break;
                case 0x13: // Pulse Sequence
                    fread(&b, 1, 1, file);
                    block_length = 2 + 2 * b;
                    break;
                case 0x14: // Pure Data Block (ignore this!)
                    fseek(file, pos + 8, SEEK_SET);
                    fread(&b, 1, 1, file);
                    data_length = b;
                    fread(&b, 1, 1, file);
                    data_length |= b << 8;
                    fread(&b, 1, 1, file);
                    data_length |= b << 16;
                    block_length = 11 + data_length;
                    break;
                case 0x15: // Direct Recording Block
                    fseek(file, pos + 6, SEEK_SET);
                    fread(&b, 1, 1, file);
                    data_length = b;
                    fread(&b, 1, 1, file);
                    data_length |= b << 8;
                    fread(&b, 1, 1, file);
                    data_length |= b << 16;
                    block_length = 9 + data_length;
                    break;
                case 0x18: // CSW Recording
                case 0x19: // Generalized Data Block
                    fseek(file, pos + 1, SEEK_SET);
                    fread(&b, 1, 1, file);
                    data_length = b;
                    fread(&b, 1, 1, file);
                    data_length |= b << 8;
                    fread(&b, 1, 1, file);
                    data_length |= b << 16;
                    fread(&b, 1, 1, file);
                    data_length |= b << 24;
                    block_length = 5 + data_length;
                    break;
                case 0x20: // Silence Block
                    block_length = 3;
                    break;
                case 0x21: // Group Start
                    fseek(file, pos + 1, SEEK_SET);
                    fread(&b, 1, 1, file);
                    block_length = 2 + b;
                    break;
                case 0x22: // Group End
                    block_length = 1;
                    break;
                case 0x23: // Jump to block
                case 0x24: // Loop Start
                    block_length = 3;
                    break;
                case 0x25: // Loop End
                    block_length = 1;
                    break;
                case 0x26: // Call Sequence
                    fseek(file, pos + 1, SEEK_SET);
                    fread(&b, 1, 1, file);
                    data_length = b;
                    fread(&b, 1, 1, file);
                    data_length |= b << 8;
                    block_length = 3 + 2 * data_length;
                    break;
                case 0x27: // Return from Sequence
                    block_length = 1;
                    break;
                case 0x28: // Select block
                    fseek(file, pos + 1, SEEK_SET);
                    fread(&b, 1, 1, file);
                    data_length = b;
                    fread(&b, 1, 1, file);
                    data_length |= b << 8;
                    block_length = 3 + data_length;
                    break;
                case 0x2A: // Stop the Tape if in 48K
                    block_length = 5;
                    break;
                case 0x2B: // Set Signal Level
                    block_length = 6;
                    break;
                case 0x30: // Text Description
                    fseek(file, pos + 1, SEEK_SET);
                    fread(&b, 1, 1, file);
                    block_length = 2 + b;
                    break;
                case 0x31: // Message Block
                    fseek(file, pos + 2, SEEK_SET);
                    fread(&b, 1, 1, file);
                    block_length = 3 + b;
                    break;
                case 0x32: // Archive Info
                    fseek(file, pos + 1, SEEK_SET);
                    fread(&b, 1, 1, file);
                    data_length = b;
                    fread(&b, 1, 1, file);
                    data_length |= b << 8;
                    block_length = 3 + data_length;
                    break;
                case 0x33: // Hardware Info
                    fseek(file, pos + 1, SEEK_SET);
                    fread(&b, 1, 1, file);
                    block_length = 2 + b * 3;
                    break;
                case 0x34: // Emulation Info
                    block_length = 9;
                    break;
                case 0x35: // Custom Info
                    fseek(file, pos + 17, SEEK_SET);
                    fread(&b, 1, 1, file);
                    data_length = b;
                    fread(&b, 1, 1, file);
                    data_length |= b << 8;
                    fread(&b, 1, 1, file);
                    data_length |= b << 16;
                    fread(&b, 1, 1, file);
                    data_length |= b << 24;
                    block_length = 21 + data_length;
                    break;
                case 0x5a:
                    block_length = 10;
                    break;
                case 0x40: // Snapshot
                    fseek(file, pos + 2, SEEK_SET);
                    fread(&b, 1, 1, file);
                    data_length = b;
                    fread(&b, 1, 1, file);
                    data_length |= b << 8;
                    fread(&b, 1, 1, file);
                    data_length |= b << 16;
                    fread(&b, 1, 1, file);
                    data_length |= b << 24;
                    block_length = 5 + data_length;
                    // Aca se podria llamar a la rutina general de snapshot dependiendo el tipo
                    // en pos + 1, si 00: z80, 01: sna
                    break;
                default:
                    fprintf(stderr, "Error: Unknown block type 0x%X at position %u\n", block_id, pos);
                    fclose(file);
                    return RENDER_PREVIEW_ERROR;
            }

            pos += block_length; // Avanzar al siguiente bloque
        }

        if (!screen_count) {
            fclose(file);
            return RENDER_PREVIEW_ERROR;
        }

        if (screen_count < screen_number + 1) {
            fclose(file);
            return RENDER_PREVIEW_REQUEST_NO_FOUND; // no more screens, keep current
        }

        if (screen_count == screen_number + 1) {
            ret = RENDER_PREVIEW_OK;
        }

        if (screen_count > screen_number + 1) {
            ret = RENDER_PREVIEW_OK_MORE;
        }

    } else if (FileUtils::hasExtension(fname.c_str(), "tap")) {
        unsigned screen_count = 0;
        unsigned pos = 0;

        while (pos < filesize && screen_count <= screen_number + 1) {
            unsigned char length_low, length_high;
            unsigned short block_length;

            fseek(file, pos, SEEK_SET);

            if (fread(&length_low, 1, 1, file) != 1) break;
            if (fread(&length_high, 1, 1, file) != 1) break;

            block_length = length_low | (length_high << 8);

            pos += 2;

            // Leer el primer byte del bloque (indicador de cabecera)
            unsigned char header_type;
            if (fread(&header_type, 1, 1, file) != 1) break;

            if (/*header_type == 0xFF &&*/
                (block_length >= 6912 && block_length <= 7200) ||
                (block_length >= 49152 - 6912) ) {

                if (screen_count == screen_number) {
                    off_t seek_pos_add = 0;

                    if (block_length > 6912) {
                        fseek(file, pos + 1, SEEK_SET);
                        fread(aux_buff, 1, sizeof(aux_buff), file);
                        seek_pos_add = check_screen_relocator(aux_buff);
                    }

                    off_t off = seek_pos_add;
                    if (screen_offset) {
                        off += *screen_offset;
                        if (pos + 1 + off + 6912 > filesize) off = filesize - 6912 - ( pos + 1 );
                        if (seek_pos_add + *screen_offset + 1 < 0) off = -1;
                        *screen_offset = off - seek_pos_add;
                    }

                    fseek(file, pos + 1 + off, SEEK_SET);
                    // Posible pantalla
                    for(int i = 0; i < WORDS_IN_SCREEN; ++i) {
                        uint32_t word;
                        fread(&word, sizeof(uint32_t), 1, file);
                        snapshot[i] = word;
                    }
                }
                screen_count++;
            }

            pos += block_length;
        }

        if (!screen_count) {
            fclose(file);
            return RENDER_PREVIEW_ERROR;
        }

        if (screen_count < screen_number + 1) {
            fclose(file);
            return RENDER_PREVIEW_REQUEST_NO_FOUND; // no more screens, keep current
        }

        if (screen_count == screen_number + 1) {
            ret = RENDER_PREVIEW_OK;
        }

        if (screen_count > screen_number + 1) {
            ret = RENDER_PREVIEW_OK_MORE;
        }

    } else {
        bool isZ80 = FileUtils::hasExtension(fname.c_str(), "z80");

        off_t seekOff = 0;

        if (!isZ80) {
            switch(filesize) {
                // SCR
                case 6144:
                    monocrome = true;
                case 6912:
                    break;

                // SNA
                case SNA_48K_SIZE:
                case SNA_128K_SIZE1:
                    seekOff = 27;
                    break;

                case SNA_48K_WITH_ROM_SIZE:
                case SNA_128K_SIZE2:
                    seekOff = 16411;
                    break;

                // SP
                case 49190:
                    seekOff = 38;
                    break;

                case 65574:
                    seekOff = 16384+38;
                    break;

                // no soportado
                default:
                    printf("error can't get screen\n");
                    fclose(file);
                    return RENDER_PREVIEW_ERROR;

            }

            fseek(file, seekOff, SEEK_SET);
            for(int i = 0; i < WORDS_IN_SCREEN; ++i) {
                uint32_t word;
                fread(&word, sizeof(uint32_t), 1, file);
                snapshot[i] = word;
            }
        }
        else { // Z80 format
            // Check Z80 version and arch
            uint8_t z80version;
            uint16_t ahdrlen = 0;

            // stack space for header, should be enough for
            // version 1 (30 bytes)
            // version 2 (55 bytes) (30 + 2 + 23)
            // version 3 (87 bytes) (30 + 2 + 55) or (86 bytes) (30 + 2 + 54)
            uint8_t header[87];

            // read first 30 bytes
            fread(header, 1, 30, file);

            if (header[6] | header[7]) { // Version 1 (PC is != 0)
                z80version = 1;
            } else { // Version 2 o 3
                // header[30]
                fread(&header[30], 1, 2, file); // Additional header len

                ahdrlen = header[30] | (header[31] << 8);
                // additional header block length
                if (ahdrlen == 23)
                    z80version = 2;
                else if (ahdrlen == 54 || ahdrlen == 55)
                    z80version = 3;
                else {
                    printf("Z80.load: unknown version, ahdrlen = %u\n", (unsigned int) ahdrlen);
                    fclose(file);
                    return RENDER_PREVIEW_ERROR;
                }
            }

            // additional vars
            bool dataCompressed = false;

            uint8_t bank = 0;

            if (z80version != 1) {
                int32_t dataOffset = 30 + 2 + ahdrlen;
                dataCompressed = true;
                while (dataOffset < filesize) {
                    fseek(file, dataOffset, SEEK_SET);
                    uint16_t compDataLen;
                    uint8_t dummy[2];
                    fread(&dummy, 1, sizeof(dummy), file); dataOffset += 2;
                    compDataLen = dummy[0] | (dummy[1] << 8);
                    fread(&bank, 1, sizeof(bank), file); dataOffset++;
                    if (bank == 8) {
                        // load Screen
                        if (compDataLen == 0xffff) dataCompressed = false;
                        break;
                    }
                    if (compDataLen == 0xffff) compDataLen = 0x4000;
                    dataOffset += compDataLen;
                }
            } else {
                dataCompressed = (header[12] & 0x20) ? true : false;
            }

            if (z80version == 1 || bank == 8) {
                if (dataCompressed) {
                    // load compressed data into memory
                    loadCompressedScreen(file, snapshot);
                } else {
                    for(int i = 0; i < WORDS_IN_SCREEN; ++i) {
                        uint32_t word;
                        fread(&word, sizeof(uint32_t), 1, file);
                        snapshot[i] = word;
                    }
                }
            }
        }
    }

    fclose(file);

    renderScreenScaled(x0, y0, snapshot, 2, monocrome);

    return ret;
}

#define REP_MARKER 0xAA  // Marca para las secuencias repetidas

void OSD::drawCompressedBMP(int x, int y, const uint8_t * bmp) {

    int l_w = (bmp[5] << 8) + bmp[4]; // Get Width
    int l_h = (bmp[7] << 8) + bmp[6]; // Get Height
    bmp += 8; // Skip header

    size_t cix = 0;
    uint8_t run_length = 0;
    uint8_t color;

    for (int i = 0; i < l_h; i++)
        for(int n = 0; n < l_w; n++) {

            if (run_length) {
                --run_length;
            } else {
                if (bmp[cix] == REP_MARKER && cix + 3 < l_w * l_h && bmp[cix + 1] == REP_MARKER) {
                    // Read length and value
                    run_length = bmp[cix + 2] - 1;
                    color = bmp[cix + 3];
                    cix += 4;
                } else {
                    // Copy non-repeated bytes
                    color = bmp[cix++];
                }

            }

            VIDEO::vga.dotFast(x + n, y + i, color);
        }
}

void OSD::drawOSD(bool bottom_info) {
    unsigned short x = scrAlignCenterX(OSD_W);
    unsigned short y = scrAlignCenterY(OSD_H);
    VIDEO::vga.fillRect(x, y, OSD_W, OSD_H, zxColor(1, 0));
    VIDEO::vga.rect(x, y, OSD_W, OSD_H, zxColor(0, 0));
    VIDEO::vga.rect(x + 1, y + 1, OSD_W - 2, OSD_H - 2, zxColor(7, 0));
    VIDEO::vga.setTextColor(zxColor(0, 0), zxColor(5, 1));
    VIDEO::vga.setFont(SystemFont);
    osdHome();
    VIDEO::vga.print(OSD_TITLE);
    osdAt(22, 0);
    if (bottom_info) {
        string bottom_line;
        switch(Config::videomode) {
            case 0: bottom_line = " Video mode: Standard VGA      "; break;
            case 1: bottom_line = Config::arch[0] == 'T' && Config::ALUTK == 2 ? " Video mode: VGA 60hz          " : " Video mode: VGA 50hz          "; break;
            case 2: bottom_line = Config::arch[0] == 'T' && Config::ALUTK == 2 ? " Video mode: CRT 60hz          " : " Video mode: CRT 50hz          "; break;
        }

        VIDEO::vga.print(bottom_line.append("   c"+string(getShortCommitDate())+" ").c_str()); // For ESPeccy
    } else {
        VIDEO::vga.print(OSD_BOTTOM);
        VIDEO::vga.print(("   c"+string(getShortCommitDate())+" ").c_str()); // For ESPeccy
    }
    osdHome();
}

void OSD::drawKbdLayout(uint8_t layout) {

    uint8_t *layoutdata;

    string vmode;

    string bottom[5] = {
        { "P: PS/2 | T: TK | Z: ZX | 8: ZX81" },  // ZX Spectrum 48K layout
        { "4: 48K | P: PS/2 | Z: ZX | 8: ZX81" },  // TK 90x layout
        { "4: 48K | T: TK | Z: ZX | 8: ZX81" }, // PS/2 kbd help
        { "4: 48K | T: TK | P: PS/2 | 8: ZX81" },  // ZX kbd help
        { "4: 48K | T: TK | Z: ZX | P: PS/2" }  // ZX81+ layout
    };

    switch(Config::videomode) {
        case 0: vmode = "Mode VGA"; break;
        case 1: vmode = Config::arch[0] == 'T' && Config::ALUTK == 2 ? "Mode VGA60" : "Mode VGA50"; break;
        case 2: vmode = Config::arch[0] == 'T' && Config::ALUTK == 2 ? "Mode CRT60" : "Mode CRT50"; break;
    }

    fabgl::VirtualKeyItem Nextkey;

    int width = 256 + OSD_FONT_W * 2;
    int height = 176 + 18;
    int maxW = width / OSD_FONT_W - 4 - vmode.length();

    drawWindow(width, height, "", "" /*bottom[layout]*/, true);

    ResetRowScrollContext(statusBarScrollCTX);

    unsigned short x = scrAlignCenterX(width);
    unsigned short y = scrAlignCenterY(height);

    while (1) {

        // Decode Logo in EBF8 format
        switch (layout) {
        case 0:
            layoutdata = (uint8_t *)Layout_ZX;
            break;
        case 1:
            layoutdata = (uint8_t *)Layout_TK;
            break;
        case 2:
            layoutdata = (uint8_t *)PS2_Kbd;
            break;
        case 3:
            layoutdata = (uint8_t *)ZX_Kbd;
            break;
        case 4:
            layoutdata = (uint8_t *)Layout_ZX81;
            break;
        }

        int pos_x, pos_y;

        if (Config::videomode == 2) {

            if (Config::arch[0] == 'T' && Config::ALUTK == 2) {

                pos_x = 48;
                pos_y = 19;

            } else {

                pos_x = 48;
                pos_y = 43;

            }

        } else {

            pos_x = Config::aspect_16_9 ? 52 : 32;
            pos_y = Config::aspect_16_9 ? 7 : 27;

        }

        drawCompressedBMP(pos_x, pos_y, layoutdata);

        while (1) {

            if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead(KBDREAD_MODEKBDLAYOUT);

            ESPeccy::readKbdJoy();

            if (ESPeccy::PS2Controller.keyboard()->virtualKeyAvailable()) {
                if (ESPeccy::readKbd(&Nextkey, KBDREAD_MODEKBDLAYOUT)) {
                    if(!Nextkey.down) continue;
                    if (Nextkey.vk == fabgl::VK_F1      ||
                        Nextkey.vk == fabgl::VK_ESCAPE  ||
                        Nextkey.vk == fabgl::VK_RETURN  ||
                        Nextkey.vk == fabgl::VK_JOY1A   ||
                        Nextkey.vk == fabgl::VK_JOY1B   ||
                        Nextkey.vk == fabgl::VK_JOY2A   ||
                        Nextkey.vk == fabgl::VK_JOY2B   ||
                        Nextkey.vk == fabgl::VK_4       ||
                        Nextkey.vk == fabgl::VK_8       ||
                        Nextkey.vk == fabgl::VK_T       ||
                        Nextkey.vk == fabgl::VK_t       ||
                        Nextkey.vk == fabgl::VK_P       ||
                        Nextkey.vk == fabgl::VK_p       ||
                        Nextkey.vk == fabgl::VK_Z       ||
                        Nextkey.vk == fabgl::VK_z       ||
                        Nextkey.vk == fabgl::VK_LEFT    ||
                        Nextkey.vk == fabgl::VK_RIGHT
                    ) break;

                }
            }

            VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
            VIDEO::vga.setFont(SystemFont);
            VIDEO::vga.setCursor(x + 3, y + height - 11);

            string text = " " + RotateLine(bottom[layout], &statusBarScrollCTX, maxW, 125, 25) + " " + vmode + " ";
            VIDEO::vga.print(text.c_str());

            vTaskDelay(5 / portTICK_PERIOD_MS);

        }

        if (Nextkey.vk == fabgl::VK_F1 || Nextkey.vk == fabgl::VK_ESCAPE || Nextkey.vk == fabgl::VK_RETURN || Nextkey.vk == fabgl::VK_JOY1A || Nextkey.vk == fabgl::VK_JOY1B || Nextkey.vk == fabgl::VK_JOY2A || Nextkey.vk == fabgl::VK_JOY2B) break;

        uint8_t lastlayout = layout;

        switch(Nextkey.vk) {
            case fabgl::VK_4:
                layout = 0;
                break;
            case fabgl::VK_T:
            case fabgl::VK_t:
                layout = 1;
                break;
            case fabgl::VK_P:
            case fabgl::VK_p:
                layout = 2;
                break;
            case fabgl::VK_Z:
            case fabgl::VK_z:
                layout = 3;
                break;
            case fabgl::VK_8:
                layout = 4;
                break;

            case fabgl::VK_LEFT:
                if (!layout) layout = 4;
                else layout--;
                break;

            case fabgl::VK_RIGHT:
                layout = ( layout + 1 ) % 5;
                break;

        };

        if (layout != lastlayout) ResetRowScrollContext(statusBarScrollCTX);

        lastlayout = layout;

        drawWindow(width, height, "", "", false);

    }

    click();

    if (VIDEO::OSD) OSD::drawStats(); // Redraw stats for 16:9 modes

}

// void OSD::UART_test() {

//     string bottom= " UART TEST                                 ";
//     fabgl::VirtualKeyItem Nextkey;

// 	FILE *fichero;

//     string f_uart = FileUtils::MountPoint + "/uartest.tap";
//     fichero = fopen(f_uart.c_str(), "wb");
//     if (fichero == NULL)
//     {
//         return;
//     }

//     drawWindow(256 + 8, 176 + 18, "", bottom, true);

//     #define UART_BUF_SIZE 1024

//     static const char *TAG = "UART TEST";

//     /* Configure parameters of an UART driver,
//     * communication pins and install the driver */
//     uart_config_t uart_config = {
//         .baud_rate = 460800,
//         .data_bits = UART_DATA_8_BITS,
//         .parity    = UART_PARITY_DISABLE,
//         .stop_bits = UART_STOP_BITS_1,
//         .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
//         .source_clk = UART_SCLK_APB,
//     };
//     int intr_alloc_flags = 0;

//     #if CONFIG_UART_ISR_IN_IRAM
//         intr_alloc_flags = ESP_INTR_FLAG_IRAM;
//     #endif

//     ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, UART_BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
//     ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_config));
//     // ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0, 4, 5, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

//     uint8_t *data;

//     // Configure a temporary buffer for the incoming data
//     data = (uint8_t *) malloc(UART_BUF_SIZE);

//     while (1) {

//         // Read data from the UART
//         int len = uart_read_bytes(UART_NUM_0, data, (UART_BUF_SIZE - 1), 100 / portTICK_PERIOD_MS);

//         // // Write data back to the UART
//         // uart_write_bytes(UART_NUM_0, (const char *) data, len);

//         if (len) {
//             // Write data to file
//             fwrite(data, 1, len, fichero);
//             // data[len] = '\0';
//             // ESP_LOGI(TAG, "Recv str: %s", (char *) data);
//         }

//         if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead();

//         ESPeccy::readKbdJoy();

//         if (ESPeccy::PS2Controller.keyboard()->virtualKeyAvailable()) {
//             if (ESPeccy::readKbd(&Nextkey)) {
//                 if(!Nextkey.down) continue;
//                 if (Nextkey.vk == fabgl::VK_F1 ||
//                     Nextkey.vk == fabgl::VK_ESCAPE ||
//                     Nextkey.vk == fabgl::VK_RETURN ||
//                     Nextkey.vk == fabgl::VK_JOY1A ||
//                     Nextkey.vk == fabgl::VK_JOY1B ||
//                     Nextkey.vk == fabgl::VK_JOY2A ||
//                     Nextkey.vk == fabgl::VK_JOY2B
//                 ) break;

//             }

//         }

//         vTaskDelay(5 / portTICK_PERIOD_MS);

//         if (Nextkey.vk == fabgl::VK_F1 || Nextkey.vk == fabgl::VK_ESCAPE || Nextkey.vk == fabgl::VK_RETURN || Nextkey.vk == fabgl::VK_JOY1A || Nextkey.vk == fabgl::VK_JOY1B || Nextkey.vk == fabgl::VK_JOY2A || Nextkey.vk == fabgl::VK_JOY2B) break;

//     }

//     free(data);

//     uart_driver_delete(UART_NUM_0);

//     fclose(fichero);

//     click();

//     if (VIDEO::OSD) OSD::drawStats(); // Redraw stats for 16:9 modes

// }

void OSD::drawStats() {

    unsigned short x,y;

    if (Config::aspect_16_9) {
        x = 156;
        y = 176;
    } else {
        x = 168;
        y = VIDEO::brdlin_osdstart;
    }

    VIDEO::vga.setTextColor(zxColor(7, 0), zxColor( ESPeccy::ESP_delay, 0));
    VIDEO::vga.setFont(SystemFont);
    VIDEO::vga.setCursor(x,y);
    VIDEO::vga.print(stats_lin1);
    VIDEO::vga.setCursor(x,y+8);
    VIDEO::vga.print(stats_lin2);

}

static bool stateSave(uint8_t slotnumber) {

    struct stat stat_buf;
    char statefname[sizeof(DISK_PSNA_FILE) + 7];
    char statefinfo[sizeof(DISK_PSNA_FILE) + 7];

    // printf(DISK_PSNA_FILE "%u.sna\n",slotnumber);
    // printf(DISK_PSNA_FILE "%u.esp\n",slotnumber);

    sprintf(statefname,DISK_PSNA_FILE "%u.sna",slotnumber);
    sprintf(statefinfo,DISK_PSNA_FILE "%u.esp",slotnumber);
    string finfo = FileUtils::MountPoint + DISK_PSNA_DIR + "/" + statefinfo;

    // Create dir if it doesn't exist
    string dir = FileUtils::MountPoint + DISK_PSNA_DIR;
    if (stat(dir.c_str(), &stat_buf) != 0) {
        if (mkdir(dir.c_str(),0775) != 0) {
            printf("stateSave: problem creating state save dir\n");
            return false;
        }
    }

    // Slot isn't void
    if (stat(finfo.c_str(), &stat_buf) == 0) {
        string title = OSD_PSNA_SAVE[Config::lang];
        string msg = OSD_PSNA_EXISTS[Config::lang];
        uint8_t res = OSD::msgDialog(title,msg);
        if (res != DLG_YES) return false;
    }

    OSD::osdCenteredMsg(OSD_PSNA_SAVING, LEVEL_INFO, 0);

    // Save info file
    FILE *f = fopen(finfo.c_str(), "w");
    if (f == NULL) {

        printf("Error opening %s\n",statefinfo);
        return false;

    } else {

        string state_ALUTK = "255";
        if (Config::arch[0] == 'T') state_ALUTK = std::to_string(Config::ALUTK);

        fputs((Config::arch + "\n" + Config::romSet + "\n" + state_ALUTK + "\n").c_str(),f);    // Put architecture, romset and ALUTK on info file
        fclose(f);

        if (!SaveSnapshot(FileUtils::MountPoint + DISK_PSNA_DIR + "/" + statefname)) OSD::osdCenteredMsg(OSD_PSNA_SAVE_ERR, LEVEL_WARN);

    }

    return true;

}

static bool stateLoad(uint8_t slotnumber) {
    char statefname[sizeof(DISK_PSNA_FILE) + 7];
    char statefinfo[sizeof(DISK_PSNA_FILE) + 7];

    sprintf(statefname,DISK_PSNA_FILE "%u.sna",slotnumber);
    sprintf(statefinfo,DISK_PSNA_FILE "%u.esp",slotnumber);

    if (!FileSNA::isStateAvailable(FileUtils::MountPoint + DISK_PSNA_DIR + "/" + statefname)) {
        OSD::osdCenteredMsg(OSD_PSNA_NOT_AVAIL, LEVEL_INFO);
        return false;
    } else {

        // Read info file
        string finfo = FileUtils::MountPoint + DISK_PSNA_DIR + "/" + statefinfo;
        FILE *f = fopen(finfo.c_str(), "r");
        if (f == NULL) {
            OSD::osdCenteredMsg(OSD_PSNA_LOAD_ERR, LEVEL_WARN);
            // printf("Error opening %s\n",statefinfo);
            return false;
        }

        char buf[256];
        char *result;
        string state_arch = "";
        string state_romset = "";
        string state_ALUTK = "255";

        if ((result = fgets(buf, sizeof(buf),f)) != NULL) {
            state_arch = buf;
            state_arch.pop_back();
        }

        if ((result = fgets(buf, sizeof(buf),f)) != NULL) {
            state_romset = buf;
            state_romset.pop_back();
        }

        if ((result = fgets(buf, sizeof(buf),f)) != NULL) {
            state_ALUTK = buf;
            state_ALUTK.pop_back();
        }

        fclose(f);

        if (!LoadSnapshot(FileUtils::MountPoint + DISK_PSNA_DIR + "/" + statefname, state_arch, state_romset, std::stoi(state_ALUTK))) {
            OSD::osdCenteredMsg(OSD_PSNA_LOAD_ERR, LEVEL_WARN);
            return false;
        } else {
            Config::ram_file = FileUtils::MountPoint + DISK_PSNA_DIR + "/" + statefname;
            Config::last_ram_file = Config::ram_file;
        }
    }

    return true;

}

static string getStringStateCatalog()
{
    char buffer[SLOTNAME_LEN+1] = {0};  // Buffer to store each line, extra char for null-terminator

    string cat[100] = {""};
    string catalog = "";

    mkdir((FileUtils::MountPoint + DISK_PSNA_DIR).c_str(), 0755);

    const string catalogPath = FileUtils::MountPoint + DISK_PSNA_DIR + "/" + "catalog";
    FILE *catalogFile = fopen(catalogPath.c_str(), "rb"); // rb+

    if ( catalogFile ) {
        fseek(catalogFile, 0, SEEK_END);
        long catalogSize = ftell( catalogFile );

        for(int i=0; i < 100; i++) {
            // Move to the correct position in the catalog file
            if (fseek(catalogFile, i * ( sizeof(buffer) - 1 ), SEEK_SET) == 0) {
                size_t bytesRead = fread(buffer, 1, sizeof(buffer) - 1, catalogFile);

                if ( feof( catalogFile ) || ferror( catalogFile ) ) bytesRead = 0;

                buffer[bytesRead] = '\0';  // Ensure null-terminated string

                int readed = bytesRead > 0;

                while( bytesRead-- ) {
                    if ( buffer[ bytesRead ] != ' '  &&
                         buffer[ bytesRead ] != '\t' &&
                         buffer[ bytesRead ] != '\n' ) break;
                    buffer[ bytesRead ] = '\0';
                }

                if ( buffer[0] ) cat[i] = "*" + string(buffer);
            }
        }

        fclose(catalogFile);
    }

    DIR* dir;
    struct dirent* de;

    string fdir = FileUtils::MountPoint + DISK_PSNA_DIR;

    if ((dir = opendir(fdir.c_str())) != nullptr) {
        while ((de = readdir(dir)) != nullptr) {
            string fname = de->d_name;
            if (de->d_type == DT_REG && FileUtils::hasExtension(fname, "sna")) {
                if (fname.substr(0,7) == "persist") {
                    // Extraer la parte entre "persist" y la extension
                    int index = stoi(fname.substr(7, fname.length() - 4 - 7))-1;
                    if (cat[index] == "") cat[index] = "*";
                }
            }
        }
        closedir(dir);

        int pidx = 0;
        for (int i = 0; i < 100; ++i) {
            if ( cat[i] == "" ) {
                catalog += (Config::lang == 0 ? "<Free Slot " :
                            Config::lang == 1 ? "<Ranura Libre " :
                                                "<Slot Livre ") + to_string(i+1) + ">\n";
            } else {
                cat[i].erase(0,1);
                if (cat[i] == "") catalog += "Snapshot " + to_string(i+1) + "\n";
                else              catalog += cat[i] + "\n";
            }
        }

    }

    return catalog;
}

// *******************************************************************************************************
// PREFERRED ROM MENU
// *******************************************************************************************************
void OSD::pref_rom_menu() {

    menu_curopt = 1;
    menu_saverect = true;

    while (1) {

        menu_level = 2;
        uint8_t opt2 = menuRun(MENU_ROM_PREF[Config::lang]);

        if (opt2) {

            menu_level = 3;
            menu_curopt = 1;
            menu_saverect = true;

            if (opt2 == 1) {

                const string menu_res[] = {"48K","48Kes","48Kcs","Last"};

                while (1) {

                    string rpref_menu = MENU_ROM_PREF_48[Config::lang];

                    menu_curopt = prepare_checkbox_menu(rpref_menu,Config::pref_romSet_48);

                    int opt3 = menuRun(rpref_menu);
                    menu_saverect = false;

                    if (opt3 == 0) break;

                    if (opt3 != menu_curopt) {
                        Config::pref_romSet_48 = menu_res[opt3 - 1];
                        Config::save("pref_romSet_48");
                    }

                }

            } else if (opt2 == 2) {

                const string menu_res[] = {"128K","128Kes","+2","+2es","+2fr","ZX81+","128Kcs","Last"};

                while (1) {

                    string rpref_menu = MENU_ROM_PREF_128[Config::lang];

                    menu_curopt = prepare_checkbox_menu(rpref_menu,Config::pref_romSet_128);

                    int opt3 = menuRun(rpref_menu);
                    menu_saverect = false;

                    if (opt3 == 0) break;

                    if (opt3 != menu_curopt) {
                        Config::pref_romSet_128 = menu_res[opt3 - 1];
                        Config::save("pref_romSet_128");
                    }

                }

            } else if (opt2 == 3) {

                const string menu_res[] = {"+2A","+2Aes","+2A41","+2A41es","+2Acs","Last"};

                while (1) {

                    string rpref_menu = MENU_ROMS2A_3_PREF[Config::lang];

                    menu_curopt = prepare_checkbox_menu(rpref_menu,Config::pref_romSet_2A);

                    int opt3 = menuRun(rpref_menu);
                    menu_saverect = false;

                    if (opt3 == 0) break;

                    if (opt3 != menu_curopt) {
                        Config::pref_romSet_2A = menu_res[opt3 - 1];
                        Config::save("pref_romSet_2A");
                    }

                }

            } else if (opt2 == 4) {

                const string menu_res[] = {"v1es","v1pt","v2es","v2pt","v3es","v3pt","v3en","TKcs","Last"};

                while (1) {

                    string rpref_menu = MENU_ROM_PREF_TK90X[Config::lang];

                    menu_curopt = prepare_checkbox_menu(rpref_menu,Config::pref_romSet_TK90X);

                    int opt3 = menuRun(rpref_menu);
                    menu_saverect = false;

                    if (opt3 == 0) break;

                    if (opt3 != menu_curopt) {
                        Config::pref_romSet_TK90X = menu_res[opt3 - 1];
                        Config::save("pref_romSet_90X");
                    }

                }

            } else if (opt2 == 5) {

                const string menu_res[] = {"95es","95pt","Last"};

                while (1) {

                    string rpref_menu = MENU_ROM_PREF_TK95[Config::lang];

                    menu_curopt = prepare_checkbox_menu(rpref_menu,Config::pref_romSet_TK95);

                    int opt3 = menuRun(rpref_menu);
                    menu_saverect = false;

                    if (opt3 == 0) break;

                    if (opt3 != menu_curopt) {
                        Config::pref_romSet_TK95 = menu_res[opt3 - 1];
                        Config::save("pref_romSet_95");
                    }

                }

            }

            menu_curopt = opt2;
            menu_saverect = false;

        } else
            break;

    }

    menu_curopt = 3;

}

Cheat OSD::currentCheat = {};

void OSD::LoadCheatFile(string snapfile) {
    if ( FileUtils::isSDReady() ) {
        if ( !CheatMngr::loadCheatFile( getSnapshotCheatPath( snapfile ) ) ) {
            CheatMngr::closeCheatFile();
        } else {
            showCheatDialog();
        }
    }
}

bool OSD::browseCheatFiles() {
    if ( FileUtils::isSDReady() ) {
//        string currentFile = CheatMngr::getCheatFilename();
        string mFile = fileDialog(FileUtils::CHT_Path, MENU_CHT_TITLE[Config::lang], DISK_CHTFILE, 51, 12);
        if (mFile != "") {
            string fname = FileUtils::MountPoint + FileUtils::CHT_Path + mFile.substr(1);
            if ( FileUtils::isSDReady() ) CheatMngr::loadCheatFile(fname);
        }
        /*
        else {
            if ( FileUtils::isSDReady() ) CheatMngr::loadCheatFile(currentFile);
        } */
    }
    return true;
}

void OSD::showCheatDialog() {

    OSD::flushBackbufferData();

    menu_level = 0;
    menu_curopt = 1;

    if (!CheatMngr::getCheatCount()) {
        menu_saverect = true;
        OSD::browseCheatFiles();
    }

    // Get original values if not set
    CheatMngr::fetchCheatOriginalValuesFromMem();

    uint16_t cheatCounts = CheatMngr::getCheatCount();

    if (cheatCounts) {
        MenuState ms;
        use_current_menu_state = false;
        menu_curopt = 1;

        while(true) {
            string statusbar;
            if (ZXKeyb::Exists || Config::zxunops2) {
                statusbar = Config::lang == 0 ? "\x05+\x06+N: Open | \x05+ENT: Enable/Disable | BRK: Finish & apply" :
                            Config::lang == 1 ? "\x05+\x06+N: Abrir | \x05+ENT: Habilitar/Deshabilitar | BRK: Finalizar y aplicar" :
                                                "\x05+\x06+N: Iniciar | \x05+ENT: Habilitar/Desabilitar | BRK: Finalizar e aplicar";
            } else {
                statusbar = Config::lang == 0 ? "F2: Open | SPC: Enable/Disable | ESC: Finish & apply" :
                            Config::lang == 1 ? "F2: Abrir | ESP: Habilitar/Deshabilitar | ESC: Finalizar y aplicar" :
                                                "F2: Iniciar | ESP: Habilitar/Desabilitar | ESC: Finalizar e aplicar";
            }

            currentCheat = {};

            menuRestoreState(ms);

            menu_saverect = true;

            short opt = menuGenericRun("Cheats", statusbar, nullptr, rowCountCheat, colsCountCheat, menuRedrawCheat, menuProcessCheat);

            menuSaveState(ms);

            if (opt == SHRT_MAX) {
                use_current_menu_state = true;
                continue;
            } else
            if (opt == SHRT_MIN) {
                OSD::restoreBackbufferData();
                menu_saverect = true;
                OSD::browseCheatFiles();
                use_current_menu_state = false;
                menu_curopt = 1;
            } else
            if (opt < 0 && currentCheat.pokeCount) {
                const string title =  Config::lang == 0 ? "Enter value/s" :
                                      Config::lang == 1 ? "Introduzca valor/es" :
                                                          "Insira o(s) valor(es)";

                menu_saverect = true;
                menu_level = 1;
                menu_curopt = 1;

                short opt2 = menuGenericRun(title, "", nullptr, rowCountPoke, colsCountPoke, menuRedrawPoke, menuProcessPokeInput);

                menu_level = 0;
                menu_saverect = false;

                menu_curopt = -opt;
                use_current_menu_state = true;

                continue;
            }
            else
            {
                // Apply cheats
                CheatMngr::applyCheats();
                break;
            }
        }
    }
}

/**
 * @brief Marks the selected option in a menu string.
 *
 * Replaces the `[?]` pattern with `[*]` to indicate the selected option,
 * and resets all other `[X]` patterns to `[ ]`.
 *
 * @param menu The menu string to process.
 * @param selectedKey The character(s) to mark as selected.
 * @return std::string The updated menu string.
 */
std::string markSelectedOption(const std::string& menu, const std::string& selectedKey) {
    std::string updatedMenu = menu;
    size_t pos = 0;

    while ((pos = updatedMenu.find('[', pos)) != std::string::npos) {
        size_t endPos = updatedMenu.find(']', pos); // Find the closing bracket
        if (endPos == std::string::npos) break; // No closing bracket, exit loop

        // Extract content between brackets
        std::string content = updatedMenu.substr(pos + 1, endPos - pos - 1);

        if (content == selectedKey) {
            // Replace `[?]` with `[*]`
            updatedMenu.replace(pos, endPos - pos + 1, "[*]");
        } else {
            // Replace other `[X]` with `[ ]`
            updatedMenu.replace(pos, endPos - pos + 1, "[ ]");
        }

        // Move past the current bracket
        pos = pos + 3; // Length of `[ ]` or `[*]`
    }

    return updatedMenu;
}

std::vector<std::string> extractValues(const std::string& input) {
    std::vector<std::string> result;
    size_t pos = 0;

    // Iterate over the string searching for '[' and then ']'
    while (true) {
        size_t start = input.find('[', pos);
        if (start == std::string::npos)
            break;  // No '[' found

        size_t end = input.find(']', start);
        if (end == std::string::npos)
            break;  // No ']' found after '['

        // Extract the content between the brackets
        std::string value = input.substr(start + 1, end - start - 1);
        result.push_back(value);

        pos = end + 1;  // Continue searching after ']'
    }

    return result;
}


void OSD::do_OSD_MenuUpdateROM(uint8_t arch) {

    string str_arch[] = { " 48K   ", " 128K  ", " +2A   ", " TK    " };


    if ( FileUtils::isSDReady() ) {

        menu_saverect = true;

        string mFile = fileDialog( FileUtils::ROM_Path, (string) MENU_ROM_TITLE[Config::lang] + str_arch[arch-1], DISK_ROMFILE, 42, 10);

        if (mFile != "") {
            mFile.erase(0, 1);
            string fname = FileUtils::MountPoint + FileUtils::ROM_Path + mFile;

            menu_saverect = false;

            uint8_t res = msgDialog((string) OSD_ROM[Config::lang] + str_arch[arch-1], OSD_DLG_SURE[Config::lang]);

            if (res == DLG_YES) {

                if ( FileUtils::isSDReady() ) {
                    // Flash custom ROM
                    FILE *customrom = fopen(fname.c_str(), "rb");
                    if (customrom == NULL) {
                        osdCenteredMsg(OSD_NOROMFILE_ERR[Config::lang], LEVEL_WARN, 2000);
                    } else {
                        esp_err_t res = updateROM(customrom, arch);
                        fclose(customrom);
                        osdCenteredMsg((string)OSD_ROM_ERR[Config::lang] + " Code = " + to_string(res), LEVEL_ERROR, 3000);
                    }
                }
            }
        }
    }

    menu_level = 1;
    menu_saverect = false;

}

void OSD::LoadState() {
    // State Load

    flushBackbufferData();

    menu_level = 0;
    menu_curopt = 1;

    if ( FileUtils::isSDReady() ) {
        // State Load
        string menuload = MENU_STATE_LOAD[Config::lang] + getStringStateCatalog();
        string statusbar;
        if (ZXKeyb::Exists || Config::zxunops2) {
            statusbar = Config::lang == 0 ? "\x05+\x06+N: Rename | \x05+\x06+D: Delete" :
                        Config::lang == 1 ? "\x05+\x06+N: Renombrar | \x05+\x06+D: Borrar" :
                                            "\x05+\x06+N: Renomear | \x05+\x06+D: Excluir";
        } else {
            statusbar = Config::lang == 0 ? "F2: Rename | F8: Delete" :
                        Config::lang == 1 ? "F2: Renombrar | F8: Borrar" :
                                            "F2: Renomear | F8: Excluir";
        }

        //uint8_t opt2 = menuRun(menuload, statusbar, menuProcessSnapshot);
        uint8_t opt2 = menuSlotsWithPreview(menuload, statusbar, menuProcessSnapshot);
        if (opt2 && FileUtils::isSDReady()) {
            if ( stateLoad(opt2) ) {
                // Clear Cheat data
                CheatMngr::closeCheatFile();
            }
        }
    }
}

void OSD::SaveState() {
    // State Save

    flushBackbufferData();

    menu_level = 0;
    menu_curopt = 1;

    if ( FileUtils::isSDReady() ) {
        string menusave = MENU_STATE_SAVE[Config::lang] + getStringStateCatalog();
        string statusbar;
        if (ZXKeyb::Exists || Config::zxunops2) {
            statusbar = Config::lang == 0 ? "\x05+\x06+N: Rename | \x05+\x06+D: Delete" :
                        Config::lang == 1 ? "\x05+\x06+N: Renombrar | \x05+\x06+D: Borrar" :
                                            "\x05+\x06+N: Renomear | \x05+\x06+D: Excluir";
        } else {
            statusbar = Config::lang == 0 ? "F2: Rename | F8: Delete" :
                        Config::lang == 1 ? "F2: Renombrar | F8: Borrar" :
                                            "F2: Renomear | F8: Excluir";
        }

        uint8_t opt2 = menuSlotsWithPreview(menusave, statusbar, menuProcessSnapshotSave);
        if (opt2 && FileUtils::isSDReady() ) if (stateSave(opt2)) return;
    }
}

void OSD::FileBrowser() {

    flushBackbufferData();

    menu_level = 0;

    bool continue_while = true;
    while( continue_while ) {
        menu_saverect = false;
        continue_while = false;
        if (FileUtils::isSDReady()) {
            string mFile = fileDialog(FileUtils::ALL_Path, MENU_FILE_OPEN_TITLE[Config::lang], DISK_ALLFILE, (scrW - OSD_FONT_W * 4) / OSD_FONT_W, 24 /*12*/);
            if (mFile != "") {
                string fprefix = mFile.substr(0,1);
                mFile.erase(0, 1);
                string fname = FileUtils::MountPoint + FileUtils::ALL_Path + mFile;

                int ftype = FileUtils::getFileType(mFile);

                if ( FileUtils::isSDReady() ) {
                    if ( fprefix == "N" || fprefix == "*" ) {
                        // Save new (check if exists)
                        uint8_t res = DLG_YES;
                        struct stat stat_buf;
                        if (stat(fname.c_str(), &stat_buf) == 0) {
                            if (access(fname.c_str(), W_OK)) {
                                OSD::osdCenteredMsg(OSD_READONLY_FILE_WARN[Config::lang], LEVEL_WARN);
                                res = DLG_NO;
                            } else
                                res = msgDialog(OSD_TAPE_SAVE_EXIST[Config::lang],OSD_DLG_SURE[Config::lang]);
                        }

                        if ( res == DLG_YES ) {
                            switch(ftype) {
                                case DISK_SNAFILE:
                                    // New Snapshot
                                    if (!SaveSnapshot(fname, fprefix == "*")) {
                                        OSD::osdCenteredMsg(OSD_PSNA_SAVE_ERR, LEVEL_WARN);
                                    } else {
                                        Config::ram_file = fname;
                                        Config::last_ram_file = fname;
                                    }
                                    break;

                                case DISK_TAPFILE:
                                {
                                    // Create empty tap
                                    int fd = open(fname.c_str(), O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
                                    if (!fd) return;
                                    close(fd);

                                    Tape::LoadTape("S"+fname);
                                    if (Tape::tape) CheatMngr::closeCheatFile(); // Clear Cheat data

                                    break;
                                }

                                default:
                                    OSD::osdCenteredMsg(ERR_INVALID_OPERATION[Config::lang], LEVEL_ERROR, 1500 );
                                    break;

                            }

                        }

                    } else {

                        switch(ftype) {

                            case DISK_SNAFILE:
                                // Load File
                                LoadSnapshot(fname,"","",0xff);
                                menu_saverect = true; // force save background
                                LoadCheatFile(fname);
                                menu_saverect = false; // disable force save background
                                Config::ram_file = fname;
                                Config::last_ram_file = fname;
                                break;

                            case DISK_TAPFILE:
                                Tape::LoadTape(fprefix+fname);
                                if (Tape::tape) CheatMngr::closeCheatFile(); // Clear Cheat data
                                break;

                            case DISK_DSKFILE:
                            {
                                // ***********************************************************************************
                                // BETADISK MENU
                                // ***********************************************************************************
                                menu_curopt = 1;
                                menu_level++;
                                menu_saverect = true; // force save background

                                // Force menu Y position
                                uint16_t oldprevy = prev_y[menu_level];
                                prev_y[menu_level] = FileUtils::fileTypes[DISK_ALLFILE].focus * OSD_FONT_H + OSD_FONT_H;

                                uint8_t dsk_num = menuRun(MENU_BETADISK[Config::lang]);

                                prev_y[menu_level] = oldprevy; // restore menu Y position

                                menu_level--;

                                menu_saverect = false; // disable force save background

                                if (dsk_num > 0) {
                                    if (FileUtils::isSDReady()) {
                                        ESPeccy::Betadisk.EjectDisk(dsk_num - 1);
                                        ESPeccy::Betadisk.InsertDisk(dsk_num - 1, fname);
                                        OSD::osdCenteredMsg(OSD_DISK_INSERTED[Config::lang], LEVEL_INFO);
                                    }
                                } else {
                                    continue_while = true;
                                }
                                break;
                            }

                            case DISK_ROMFILE:
                                if (!ROMLoad::load(fname)) {
                                    OSD::osdCenteredMsg(OSD_ROM_LOAD_ERR, LEVEL_WARN);
                                } else {
                                    Config::ram_file = NO_RAM_FILE;
                                    Config::last_ram_file = NO_RAM_FILE;
                                    Config::save("ram");

                                    Config::rom_file = fname;
                                    Config::last_rom_file = Config::rom_file;
                                    Config::save("rom");

                                    OSD::osdCenteredMsg(OSD_ROM_INSERTED[Config::lang], LEVEL_INFO);
                                }
                                break;

                            case DISK_ESPFILE:
                                break;

                            case DISK_CHTFILE:
                                if (CheatMngr::loadCheatFile(fname)) showCheatDialog();
                                break;

                            case DISK_SCRFILE:
                                if (mFile != "") {
                                    uint32_t *screen_scr = (uint32_t *) VIDEO::SaveRect;
                                    uint32_t *screen_mem = (uint32_t *)(MemESP::videoLatch ? MemESP::ram[7] : MemESP::ram[5]);

                                    // Escribe los primeros 6912 bytes del bitmap en palabras de 32 bits
                                    for(int i = 0; i < WORDS_IN_SCREEN; ++i) *screen_mem++ = *screen_scr++;
                                }
                                break;

                        }
                    }
                }
            }
        }
    }
    if (VIDEO::OSD) OSD::drawStats(); // Redraw stats for 16:9 modes
}

void OSD::FileEject() {

//    menu_level = 0;

    menu_curopt = 1;
    menu_saverect = true; // force save background

    while( 1 ) {

        uint8_t file_close_mnu = menuRun(MENU_FILE_CLOSE[Config::lang]);
        if (file_close_mnu == 1) {
            // Eject Tape
            if (Tape::tapeFileName=="none") {
                osdCenteredMsg(OSD_TAPE_SELECT_ERR[Config::lang], LEVEL_WARN);
            } else {
                Tape::Eject();
                osdCenteredMsg(OSD_TAPE_EJECT[Config::lang], LEVEL_INFO, 1000);
                if (!menu_level) return;
            }
        }
        else
        if (file_close_mnu == 2) {
            // ***********************************************************************************
            // BETADISK MENU
            // ***********************************************************************************
            menu_curopt = 1;
            menu_saverect = true; // force save background
            while(1) {
                menu_level++;
                uint8_t dsk_num = menuRun(MENU_BETADISK[Config::lang]);
                menu_level--;

                if (dsk_num > 0) {
                    if (ESPeccy::Betadisk.DiskInserted(dsk_num - 1)) {
                        ESPeccy::Betadisk.EjectDisk(dsk_num - 1);
                        osdCenteredMsg(OSD_DISK_EJECTED[Config::lang], LEVEL_INFO);
                        if (!menu_level) return;
                    } else {
                        osdCenteredMsg(OSD_DISK_ERR[Config::lang], LEVEL_WARN);
                    }
                    // restoreBackbufferData(true);  // force restore background from last menuRun (BETADISK)
                } else
                    break;

                menu_curopt = dsk_num;
            }
        } else
        if (file_close_mnu == 3) {
            if ( Config::last_rom_file != NO_ROM_FILE || Config::rom_file != NO_ROM_FILE ) {
                Config::rom_file = NO_ROM_FILE;
                Config::last_rom_file = NO_ROM_FILE;
                Config::save("rom");
                osdCenteredMsg(OSD_ROM_EJECT[Config::lang], LEVEL_INFO, 1000);
                ESPeccy::reset();
                if (!menu_level) return;
            } else {
                osdCenteredMsg(OSD_ROM_INSERT_ERR[Config::lang], LEVEL_WARN);
            }
        } else
            break;

        menu_curopt = file_close_mnu;

    }

    if (VIDEO::OSD) OSD::drawStats(); // Redraw stats for 16:9 modes

}

// OSD Main Loop
void OSD::do_OSD(fabgl::VirtualKey KeytoESP, bool CTRL, bool SHIFT) {
    fabgl::VirtualKeyItem Nextkey;

    ESPeccy::sync_realtape = true;

    if (SHIFT && !CTRL) {
        if (KeytoESP == fabgl::VK_F1) { // Show H/W info
            OSD::HWInfo();
        }
        else if (KeytoESP == fabgl::VK_F2) {
            SaveState();
        }
        else if (KeytoESP == fabgl::VK_F5) {
            // Tape Browser
            if (Tape::tapeFileName=="none") {
                OSD::osdCenteredMsg(OSD_TAPE_SELECT_ERR[Config::lang], LEVEL_WARN);
            } else {
                menu_level = 0;
                menu_curopt = 1;
                // int tBlock = menuTape(Tape::tapeFileName.substr(6,28));
                while ( 1 ) {
                    menu_saverect = true;
                    int tBlock = menuTape(Tape::tapeFileName.substr(0,22));
                    if (tBlock >= 0) {
                        Tape::tapeCurBlock = tBlock;
                        Tape::Stop();
                    }
                    if ( tBlock == -2 ) {
                        OSD::restoreBackbufferData();
                    } else
                        break;
                }
            }
        }
        else if (KeytoESP == fabgl::VK_F6) {
            menu_level = 0;
            FileEject();
        }
        else if (KeytoESP == fabgl::VK_F9) { // .pok file manager
            showCheatDialog();
        }
        else
            ESPeccy::sync_realtape = false;

    } else if (CTRL && !SHIFT) {
        if (KeytoESP == fabgl::VK_F1) { // Show kbd layout
            uint8_t layout = 0;
            if (Config::arch[0] == 'T') {
                layout = 1;
            } else if (Config::arch == "128K" && Config::romSet == "ZX81+") {
                layout = 4;
            }
            OSD::drawKbdLayout(layout);

        } else
        if (KeytoESP == fabgl::VK_F2) { // Turbo mode
            ++ESPeccy::ESP_delay &= 0x03;
            ESPeccy::TurboModeSet();

        } else
        if (KeytoESP == fabgl::VK_F5) {
            if (Config::CenterH > -16) Config::CenterH--;
            Config::save("CenterH");
            osdCenteredMsg("Horiz. center: " + to_string(Config::CenterH), LEVEL_INFO, 375);
        } else
        if (KeytoESP == fabgl::VK_F6) {
            if (Config::CenterH < 16) Config::CenterH++;
            Config::save("CenterH");
            osdCenteredMsg("Horiz. center: " + to_string(Config::CenterH), LEVEL_INFO, 375);
        } else
        if (KeytoESP == fabgl::VK_F7) {
            if (Config::CenterV > -16) Config::CenterV--;
            Config::save("CenterV");
            osdCenteredMsg("Vert. center: " + to_string(Config::CenterV), LEVEL_INFO, 375);
        } else
        if (KeytoESP == fabgl::VK_F8) {
            if (Config::CenterV < 16) Config::CenterV++;
            Config::save("CenterV");
            osdCenteredMsg("Vert. center: " + to_string(Config::CenterV), LEVEL_INFO, 375);
        } else
        if (KeytoESP == fabgl::VK_F9) { // Input Poke
            pokeDialog();
        } else
        if (KeytoESP == fabgl::VK_F10) { // NMI
            Z80::triggerNMI();
        } else
        if (KeytoESP == fabgl::VK_F11) { // Reset to TR-DOS (48K only)
            if ((Config::DiskCtrl || Z80Ops::isPentagon) && !Z80Ops::is2a3) {
                // Config::rom_file = NO_ROM_FILE;
                // Config::last_rom_file = NO_ROM_FILE;

                Config::ram_file = NO_RAM_FILE;
                Config::last_ram_file = NO_RAM_FILE;

                // Clear Cheat data
                CheatMngr::closeCheatFile();

                ESPeccy::reset();

                if (Z80Ops::is128 || Z80Ops::isPentagon) MemESP::romLatch = 1;
                MemESP::romInUse = 4;
                MemESP::ramCurrent[0] = MemESP::rom[MemESP::romInUse];
                ESPeccy::trdos = true;

            } else {
                OSD::osdCenteredMsg(TRDOS_RESET_ERR[Config::lang], LEVEL_ERROR, 1500 );
            }
        }
        else
            ESPeccy::sync_realtape = false;


    } else {

        if (KeytoESP == fabgl::VK_PAUSE) {

            osdCenteredMsg(OSD_PAUSE[Config::lang], LEVEL_INFO, 1000);

            while (1) {

                if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead(KBDREAD_MODENORMAL);

                ESPeccy::readKbdJoy();

                if (ESPeccy::PS2Controller.keyboard()->virtualKeyAvailable()) {
                    if (ESPeccy::readKbd(&Nextkey, KBDREAD_MODENORMAL)) {
                        if(!Nextkey.down) continue;
                        if (Nextkey.vk == fabgl::VK_RETURN || Nextkey.vk == fabgl::VK_ESCAPE || Nextkey.vk == fabgl::VK_JOY1A || Nextkey.vk == fabgl::VK_JOY2A || Nextkey.vk == fabgl::VK_JOY1B || Nextkey.vk == fabgl::VK_JOY2B || Nextkey.vk == fabgl::VK_PAUSE) {
                            break;
                        } else
                            osdCenteredMsg(OSD_PAUSE[Config::lang], LEVEL_INFO, 500);
                    }
                }

                vTaskDelay(5 / portTICK_PERIOD_MS);

            }

        }
        else if (KeytoESP == fabgl::VK_F2) {

            // if (MemESP::cur_timemachine > 0)
            //     MemESP::cur_timemachine--;
            // else
            //     MemESP::cur_timemachine=7;

            // if (MemESP::tm_slotbanks[MemESP::cur_timemachine][2] != 0xff)
            //     MemESP::Tm_Load(MemESP::cur_timemachine);

            LoadState();
        }
        else if (KeytoESP == fabgl::VK_F5) {
            FileBrowser();

        }
        else if (KeytoESP == fabgl::VK_F6) {
            // Start / Stop .tap reproduction
            if (Tape::tapeFileName=="none") {
                OSD::osdCenteredMsg(OSD_TAPE_SELECT_ERR[Config::lang], LEVEL_WARN);
            } else {
                if (Tape::tapeStatus & TAPE_STOPPED_FORCED) { OSD::osdCenteredMsg(OSD_TAPE_PLAY[Config::lang], LEVEL_INFO); Tape::tapeStatus = TAPE_STOPPED; } // Play
                else
                if (Tape::tapeStatus == TAPE_LOADING)       { OSD::osdCenteredMsg(OSD_TAPE_STOP[Config::lang], LEVEL_INFO); Tape::tapeStatus = TAPE_STOPPED | TAPE_STOPPED_FORCED; } // Stop
            }
        }
        else if (KeytoESP == fabgl::VK_F8) {
            // Show / hide OnScreen Stats
            if ((VIDEO::OSD & 0x03) == 0)
                VIDEO::OSD |= Tape::tapeStatus == TAPE_LOADING ? 1 : 2;
            else
                VIDEO::OSD++;

            if ((VIDEO::OSD & 0x03) > 2) {
                if ((VIDEO::OSD & 0x04) == 0) {
                    if (Config::aspect_16_9)
                        VIDEO::Draw_OSD169 = Z80Ops::is2a3 ? VIDEO::MainScreen_2A3 : VIDEO::MainScreen;
                    else
                        VIDEO::Draw_OSD43 = Z80Ops::isPentagon ? VIDEO::BottomBorder_Pentagon :  VIDEO::BottomBorder;
                }
                VIDEO::OSD &= 0xfc;

            } else {

                if ((VIDEO::OSD & 0x04) == 0) {
                    if (Config::aspect_16_9)
                        VIDEO::Draw_OSD169 = Z80Ops::is2a3 ? VIDEO::MainScreen_OSD_2A3 : VIDEO::MainScreen_OSD;
                    else
                        VIDEO::Draw_OSD43  = Z80Ops::isPentagon ? VIDEO::BottomBorder_OSD_Pentagon : VIDEO::BottomBorder_OSD;

                    OSD::drawStats();
                }

                ESPeccy::TapeNameScroller = 0;

            }

        }
        else if (KeytoESP == fabgl::VK_F9 || KeytoESP == fabgl::VK_VOLUMEDOWN ||
                 KeytoESP == fabgl::VK_F10 || KeytoESP == fabgl::VK_VOLUMEUP) {

            // EXPERIMENTAL: TIME MACHINE TEST

            // uint8_t slottoload;

            // if (MemESP::tm_framecnt >= 200) {
            //     if (MemESP::cur_timemachine > 0)
            //         slottoload = MemESP::cur_timemachine - 1;
            //     else
            //         slottoload = 7;
            // } else {
            //     if (MemESP::cur_timemachine > 1)
            //         slottoload = MemESP::cur_timemachine - 2;
            //     else
            //         slottoload = MemESP::cur_timemachine == 1 ? 7 : 6;
            // }

            // if (MemESP::tm_slotbanks[slottoload][2] != 0xff)
            //     MemESP::Tm_Load(slottoload);

            // return;

            if (VIDEO::OSD == 0) {

                if (Config::aspect_16_9)
                    VIDEO::Draw_OSD169 = Z80Ops::is2a3 ? VIDEO::MainScreen_OSD_2A3 : VIDEO::MainScreen_OSD;
                else
                    VIDEO::Draw_OSD43  = Z80Ops::isPentagon ? VIDEO::BottomBorder_OSD_Pentagon : VIDEO::BottomBorder_OSD;

                VIDEO::OSD = 0x04;

            } else VIDEO::OSD |= 0x04;

            ESPeccy::totalseconds = 0;
            ESPeccy::totalsecondsnodelay = 0;
            VIDEO::framecnt = 0;

            if (KeytoESP == fabgl::VK_F9 || KeytoESP == fabgl::VK_VOLUMEDOWN) {
                if (ESPeccy::aud_volume>ESP_VOLUME_MIN) {
                    ESPeccy::aud_volume--;
                    pwm_audio_set_volume(ESPeccy::aud_volume);
                }
            } else {
                if (ESPeccy::aud_volume<ESP_VOLUME_MAX) {
                    ESPeccy::aud_volume++;
                    pwm_audio_set_volume(ESPeccy::aud_volume);
                }
            }

            unsigned short x,y;

            if (Config::aspect_16_9) {
                x = 156;
                y = 180;
            } else {
                x = 168;
                y = VIDEO::brdlin_osdstart + 4;
            }

            VIDEO::vga.fillRect(x, y - 4, 24 * OSD_FONT_W, 16, zxColor(1, 0));
            VIDEO::vga.setTextColor(zxColor(7, 0), zxColor(1, 0));
            VIDEO::vga.setFont(SystemFont);
            VIDEO::vga.setCursor(x + OSD_FONT_W, y + 1);
            VIDEO::vga.print(Config::load_monitor ? "TAP" : "VOL");
            for (int i = 0; i < ESPeccy::aud_volume + 16; i++)
                VIDEO::vga.fillRect(x + (i + 7) * OSD_FONT_W, y + 1, OSD_FONT_W - 1, 7, zxColor( 7, 0));

        }
        else if (KeytoESP == fabgl::VK_F11) { // Hard reset
            Config::ram_file = NO_RAM_FILE;
            Config::last_ram_file = NO_RAM_FILE;

            // Clear Cheat data
            CheatMngr::closeCheatFile();

            if (Config::last_rom_file != NO_ROM_FILE) {
                if ( FileUtils::isSDReady() ) ROMLoad::load(Config::last_rom_file);
                Config::rom_file = Config::last_rom_file;
            } else
                ESPeccy::reset();
        }
        else if (KeytoESP == fabgl::VK_F12) { // ESP32 reset
            // ESP host reset
            Config::rom_file = NO_ROM_FILE;
            Config::save("rom");

            Config::ram_file = NO_RAM_FILE;
            Config::save("ram");
            esp_hard_reset();
        }
        else if (KeytoESP == fabgl::VK_F1) {

            menu_curopt = 1;

            while(1) {

                // Main menu
                menu_level = 0;
                menu_saverect = false;

                uint8_t opt = menuRun("ESPeccy " + Config::arch + "\n" + MENU_MAIN[Config::lang]);


                if (opt == 1) {
                    // ***********************************************************************************
                    // FILE MENU
                    // ***********************************************************************************

                    menu_curopt = 1;
                    menu_saverect = true;

                    while(1) {
                        // File menu
                        menu_level = 1;
                        uint8_t file_mnu = menuRun(MENU_FILE[Config::lang]);
                        if (file_mnu == 1) {
                            FileBrowser();
                            return;
                        }
                        else if (file_mnu == 2) {
                            // Tape Browser
                            if (Tape::tapeFileName=="none") {
                                OSD::osdCenteredMsg(OSD_TAPE_SELECT_ERR[Config::lang], LEVEL_WARN);
                            } else {
                                menu_curopt = 1;
                                menu_saverect = true;
                                while ( 1 ) {
                                    menu_saverect = true;
                                    int tBlock = menuTape(Tape::tapeFileName.substr(0,22));
                                    if (tBlock >= 0) {
                                        Tape::tapeCurBlock = tBlock;
                                        Tape::Stop();
                                    }
                                    if ( tBlock == -2 ) {
                                        OSD::restoreBackbufferData();
                                    } else
                                        break;
                                }
                            }
                        }
                        else if (file_mnu == 3) {
                            menu_level = 2;
                            FileEject();
                        }
                        else break;

                        menu_curopt = file_mnu;
                    }

                    menu_curopt = opt;

                }
                else if (opt == 2) {
                    // ***********************************************************************************
                    // STATES MENU
                    // ***********************************************************************************

                    menu_curopt = 1;
                    menu_saverect = true;

                    while(1) {
                        menu_level = 1;
                        // Snapshot menu
                        uint8_t sna_mnu = menuRun(MENU_STATE[Config::lang]);
                        if (sna_mnu > 0) {
                            if (sna_mnu == 1) {
                                // State Load
                                LoadState();
                                return;
                            }
                            else if (sna_mnu == 2) {
                                // State Save
                                SaveState();
                                return;
                            }
                            menu_curopt = sna_mnu;
                        } else {
                            menu_curopt = opt;
                            break;
                        }
                    }
                }
                else if (opt == 3) {
                    // ***********************************************************************************
                    // MACHINE MENU
                    // ***********************************************************************************
                    menu_saverect = true;

                    // Set curopt to reflect current machine
                    if (Config::arch == "48K") menu_curopt = 1;
                    else if (Config::arch == "128K") menu_curopt = 2;
                    else if (Config::arch == "+2A") menu_curopt = 3;
                    else if (Config::arch == "Pentagon") menu_curopt = 4;
                    else if (Config::arch == "TK90X") menu_curopt = 5;
                    else if (Config::arch == "TK95") menu_curopt = 6;

                    while (1) {
                        menu_level = 1;

                        uint8_t arch_num = menuRun(MENU_ARCH[Config::lang]);
                        if (arch_num) {
                            string arch = Config::arch;
                            string romset = Config::romSet;
                            uint8_t opt2 = 0;
                            if (arch_num == 1) { // 48K

                                menu_level = 2;

                                // Set curopt to reflect current romset
                                if (Config::romSet == "48K") menu_curopt = 1;
                                else if (Config::romSet == "48Kes") menu_curopt = 2;
                                else if (Config::romSet == "48Kcs") menu_curopt = 3;
                                else menu_curopt = 1;

                                menu_saverect = true;
                                opt2 = menuRun(MENU_ROMS48[Config::lang]);
                                if (opt2) {
                                    arch = "48K";

                                    if (opt2 == 1) romset = "48K";
                                    else if (opt2 == 2) romset = "48Kes";
                                    else if (opt2 == 3) romset = "48Kcs";

                                    menu_curopt = opt2;
                                    menu_saverect = false;
                                } else {
                                    menu_curopt = 1;
                                    menu_level = 2;
                                }
                            } else if (arch_num == 2) { // 128K
                                menu_level = 2;

                                // Set curopt to reflect current romset
                                if (Config::romSet == "128K") menu_curopt = 1;
                                else if (Config::romSet == "128Kes") menu_curopt = 2;
                                else if (Config::romSet == "+2") menu_curopt = 3;
                                else if (Config::romSet == "+2es") menu_curopt = 4;
                                else if (Config::romSet == "+2fr") menu_curopt = 5;
                                else if (Config::romSet == "ZX81+") menu_curopt = 6;
                                else if (Config::romSet == "128Kcs") menu_curopt = 7;
                                else menu_curopt = 1;

                                menu_saverect = true;
                                opt2 = menuRun(MENU_ROMS128[Config::lang]);
                                if (opt2) {

                                    arch = "128K";

                                    if (opt2 == 1) romset = "128K";
                                    else if (opt2 == 2) romset = "128Kes";
                                    else if (opt2 == 3) romset = "+2";
                                    else if (opt2 == 4) romset = "+2es";
                                    else if (opt2 == 5) romset = "+2fr";
                                    else if (opt2 == 6) romset = "ZX81+";
                                    else if (opt2 == 7) romset = "128Kcs";

                                    menu_curopt = opt2;
                                    menu_saverect = false;

                                } else {
                                    menu_curopt = 1;
                                    menu_level = 2;
                                }
                            } else if (arch_num == 3) {
                                menu_level = 2;

                                // Set curopt to reflect current romset
                                if (Config::romSet == "+2A") menu_curopt = 1;
                                else if (Config::romSet == "+2Aes") menu_curopt = 2;
                                else if (Config::romSet == "+2A41") menu_curopt = 3;
                                else if (Config::romSet == "+2A41es") menu_curopt = 4;
                                else if (Config::romSet == "+2Acs") menu_curopt = 5;
                                else menu_curopt = 1;

                                menu_saverect = true;
                                opt2 = menuRun(MENU_ROMS2A_3[Config::lang]);
                                if (opt2) {

                                    arch = "+2A";

                                    if (opt2 == 1) romset = "+2A";
                                    else if (opt2 == 2) romset = "+2Aes";
                                    else if (opt2 == 3) romset = "+2A41";
                                    else if (opt2 == 4) romset = "+2A41es";
                                    else if (opt2 == 5) romset = "+2Acs";

                                    menu_curopt = opt2;
                                    menu_saverect = false;

                                } else {
                                    menu_curopt = 1;
                                    menu_level = 2;
                                }
                            } else if (arch_num == 4) {
                                arch = "Pentagon";
                                romset = "Pentagon";
                                opt2 = 1;
                            } else if (arch_num == 5) { // TK90X
                                menu_level = 2;

                                // Set curopt to reflect current romset
                                if (Config::romSet == "v1es") menu_curopt = 1;
                                else if (Config::romSet == "v1pt") menu_curopt = 2;
                                else if (Config::romSet == "v2es") menu_curopt = 3;
                                else if (Config::romSet == "v2pt") menu_curopt = 4;
                                else if (Config::romSet == "v3es") menu_curopt = 5;
                                else if (Config::romSet == "v3pt") menu_curopt = 6;
                                else if (Config::romSet == "v3en") menu_curopt = 7;
                                else if (Config::romSet == "TKcs") menu_curopt = 8;
                                else menu_curopt = 1;

                                menu_saverect = true;
                                opt2 = menuRun(MENU_ROMSTK[Config::lang]);
                                if (opt2) {
                                    arch = "TK90X";

                                    if (opt2 == 1) romset = "v1es";
                                    else if (opt2 == 2) romset = "v1pt";
                                    else if (opt2 == 3) romset = "v2es";
                                    else if (opt2 == 4) romset = "v2pt";
                                    else if (opt2 == 5) romset = "v3es";
                                    else if (opt2 == 6) romset = "v3pt";
                                    else if (opt2 == 7) romset = "v3en";
                                    else if (opt2 == 8) romset = "TKcs";

                                    menu_curopt = opt2;
                                    menu_saverect = false;
                                } else {
                                    menu_curopt = 1;
                                    menu_level = 2;
                                }
                            } else if (arch_num == 6) { // TK95
                                menu_level = 2;

                                // Set curopt to reflect current romset
                                if (Config::romSet == "95es") menu_curopt = 1;
                                else if (Config::romSet == "95pt") menu_curopt = 2;
                                else menu_curopt = 1;

                                menu_saverect = true;
                                opt2 = menuRun(MENU_ROMSTK95[Config::lang]);
                                if (opt2) {
                                    arch = "TK95";

                                    if (opt2 == 1) romset = "95es";
                                    else if (opt2 == 2) romset = "95pt";

                                    menu_curopt = opt2;
                                    menu_saverect = false;
                                } else {
                                    menu_curopt = 1;
                                    menu_level = 2;
                                }
                            }

                            if (opt2) {

                                if (arch != Config::arch || romset != Config::romSet) {

                                    Config::ram_file = "none";
                                    Config::save("ram");

                                    Config::rom_file = "none";
                                    Config::save("rom");

                                    if (romset != Config::romSet) {

                                        if (arch == "48K") {

                                            if (Config::pref_romSet_48 == "Last") {

                                                Config::romSet = romset;
                                                Config::save("romSet");
                                                Config::romSet48 = romset;
                                                Config::save("romSet48");

                                            }

                                        } else if (arch == "128K") {

                                            if (Config::pref_romSet_128 == "Last") {

                                                Config::romSet = romset;
                                                Config::save("romSet");
                                                Config::romSet128 = romset;
                                                Config::save("romSet128");

                                            }

                                        } else if (arch == "TK90X") {

                                            if (Config::pref_romSet_TK90X == "Last") {

                                                Config::romSet = romset;
                                                Config::save("romSet");
                                                Config::romSetTK90X = romset;
                                                Config::save("romSetTK90X");

                                            }

                                        } else if (arch == "TK95") {

                                            if (Config::pref_romSet_TK95 == "Last") {

                                                Config::romSet = romset;
                                                Config::save("romSet");
                                                Config::romSetTK95 = romset;
                                                Config::save("romSetTK95");

                                            }
                                        }

                                    }

                                    if (arch != Config::arch) {

                                        bool vreset = Config::videomode;

                                        // If switching between TK models there's no need to reset in vidmodes > 0
                                        if (arch[0] == 'T' && Config::arch[0] == 'T') vreset = false;

                                        if (Config::pref_arch == "Last") {
                                            Config::arch = arch;
                                            Config::save("arch");
                                        }

                                        if (vreset) {

                                            Config::pref_arch += "R";
                                            Config::save("pref_arch");

                                            Config::arch = arch;
                                            Config::save("arch");

                                            Config::romSet = romset;
                                            Config::save("romSet");

                                            Config::rom_file = Config::last_rom_file;
                                            Config::save("rom");

                                            esp_hard_reset();

                                        }

                                    }

                                    Config::requestMachine(arch, romset);

                                }

                                // Clear Cheat data
                                CheatMngr::closeCheatFile();
                                ESPeccy::reset();

                                return;

                            }

                            menu_curopt = arch_num;
                            menu_saverect = false;

                        } else {
                            menu_curopt = opt;
                            break;
                        }
                    }
                }
                else if (opt == 4) {
                    // ***********************************************************************************
                    // RESET MENU
                    // ***********************************************************************************
                    menu_saverect = true;
                    menu_curopt = 1;
                    while(1) {
                        menu_level = 1;
                        // Reset
                        uint8_t opt2 = menuRun(MENU_RESET[Config::lang]);
                        if (opt2 == 1) {
                            // Soft
                            if (Config::last_ram_file != NO_RAM_FILE) {
                                LoadSnapshot(Config::last_ram_file,"","",0xff);
                                LoadCheatFile(Config::last_ram_file);
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
                        else if (opt2 == 2) {
                            // Reset to TR-DOS
                            if (Config::DiskCtrl == 0 && !Z80Ops::isPentagon) {
                                OSD::osdCenteredMsg(TRDOS_RESET_ERR[Config::lang], LEVEL_ERROR, 1500 );
                                return;
                            }

                            Config::ram_file = NO_RAM_FILE;
                            Config::last_ram_file = NO_RAM_FILE;

                            // Config::rom_file = NO_ROM_FILE;
                            // Config::last_rom_file = NO_ROM_FILE;

                            // Clear Cheat data
                            CheatMngr::closeCheatFile();

                            ESPeccy::reset();

                            if (Z80Ops::is128 || Z80Ops::isPentagon) MemESP::romLatch = 1;
                            MemESP::romInUse = 4;
                            MemESP::ramCurrent[0] = MemESP::rom[MemESP::romInUse];
                            ESPeccy::trdos = true;
                            return;

                        }
                        else if (opt2 == 3) {
                            // Hard reset
                            Config::ram_file = NO_RAM_FILE;
                            Config::last_ram_file = NO_RAM_FILE;

                            // Clear Cheat data
                            CheatMngr::closeCheatFile();

                            if (Config::last_rom_file != NO_ROM_FILE) {
                                if ( FileUtils::isSDReady() ) ROMLoad::load(Config::last_rom_file);
                                Config::rom_file = Config::last_rom_file;
                            } else
                                ESPeccy::reset();
                            return;
                        }
                        else if (opt2 == 4) {
                            // ESP host reset
                            Config::rom_file = NO_ROM_FILE;
                            Config::save("rom");
                            Config::ram_file = NO_RAM_FILE;
                            Config::save("ram");
                            esp_hard_reset();
                        }
                        else {
                            menu_curopt = opt;
                            break;
                        }
                    }
                }
                else if (opt == 5) {
                    // ***********************************************************************************
                    // OPTIONS MENU
                    // ***********************************************************************************
                    menu_saverect = true;
                    menu_curopt = 1;
                    while(1) {
                        menu_level = 1;
                        // Options menu
                        uint8_t options_num = menuRun(MENU_OPTIONS[Config::lang]);
                        if (options_num == 1) {
                            menu_level = 2;
                            menu_curopt = 1;
                            menu_saverect = true;
                            while (1) {
                                string stor_menu = MENU_STORAGE[Config::lang];
                                uint8_t opt2 = menuRun(stor_menu);
                                if (opt2) {
                                    if (opt2 == 1) {
                                        menu_level = 3;
                                        menu_curopt = 1;
                                        menu_saverect = true;
                                        while (1) {
                                            string opt_menu = MENU_DISKCTRL[Config::lang];
                                            bool prev_opt = Config::DiskCtrl;
                                            if (prev_opt) {
                                                menu_curopt = 1;
                                                opt_menu += markSelectedOption(MENU_YESNO[Config::lang], "Y");
                                            } else {
                                                menu_curopt = 2;
                                                opt_menu += markSelectedOption(MENU_YESNO[Config::lang], "N");
                                            }
                                            uint8_t opt2 = menuRun(opt_menu);
                                            if (opt2) {
                                                if (opt2 == 1)
                                                    Config::DiskCtrl = 1;
                                                else
                                                    Config::DiskCtrl = 0;

                                                if (Config::DiskCtrl != prev_opt) {
                                                    Config::save("DiskCtrl");
                                                }
                                                menu_curopt = opt2;
                                                menu_saverect = false;
                                            } else {
                                                break;
                                            }
                                        }
                                    }
                                    else if (opt2 == 2) {
                                        menu_level = 3;
                                        menu_curopt = 1;
                                        menu_saverect = true;
                                        while (1) {
                                            string TapeAutoload_menu = MENU_AUTOLOAD[Config::lang];
                                            bool prev_TapeAutoload = Config::TapeAutoload;
                                            if (prev_TapeAutoload) {
                                                menu_curopt = 1;
                                                TapeAutoload_menu += markSelectedOption(MENU_YESNO[Config::lang], "Y");
                                            } else {
                                                menu_curopt = 2;
                                                TapeAutoload_menu += markSelectedOption(MENU_YESNO[Config::lang], "N");
                                            }
                                            uint8_t opt2 = menuRun(TapeAutoload_menu);
                                            if (opt2) {
                                                if (opt2 == 1)
                                                    Config::TapeAutoload = true;
                                                else
                                                    Config::TapeAutoload = false;

                                                if (Config::TapeAutoload != prev_TapeAutoload) {
                                                    Config::save("TapeAutoload");
                                                }
                                                menu_curopt = opt2;
                                                menu_saverect = false;
                                            } else {
                                                break;
                                            }
                                        }
                                    }
                                    else if (opt2 == 3) {
                                        menu_level = 3;
                                        menu_curopt = 1;
                                        menu_saverect = true;
                                        while (1) {
                                            string flash_menu = MENU_FLASHLOAD[Config::lang];
                                            bool prev_flashload = Config::flashload;
                                            if (prev_flashload) {
                                                menu_curopt = 1;
                                                flash_menu += markSelectedOption(MENU_YESNO[Config::lang], "Y");
                                            } else {
                                                menu_curopt = 2;
                                                flash_menu += markSelectedOption(MENU_YESNO[Config::lang], "N");
                                            }
                                            uint8_t opt2 = menuRun(flash_menu);
                                            if (opt2) {
                                                if (opt2 == 1)
                                                    Config::flashload = true;
                                                else
                                                    Config::flashload = false;

                                                if (Config::flashload != prev_flashload) {
                                                    Config::save("flashload");
                                                }
                                                menu_curopt = opt2;
                                                menu_saverect = false;
                                            } else {
                                                break;
                                            }
                                        }
                                    }
                                    else if (opt2 == 4) {
                                        menu_level = 3;
                                        menu_curopt = 1;
                                        menu_saverect = true;
                                        while (1) {
                                            string mnu_str = MENU_RGTIMINGS[Config::lang];
                                            bool prev_opt = Config::tape_timing_rg;
                                            if (prev_opt) {
                                                menu_curopt = 1;
                                                mnu_str += markSelectedOption(MENU_YESNO[Config::lang], "Y");
                                            } else {
                                                menu_curopt = 2;
                                                mnu_str += markSelectedOption(MENU_YESNO[Config::lang], "N");
                                            }
                                            uint8_t opt2 = menuRun(mnu_str);
                                            if (opt2) {
                                                if (opt2 == 1)
                                                    Config::tape_timing_rg = true;
                                                else
                                                    Config::tape_timing_rg = false;

                                                if (Config::tape_timing_rg != prev_opt) {
                                                    Config::save("tape_timing_rg");

                                                    if (Tape::tape != NULL && Tape::tapeFileType == TAPE_FTYPE_TAP) {
                                                        Tape::TAP_setBlockTimings();
                                                    }
                                                }
                                                menu_curopt = opt2;
                                                menu_saverect = false;
                                            } else {
                                                break;
                                            }
                                        }
                                    }
                                    else if (opt2 == 5) {
                                        menu_level = 3;
                                        menu_curopt = 1;
                                        menu_saverect = true;
                                        while (1) {
                                            string Mnustr = MENU_TAPEMONITOR[Config::lang];
                                            bool prev_opt = Config::load_monitor;
                                            if (prev_opt) {
                                                menu_curopt = 1;
                                                Mnustr += markSelectedOption(MENU_YESNO[Config::lang], "Y");
                                            } else {
                                                menu_curopt = 2;
                                                Mnustr += markSelectedOption(MENU_YESNO[Config::lang], "N");
                                            }

                                            uint8_t opt3 = menuRun(Mnustr);
                                            if (opt3) {
                                                if (opt3 == 1)
                                                    Config::load_monitor = true;
                                                else
                                                    Config::load_monitor = false;

                                                if (Config::load_monitor != prev_opt) {
                                                    if (Config::load_monitor) {
                                                        ESPeccy::aud_volume = ESP_VOLUME_MAX;
                                                    } else {
                                                        ESPeccy::aud_volume = Config::volume;
                                                    }
                                                    pwm_audio_set_volume(ESPeccy::aud_volume);
                                                    Config::save("load_monitor");
                                                }
                                                menu_curopt = opt3;
                                                menu_saverect = false;
                                            } else {
                                                break;
                                            }
                                        }
                                    }
                                    else if (opt2 == 6) {
                                        menu_level = 3;
                                        menu_curopt = 1;
                                        menu_saverect = true;
                                        while (1) {
                                            uint8_t prev_opt = Config::realtape_mode;

                                            string Mnustr = markSelectedOption(MENU_REALTAPE[Config::lang], to_string(prev_opt));
                                            menu_curopt = Config::realtape_mode + 1;

                                            uint8_t opt3 = menuRun(Mnustr);
                                            if (opt3) {
                                                if (opt3 - 1 != prev_opt) {
                                                    Config::realtape_mode = opt3 - 1;
                                                    Config::save("RealTapeMode");
                                                }
                                                menu_curopt = opt3;
                                                menu_saverect = false;
                                            } else {
                                                break;
                                            }
                                        }
                                    }

                                    menu_level = 2;
                                    menu_curopt = opt2;
                                    menu_saverect = false;
                                } else {
                                    menu_curopt = options_num;
                                    break;
                                }
                            }
                        }
                        else if (options_num == 2) {
                            menu_level = 2;
                            menu_curopt = 1;
                            menu_saverect = true;
                            while (1) {
                                string archprefmenu;
                                string prev_archpref = Config::pref_arch;
                                menu_curopt = 7;
                                if (Config::pref_arch == "48K") {
                                    menu_curopt = 1;
                                    archprefmenu = markSelectedOption(MENU_ARCH_PREF[Config::lang], "4");
                                } else if (Config::pref_arch == "128K") {
                                    menu_curopt = 2;
                                    archprefmenu = markSelectedOption(MENU_ARCH_PREF[Config::lang], "1");
                                } else if (Config::pref_arch == "+2A") {
                                    menu_curopt = 3;
                                    archprefmenu = markSelectedOption(MENU_ARCH_PREF[Config::lang], "2");
                                } else if (Config::pref_arch == "Pentagon") {
                                    menu_curopt = 4;
                                    archprefmenu = markSelectedOption(MENU_ARCH_PREF[Config::lang], "P");
                                } else if (Config::pref_arch == "TK90X") {
                                    menu_curopt = 5;
                                    archprefmenu = markSelectedOption(MENU_ARCH_PREF[Config::lang], "T");
                                } else if (Config::pref_arch == "TK95") {
                                    menu_curopt = 6;
                                    archprefmenu = markSelectedOption(MENU_ARCH_PREF[Config::lang], "9");
                                }
                                if (menu_curopt == 7)
                                    archprefmenu = markSelectedOption(MENU_ARCH_PREF[Config::lang], "L");

                                uint8_t opt2 = menuRun(archprefmenu);
                                if (opt2) {

                                    if (opt2 == 1)
                                        Config::pref_arch = "48K";
                                    else
                                    if (opt2 == 2)
                                        Config::pref_arch = "128K";
                                    else
                                    if (opt2 == 3)
                                        Config::pref_arch = "+2A";
                                    else
                                    if (opt2 == 4)
                                        Config::pref_arch = "Pentagon";
                                    else
                                    if (opt2 == 5)
                                        Config::pref_arch = "TK90X";
                                    else
                                    if (opt2 == 6)
                                        Config::pref_arch = "TK95";
                                    else
                                    if (opt2 == 7)
                                        Config::pref_arch = "Last";

                                    if (Config::pref_arch != prev_archpref) {
                                        Config::save("pref_arch");
                                    }

                                    menu_curopt = opt2;
                                    menu_saverect = false;

                                } else {
                                    menu_curopt = options_num;
                                    break;
                                }
                            }
                        }
                        else if (options_num == 3) {
                            pref_rom_menu();
                        }
                        else if (options_num == 4) {
                            menu_level = 2;
                            menu_curopt = 1;
                            menu_saverect = true;
                            while (1) {
                                // joystick
                                string Mnustr = MENU_JOY[Config::lang];
                                uint8_t opt2 = menuRun(Mnustr);
                                if (opt2) {
                                    // Joystick customization
                                    menu_level = 3;
                                    menu_curopt = 1;
                                    menu_saverect = true;
                                    while (1) {
                                        menu_curopt = (opt2 == 1 ? Config::joystick1 : Config::joystick2) + 1;
                                        string joy_menu = markSelectedOption(MENU_DEFJOY[Config::lang], to_string(menu_curopt - 1));
                                        joy_menu.replace(joy_menu.find("#",0),1,(string)" " + char(48 + opt2));
                                        uint8_t optjoy = menuRun(joy_menu);
                                        if (optjoy>0 && optjoy<6) {
                                            if (opt2 == 1) {
                                                Config::joystick1 = optjoy - 1;
                                                Config::save("joystick1");
                                            } else {
                                                Config::joystick2 = optjoy - 1;
                                                Config::save("joystick2");
                                            }
                                            Config::setJoyMap(opt2,optjoy - 1);
                                            menu_curopt = optjoy;
                                            menu_saverect = false;
                                        } else if (optjoy == 6) {
                                            joyDialog(opt2);
                                            if (VIDEO::OSD) OSD::drawStats(); // Redraw stats for 16:9 modes
                                            return;
                                        } else {
                                            menu_curopt = opt2;
                                            menu_level = 2;
                                            break;
                                        }
                                    }
                                } else {
                                    menu_curopt = options_num;
                                    break;
                                }
                            }
                        }
                        else if (options_num == 5) {
                            menu_level = 2;
                            menu_curopt = 1;
                            menu_saverect = true;
                            while (1) {
                                // joystick
                                string Mnustr = MENU_JOYPS2[Config::lang];
                                uint8_t opt2 = menuRun(Mnustr);
                                if (opt2 == 1) {
                                    // Joystick type
                                    menu_level = 3;
                                    menu_curopt = 1;
                                    menu_saverect = true;
                                    while (1) {
                                        string joy_menu = markSelectedOption(MENU_PS2JOYTYPE[Config::lang], to_string(Config::joyPS2));
                                        menu_curopt = Config::joyPS2 + 1;
                                        uint8_t optjoy = menuRun(joy_menu);
                                        if (optjoy > 0 && optjoy < 6) {
                                            if (Config::joyPS2 != (optjoy - 1)) {
                                                Config::joyPS2 = optjoy - 1;
                                                Config::save("joyPS2");
                                            }
                                            menu_curopt = optjoy;
                                            menu_saverect = false;
                                        } else {
                                            menu_curopt = 1;
                                            menu_level = 2;
                                            break;
                                        }
                                    }
                                } else if (opt2 == 2) {
                                    // Menu cursor keys as joy
                                    menu_level = 3;
                                    menu_curopt = 1;
                                    menu_saverect = true;
                                    while (1) {
                                        string csasjoy_menu = MENU_CURSORJOY[Config::lang];
                                        if (Config::CursorAsJoy) {
                                            menu_curopt = 1;
                                            csasjoy_menu += markSelectedOption(MENU_YESNO[Config::lang], "Y");
                                        } else {
                                            menu_curopt = 2;
                                            csasjoy_menu += markSelectedOption(MENU_YESNO[Config::lang], "N");
                                        }
                                        uint8_t opt2 = menuRun(csasjoy_menu);
                                        if (opt2) {
                                            if (opt2 == 1)
                                                Config::CursorAsJoy = true;
                                            else
                                                Config::CursorAsJoy = false;

                                            ESPeccy::PS2Controller.keyboard()->setLEDs(false,false,Config::CursorAsJoy);
                                            if(ESPeccy::ps2kbd2)
                                                ESPeccy::PS2Controller.keybjoystick()->setLEDs(false, false, Config::CursorAsJoy);
                                            Config::save("CursorAsJoy");

                                            menu_curopt = opt2;
                                            menu_saverect = false;
                                        } else {
                                            menu_curopt = 2;
                                            menu_level = 2;
                                            break;
                                        }
                                    }
                                } else if (opt2 == 3) {
                                    // Menu TAB as fire 1
                                    menu_level = 3;
                                    menu_curopt = 1;
                                    menu_saverect = true;
                                    while (1) {
                                        string csasjoy_menu = MENU_TABASFIRE[Config::lang];
                                        bool prev_opt = Config::TABasfire1;
                                        if (prev_opt) {
                                            menu_curopt = 1;
                                            csasjoy_menu += markSelectedOption(MENU_YESNO[Config::lang], "Y");
                                        } else {
                                            menu_curopt = 2;
                                            csasjoy_menu += markSelectedOption(MENU_YESNO[Config::lang], "N");
                                        }
                                        uint8_t opt2 = menuRun(csasjoy_menu);
                                        if (opt2) {
                                            if (opt2 == 1)
                                                Config::TABasfire1 = true;
                                            else
                                                Config::TABasfire1 = false;

                                            if (Config::TABasfire1 != prev_opt) {

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

                                                Config::save("TABasfire1");
                                            }

                                            menu_curopt = opt2;
                                            menu_saverect = false;
                                        } else {
                                            menu_curopt = 3;
                                            menu_level = 2;
                                            break;
                                        }
                                    }
                                } else {
                                    menu_curopt = options_num;
                                    break;
                                }
                            }
                        }
                        else if (options_num == 6) {

                            menu_level = 2;
                            menu_curopt = 1;
                            menu_saverect = true;
                            while (1) {
                                // Video
                                uint8_t opt1 = menuRun(MENU_VIDEO[Config::lang]);
                                if (opt1 > 0) {
                                    if (opt1 == 1) {
                                        menu_level = 3;
                                        menu_curopt = 1;
                                        menu_saverect = true;
                                        while (1) {
                                            string opt_menu;
                                            uint8_t prev_opt = Config::render;
                                            if (prev_opt) {
                                                menu_curopt = 2;
                                                opt_menu = markSelectedOption(MENU_RENDER[Config::lang], "A");
                                            } else {
                                                menu_curopt = 1;
                                                opt_menu = markSelectedOption(MENU_RENDER[Config::lang], "S");
                                            }
                                            uint8_t opt2 = menuRun(opt_menu);
                                            if (opt2) {
                                                if (opt2 == 1)
                                                    Config::render = 0;
                                                else
                                                    Config::render = 1;

                                                if (Config::render != prev_opt) {
                                                    Config::save("render");

                                                    VIDEO::snow_toggle = Config::arch != "Pentagon" ? Config::render : false;

                                                    if (VIDEO::snow_toggle) {
                                                        VIDEO::Draw = &VIDEO::MainScreen_Blank_Snow;
                                                        VIDEO::Draw_Opcode = &VIDEO::MainScreen_Blank_Snow_Opcode;
                                                    } else {
                                                        VIDEO::Draw = &VIDEO::MainScreen_Blank;
                                                        VIDEO::Draw_Opcode = &VIDEO::MainScreen_Blank_Opcode;
                                                    }

                                                }
                                                menu_curopt = opt2;
                                                menu_saverect = false;
                                            } else {
                                                menu_curopt = 1;
                                                menu_level = 2;
                                                break;
                                            }
                                        }
                                    }
                                    else if (opt1 == 2) {
                                        menu_level = 3;
                                        menu_curopt = 1;
                                        menu_saverect = true;
                                        while (1) {

                                            // aspect ratio
                                            string asp_menu;
                                            bool prev_asp = Config::aspect_16_9;
                                            if (prev_asp) {
                                                menu_curopt = 2;
                                                asp_menu = markSelectedOption(MENU_ASPECT[Config::lang], "1");
                                            } else {
                                                menu_curopt = 1;
                                                asp_menu = markSelectedOption(MENU_ASPECT[Config::lang], "4");
                                            }
                                            uint8_t opt2 = menuRun(asp_menu);
                                            if (opt2) {

                                                if (Config::videomode == 2) opt2 = 1; // Force 4:3 aspect ratio in CRT mode

                                                if (opt2 == 1)
                                                    Config::aspect_16_9 = false;
                                                else
                                                    Config::aspect_16_9 = true;

                                                if (Config::aspect_16_9 != prev_asp) {
                                                    Config::rom_file = "none";
                                                    Config::ram_file = "none";
                                                    Config::save("asp169");
                                                    Config::save("ram");
                                                    Config::save("rom");
                                                    esp_hard_reset();
                                                }

                                                menu_curopt = opt2;
                                                menu_saverect = false;

                                            } else {
                                                menu_curopt = 2;
                                                menu_level = 2;
                                                break;
                                            }
                                        }
                                    }
                                    else if (opt1 == 3) {
                                        menu_level = 3;
                                        menu_curopt = 1;
                                        menu_saverect = true;
                                        while (1) {
                                            string opt_menu = MENU_SCANLINES[Config::lang];
                                            uint8_t prev_opt = Config::scanlines;
                                            if (prev_opt) {
                                                menu_curopt = 1;
                                                opt_menu += markSelectedOption(MENU_YESNO[Config::lang], "Y");
                                            } else {
                                                menu_curopt = 2;
                                                opt_menu += markSelectedOption(MENU_YESNO[Config::lang], "N");
                                            }
                                            uint8_t opt2 = menuRun(opt_menu);
                                            if (opt2) {
                                                if (opt2 == 1)
                                                    Config::scanlines = 1;
                                                else
                                                    Config::scanlines = 0;

                                                if (Config::scanlines != prev_opt) {
                                                    Config::rom_file = "none";
                                                    Config::ram_file = "none";
                                                    Config::save("scanlines");
                                                    Config::save("ram");
                                                    Config::save("rom");
                                                    // Reset to apply if mode != CRT
                                                    if (Config::videomode!=2) esp_hard_reset();
                                                }
                                                menu_curopt = opt2;
                                                menu_saverect = false;
                                            } else {
                                                menu_curopt = 3;
                                                menu_level = 2;
                                                break;
                                            }
                                        }
                                    }
                                } else {
                                    menu_curopt = options_num;
                                    break;
                                }
                            }
                        }
                        else if (options_num == 7) {
                            menu_level = 2;
                            menu_curopt = 1;
                            menu_saverect = true;
                            while (1) {
                                // Mouse
                                uint8_t opt1 = menuRun(MENU_MOUSE[Config::lang]);
                                if (opt1 > 0) {
                                    if (opt1 == 1) {
                                        menu_level = 3;
                                        menu_curopt = 1;
                                        menu_saverect = true;
                                        while (1) {
                                            uint8_t values[] = { 10, 20, 40, 60, 80, 100, 200 };

                                            string mouse_sample_rate_menu = markSelectedOption(MENU_MOUSE_SAMPLE_RATE[Config::lang], to_string(Config::mousesamplerate));
                                            menu_curopt = std::find(values, values + sizeof(values)/sizeof(values[0]), Config::mousesamplerate) - values;
                                            if ( menu_curopt > sizeof(values)/sizeof(values[0]) ) menu_curopt = 1;
                                            else menu_curopt = menu_curopt + 1;
                                            uint8_t opt2 = menuRun(mouse_sample_rate_menu);
                                            if (opt2) {
                                                if (Config::mousesamplerate != values[opt2 - 1]) {
                                                    Config::mousesamplerate = values[opt2 - 1];
                                                    Config::save("MouseSampleRate");
                                                    if (ESPeccy::PS2Controller.mouse()) ESPeccy::PS2Controller.mouse()->setSampleRate(Config::mousesamplerate);
                                                }
                                                menu_curopt = opt2;
                                                menu_saverect = false;
                                            } else {
                                                menu_curopt = 1;
                                                menu_level = 2;
                                                break;
                                            }
                                        }
                                    }
                                    else if (opt1 == 2) {
                                        menu_level = 3;
                                        menu_curopt = 1;
                                        menu_saverect = true;
                                        while (1) {
                                            string mouse_dpi_menu = markSelectedOption(MENU_MOUSE_DPI[Config::lang], to_string(Config::mousedpi));
                                            menu_curopt = Config::mousedpi + 1;
                                            uint8_t opt2 = menuRun(mouse_dpi_menu);
                                            if (opt2) {
                                                if (Config::mousedpi != opt2 - 1) {
                                                    Config::mousedpi = opt2 - 1;
                                                    Config::save("MouseDPI");
                                                    if (ESPeccy::PS2Controller.mouse()) ESPeccy::PS2Controller.mouse()->setResolution(Config::mousedpi);
                                                }
                                                menu_curopt = opt2;
                                                menu_saverect = false;
                                            } else {
                                                menu_curopt = 2;
                                                menu_level = 2;
                                                break;
                                            }
                                        }
                                    }
                                    else if (opt1 == 3) {
                                        menu_level = 3;
                                        menu_curopt = 1;
                                        menu_saverect = true;
                                        while (1) {
                                            string mouse_scaling_menu = markSelectedOption(MENU_MOUSE_SCALING[Config::lang], to_string(Config::mousescaling));
                                            menu_curopt = Config::mousescaling;
                                            uint8_t opt2 = menuRun(mouse_scaling_menu);
                                            if (opt2) {
                                                if (Config::mousescaling != opt2) {
                                                    Config::mousescaling = opt2;
                                                    Config::save("MouseScaling");
                                                    if (ESPeccy::PS2Controller.mouse()) ESPeccy::PS2Controller.mouse()->setScaling(Config::mousescaling);
                                                }
                                                menu_curopt = opt2;
                                                menu_saverect = false;
                                            } else {
                                                menu_curopt = 3;
                                                menu_level = 2;
                                                break;
                                            }
                                        }
                                    }
                                } else {
                                    menu_curopt = options_num;
                                    break;
                                }
                            }
                        }
                        else if (options_num == 8) {
                            menu_level = 2;
                            menu_curopt = 1;
                            menu_saverect = true;
                            while (1) {
                                // Sound
                                uint8_t opt1 = menuRun(MENU_SOUND[Config::lang]);
                                if (opt1 > 0) {
                                    if (opt1 == 1) {
                                        menu_level = 3;
                                        menu_curopt = 1;
                                        menu_saverect = true;
                                        while (1) {
                                            string ay_menu = MENU_AY48[Config::lang];
                                            bool prev_ay48 = Config::AY48;
                                            if (prev_ay48) {
                                                menu_curopt = 1;
                                                ay_menu += markSelectedOption(MENU_YESNO[Config::lang], "Y");
                                            } else {
                                                menu_curopt = 2;
                                                ay_menu += markSelectedOption(MENU_YESNO[Config::lang], "N");
                                            }
                                            uint8_t opt2 = menuRun(ay_menu);
                                            if (opt2) {
                                                if (opt2 == 1)
                                                    Config::AY48 = true;
                                                else
                                                    Config::AY48 = false;

                                                if (Config::AY48 != prev_ay48) {
                                                if (Z80Ops::is48) {
                                                    ESPeccy::AY_emu = Config::AY48;
                                                    if (ESPeccy::AY_emu == false) {
                                                    for (int i=0; i < ESPeccy::samplesPerFrame; i++)
                                                        AySound::SamplebufAY[i] = ESPeccy::audioBuffer[i] = 0;
                                                }
                                                    ESPeccy::aud_active_sources = (Config::Covox & 0x01) | (ESPeccy::AY_emu << 1);
                                                }

                                                Config::save("AY48");
                                            }
                                                menu_curopt = opt2;
                                                menu_saverect = false;
                                            } else {
                                                menu_curopt = 1;
                                                menu_level = 2;
                                                break;
                                            }
                                        }
                                    }
                                    else if (opt1 == 2) {
                                        menu_level = 3;
                                        menu_curopt = 1;
                                        menu_saverect = true;
                                        while (1) {
                                            string optmenu;
                                            uint8_t prev_opt = Config::Covox;
                                            if (prev_opt == 0) {
                                                menu_curopt = 1;
                                                optmenu = markSelectedOption(MENU_COVOX[Config::lang], "N");
                                            } else
                                            if (prev_opt == 1) {
                                                menu_curopt = 2;
                                                optmenu = markSelectedOption(MENU_COVOX[Config::lang], "M");
                                            } else
                                            if (prev_opt == 2) {
                                                menu_curopt = 3;
                                                optmenu = markSelectedOption(MENU_COVOX[Config::lang], "S");
                                            } else
                                            if (prev_opt == 3) {
                                                menu_curopt = 4;
                                                optmenu = markSelectedOption(MENU_COVOX[Config::lang], "1");
                                            } else
                                            if (prev_opt == 4) {
                                                menu_curopt = 5;
                                                optmenu = markSelectedOption(MENU_COVOX[Config::lang], "2");
                                            }
                                            uint8_t opt2 = menuRun(optmenu);
                                            if (opt2) {
                                                if (Config::Covox != (opt2 - 1)) {
                                                    Config::Covox = opt2 -1;
                                                    Config::save("Covox");
                                                    ESPeccy::covoxData[0] = ESPeccy::covoxData[1] = ESPeccy::covoxData[2] = ESPeccy::covoxData[3] = 0;
                                                    for (int i=0; i < ESPeccy::samplesPerFrame; i++)
                                                        ESPeccy::SamplebufCOVOX[i] = ESPeccy::audioBuffer[i] = 0;
                                                    ESPeccy::aud_active_sources = (Config::Covox & 0x01) | (ESPeccy::AY_emu << 1);
                                                }
                                                menu_curopt = opt2;
                                                menu_saverect = false;
                                            } else {
                                                menu_curopt = 2;
                                                menu_level = 2;
                                                break;
                                            }
                                        }
                                    }
                                } else {
                                    menu_curopt = options_num;
                                    break;
                                }
                            }
                        }
                        else if (options_num == 9) {
                            menu_level = 2;
                            menu_curopt = 1;
                            menu_saverect = true;
                            while (1) {
                                // Other
                                uint8_t opt1 = menuRun(MENU_OTHER[Config::lang]);
                                if (opt1 > 0) {
                                    if (opt1 == 1) {
                                        menu_level = 3;
                                        menu_curopt = 1;
                                        menu_saverect = true;
                                        while (1) {
                                            string alu_menu;
                                            uint8_t prev_AluTiming = Config::AluTiming;
                                            if (prev_AluTiming == 0) {
                                                menu_curopt = 1;
                                                alu_menu = markSelectedOption(MENU_ALUTIMING[Config::lang], "E");
                                            } else {
                                                menu_curopt = 2;
                                                alu_menu = markSelectedOption(MENU_ALUTIMING[Config::lang], "L");
                                            }
                                            uint8_t opt2 = menuRun(alu_menu);
                                            if (opt2) {
                                                if (opt2 == 1)
                                                    Config::AluTiming = 0;
                                                else
                                                    Config::AluTiming = 1;

                                                if (Config::AluTiming != prev_AluTiming) {
                                                    CPU::latetiming = Config::AluTiming;
                                                    Config::save("AluTiming");
                                                }
                                                menu_curopt = opt2;
                                                menu_saverect = false;
                                            } else {
                                                menu_curopt = opt1;
                                                menu_level = 2;
                                                break;
                                            }
                                        }
                                    }
                                    else if (opt1 == 2) {
                                        menu_level = 3;
                                        menu_curopt = 1;
                                        menu_saverect = true;
                                        while (1) {
                                            string iss_menu = MENU_ISSUE2[Config::lang];
                                            bool prev_iss = Config::Issue2;
                                            if (prev_iss) {
                                                menu_curopt = 1;
                                                iss_menu += markSelectedOption(MENU_YESNO[Config::lang], "Y");
                                            } else {
                                                menu_curopt = 2;
                                                iss_menu += markSelectedOption(MENU_YESNO[Config::lang], "N");
                                            }
                                            uint8_t opt2 = menuRun(iss_menu);
                                            if (opt2) {
                                                if (opt2 == 1)
                                                    Config::Issue2 = true;
                                                else
                                                    Config::Issue2 = false;

                                                if (Config::Issue2 != prev_iss) {
                                                    Config::save("Issue2");
                                                }
                                                menu_curopt = opt2;
                                                menu_saverect = false;
                                            } else {
                                                menu_curopt = opt1;
                                                menu_level = 2;
                                                break;
                                            }
                                        }
                                    }
                                    else if (opt1 == 3) {
                                        menu_level = 3;
                                        menu_curopt = 1;
                                        menu_saverect = true;
                                        while (1) {
                                            string alu_menu;
                                            int prev_alu = Config::ALUTK;
                                            if (prev_alu == 0) {
                                                menu_curopt = 1;
                                                alu_menu = markSelectedOption(MENU_ALUTK_PREF[Config::lang], "F");
                                            } else if (prev_alu == 1) {
                                                menu_curopt = 2;
                                                alu_menu = markSelectedOption(MENU_ALUTK_PREF[Config::lang], "5");
                                            } else if (prev_alu == 2) {
                                                menu_curopt = 3;
                                                alu_menu = markSelectedOption(MENU_ALUTK_PREF[Config::lang], "6");
                                            }
                                            uint8_t opt2 = menuRun(alu_menu);
                                            if (opt2) {
                                                if (opt2 == 1)
                                                    Config::ALUTK = 0;
                                                else if (opt2 == 2)
                                                    Config::ALUTK = 1;
                                                else if (opt2 == 3)
                                                    Config::ALUTK = 2;

                                                if (Config::ALUTK != prev_alu) {

                                                    Config::save("ALUTK");

                                                    // ALU Changed, Reset the emulator if we're using some TK model
                                                    if (Config::arch[0] == 'T') {

                                                        if (Config::videomode) {
                                                            // ESP host reset
                                                            Config::rom_file = NO_ROM_FILE;
                                                            Config::save("rom");
                                                            Config::ram_file = NO_RAM_FILE;
                                                            Config::save("ram");
                                                            esp_hard_reset();
                                                        } else {
                                                            Config::rom_file = NO_ROM_FILE;
                                                            Config::last_rom_file = NO_ROM_FILE;

                                                            Config::ram_file = NO_RAM_FILE;
                                                            Config::last_ram_file = NO_RAM_FILE;

                                                            // Clear Cheat data
                                                            CheatMngr::closeCheatFile();

                                                            ESPeccy::reset();
                                                        }

                                                        return;

                                                    }

                                                }
                                                menu_curopt = opt2;
                                                menu_saverect = false;
                                            } else {
                                                menu_curopt = opt1;
                                                menu_level = 2;
                                                break;
                                            }
                                        }
                                    }
                                    else if (opt1 == 4) {
                                        menu_level = 3;
                                        menu_curopt = 1;
                                        menu_saverect = true;
                                        while (1) {
                                            string ps2_menu;
                                            uint8_t prev_ps2 = Config::ps2_dev2;
                                            if (prev_ps2 == 1) {
                                                menu_curopt = 2;
                                                ps2_menu = markSelectedOption(MENU_KBD2NDPS2[Config::lang], "K");
                                            } else if (prev_ps2 == 2) {
                                                menu_curopt = 3;
                                                ps2_menu = markSelectedOption(MENU_KBD2NDPS2[Config::lang], "M");
                                            } else {
                                                menu_curopt = 1;
                                                ps2_menu = markSelectedOption(MENU_KBD2NDPS2[Config::lang], "N");
                                            }
                                            uint8_t opt2 = menuRun(ps2_menu);
                                            if (opt2) {
                                                if (opt2 == 1)
                                                    Config::ps2_dev2 = 0;
                                                else if (opt2 == 2)
                                                    Config::ps2_dev2 = 1;
                                                else
                                                    Config::ps2_dev2 = 2;

                                                if (Config::ps2_dev2 != prev_ps2) {
                                                    Config::save("PS2Dev2");
                                                }
                                                menu_curopt = opt2;
                                                menu_saverect = false;
                                            } else {
                                                menu_curopt = opt1;
                                                menu_level = 2;
                                                break;
                                            }
                                        }
                                    }
                                } else {
                                    menu_curopt = options_num;
                                    break;
                                }
                            }
                        }
                        else if (options_num == 10) {
                            menu_level = 2;
                            menu_curopt = 1;
                            menu_saverect = true;
                            while (1) {
                                // language
                                uint8_t opt2;
                                string Mnustr;
                                uint8_t prev_lang = Config::lang;
                                if (prev_lang == 0) {
                                    Mnustr = markSelectedOption(MENU_INTERFACE_LANG[Config::lang], "E");
                                } else if (prev_lang == 1) {
                                    Mnustr = markSelectedOption(MENU_INTERFACE_LANG[Config::lang], "S");
                                } else if (prev_lang == 2) {
                                    Mnustr = markSelectedOption(MENU_INTERFACE_LANG[Config::lang], "P");
                                }
                                menu_curopt = Config::lang + 1;
                                opt2 = menuRun(Mnustr);
                                if (opt2) {
                                    if (Config::lang != (opt2 - 1)) {
                                        Config::lang = opt2 - 1;
                                        Config::save("language");
                                        VIDEO::vga.setCodepage(LANGCODEPAGE[Config::lang]);
                                        return;
                                    }
                                    menu_curopt = opt2;
                                    menu_saverect = false;
                                } else {
                                    menu_curopt = options_num;
                                    break;
                                }
                            }
                        }
                        else if (options_num == 11) {
                            menu_level = 2;
                            menu_curopt = 1;
                            menu_saverect = true;
                            while (1) {
                                // options for UI
                                string opt_menu = MENU_UI[Config::lang];
                                uint8_t opt2 = menuRun(opt_menu);
                                if (opt2) {
                                    if (opt2 == 1)
                                    {
                                        menu_level = 3;
                                        menu_curopt = 1;
                                        menu_saverect = true;

                                        while (1){
                                            string opt_menu = MENU_UI_OPT[Config::lang];
                                            bool prev_opt = Config::osd_LRNav;
                                            if (prev_opt) {
                                                menu_curopt = 1;
                                                opt_menu += markSelectedOption(MENU_YESNO[Config::lang], "Y");
                                            } else {
                                                menu_curopt = 2;
                                                opt_menu += markSelectedOption(MENU_YESNO[Config::lang], "N");
                                            }

                                            uint8_t opt2 = menuRun(opt_menu);
                                            if (opt2) {
                                                if (opt2 == 1)  Config::osd_LRNav = 1;
                                                else            Config::osd_LRNav = 0;

                                                if (Config::osd_LRNav != prev_opt) Config::save("osd_LRNav");

                                                menu_curopt = opt2;
                                                menu_saverect = false;
                                            } else {
                                                menu_curopt = 1;
                                                menu_level = 2;
                                                break;
                                            }
                                        }
                                    }
                                    else if (opt2==2)
                                    {
                                        menu_level = 3;
                                        menu_curopt = 1;
                                        menu_saverect = true;

                                        while (1){
                                            string opt_menu = MENU_UI_OPT[Config::lang];
                                            bool prev_opt = Config::osd_AltRot;
                                            if (prev_opt) {
                                                menu_curopt = 2;
                                                opt_menu += markSelectedOption(MENU_UI_TEXT_SCROLL, "P");
                                            } else {
                                                menu_curopt = 1;
                                                opt_menu += markSelectedOption(MENU_UI_TEXT_SCROLL, "N");
                                            }

                                            uint8_t opt2 = menuRun(opt_menu);
                                            if (opt2) {
                                                if (opt2 == 1)  Config::osd_AltRot = 0;
                                                else            Config::osd_AltRot = 1;

                                                menu_curopt = opt2;

                                                if (Config::osd_AltRot != prev_opt) Config::save("osd_AltRot");

                                                menu_saverect = false;
                                            } else {
                                                menu_curopt = 1;
                                                menu_level = 2;
                                                break;
                                            }
                                        }
                                    }
                                    else if (opt2==3)
                                    {
                                        menu_level = 3;
                                        menu_curopt = 1;
                                        menu_saverect = true;

                                        while (1){
                                            string opt_menu = MENU_UI_OPT[Config::lang];
                                            bool prev_opt = Config::thumbsEnabled;
                                            if (prev_opt) {
                                                menu_curopt = 1;
                                                opt_menu += markSelectedOption(MENU_YESNO[Config::lang], "Y");
                                            } else {
                                                menu_curopt = 2;
                                                opt_menu += markSelectedOption(MENU_YESNO[Config::lang], "N");
                                            }

                                            uint8_t opt2 = menuRun(opt_menu);
                                            if (opt2) {
                                                if (opt2 == 1)  Config::thumbsEnabled = 1;
                                                else            Config::thumbsEnabled = 0;

                                                menu_curopt = opt2;

                                                if (Config::thumbsEnabled != prev_opt) Config::save("thumbsEnabled");

                                                menu_saverect = false;
                                            } else {
                                                menu_curopt = 1;
                                                menu_level = 2;
                                                break;
                                            }
                                        }
                                    }
                                    else if (opt2==4)
                                    {
                                        menu_level = 3;
                                        menu_curopt = 1;
                                        menu_saverect = true;

                                        while (1){
                                            string opt_menu = MENU_UI_OPT[Config::lang];
                                            bool prev_opt = Config::instantPreview;
                                            if (prev_opt) {
                                                menu_curopt = 1;
                                                opt_menu += markSelectedOption(MENU_YESNO[Config::lang], "Y");
                                            } else {
                                                menu_curopt = 2;
                                                opt_menu += markSelectedOption(MENU_YESNO[Config::lang], "N");
                                            }

                                            uint8_t opt2 = menuRun(opt_menu);
                                            if (opt2) {
                                                if (opt2 == 1)  Config::instantPreview = 1;
                                                else            Config::instantPreview = 0;

                                                menu_curopt = opt2;

                                                if (Config::instantPreview != prev_opt) Config::save("instantPreview");

                                                menu_saverect = false;
                                            } else {
                                                menu_curopt = 1;
                                                menu_level = 2;
                                                break;
                                            }
                                        }
                                    }
                                    menu_curopt = opt2;
                                    menu_saverect = false;
                                } else {
                                    menu_curopt = options_num;
                                    break;
                                }

                            }

                        }
                        else if (options_num == 12) {
                            menu_level = 2;
                            menu_curopt = 1;
                            menu_saverect = true;
                            while (1) {
                                string realtape_gpio_menu;
                                uint8_t prev_realtape_gpio_num = Config::realtape_gpio_num;

                                if (ZXKeyb::Exists) {
                                    if (Config::psramsize > 0) {
                                        realtape_gpio_menu = MENU_REALTAPE_OPTIONS_VILLENA_BOARD_PSRAM;
                                    } else {
                                        realtape_gpio_menu = MENU_REALTAPE_OPTIONS_VILLENA_BOARD_NO_PSRAM;
                                    }
                                } else {
                                    realtape_gpio_menu = MENU_REALTAPE_OPTIONS_LILY;
                                }

                                std::vector<std::string> values = extractValues(realtape_gpio_menu);
                                uint8_t val = Config::realtape_gpio_num;

                                auto it = std::find(values.begin(), values.end(), to_string(val));
                                if (it != values.end()) {
                                    menu_curopt = std::distance(values.begin(), it) + 1;
                                } else {
                                    val = std::stoi(values[0]);
                                    menu_curopt = 1;
                                }

                                realtape_gpio_menu = markSelectedOption(realtape_gpio_menu, to_string(val));

                                uint8_t opt2 = menuRun(realtape_gpio_menu);
                                if (opt2) {

                                    if (std::stoi(values[opt2-1]) != prev_realtape_gpio_num) {
                                        Config::realtape_gpio_num = std::stoi(values[opt2-1]);
                                        Config::save("RealTapeGPIO");
                                        RealTape_init(NULL);
                                    }

                                    menu_curopt = opt2;
                                    menu_saverect = false;

                                } else {
                                    menu_curopt = options_num;
                                    break;
                                }
                            }
                        }
                        else {
                            menu_curopt = opt;
                            break;
                        }
                    }
                } else if (opt == 6) {
                    menu_level = 1;
                    menu_curopt = 1;
                    menu_saverect = true;
                    while (1) {
                        // Update
                        string Mnustr = MENU_UPDATE_FW[Config::lang];
                        uint8_t opt2 = menuRun(Mnustr);
                        if (opt2) {
                            // Update
                            if (opt2 == 1) {

                                if ( FileUtils::isSDReady() ) {

                                    menu_saverect = true;

                                    string mFile = fileDialog( FileUtils::UPG_Path, (string) MENU_UPG_TITLE[Config::lang], DISK_UPGFILE, 42, 10);

                                    if (mFile != "") {
                                        mFile.erase(0, 1);
                                        string fname = FileUtils::MountPoint + FileUtils::UPG_Path + mFile;

                                        menu_saverect = false;

                                        uint8_t res = msgDialog((string) OSD_FIRMW_UPDATE[Config::lang], OSD_DLG_SURE[Config::lang]);

                                        if (res == DLG_YES) {

                                            if ( FileUtils::isSDReady() ) {
                                                // Open firmware file
                                                FILE *firmware = fopen(fname.c_str(), "rb");
                                                if (firmware == NULL) {
                                                    osdCenteredMsg(OSD_NOFIRMW_ERR[Config::lang], LEVEL_WARN, 2000);
                                                } else {
                                                    esp_err_t res = updateFirmware(firmware);
                                                    fclose(firmware);
                                                    osdCenteredMsg((string)OSD_FIRMW_ERR[Config::lang] + " Code = " + to_string(res), LEVEL_ERROR, 3000);
                                                }
                                            }
                                        }
                                    }
                                }

                                menu_curopt = opt2;

                            } else if (opt2 == 2) {
                                do_OSD_MenuUpdateROM(1); // 48k
                                menu_curopt = opt2;

                            } else if (opt2 == 3) {
                                do_OSD_MenuUpdateROM(2); // 128k
                                menu_curopt = opt2;

                            } else if (opt2 == 4) {
                                do_OSD_MenuUpdateROM(3); // +2a
                                menu_curopt = opt2;

                            } else if (opt2 == 5) {
                                do_OSD_MenuUpdateROM(4); // TK
                                menu_curopt = opt2;

                            }

                        } else {
                            menu_curopt = opt;
                            break;
                        }
                    }
                }
                else if (opt == 7) { // Help
                    OSD::drawKbdLayout(ZXKeyb::Exists ? 3 : 2);
                    return;
                }
                else if (opt == 8) {
                    // About
                    drawOSD(false);

                    VIDEO::vga.fillRect(osdInsideX(), osdInsideY(), OSD_COLS * OSD_FONT_W, 50, zxColor(0, 0));

                    // Decode Logo in EBF8 format
                    int logo_w = (ESPeccy_logo[5] << 8) + ESPeccy_logo[4]; // Get Width
                    int logo_h = (ESPeccy_logo[7] << 8) + ESPeccy_logo[6]; // Get Height
                    int pos_x = osdInsideX() + ( OSD_COLS * OSD_FONT_W - logo_w ) / 2;
                    int pos_y = osdInsideY() + ( 50 - logo_h ) / 2;

                    drawCompressedBMP(pos_x, pos_y, ESPeccy_logo);

                    VIDEO::vga.setTextColor(zxColor(7, 0), zxColor(1, 0));

                    pos_x = osdInsideX() + OSD_FONT_W;
                    pos_y = osdInsideY() + 50 + 2;

                    int osdRow = 0; int osdCol = 0;
                    int msgIndex = 0; int msgChar = 0;
                    int msgDelay = 0; int cursorBlink = 16; int nextChar = 0;
                    uint16_t cursorCol = zxColor(7,1);
                    uint16_t cursorCol2 = zxColor(1,0);

                    while (1) {
                        if (msgDelay == 0) {
                            nextChar = AboutMsg[Config::lang][msgIndex][msgChar];
                            if (nextChar != 13) { // \r
                                if (nextChar == 10) { // \n
                                    char fore = AboutMsg[Config::lang][msgIndex][++msgChar];
                                    char back = AboutMsg[Config::lang][msgIndex][++msgChar];
                                    int foreint = (fore >= 'A') ? (fore - 'A' + 10) : (fore - '0');
                                    int backint = (back >= 'A') ? (back - 'A' + 10) : (back - '0');
                                    VIDEO::vga.setTextColor(zxColor(foreint & 0x7, foreint >> 3), zxColor(backint & 0x7, backint >> 3));
                                    msgChar++;
                                    continue;
                                } else {
                                    VIDEO::vga.drawChar(pos_x + (osdCol * OSD_FONT_W), pos_y + (osdRow * OSD_FONT_H), nextChar);
                                }
                            } else {
                                VIDEO::vga.fillRect(pos_x + (osdCol * OSD_FONT_W), pos_y + (osdRow * OSD_FONT_H), OSD_FONT_W, OSD_FONT_H, zxColor(1, 0) );
                            }

                            osdCol++;

                            if (nextChar && nextChar != 13) msgChar++;

                            if (osdCol == OSD_COLS - 2) {
                                if (osdRow == 13 || !nextChar) {
                                    osdCol--;
                                    msgDelay = 192;
                                } else {
                                    VIDEO::vga.fillRect(pos_x + (osdCol * OSD_FONT_W), pos_y + (osdRow * OSD_FONT_H), OSD_FONT_W,OSD_FONT_H, zxColor(1, 0) );
                                    osdCol = 0;
                                    msgChar++;
                                    osdRow++;
                                }
                            }
                        } else {
                            msgDelay--;
                            if (msgDelay==0) {
                                VIDEO::vga.fillRect(osdInsideX(), osdInsideY() + 50 + 2, OSD_W - OSD_FONT_W - 2, ( osdRow + 1 ) * OSD_FONT_H, zxColor(1, 0)); // Clean page

                                osdCol = 0;
                                osdRow  = 0;
                                msgChar = 0;
                                msgIndex++;
                                if (msgIndex >= sizeof(AboutMsg[0])/sizeof(AboutMsg[0][0])) msgIndex = 0;
                            }
                        }

                        if (--cursorBlink == 0) {
                            uint16_t cursorSwap = cursorCol;
                            cursorCol = cursorCol2;
                            cursorCol2 = cursorSwap;
                            cursorBlink = 16;
                        }

                        VIDEO::vga.fillRect(pos_x + ((osdCol + 1) * OSD_FONT_W), pos_y + (osdRow * OSD_FONT_H), OSD_FONT_W,OSD_FONT_H, cursorCol );

                        if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead(KBDREAD_MODENORMAL);

                        ESPeccy::readKbdJoy();

                        if (ESPeccy::PS2Controller.keyboard()->virtualKeyAvailable()) {
                            if (ESPeccy::readKbd(&Nextkey, KBDREAD_MODENORMAL)) {
                                if(!Nextkey.down) continue;
                                if (Nextkey.vk == fabgl::VK_F1 || Nextkey.vk == fabgl::VK_ESCAPE || Nextkey.vk == fabgl::VK_RETURN || Nextkey.vk == fabgl::VK_JOY1A || Nextkey.vk == fabgl::VK_JOY1B || Nextkey.vk == fabgl::VK_JOY2A || Nextkey.vk == fabgl::VK_JOY2B) break;
                            }
                        }

                        vTaskDelay(20 / portTICK_PERIOD_MS);

                    }

                    click();

                    if (VIDEO::OSD) OSD::drawStats(); // Redraw stats for 16:9 modes

                    return;

                }
                else break;

            }
        }
        else
            ESPeccy::sync_realtape = false;
    }
}

void OSD::HWInfo() {

    click();

    // Draw Hardware and memory info
    drawOSD(true);
    osdAt(2, 0);

    VIDEO::vga.setTextColor(zxColor(7, 0), zxColor(1, 0));

    // Get chip information
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    VIDEO::vga.print(" Hardware info\n");
    VIDEO::vga.print(" --------------------------------------------\n");
    VIDEO::vga.print(ESPeccy::getHardwareInfo().c_str());

    VIDEO::vga.print("\n Memory info\n");
    VIDEO::vga.print(" --------------------------------------------\n");

    string textout;

    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); // internal RAM, memory capable to store data or to create new task
    textout = " Total free bytes         : " + to_string(info.total_free_bytes) + "\n";
    VIDEO::vga.print(textout.c_str());

    textout = " Minimum free ever        : " + to_string(info.minimum_free_bytes) + "\n";
    VIDEO::vga.print(textout.c_str());

    textout = " Largest free block       : " + to_string(info.largest_free_block) + "\n";
    VIDEO::vga.print(textout.c_str());

    textout = " Free (MALLOC_CAP_32BIT)  : " + to_string(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_32BIT)) + "\n";
    VIDEO::vga.print(textout.c_str());

//    textout = " PSRAM Free               : " + to_string(psramfree) + "/" + to_string(psramfree + psramused) + "\n";
//    VIDEO::vga.print(textout.c_str());
//
//    textout = " PSRAM Used               : " + to_string(psramused) + "/" + to_string(psramfree + psramused) + "\n";
//    VIDEO::vga.print(textout.c_str());

    UBaseType_t wm;
    wm = uxTaskGetStackHighWaterMark(NULL);
    textout = " Main  Task Stack HWM     : " + to_string(wm) + "\n";
    VIDEO::vga.print(textout.c_str());

    wm = uxTaskGetStackHighWaterMark(ESPeccy::audioTaskHandle);
    textout = " Audio Task Stack HWM     : " + to_string(wm) + "\n";
    VIDEO::vga.print(textout.c_str());

    // wm = uxTaskGetStackHighWaterMark(loopTaskHandle);
    // printf("Loop Task Stack HWM: %u\n", wm);

    wm = uxTaskGetStackHighWaterMark(VIDEO::videoTaskHandle);
    textout = " Video Task Stack HWM     : " + (Config::videomode ? to_string(wm) : "N/A") + "\n";
    VIDEO::vga.print(textout.c_str());

    // Wait for key
    while (1) {
        if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead(KBDREAD_MODEDIALOG);

        ESPeccy::readKbdJoy();

        if (ESPeccy::PS2Controller.keyboard()->virtualKeyAvailable()) {
            fabgl::VirtualKeyItem Nextkey;
            ESPeccy::readKbd(&Nextkey, KBDREAD_MODEDIALOG);
            if(!Nextkey.down) continue;
            if (Nextkey.vk == fabgl::VK_F1 || Nextkey.vk == fabgl::VK_ESCAPE || Nextkey.vk == fabgl::VK_RETURN || Nextkey.vk == fabgl::VK_JOY1A || Nextkey.vk == fabgl::VK_JOY1B || Nextkey.vk == fabgl::VK_JOY2A || Nextkey.vk == fabgl::VK_JOY2B) {
                click();
                break;
            }
        }

        vTaskDelay(5 / portTICK_PERIOD_MS);

    }

    if (VIDEO::OSD) OSD::drawStats(); // Redraw stats for 16:9 modes

}

#define FWBUFFSIZE 512 /* 4096 */

esp_err_t OSD::updateROM(FILE *customrom, uint8_t arch) {

    // get the currently running partition
    const esp_partition_t *partition = esp_ota_get_running_partition();
    if (partition == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    // printf("Running partition:\n");
    // printf("address: 0x%lX\n", partition->address);
    // printf("size: %ld\n", partition->size); // size of partition, not binary
    // printf("partition label: %s\n", partition->label);

    // Grab next update target
    // const esp_partition_t *target = esp_ota_get_next_update_partition(NULL);

    // char splabel[17]="esp0";
    // if (strcmp(partition->label,splabel)==0) strcpy(splabel,"esp1"); else strcpy(splabel,"esp1");
    // const esp_partition_t *target = esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_ANY, (const char *) splabel);

    string splabel;
    if (strcmp(partition->label,"esp0")==0) splabel = "esp1"; else splabel= "esp0";
    const esp_partition_t *target = esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_ANY,splabel.c_str());
    if (target == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    // printf("Running partition %s type %d subtype %d at offset 0x%x.\n", partition->label, partition->type, partition->subtype, partition->address);
    // printf("Target  partition %s type %d subtype %d at offset 0x%x.\n", target->label, target->type, target->subtype, target->address);

    // Get firmware size
    fseek(customrom, 0, SEEK_END);
    long bytesrom = ftell(customrom);
    rewind(customrom);

    // printf("Custom ROM lenght: %ld\n", bytesrom);

    string dlgTitle = OSD_ROM[Config::lang];

    if (arch == 1) {

        // Check rom size
        if (bytesrom > 0x4000) {
            return ESP_ERR_INVALID_SIZE;
        }

        dlgTitle += " 48K   ";

    } else if (arch == 2) {

        // Check rom size
        if (bytesrom != 0x4000 && bytesrom != 0x8000) {
            return ESP_ERR_INVALID_SIZE;
        }

        dlgTitle += " 128K  ";

    } else if (arch == 3) {

        // Check rom size
        if (bytesrom != 0x10000) {
            return ESP_ERR_INVALID_SIZE;
        }

        dlgTitle += " +2A   ";

    } else if (arch == 4) {

        // Check rom size
        if (bytesrom > 0x4000) {
            return ESP_ERR_INVALID_SIZE;
        }

        dlgTitle += " TK    ";

    }

    uint8_t data[FWBUFFSIZE] = { 0 };

    int32_t sindex = 0;
    int32_t sindextk = 0;
    int32_t sindex128 = 0;
    int32_t sindex2a3 = 0;
    uint32_t rom_off = 0;
    uint32_t rom_off_tk = 0;
    uint32_t rom_off_128 = 0;
    uint32_t rom_off_2a3 = 0;

    uint8_t magic[]    = { 'E', 'S', 'P', 'C', 'Y', '?', '8', 'K' };
    uint8_t magictk[]  = { 'E', 'S', 'P', 'C', 'Y', '?', 'K', 'X' };
    uint8_t magic128[] = { 'E', 'S', 'P', 'C', 'Y', '?', '2', '8' };
    uint8_t magic2a3[] = { 'E', 'S', 'P', 'C', 'Y', '?', 'A', '3' };

    // truco sucio para evitar que la busqueda de la custom encuentre match en medio del firmware
    magic[5] = '4';
    magictk[5] = 'T';
    magic128[5] = '1';
    magic2a3[5] = '2';

    progressDialog(dlgTitle,OSD_ROM_BEGIN[Config::lang],0,0);

    for (uint32_t offset = 0; offset < partition->size; offset+=FWBUFFSIZE) {
        esp_err_t result = esp_partition_read(partition, offset, data, FWBUFFSIZE);
        if (result == ESP_OK) {
            for (int n=0; n < FWBUFFSIZE; n++) {
                if (rom_off == 0 && sindex < sizeof( magic ) && data[n] == magic[sindex]) {
                    sindex++;
                    if (sindex == sizeof(magic)) {
                        rom_off = offset + n - (sizeof(magic) - 1);
                        // printf("FOUND! OFFSET -> %ld\n",rom_off);
                    }
                } else {
                    sindex = 0;
                }

                if (rom_off_128 == 0 && sindex128 < sizeof( magic128 ) && data[n] == magic128[sindex128]) {
                    sindex128++;
                    if (sindex128 == sizeof(magic128)) {
                        rom_off_128 = offset + n - (sizeof(magic128) - 1);
                        // printf("FOUND! OFFSET 128 -> %ld\n",rom_off_128);
                    }
                } else {
                    sindex128 = 0;
                }

                if (rom_off_2a3 == 0 && sindex2a3 < sizeof( magic2a3 ) && data[n] == magic2a3[sindex2a3]) {
                    sindex2a3++;
                    if (sindex2a3 == sizeof(magic2a3)) {
                        rom_off_2a3 = offset + n - (sizeof(magic2a3) - 1);
                        // printf("FOUND! OFFSET 2A3 -> %ld\n",rom_off_2a3);
                    }
                } else {
                    sindex2a3 = 0;
                }

                if (rom_off_tk == 0 && sindextk < sizeof( magictk ) && data[n] == magictk[sindextk]) {
                    sindextk++;
                    if (sindextk == sizeof(magictk)) {
                        rom_off_tk = offset + n - (sizeof(magictk) - 1);
                        // printf("FOUND! OFFSET TK -> %ld\n",rom_off_tk);
                    }
                } else {
                    sindextk = 0;
                }
            }
        } else {
            printf("esp_partition_read failed, err=0x%x.\n", result);
            progressDialog("","",0,2);
            return result;
        }
    }

    // Fake erase progress bar ;D
    delay(100);
    for(int n=0; n <= 100; n += 10) {
        progressDialog("","",n,1);
        delay(100);
    }

    // Erase entire target partition
    esp_err_t result = esp_partition_erase_range(target, 0, target->size & ~4095);
    if (result != ESP_OK) {
        printf("esp_partition_erase_range failed, err=0x%x.\n", result);
        progressDialog("","",0,2);
        return result;
    }

    // Copy active to target injecting new custom roms
    uint32_t psize = partition->size;

    // printf("Before -> %ld\n",psize);

    rom_off += sizeof(magic);
    rom_off_tk += sizeof(magic128);
    rom_off_128 += sizeof(magic2a3);
    rom_off_2a3 += sizeof(magictk);

    //printf("partition size %ld %ld\n", (long) psize, (long) target->size);
    //printf("rom_48 %ld %ld\n", (long) rom_off, (long) sindex);
    //printf("rom_tk %ld %ld\n", (long) rom_off_tk, (long) sindextk);
    //printf("rom_128 %ld %ld\n", (long) rom_off_128, (long) sindex128);
    //printf("rom_2a3 %ld %ld\n", (long) rom_off_2a3, (long) sindex2a3);

    progressDialog(dlgTitle,OSD_ROM_WRITE[Config::lang],0,1);

    for(uint32_t i=0; i < partition->size; i += FWBUFFSIZE) {

            esp_err_t result = esp_partition_read(partition, i, data, FWBUFFSIZE);
            if (result == ESP_OK) {

                for(uint32_t m=i; m < i + FWBUFFSIZE; m++) {

                    if (m >= rom_off && m < rom_off + 0x4000) {
                        data[m - i]=0xff;
                    } else if (m >= rom_off_tk && m < rom_off_tk + 0x4000) {
                        data[m - i]=0xff;
                    } else if (m >= rom_off_128 && m < rom_off_128 + 0x8000) {
                        data[m - i]=0xff;
                    } else if (m >= rom_off_2a3 && m < rom_off_2a3 + 0x10000) {
                        data[m - i]=0xff;
                    }

                }

                // Write the data, starting from the beginning of the partition
                esp_err_t result = esp_partition_write(target, i, data, FWBUFFSIZE);
                if (result != ESP_OK) {
                    printf("esp_partition_write failed, err=0x%x.\n", result);
                }

            } else {
                printf("esp_partition_read failed, err=0x%x.\n", result);
            }

            if (result != ESP_OK) {
                progressDialog("","",0,2);
                return result;
            }

            psize -= FWBUFFSIZE;

    }

    progressDialog("","",25,1);


    result = esp_ota_set_boot_partition(target);
    if (result != ESP_OK) {
        printf("esp_ota_set_boot_partition failed, err=0x%x.\n", result);
        progressDialog("","",0,2);
        return result;
    }

    size_t bytesread;

    if (arch == 1) {

        progressDialog("","",50,1);

        // Inject new 48K custom ROM
        for (uint32_t i=0; i < 0x4000; i += FWBUFFSIZE ) {
            bytesread = fread(data, 1, FWBUFFSIZE, customrom);
            result = esp_partition_write(target, rom_off + i, data, FWBUFFSIZE);
            if (result != ESP_OK) {
                printf("esp_partition_write failed, err=0x%x.\n", result);
                progressDialog("","",0,2);
                return result;
            }
        }

        progressDialog("","",60,1);

        // Copy previous TK custom ROM
        for (uint32_t i=0; i < 0x4000; i += FWBUFFSIZE) {
            esp_err_t result = esp_partition_read(partition, rom_off_tk + i, data, FWBUFFSIZE);
            if (result == ESP_OK) {
                result = esp_partition_write(target, rom_off_tk + i, data, FWBUFFSIZE);
                if (result != ESP_OK) {
                    printf("esp_partition_write failed, err=0x%x.\n", result);
                }
            } else {
                    printf("esp_partition_read failed, err=0x%x.\n", result);
            }

            if (result != ESP_OK) {
                progressDialog("","",0,2);
                return result;
            }
        }

        progressDialog("","",70,1);

        // Copy previous 128K custom ROM
        for (uint32_t i=0; i < 0x8000; i += FWBUFFSIZE) {
            esp_err_t result = esp_partition_read(partition, rom_off_128 + i, data, FWBUFFSIZE);
            if (result == ESP_OK) {
                result = esp_partition_write(target, rom_off_128 + i, data, FWBUFFSIZE);
                if (result != ESP_OK) {
                    printf("esp_partition_write failed, err=0x%x.\n", result);
                }
            } else {
                printf("esp_partition_read failed, err=0x%x.\n", result);
            }
            if (result != ESP_OK) {
                progressDialog("","",0,2);
                return result;
            }
        }

        progressDialog("","",80,1);

        // Copy previous +2a custom ROM
        for (uint32_t i=0; i < 0x10000; i += FWBUFFSIZE) {
            esp_err_t result = esp_partition_read(partition, rom_off_2a3 + i, data, FWBUFFSIZE);
            if (result == ESP_OK) {
                result = esp_partition_write(target, rom_off_2a3 + i, data, FWBUFFSIZE);
                if (result != ESP_OK) {
                    printf("esp_partition_write failed, err=0x%x.\n", result);
                }
            } else {
                printf("esp_partition_read failed, err=0x%x.\n", result);
            }
            if (result != ESP_OK) {
                progressDialog("","",0,2);
                return result;
            }
        }

        progressDialog("","",100,1);

    } else if (arch == 2) {

        progressDialog("","",50,1);

        // Inject new 128K custom ROM part 1
        for (uint32_t i=0; i < 0x4000; i += FWBUFFSIZE) {
            bytesread = fread(data, 1, FWBUFFSIZE, customrom);
            result = esp_partition_write(target, rom_off_128 + i, data, FWBUFFSIZE);
            if (result != ESP_OK) {
                printf("esp_partition_write failed, err=0x%x.\n", result);
                progressDialog("","",0,2);
                return result;
            }
        }

        progressDialog("","",60,1);

        // Inject new 128K custom ROM part 2
        for (uint32_t i=0; i < 0x4000; i += FWBUFFSIZE) {

            if (bytesrom == 0x4000) {
                for (int n=0;n<FWBUFFSIZE;n++)
                    data[n] = gb_rom_1_sinclair_128k[i + n];
            } else {
                bytesread = fread(data, 1, FWBUFFSIZE, customrom);
            }

            result = esp_partition_write(target, rom_off_128 + i + 0x4000, data, FWBUFFSIZE);
            if (result != ESP_OK) {
                printf("esp_partition_write failed, err=0x%x.\n", result);
                progressDialog("","",0,2);
                return result;
            }

        }

        progressDialog("","",70,1);

        // Copy previous 48K custom ROM
        for (uint32_t i=0; i < 0x4000; i += FWBUFFSIZE) {
            esp_err_t result = esp_partition_read(partition, rom_off + i, data, FWBUFFSIZE);
            if (result == ESP_OK) {
                result = esp_partition_write(target, rom_off + i, data, FWBUFFSIZE);
                if (result != ESP_OK) {
                    printf("esp_partition_write failed, err=0x%x.\n", result);
                }
            } else {
                printf("esp_partition_read failed, err=0x%x.\n", result);
            }
            if (result != ESP_OK) {
                progressDialog("","",0,2);
                return result;
            }
        }

        progressDialog("","",80,1);

        // Copy previous TK custom ROM
        for (uint32_t i=0; i < 0x4000; i += FWBUFFSIZE) {
            esp_err_t result = esp_partition_read(partition, rom_off_tk + i, data, FWBUFFSIZE);
            if (result == ESP_OK) {
                result = esp_partition_write(target, rom_off_tk + i, data, FWBUFFSIZE);
                if (result != ESP_OK) {
                    printf("esp_partition_write failed, err=0x%x.\n", result);
                }
            } else {
                printf("esp_partition_read failed, err=0x%x.\n", result);
            }
            if (result != ESP_OK) {
                progressDialog("","",0,2);
                return result;
            }
        }

        progressDialog("","",90,1);

        // Copy previous +2a custom ROM
        for (uint32_t i=0; i < 0x10000; i += FWBUFFSIZE) {
            esp_err_t result = esp_partition_read(partition, rom_off_2a3 + i, data, FWBUFFSIZE);
            if (result == ESP_OK) {
                result = esp_partition_write(target, rom_off_2a3 + i, data, FWBUFFSIZE);
                if (result != ESP_OK) {
                    printf("esp_partition_write failed, err=0x%x.\n", result);
                }
            } else {
                printf("esp_partition_read failed, err=0x%x.\n", result);
            }
            if (result != ESP_OK) {
                progressDialog("","",0,2);
                return result;
            }
        }

        progressDialog("","",100,1);

    } else if (arch == 3) {

        progressDialog("","",50,1);

        // Inject new +2a custom ROM
        for (uint32_t i=0; i < 0x10000; i += FWBUFFSIZE) {
            bytesread = fread(data, 1, FWBUFFSIZE, customrom);
            result = esp_partition_write(target, rom_off_2a3 + i, data, FWBUFFSIZE);
            if (result != ESP_OK) {
                printf("esp_partition_write failed, err=0x%x.\n", result);
                progressDialog("","",0,2);
                return result;
            }
        }

        progressDialog("","",70,1);

        // Copy previous 48K custom ROM
        for (uint32_t i=0; i < 0x4000; i += FWBUFFSIZE) {
            esp_err_t result = esp_partition_read(partition, rom_off + i, data, FWBUFFSIZE);
            if (result == ESP_OK) {
                result = esp_partition_write(target, rom_off + i, data, FWBUFFSIZE);
                if (result != ESP_OK) {
                    printf("esp_partition_write failed, err=0x%x.\n", result);
                }
            } else {
                printf("esp_partition_read failed, err=0x%x.\n", result);
            }
            if (result != ESP_OK) {
                progressDialog("","",0,2);
                return result;
            }
        }

        progressDialog("","",80,1);

        // Copy previous TK custom ROM
        for (uint32_t i=0; i < 0x4000; i += FWBUFFSIZE) {
            esp_err_t result = esp_partition_read(partition, rom_off_tk + i, data, FWBUFFSIZE);
            if (result == ESP_OK) {
                result = esp_partition_write(target, rom_off_tk + i, data, FWBUFFSIZE);
                if (result != ESP_OK) {
                    printf("esp_partition_write failed, err=0x%x.\n", result);
                }
            } else {
                printf("esp_partition_read failed, err=0x%x.\n", result);
            }
            if (result != ESP_OK) {
                progressDialog("","",0,2);
                return result;
            }
        }

        progressDialog("","",90,1);

        // Copy previous 128K custom ROM
        for (uint32_t i=0; i < 0x8000; i += FWBUFFSIZE) {
            esp_err_t result = esp_partition_read(partition, rom_off_128 + i, data, FWBUFFSIZE);
            if (result == ESP_OK) {
                result = esp_partition_write(target, rom_off_128 + i, data, FWBUFFSIZE);
                if (result != ESP_OK) {
                    printf("esp_partition_write failed, err=0x%x.\n", result);
                }
            } else {
                printf("esp_partition_read failed, err=0x%x.\n", result);
            }
            if (result != ESP_OK) {
                progressDialog("","",0,2);
                return result;
            }
        }

        progressDialog("","",100,1);

    } else if (arch == 4) {

        progressDialog("","",50,1);

        // Inject new TK custom ROM
        for (uint32_t i=0; i < 0x4000; i += FWBUFFSIZE ) {
            bytesread = fread(data, 1, FWBUFFSIZE, customrom);
            result = esp_partition_write(target, rom_off_tk + i, data, FWBUFFSIZE);
            if (result != ESP_OK) {
                printf("esp_partition_write failed, err=0x%x.\n", result);
                progressDialog("","",0,2);
                return result;
            }
        }

        progressDialog("","",60,1);

        // Copy previous 48K custom ROM
        for (uint32_t i=0; i < 0x4000; i += FWBUFFSIZE) {
            esp_err_t result = esp_partition_read(partition, rom_off + i, data, FWBUFFSIZE);
            if (result == ESP_OK) {
                result = esp_partition_write(target, rom_off + i, data, FWBUFFSIZE);
                if (result != ESP_OK) {
                    printf("esp_partition_write failed, err=0x%x.\n", result);
                }
            } else {
                printf("esp_partition_read failed, err=0x%x.\n", result);
            }
        }

        progressDialog("","",70,1);

        // Copy previous 128K custom ROM
        for (uint32_t i=0; i < 0x8000; i += FWBUFFSIZE) {
            esp_err_t result = esp_partition_read(partition, rom_off_128 + i, data, FWBUFFSIZE);
            if (result == ESP_OK) {
                result = esp_partition_write(target, rom_off_128 + i, data, FWBUFFSIZE);
                if (result != ESP_OK) {
                    printf("esp_partition_write failed, err=0x%x.\n", result);
                }
            } else {
                printf("esp_partition_read failed, err=0x%x.\n", result);
            }
            if (result != ESP_OK) {
                progressDialog("","",0,2);
                return result;
            }
        }

        progressDialog("","",85,1);

        // Copy previous +2a custom ROM
        for (uint32_t i=0; i < 0x10000; i += FWBUFFSIZE) {
            esp_err_t result = esp_partition_read(partition, rom_off_2a3 + i, data, FWBUFFSIZE);
            if (result == ESP_OK) {
                result = esp_partition_write(target, rom_off_2a3 + i, data, FWBUFFSIZE);
                if (result != ESP_OK) {
                    printf("esp_partition_write failed, err=0x%x.\n", result);
                }
            } else {
                printf("esp_partition_read failed, err=0x%x.\n", result);
            }
            if (result != ESP_OK) {
                progressDialog("","",0,2);
                return result;
            }
        }

        progressDialog("","",100,1);

    }

    progressDialog(dlgTitle,OSD_FIRMW_END[Config::lang],0,1);

    delay(1000);

    // Firmware written: reboot
    OSD::esp_hard_reset();


}

esp_err_t OSD::updateFirmware(FILE *firmware) {

    char ota_write_data[FWBUFFSIZE + 1] = { 0 };

    // get the currently running partition
    const esp_partition_t *partition = esp_ota_get_running_partition();
    if (partition == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    // Grab next update target
    // const esp_partition_t *target = esp_ota_get_next_update_partition(NULL);
    string splabel;
    if (strcmp(partition->label,"esp0")==0) splabel = "esp1"; else splabel= "esp0";
    const esp_partition_t *target = esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_ANY,splabel.c_str());
    if (target == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    // printf("Running partition %s type %d subtype %d at offset 0x%x.\n", partition->label, partition->type, partition->subtype, partition->address);
    // printf("Target  partition %s type %d subtype %d at offset 0x%x.\n", target->label, target->type, target->subtype, target->address);

    // osdCenteredMsg(OSD_FIRMW_BEGIN[Config::lang], LEVEL_INFO,0);

    progressDialog(OSD_FIRMW[Config::lang],OSD_FIRMW_BEGIN[Config::lang],0,0);

    // Fake erase progress bar ;D
    delay(100);
    for(int n=0; n <= 100; n += 10) {
        progressDialog("","",n,1);
        delay(100);
    }

    esp_ota_handle_t ota_handle;
    esp_err_t result = esp_ota_begin(target, OTA_SIZE_UNKNOWN, &ota_handle);
    if (result != ESP_OK) {
        progressDialog("","",0,2);
        return result;
    }

    size_t bytesread;
    uint32_t byteswritten = 0;

    // osdCenteredMsg(OSD_FIRMW_WRITE[Config::lang], LEVEL_INFO,0);
    progressDialog(OSD_FIRMW[Config::lang],OSD_FIRMW_WRITE[Config::lang],0,1);

    // Get firmware size
    fseek(firmware, 0, SEEK_END);
    long bytesfirmware = ftell(firmware);
    rewind(firmware);

    while (1) {
        bytesread = fread(ota_write_data, 1, FWBUFFSIZE, firmware);
        result = esp_ota_write(ota_handle,(const void *) ota_write_data, bytesread);
        if (result != ESP_OK) {
            progressDialog("","",0,2);
            return result;
        }
        byteswritten += bytesread;
        progressDialog("","",(float) 100 / ((float) bytesfirmware / (float) byteswritten),1);
        // printf("Bytes written: %d\n",byteswritten);
        if (feof(firmware)) break;
    }

    result = esp_ota_end(ota_handle);
    if (result != ESP_OK) {
        // printf("esp_ota_end failed, err=0x%x.\n", result);
        progressDialog("","",0,2);
        return result;
    }

    result = esp_ota_set_boot_partition(target);
    if (result != ESP_OK) {
        // printf("esp_ota_set_boot_partition failed, err=0x%x.\n", result);
        progressDialog("","",0,2);
        return result;
    }

    // osdCenteredMsg(OSD_FIRMW_END[Config::lang], LEVEL_INFO, 0);
    progressDialog(OSD_FIRMW[Config::lang],OSD_FIRMW_END[Config::lang],100,1);

    // Enable StartMsg
    Config::StartMsg = true;
    Config::save("StartMsg");

    delay(1000);

    // Firmware written: reboot
    OSD::esp_hard_reset();

}

void OSD::progressDialog(string title, string msg, int percent, int action, bool noprogressbar ) {

    static unsigned short h;
    static unsigned short y;
    static unsigned short w;
    static unsigned short x;
    static unsigned short progress_x;
    static unsigned short progress_y;
    static unsigned int j;

    bool curr_menu_saverect = menu_saverect;

    if (action == 0 ) { // SHOW

        h = (OSD_FONT_H * (noprogressbar ? 4 : 6)) + 2;
        y = scrAlignCenterY(h);

        if (msg.length() > (scrW / 6) - 4) msg = msg.substr(0,(scrW / 6) - 4);
        if (title.length() > (scrW / 6) - 4) title = title.substr(0,(scrW / 6) - 4);

        w = (((msg.length() > title.length() + 6 ? msg.length(): title.length() + 6) + 2) * OSD_FONT_W) + 2;
        x = scrAlignCenterX(w);

        // Save backbuffer data
        OSD::saveBackbufferData(x,y,w,h,true);

        // printf("SaveRectPos: %04X\n",SaveRectpos << 2);

        // Set font
        VIDEO::vga.setFont(SystemFont);

        // Menu border
        VIDEO::vga.rect(x, y, w, h, zxColor(0, 0));

        VIDEO::vga.fillRect(x + 1, y + 1, w - 2, OSD_FONT_H, zxColor(0,0));
        VIDEO::vga.fillRect(x + 1, y + 1 + OSD_FONT_H, w - 2, h - OSD_FONT_H - 2, zxColor(7,1));

        // Title
        VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(0, 0));
        VIDEO::vga.setCursor(x + OSD_FONT_W + 1, y + 1);
        VIDEO::vga.print(title.c_str());

        // Msg
        VIDEO::vga.setTextColor(zxColor(0, 0), zxColor(7, 1));
        VIDEO::vga.setCursor(scrAlignCenterX(msg.length() * OSD_FONT_W), y + 1 + (OSD_FONT_H * 2));
        VIDEO::vga.print(msg.c_str());

        // Rainbow
        unsigned short rb_y = y + 8;
        unsigned short rb_paint_x = x + w - 30;
        uint8_t rb_colors[] = {2, 6, 4, 5};
        for (uint8_t c = 0; c < 4; c++) {
            for (uint8_t i = 0; i < 5; i++) {
                VIDEO::vga.line(rb_paint_x + i, rb_y, rb_paint_x + 8 + i, rb_y - 8, zxColor(rb_colors[c], 1));
            }
            rb_paint_x += 5;
        }

        if ( !noprogressbar ) {
            // Progress bar frame
            progress_x = scrAlignCenterX(72);
            progress_y = y + (OSD_FONT_H * 4);
            VIDEO::vga.rect(progress_x, progress_y, 72, OSD_FONT_H + 2, zxColor(0, 0));
            progress_x++;
            progress_y++;
        }

    } else if (action == 1 ) { // UPDATE

        // Msg
        VIDEO::vga.setTextColor(zxColor(0, 0), zxColor(7, 1));
        VIDEO::vga.setCursor(scrAlignCenterX(msg.length() * OSD_FONT_W), y + 1 + (OSD_FONT_H * 2));
        VIDEO::vga.print(msg.c_str());

        if ( !noprogressbar ) {
            // Progress bar
            int barsize = (70 * percent) / 100;
            VIDEO::vga.fillRect(progress_x, progress_y, barsize, OSD_FONT_H, zxColor(5,1));
            VIDEO::vga.fillRect(progress_x + barsize, progress_y, 70 - barsize, OSD_FONT_H, zxColor(7,1));
        }

    } else if (action == 2) { // CLOSE

        // Restore backbuffer data
        OSD::restoreBackbufferData(true);
        menu_saverect = curr_menu_saverect;
    }
}

uint8_t OSD::msgDialog(string title, string msg) {

    const unsigned short h = (OSD_FONT_H * 6) + 2;
    const unsigned short y = scrAlignCenterY(h);
    uint8_t res = DLG_NO;

    if (msg.length() > (scrW / 6) - 4) msg = msg.substr(0,(scrW / 6) - 4);
    if (title.length() > (scrW / 6) - 4) title = title.substr(0,(scrW / 6) - 4);

    const unsigned short w = (((msg.length() > title.length() + 6 ? msg.length() : title.length() + 6) + 2) * OSD_FONT_W) + 2;
    const unsigned short x = scrAlignCenterX(w);

    // Save backbuffer data
    OSD::saveBackbufferData(x,y,w,h,true);

    // printf("SaveRectPos: %04X\n",SaveRectpos << 2);

    // Set font
    VIDEO::vga.setFont(SystemFont);

    // Menu border
    VIDEO::vga.rect(x, y, w, h, zxColor(0, 0));

    VIDEO::vga.fillRect(x + 1, y + 1, w - 2, OSD_FONT_H, zxColor(0,0));
    VIDEO::vga.fillRect(x + 1, y + 1 + OSD_FONT_H, w - 2, h - OSD_FONT_H - 2, zxColor(7,1));

    // Title
    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(0, 0));
    VIDEO::vga.setCursor(x + OSD_FONT_W + 1, y + 1);
    VIDEO::vga.print(title.c_str());

    // Msg
    VIDEO::vga.setTextColor(zxColor(0, 0), zxColor(7, 1));
    VIDEO::vga.setCursor(scrAlignCenterX(msg.length() * OSD_FONT_W), y + 1 + (OSD_FONT_H * 2));
    VIDEO::vga.print(msg.c_str());

    // Yes
    VIDEO::vga.setTextColor(zxColor(0, 0), zxColor(7, 1));
    VIDEO::vga.setCursor(scrAlignCenterX(6 * OSD_FONT_W) - (w >> 2), y + 1 + (OSD_FONT_H * 4));
    // VIDEO::vga.print(Config::lang ? "  S\xA1  " : " Yes  ");
    VIDEO::vga.print(OSD_MSGDIALOG_YES[Config::lang]);

    // // Ruler
    // VIDEO::vga.setTextColor(zxColor(0, 0), zxColor(7, 1));
    // VIDEO::vga.setCursor(x + 1, y + 1 + (OSD_FONT_H * 3));
    // VIDEO::vga.print("123456789012345678901234567");

    // No
    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
    VIDEO::vga.setCursor(scrAlignCenterX(6 * OSD_FONT_W) + (w >> 2), y + 1 + (OSD_FONT_H * 4));
    VIDEO::vga.print(OSD_MSGDIALOG_NO[Config::lang]);
    // VIDEO::vga.print("  No  ");

    // Rainbow
    unsigned short rb_y = y + 8;
    unsigned short rb_paint_x = x + w - 30;
    uint8_t rb_colors[] = {2, 6, 4, 5};
    for (uint8_t c = 0; c < 4; c++) {
        for (uint8_t i = 0; i < 5; i++) {
            VIDEO::vga.line(rb_paint_x + i, rb_y, rb_paint_x + 8 + i, rb_y - 8, zxColor(rb_colors[c], 1));
        }
        rb_paint_x += 5;
    }

    // Keyboard loop
    fabgl::VirtualKeyItem Menukey;
    while (1) {

        if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead(KBDREAD_MODEDIALOG);

        ESPeccy::readKbdJoy();

        // Process external keyboard
        if (ESPeccy::PS2Controller.keyboard()->virtualKeyAvailable()) {
            if (ESPeccy::readKbd(&Menukey, KBDREAD_MODEDIALOG)) {
                if (!Menukey.down) continue;

                if (Menukey.vk == fabgl::VK_LEFT || Menukey.vk == fabgl::VK_JOY1LEFT || Menukey.vk == fabgl::VK_JOY2LEFT) {
                    // Yes
                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
                    VIDEO::vga.setCursor(scrAlignCenterX(6 * OSD_FONT_W) - (w >> 2), y + 1 + (OSD_FONT_H * 4));
                    //VIDEO::vga.print(Config::lang ? "  S\xA1  " : " Yes  ");
                    VIDEO::vga.print(OSD_MSGDIALOG_YES[Config::lang]);
                    // No
                    VIDEO::vga.setTextColor(zxColor(0, 0), zxColor(7, 1));
                    VIDEO::vga.setCursor(scrAlignCenterX(6 * OSD_FONT_W) + (w >> 2), y + 1 + (OSD_FONT_H * 4));
                    VIDEO::vga.print(OSD_MSGDIALOG_NO[Config::lang]);
                    // VIDEO::vga.print("  No  ");
                    click();
                    res = DLG_YES;
                } else if (Menukey.vk == fabgl::VK_RIGHT || Menukey.vk == fabgl::VK_JOY1RIGHT || Menukey.vk == fabgl::VK_JOY2RIGHT) {
                    // Yes
                    VIDEO::vga.setTextColor(zxColor(0, 0), zxColor(7, 1));
                    VIDEO::vga.setCursor(scrAlignCenterX(6 * OSD_FONT_W) - (w >> 2), y + 1 + (OSD_FONT_H * 4));
                    VIDEO::vga.print(OSD_MSGDIALOG_YES[Config::lang]);
                    //VIDEO::vga.print(Config::lang ? "  S\xA1  " : " Yes  ");
                    // No
                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
                    VIDEO::vga.setCursor(scrAlignCenterX(6 * OSD_FONT_W) + (w >> 2), y + 1 + (OSD_FONT_H * 4));
                    VIDEO::vga.print(OSD_MSGDIALOG_NO[Config::lang]);
                    // VIDEO::vga.print("  No  ");
                    click();
                    res = DLG_NO;
                } else if (Menukey.vk == fabgl::VK_RETURN || Menukey.vk == fabgl::VK_SPACE || Menukey.vk == fabgl::VK_JOY1B || Menukey.vk == fabgl::VK_JOY2B || Menukey.vk == fabgl::VK_JOY1C || Menukey.vk == fabgl::VK_JOY2C) {
                    break;
                } else if (Menukey.vk == fabgl::VK_ESCAPE || Menukey.vk == fabgl::VK_JOY1A || Menukey.vk == fabgl::VK_JOY2A) {
                    res = DLG_CANCEL;
                    break;
                }
            }

        }

        vTaskDelay(5 / portTICK_PERIOD_MS);

    }

    click();

    // Restore backbuffer data
    OSD::restoreBackbufferData(true);

    return res;

}

#define MENU_JOYSELKEY_EN "Key      \n"\
    "A-Z      \n"\
    "1-0      \n"\
    "Special  \n"\
    "PS/2     \n"
#define MENU_JOYSELKEY_ES_PT "Tecla    \n"\
    "A-Z      \n"\
    "1-0      \n"\
    "Especial \n"\
    "PS/2     \n"
static const char *MENU_JOYSELKEY[NLANGS] = { MENU_JOYSELKEY_EN, MENU_JOYSELKEY_ES_PT, MENU_JOYSELKEY_ES_PT };

#define MENU_JOY_AZ "A-Z\n"\
    "A\n"\
    "B\n"\
    "C\n"\
    "D\n"\
    "E\n"\
    "F\n"\
    "G\n"\
    "H\n"\
    "I\n"\
    "J\n"\
    "K\n"\
    "L\n"\
    "M\n"\
    "N\n"\
    "O\n"\
    "P\n"\
    "Q\n"\
    "R\n"\
    "S\n"\
    "T\n"\
    "U\n"\
    "V\n"\
    "W\n"\
    "X\n"\
    "Y\n"\
    "Z\n"

#define MENU_JOY_09 "0-9\n"\
    "0\n"\
    "1\n"\
    "2\n"\
    "3\n"\
    "4\n"\
    "5\n"\
    "6\n"\
    "7\n"\
    "8\n"\
    "9\n"

#define MENU_JOY_SPECIAL "Enter\n"\
    "Caps\n"\
    "SymbShift\n"\
    "Brk/Space\n"\
    "None\n"

#define MENU_JOY_PS2 "PS/2\n"\
    "F1\n"\
    "F2\n"\
    "F3\n"\
    "F4\n"\
    "F5\n"\
    "F6\n"\
    "F7\n"\
    "F8\n"\
    "F9\n"\
    "F10\n"\
    "F11\n"\
    "F12\n"\
    "Pause\n"\
    "PrtScr\n"\
    "Left\n"\
    "Right\n"\
    "Up\n"\
    "Down\n"

#define MENU_JOY_KEMPSTON "Kempston\n"\
    "Left\n"\
    "Right\n"\
    "Up\n"\
    "Down\n"\
    "Fire 1\n"\
    "Fire 2\n"

#define MENU_JOY_FULLER "Fuller\n"\
    "Left\n"\
    "Right\n"\
    "Up\n"\
    "Down\n"\
    "Fire\n"

string vkToText(int key) {

fabgl::VirtualKey vk = (fabgl::VirtualKey) key;

switch (vk)
{
case fabgl::VK_0:
    return "    0    ";
case fabgl::VK_1:
    return "    1    ";
case fabgl::VK_2:
    return "    2    ";
case fabgl::VK_3:
    return "    3    ";
case fabgl::VK_4:
    return "    4    ";
case fabgl::VK_5:
    return "    5    ";
case fabgl::VK_6:
    return "    6    ";
case fabgl::VK_7:
    return "    7    ";
case fabgl::VK_8:
    return "    8    ";
case fabgl::VK_9:
    return "    9    ";
case fabgl::VK_A:
    return "    A    ";
case fabgl::VK_B:
    return "    B    ";
case fabgl::VK_C:
    return "    C    ";
case fabgl::VK_D:
    return "    D    ";
case fabgl::VK_E:
    return "    E    ";
case fabgl::VK_F:
    return "    F    ";
case fabgl::VK_G:
    return "    G    ";
case fabgl::VK_H:
    return "    H    ";
case fabgl::VK_I:
    return "    I    ";
case fabgl::VK_J:
    return "    J    ";
case fabgl::VK_K:
    return "    K    ";
case fabgl::VK_L:
    return "    L    ";
case fabgl::VK_M:
    return "    M    ";
case fabgl::VK_N:
    return "    N    ";
case fabgl::VK_O:
    return "    O    ";
case fabgl::VK_P:
    return "    P    ";
case fabgl::VK_Q:
    return "    Q    ";
case fabgl::VK_R:
    return "    R    ";
case fabgl::VK_S:
    return "    S    ";
case fabgl::VK_T:
    return "    T    ";
case fabgl::VK_U:
    return "    U    ";
case fabgl::VK_V:
    return "    V    ";
case fabgl::VK_W:
    return "    W    ";
case fabgl::VK_X:
    return "    X    ";
case fabgl::VK_Y:
    return "    Y    ";
case fabgl::VK_Z:
    return "    Z    ";
case fabgl::VK_RETURN:
    return "  Enter  ";
case fabgl::VK_SPACE:
    return "Brk/Space";
case fabgl::VK_LSHIFT:
    return "  Caps   ";
case fabgl::VK_LCTRL:
    return "SymbShift";
case fabgl::VK_F1:
    return "   F1    ";
case fabgl::VK_F2:
    return "   F2    ";
case fabgl::VK_F3:
    return "   F3    ";
case fabgl::VK_F4:
    return "   F4    ";
case fabgl::VK_F5:
    return "   F5    ";
case fabgl::VK_F6:
    return "   F6    ";
case fabgl::VK_F7:
    return "   F7    ";
case fabgl::VK_F8:
    return "   F8    ";
case fabgl::VK_F9:
    return "   F9    ";
case fabgl::VK_F10:
    return "   F10   ";
case fabgl::VK_F11:
    return "   F11   ";
case fabgl::VK_F12:
    return "   F12   ";
case fabgl::VK_PAUSE:
    return "  Pause  ";
case fabgl::VK_PRINTSCREEN:
    return " PrtScr  ";
case fabgl::VK_LEFT:
    return "  Left   ";
case fabgl::VK_RIGHT:
    return "  Right  ";
case fabgl::VK_UP:
    return "   Up    ";
case fabgl::VK_DOWN:
    return "  Down   ";
case fabgl::VK_KEMPSTON_LEFT:
    return "Kmp.Left ";
case fabgl::VK_KEMPSTON_RIGHT:
    return "Kmp.Right";
case fabgl::VK_KEMPSTON_UP:
    return " Kmp.Up  ";
case fabgl::VK_KEMPSTON_DOWN:
    return "Kmp.Down ";
case fabgl::VK_KEMPSTON_FIRE:
    return "Kmp.Fire1";
case fabgl::VK_KEMPSTON_ALTFIRE:
    return "Kmp.Fire2";
case fabgl::VK_FULLER_LEFT:
    return "Fll.Left ";
case fabgl::VK_FULLER_RIGHT:
    return "Fll.Right";
case fabgl::VK_FULLER_UP:
    return " Fll.Up  ";
case fabgl::VK_FULLER_DOWN:
    return "Fll.Down ";
case fabgl::VK_FULLER_FIRE:
    return "Fll.Fire ";
default:
    return "  None   ";
}

}

unsigned int joyControl[12][3]={
    {6*OSD_FONT_W-OSD_FONT_W/2,((55-4)/8)*OSD_FONT_H+OSD_FONT_H/2,zxColor(0,0)}, // Left
    {15*OSD_FONT_W-OSD_FONT_W/2,((55-4)/8)*OSD_FONT_H+OSD_FONT_H/2,zxColor(0,0)}, // Right
    {11*OSD_FONT_W-OSD_FONT_W/2,((30-4)/8)*OSD_FONT_H+OSD_FONT_H/2,zxColor(0,0)}, // Up
    {11*OSD_FONT_W-OSD_FONT_W/2,((78-4)/8)*OSD_FONT_H+OSD_FONT_H/2,zxColor(0,0)}, // Down
    {((49-4)/6)*OSD_FONT_W+OSD_FONT_W/2,((109-4)/8)*OSD_FONT_H+OSD_FONT_H/2,zxColor(0,0)}, // Start
    {((136-4)/6)*OSD_FONT_W+OSD_FONT_W/2,((109-4)/8)*OSD_FONT_H+OSD_FONT_H/2,zxColor(0,0)}, // Mode
    {((145-4)/6)*OSD_FONT_W+OSD_FONT_W/2,((69-4)/8)*OSD_FONT_H+OSD_FONT_H/2,zxColor(0,0)}, // A
    {((205-4)/6)*OSD_FONT_W+OSD_FONT_W/2,((69-4)/8)*OSD_FONT_H+OSD_FONT_H/2,zxColor(0,0)}, // B
    {((265-4)/6)*OSD_FONT_W+OSD_FONT_W/2,((69-4)/8)*OSD_FONT_H+OSD_FONT_H/2,zxColor(0,0)}, // C
    {((145-4)/6)*OSD_FONT_W+OSD_FONT_W/2,((37-4)/8)*OSD_FONT_H+OSD_FONT_H/2,zxColor(0,0)}, // X
    {((205-4)/6)*OSD_FONT_W+OSD_FONT_W/2,((37-4)/8)*OSD_FONT_H+OSD_FONT_H/2,zxColor(0,0)}, // Y
    {((265-4)/6)*OSD_FONT_W+OSD_FONT_W/2,((37-4)/8)*OSD_FONT_H+OSD_FONT_H/2,zxColor(0,0)} // Z
};

void DrawjoyControls(unsigned short x, unsigned short y) {

    // Draw joy controls

    // Left arrow
    for (int i = 0; i <= 5; i++) {
        VIDEO::vga.line(x + joyControl[0][0] + i, y + joyControl[0][1] - i, x + joyControl[0][0] + i, y + joyControl[0][1] + i, joyControl[0][2]);
    }

    // Right arrow
    for (int i = 0; i <= 5; i++) {
        VIDEO::vga.line(x + joyControl[1][0] + i, y + joyControl[1][1] - ( 5 - i), x + joyControl[1][0] + i, y + joyControl[1][1] + ( 5 - i), joyControl[1][2]);
    }

    // Up arrow
    for (int i = 0; i <= 6; i++) {
        VIDEO::vga.line(x + joyControl[2][0] - i, y + joyControl[2][1] + i, x + joyControl[2][0] + i, y + joyControl[2][1] + i, joyControl[2][2]);
    }

    // Down arrow
    for (int i = 0; i <= 6; i++) {
        VIDEO::vga.line(x + joyControl[3][0] - (6 - i), y + joyControl[3][1] + i, x + joyControl[3][0] + ( 6 - i), y + joyControl[3][1] + i, joyControl[3][2]);
    }

    // START text
    VIDEO::vga.setTextColor(joyControl[4][2], zxColor(7, 1));
    VIDEO::vga.setCursor(x + joyControl[4][0], y + joyControl[4][1]);
    VIDEO::vga.print("START");

    // MODE text
    VIDEO::vga.setTextColor(joyControl[5][2], zxColor(7, 1));
    VIDEO::vga.setCursor(x + joyControl[5][0], y + joyControl[5][1]);
    VIDEO::vga.print("MODE");

    // Text A
    VIDEO::vga.setTextColor( joyControl[6][2],zxColor(7, 1));
    VIDEO::vga.setCursor(x + joyControl[6][0], y + joyControl[6][1]);
    VIDEO::vga.circle(x + joyControl[6][0] + 3, y + joyControl[6][1] + 3, 6,  joyControl[6][2]);
    VIDEO::vga.print("A");

    // Text B
    VIDEO::vga.setTextColor(joyControl[7][2],zxColor(7, 1));
    VIDEO::vga.setCursor(x + joyControl[7][0], y + joyControl[7][1]);
    VIDEO::vga.circle(x + joyControl[7][0] + 3, y + joyControl[7][1] + 3, 6,  joyControl[7][2]);
    VIDEO::vga.print("B");

    // Text C
    VIDEO::vga.setTextColor(joyControl[8][2],zxColor(7, 1));
    VIDEO::vga.setCursor(x + joyControl[8][0], y + joyControl[8][1]);
    VIDEO::vga.circle(x + joyControl[8][0] + 3, y + joyControl[8][1] + 3, 6, joyControl[8][2]);
    VIDEO::vga.print("C");

    // Text X
    VIDEO::vga.setTextColor(joyControl[9][2],zxColor(7, 1));
    VIDEO::vga.setCursor(x + joyControl[9][0], y + joyControl[9][1]);
    VIDEO::vga.circle(x + joyControl[9][0] + 3, y + joyControl[9][1] + 3, 6, joyControl[9][2]);
    VIDEO::vga.print("X");

    // Text Y
    VIDEO::vga.setTextColor(joyControl[10][2],zxColor(7, 1));
    VIDEO::vga.setCursor(x + joyControl[10][0], y + joyControl[10][1]);
    VIDEO::vga.circle(x + joyControl[10][0] + 3, y + joyControl[10][1] + 3, 6, joyControl[10][2]);
    VIDEO::vga.print("Y");

    // Text Z
    VIDEO::vga.setTextColor(joyControl[11][2],zxColor(7, 1));
    VIDEO::vga.setCursor(x + joyControl[11][0], y + joyControl[11][1]);
    VIDEO::vga.circle(x + joyControl[11][0] + 3, y + joyControl[11][1] + 3, 6, joyControl[11][2]);
    VIDEO::vga.print("Z");

}

void OSD::joyDialog(uint8_t joynum) {

    int joyDropdown[14][7]={
        {((7-1)/6)*OSD_FONT_W+1,((65-1)/8)*OSD_FONT_H+1,-1,1,2,3,0}, // Left
        {((67-1)/6)*OSD_FONT_W+1,((65-1)/8)*OSD_FONT_H+1,0,-1,2,3,0}, // Right
        {((37-1)/6)*OSD_FONT_W+1,((17-1)/8)*OSD_FONT_H+1,-1,9,-1,0,0}, // Up
        {((37-1)/6)*OSD_FONT_W+1,((89-1)/8)*OSD_FONT_H+1,-1,6,0,4,0}, // Down
        {((37-1)/6)*OSD_FONT_W+1,((121-1)/8)*OSD_FONT_H+1,-1,5,3,-1,0}, // Start
        {((121-1)/6)*OSD_FONT_W+1,((121-1)/8)*OSD_FONT_H+1,4,12,6,-1,0}, // Mode
        {((121-1)/6)*OSD_FONT_W+1,((89-1)/8)*OSD_FONT_H+1,3,7,9,5,0}, // A
        {((181-1)/6)*OSD_FONT_W+1,((89-1)/8)*OSD_FONT_H+1,6,8,10,12,0}, // B
        {((241-1)/6)*OSD_FONT_W+1,((89-1)/8)*OSD_FONT_H+1,7,-1,11,13,0}, // C
        {((121-1)/6)*OSD_FONT_W+1,((17-1)/8)*OSD_FONT_H+1,2,10,-1,6,0}, // X
        {((181-1)/6)*OSD_FONT_W+1,((17-1)/8)*OSD_FONT_H+1,9,11,-1,7,0}, // Y
        {((241-1)/6)*OSD_FONT_W+1,((17-1)/8)*OSD_FONT_H+1,10,-1,-1,8,0}, // Z
        {((181-1)/6)*OSD_FONT_W+1,((121-1)/8)*OSD_FONT_H+1,5,13,7,-1,0}, // Ok
        {((241-1)/6)*OSD_FONT_W+1,((121-1)/8)*OSD_FONT_H+1,12,-1,8,-1,0} // Test
    };

    string keymenu = MENU_JOYSELKEY[Config::lang];
    int joytype = joynum == 1 ? Config::joystick1 : Config::joystick2;

    string selkeymenu[5] = {
        MENU_JOY_AZ,
        MENU_JOY_09,
        "",
        MENU_JOY_PS2,
        ""
    };

    selkeymenu[2] = (Config::lang ? "Especial\n" : "Special\n");
    selkeymenu[2] += MENU_JOY_SPECIAL;

    if (joytype == JOY_KEMPSTON) {
        keymenu += "Kempston \n";
        selkeymenu[4] = MENU_JOY_KEMPSTON;
    } else
    if (joytype == JOY_FULLER) {
        keymenu += "Fuller   \n";
        selkeymenu[4] = MENU_JOY_FULLER;
    }

    int curDropDown = 2;
    uint8_t joyDialogMode = 0; // 0 -> Define, 1 -> Test

    const unsigned short h = (OSD_FONT_H * 18) + 2;
    const unsigned short y = scrAlignCenterY(h) - 8;

    const unsigned short w = (50 * OSD_FONT_W) + 2;
    const unsigned short x = scrAlignCenterX(w) - 3;

    // Set font
    VIDEO::vga.setFont(SystemFont);

    // Menu border
    VIDEO::vga.rect(x, y, w, h, zxColor(0, 0));

    VIDEO::vga.fillRect(x + 1, y + 1, w - 2, OSD_FONT_H, zxColor(0,0));
    VIDEO::vga.fillRect(x + 1, y + 1 + OSD_FONT_H, w - 2, h - OSD_FONT_H - 2, zxColor(7,1));

    // Title
    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(0, 0));
    VIDEO::vga.setCursor(x + OSD_FONT_W + 1, y + 1);
    VIDEO::vga.print((joynum == 1 ? "Joystick 1" : "Joystick 2"));

    // Rainbow
    unsigned short rb_y = y + 8;
    unsigned short rb_paint_x = x + w - 30;
    uint8_t rb_colors[] = {2, 6, 4, 5};
    for (uint8_t c = 0; c < 4; c++) {
        for (uint8_t i = 0; i < 5; i++) {
            VIDEO::vga.line(rb_paint_x + i, rb_y, rb_paint_x + 8 + i, rb_y - 8, zxColor(rb_colors[c], 1));
        }
        rb_paint_x += 5;
    }

    // Read joy definition into joyDropdown
    for (int n=0; n<12; n++)
        joyDropdown[n][6] = ESPeccy::JoyVKTranslation[n + (joynum == 1 ? 0 : 12)];

    // Draw Joy DropDowns
    for (int n=0; n<12; n++) {
        VIDEO::vga.rect(x + joyDropdown[n][0] - 2, y + joyDropdown[n][1] - 2, ((58-4)/6)*OSD_FONT_W+4, ((12-4)/8)*OSD_FONT_H+4, zxColor(0, 0));
        if (n == curDropDown)
            VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
        else
            VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
        VIDEO::vga.setCursor(x + joyDropdown[n][0], y + joyDropdown[n][1]);
        VIDEO::vga.print(vkToText(joyDropdown[n][6]).c_str());
    }

    // Draw dialog buttons
    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
    VIDEO::vga.setCursor(x + joyDropdown[12][0], y + joyDropdown[12][1]);
    VIDEO::vga.print("   Ok    ");
    VIDEO::vga.setCursor(x + joyDropdown[13][0], y + joyDropdown[13][1]);
    VIDEO::vga.print(" JoyTest ");

    // // Ruler
    // VIDEO::vga.setTextColor(zxColor(0, 0), zxColor(7, 1));
    // VIDEO::vga.setCursor(x + 1, y + 1 + (OSD_FONT_H * 3));
    // VIDEO::vga.print("123456789012345678901234567");

    DrawjoyControls(x,y);

    // Wait for key
    fabgl::VirtualKeyItem Nextkey;

    int joyTestExitCount1 = 0;
    int joyTestExitCount2 = 0;

    while (1) {

        if (joyDialogMode) {
            DrawjoyControls(x,y);
        }

        if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead(KBDREAD_MODEINPUTMULTI);

        ESPeccy::readKbdJoy();

        while (ESPeccy::PS2Controller.keyboard()->virtualKeyAvailable()) {

            ESPeccy::readKbd(&Nextkey, KBDREAD_MODEINPUTMULTI);

            if(!Nextkey.down) continue;

            if (Nextkey.vk == fabgl::VK_LEFT || Nextkey.vk == fabgl::VK_JOY1LEFT || Nextkey.vk == fabgl::VK_JOY2LEFT) {

                if (joyDialogMode == 0 && joyDropdown[curDropDown][2] >= 0) {

                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
                    VIDEO::vga.setCursor(x + joyDropdown[curDropDown][0], y + joyDropdown[curDropDown][1]);
                    if (curDropDown < 12)
                        VIDEO::vga.print(vkToText(joyDropdown[curDropDown][6]).c_str());
                    else
                        VIDEO::vga.print(curDropDown == 12 ? "   Ok    " : " JoyTest ");

                    curDropDown = joyDropdown[curDropDown][2];

                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
                    VIDEO::vga.setCursor(x + joyDropdown[curDropDown][0], y + joyDropdown[curDropDown][1]);
                    if (curDropDown < 12)
                        VIDEO::vga.print(vkToText(joyDropdown[curDropDown][6]).c_str());
                    else
                        VIDEO::vga.print(curDropDown == 12 ? "   Ok    " : " JoyTest ");

                    click();

                }

            } else
            if (Nextkey.vk == fabgl::VK_RIGHT || Nextkey.vk == fabgl::VK_JOY1RIGHT || Nextkey.vk == fabgl::VK_JOY2RIGHT) {

                if (joyDialogMode == 0 && joyDropdown[curDropDown][3] >= 0) {

                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
                    VIDEO::vga.setCursor(x + joyDropdown[curDropDown][0], y + joyDropdown[curDropDown][1]);
                    if (curDropDown < 12)
                        VIDEO::vga.print(vkToText(joyDropdown[curDropDown][6]).c_str());
                    else
                        VIDEO::vga.print(curDropDown == 12 ? "   Ok    " : " JoyTest ");

                    curDropDown = joyDropdown[curDropDown][3];

                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
                    VIDEO::vga.setCursor(x + joyDropdown[curDropDown][0], y + joyDropdown[curDropDown][1]);
                    if (curDropDown < 12)
                        VIDEO::vga.print(vkToText(joyDropdown[curDropDown][6]).c_str());
                    else
                        VIDEO::vga.print(curDropDown == 12 ? "   Ok    " : " JoyTest ");

                    click();

                }

            } else
            if (Nextkey.vk == fabgl::VK_UP || Nextkey.vk == fabgl::VK_JOY1UP || Nextkey.vk == fabgl::VK_JOY2UP) {

                if (joyDialogMode == 0 && joyDropdown[curDropDown][4] >= 0) {

                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
                    VIDEO::vga.setCursor(x + joyDropdown[curDropDown][0], y + joyDropdown[curDropDown][1]);
                    if (curDropDown < 12)
                        VIDEO::vga.print(vkToText(joyDropdown[curDropDown][6]).c_str());
                    else
                        VIDEO::vga.print(curDropDown == 12 ? "   Ok    " : " JoyTest ");

                    curDropDown = joyDropdown[curDropDown][4];

                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
                    VIDEO::vga.setCursor(x + joyDropdown[curDropDown][0], y + joyDropdown[curDropDown][1]);
                    if (curDropDown < 12)
                        VIDEO::vga.print(vkToText(joyDropdown[curDropDown][6]).c_str());
                    else
                        VIDEO::vga.print(curDropDown == 12 ? "   Ok    " : " JoyTest ");

                    click();

                }

            } else
            if (Nextkey.vk == fabgl::VK_DOWN || Nextkey.vk == fabgl::VK_JOY1DOWN || Nextkey.vk == fabgl::VK_JOY2DOWN) {

                if (joyDialogMode == 0 && joyDropdown[curDropDown][5] >= 0) {

                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
                    VIDEO::vga.setCursor(x + joyDropdown[curDropDown][0], y + joyDropdown[curDropDown][1]);
                    if (curDropDown < 12)
                        VIDEO::vga.print(vkToText(joyDropdown[curDropDown][6]).c_str());
                    else
                        VIDEO::vga.print(curDropDown == 12 ? "   Ok    " : " JoyTest ");

                    curDropDown = joyDropdown[curDropDown][5];

                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
                    VIDEO::vga.setCursor(x + joyDropdown[curDropDown][0], y + joyDropdown[curDropDown][1]);
                    if (curDropDown < 12)
                        VIDEO::vga.print(vkToText(joyDropdown[curDropDown][6]).c_str());
                    else
                        VIDEO::vga.print(curDropDown == 12 ? "   Ok    " : " JoyTest ");

                    click();

                }

            } else
            if (Nextkey.vk == fabgl::VK_RETURN || Nextkey.vk == fabgl::VK_JOY1B || Nextkey.vk == fabgl::VK_JOY2B) {

                if (joyDialogMode == 0) {

                    if (curDropDown>=0 && curDropDown<12) {

                        click();

                        // Launch assign menu
                        menu_curopt = 1;
                        while (1) {
                            menu_level = 0;
                            menu_saverect = true;
                            uint8_t opt = simpleMenuRun(keymenu,x + joyDropdown[curDropDown][0],y + joyDropdown[curDropDown][1],6,11);
                            if(opt!=0) {
                                // Key select menu
                                menu_saverect = true;
                                menu_level = 0;
                                menu_curopt = 1;
                                uint8_t opt2 = simpleMenuRun(selkeymenu[opt - 1],x + joyDropdown[curDropDown][0],y + joyDropdown[curDropDown][1],6,11);
                                if(opt2!=0) {

                                    if (opt == 1) {// A-Z
                                        joyDropdown[curDropDown][6] = (fabgl::VirtualKey) 47 + opt2;
                                    } else
                                    if (opt == 2) {// 1-0
                                        joyDropdown[curDropDown][6] = (fabgl::VirtualKey) 1 + opt2;
                                    } else
                                    if (opt == 3) {// Special
                                        if (opt2 == 1) {
                                            joyDropdown[curDropDown][6] = fabgl::VK_RETURN;
                                        } else
                                        if (opt2 == 2) {
                                            joyDropdown[curDropDown][6] = fabgl::VK_LSHIFT;
                                        } else
                                        if (opt2 == 3) {
                                            joyDropdown[curDropDown][6] = fabgl::VK_LCTRL;
                                        } else
                                        if (opt2 == 4) {
                                            joyDropdown[curDropDown][6] = fabgl::VK_SPACE;
                                        } else
                                        if (opt2 == 5) {
                                            joyDropdown[curDropDown][6] = fabgl::VK_NONE;
                                        }
                                    } else
                                    if (opt == 4) {// PS/2
                                        if (opt2 < 13) {
                                            joyDropdown[curDropDown][6] = (fabgl::VirtualKey) 158 + opt2;
                                        } else
                                        if (opt2 == 13) {
                                            joyDropdown[curDropDown][6] = fabgl::VirtualKey::VK_PAUSE;
                                        } else
                                        if (opt2 == 14) {
                                            joyDropdown[curDropDown][6] = fabgl::VirtualKey::VK_PRINTSCREEN;
                                        } else
                                        if (opt2 == 15) {
                                            joyDropdown[curDropDown][6] = fabgl::VirtualKey::VK_LEFT;
                                        } else
                                        if (opt2 == 16) {
                                            joyDropdown[curDropDown][6] = fabgl::VirtualKey::VK_RIGHT;
                                        } else
                                        if (opt2 == 17) {
                                            joyDropdown[curDropDown][6] = fabgl::VirtualKey::VK_UP;
                                        } else
                                        if (opt2 == 18) {
                                            joyDropdown[curDropDown][6] = fabgl::VirtualKey::VK_DOWN;
                                        }
                                    } else
                                    if (opt == 5) {// Kempston / Fuller
                                        if (opt2 == 1) {
                                            joyDropdown[curDropDown][6] = fabgl::VirtualKey::VK_KEMPSTON_LEFT;
                                        } else
                                        if (opt2 == 2) {
                                            joyDropdown[curDropDown][6] = fabgl::VirtualKey::VK_KEMPSTON_RIGHT;
                                        } else
                                        if (opt2 == 3) {
                                            joyDropdown[curDropDown][6] = fabgl::VirtualKey::VK_KEMPSTON_UP;
                                        } else
                                        if (opt2 == 4) {
                                            joyDropdown[curDropDown][6] = fabgl::VirtualKey::VK_KEMPSTON_DOWN;
                                        } else
                                        if (opt2 == 5) {
                                            joyDropdown[curDropDown][6] = fabgl::VirtualKey::VK_KEMPSTON_FIRE;
                                        } else
                                        if (opt2 == 6) {
                                            joyDropdown[curDropDown][6] = fabgl::VirtualKey::VK_KEMPSTON_ALTFIRE;
                                        }

                                        if (joytype == JOY_FULLER)
                                            joyDropdown[curDropDown][6] += 6;

                                    }

                                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
                                    VIDEO::vga.setCursor(x + joyDropdown[curDropDown][0], y + joyDropdown[curDropDown][1]);
                                    VIDEO::vga.print(vkToText(joyDropdown[curDropDown][6]).c_str());

                                    break;

                                }
                            } else break;
                            menu_curopt = opt;
                        }

                    } else
                    if (curDropDown == 12) {
                        // Ok button

                        // Check if there are changes and ask to save them
                        bool changed = false;
                        for (int n = 0; n < 12; n++) {
                            if (ESPeccy::JoyVKTranslation[n + (joynum == 1 ? 0 : 12)] != joyDropdown[n][6]) {
                                changed = true;
                                break;
                            }
                        }

                        // Ask to save changes
                        if (changed) {

                            string title = (joynum == 1 ? "Joystick 1" : "Joystick 2");
                            string msg = OSD_DLG_JOYSAVE[Config::lang];
                            uint8_t res = OSD::msgDialog(title,msg);
                            if (res == DLG_YES) {

                                // Fill joystick values in Config
                                int m = (joynum == 1) ? 0 : 12;
                                for (int n = m; n < m + 12; n++) {
                                    // Save to config (only changes)
                                    if (Config::joydef[n] != (uint16_t) joyDropdown[n - m][6]) {
                                        ESPeccy::JoyVKTranslation[n] = (fabgl::VirtualKey) joyDropdown[n - m][6];
                                        Config::joydef[n] = (uint16_t) joyDropdown[n - m][6];
                                        char joykey[9];
                                        sprintf(joykey,"joydef%02u",n);
                                        Config::save(joykey);
                                        // printf("%s %u\n",joykey, joydef[n]);
                                    }
                                }

                                click();
                                return;
                                // break;

                            } else
                            if (res == DLG_NO) {
                                click();
                                return;
                                // break;
                            }

                        } else {
                            click();
                            return;
                            // break;
                        }

                    } else
                    if (curDropDown == 13) {
                        // Enable joyTest
                        joyDialogMode = 1;

                        joyTestExitCount1 = 0;
                        joyTestExitCount2 = 0;

                        VIDEO::vga.setTextColor(zxColor(4, 1), zxColor(5, 1));
                        VIDEO::vga.setCursor(x + joyDropdown[13][0], y + joyDropdown[13][1]);
                        VIDEO::vga.print(" JoyTest ");

                        click();

                    }

                }

            } else
            if (Nextkey.vk == fabgl::VK_ESCAPE || Nextkey.vk == fabgl::VK_JOY1A || Nextkey.vk == fabgl::VK_JOY2A) {

                if (joyDialogMode) {

                    if (Nextkey.vk == fabgl::VK_ESCAPE) {

                        // Disable joyTest
                        joyDialogMode = 0;

                        VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
                        VIDEO::vga.setCursor(x + joyDropdown[13][0], y + joyDropdown[13][1]);
                        VIDEO::vga.print(" JoyTest ");

                        for (int n = 0; n < 12; n++)
                            joyControl[n][2] = zxColor(0,0);

                        DrawjoyControls(x,y);

                        click();

                    }

                } else {

                    // Check if there are changes and ask to discard them
                    bool changed = false;
                    for (int n = 0; n < 12; n++) {
                        if (ESPeccy::JoyVKTranslation[n + (joynum == 1 ? 0 : 12)] != joyDropdown[n][6]) {
                            changed = true;
                            break;
                        }
                    }

                    // Ask to discard changes
                    if (changed) {
                        string title = (joynum == 1 ? "Joystick 1" : "Joystick 2");
                        string msg = OSD_DLG_JOYDISCARD[Config::lang];
                        uint8_t res = OSD::msgDialog(title,msg);
                        if (res == DLG_YES) {
                            click();
                            return;
                            // break;
                        }
                    } else {
                        click();
                        return;
                        // break;
                    }

                }

            }

        }

        // Joy Test Mode: Check joy status and color controls
        if (joyDialogMode) {

            for (int n = (joynum == 1 ? fabgl::VK_JOY1LEFT : fabgl::VK_JOY2LEFT); n <= (joynum == 1 ? fabgl::VK_JOY1Z : fabgl::VK_JOY2Z); n++) {
                if (ESPeccy::PS2Controller.keyboard()->isVKDown((fabgl::VirtualKey) n))
                    joyControl[n - (joynum == 1 ? 248 : 260)][2] = zxColor(4,1);
                else
                    joyControl[n - (joynum == 1 ? 248 : 260)][2] = zxColor(0,0);
            }

            if (ESPeccy::PS2Controller.keyboard()->isVKDown(fabgl::VK_JOY1A)) {
                joyTestExitCount1++;
                if (joyTestExitCount1 == 30)
                    ESPeccy::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_ESCAPE,true,false);
            } else
                joyTestExitCount1 = 0;

            if (ESPeccy::PS2Controller.keyboard()->isVKDown(fabgl::VK_JOY2A)) {
                joyTestExitCount2++;
                if (joyTestExitCount2 == 30)
                    ESPeccy::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_ESCAPE,true,false);
            } else
                joyTestExitCount2 = 0;

        }

        vTaskDelay(50 / portTICK_PERIOD_MS);

    }

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// POKE DIALOG
////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define DLG_OBJ_BUTTON 0
#define DLG_OBJ_INPUT 1
#define DLG_OBJ_COMBO 2

struct dlgObject {
    string Name;
//    unsigned short int posx;
//    unsigned short int posy;
    float posx;
    float posy;
    int objLeft;
    int objRight;
    int objTop;
    int objDown;
    unsigned char objType;
    string Label[3];
};

const dlgObject dlg_Objects[5] = {
    {"Bank",70/6.0,16/8.0,-1,-1, 4, 1, DLG_OBJ_COMBO, {"RAM Bank  ","Banco RAM ","Banco RAM " }},
    {"Address",70/6.0,32/8.0,-1,-1, 0, 2, DLG_OBJ_INPUT, {"Address   ","Direccion ","Endere\x87o  "}},
    {"Value",70/6.0,48/8.0,-1,-1, 1, 4, DLG_OBJ_INPUT, {"Value     ","Valor     ","Valor     "}},
    {"Ok",7/6.0,65/8.0,-1, 4, 2, 0, DLG_OBJ_BUTTON,  {"  Ok  "  ,"  Ok  ","  Ok  "}},
    {"Cancel",52/6.0,65/8.0, 3,-1, 2, 0, DLG_OBJ_BUTTON, {"  Cancel  "," Cancelar "," Cancelar "}}
};

const string BankCombo[9] = { "   -   ", "   0   ", "   1   ", "   2   ", "   3   ", "   4   ", "   5   ", "   6   ", "   7   " };

void OSD::pokeDialog() {

    string dlgValues[5]={
        "   -   ", // Bank
        "16384", // Address
        "0", // Value
        "",
        ""
    };

    string Bankmenu = POKE_BANK_MENU[Config::lang];
    for (int i=0;i<9;i++) Bankmenu += BankCombo[i] + "\n";

    int curObject = 0;
    uint8_t dlgMode = 0; // 0 -> Move, 1 -> Input

    const unsigned short h = (OSD_FONT_H * 10) + 2;
    const unsigned short y = scrAlignCenterY(h) - OSD_FONT_H;

    const unsigned short w = (OSD_FONT_W * 20) + 2;
    const unsigned short x = scrAlignCenterX(w) - OSD_FONT_W / 2;

    click();

    // Set font
    VIDEO::vga.setFont(SystemFont);

    // Menu border
    VIDEO::vga.rect(x, y, w, h, zxColor(0, 0));

    VIDEO::vga.fillRect(x + 1, y + 1, w - 2, OSD_FONT_H, zxColor(0,0));
    VIDEO::vga.fillRect(x + 1, y + 1 + OSD_FONT_H, w - 2, h - OSD_FONT_H - 2, zxColor(7,1));

    // Title
    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(0, 0));
    VIDEO::vga.setCursor(x + OSD_FONT_W + 1, y + 1);

    // string inputpok[NLANGS] = {"Input Poke","A" "\xA4" "adir Poke","Adicionar Poke"};
    // VIDEO::vga.print(inputpok[Config::lang].c_str());

    VIDEO::vga.print(DLG_TITLE_INPUTPOK[Config::lang]);

    // Rainbow
    unsigned short rb_y = y + 8;
    unsigned short rb_paint_x = x + w - 30;
    uint8_t rb_colors[] = {2, 6, 4, 5};
    for (uint8_t c = 0; c < 4; c++) {
        for (uint8_t i = 0; i < 5; i++) {
            VIDEO::vga.line(rb_paint_x + i, rb_y, rb_paint_x + 8 + i, rb_y - 8, zxColor(rb_colors[c], 1));
        }
        rb_paint_x += 5;
    }

    // Draw objects
    for (int n = 0; n < 5; n++) {

        if (dlg_Objects[n].Label[Config::lang] != "" && dlg_Objects[n].objType != DLG_OBJ_BUTTON) {
            VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
            VIDEO::vga.setCursor(x + ( dlg_Objects[n].posx - 63/6.0 ) * OSD_FONT_W, y + dlg_Objects[n].posy * OSD_FONT_H);
            VIDEO::vga.print(dlg_Objects[n].Label[Config::lang].c_str());
            VIDEO::vga.rect(x + dlg_Objects[n].posx * OSD_FONT_W - 2, y + dlg_Objects[n].posy * OSD_FONT_H - 2, (46/6.0)*OSD_FONT_W, (12/8.0)*OSD_FONT_H, zxColor(0, 0));
        }

        if (n == curObject)
            VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
        else
            VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));

        VIDEO::vga.setCursor(x + dlg_Objects[n].posx * OSD_FONT_W, y + dlg_Objects[n].posy * OSD_FONT_H);
        if (dlg_Objects[n].objType == DLG_OBJ_BUTTON) {
            VIDEO::vga.print(dlg_Objects[n].Label[Config::lang].c_str());
        } else {
            VIDEO::vga.print(dlgValues[n].c_str());
        }

    }

    // Wait for key
    fabgl::VirtualKeyItem Nextkey;

    uint8_t CursorFlash = 0;

    while (1) {

        if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead(KBDREAD_MODEINPUTMULTI);

        ESPeccy::readKbdJoy();

        if (ESPeccy::PS2Controller.keyboard()->virtualKeyAvailable()) {

            ESPeccy::readKbd(&Nextkey, KBDREAD_MODEINPUTMULTI);

            if(!Nextkey.down) continue;

            if ((Nextkey.vk >= fabgl::VK_0) && (Nextkey.vk <= fabgl::VK_9)) {

                if (dlg_Objects[curObject].objType == DLG_OBJ_INPUT) {
                    if (dlgValues[curObject].length() < (curObject == 1 ? 5 : 3)) {
                        dlgValues[curObject] += char(Nextkey.vk + 46);
                    }
                }

                click();

            } else
            if (Nextkey.vk == fabgl::VK_LEFT || Nextkey.vk == fabgl::VK_JOY1LEFT || Nextkey.vk == fabgl::VK_JOY2LEFT) {

                if (dlg_Objects[curObject].objLeft >= 0) {

                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
                    VIDEO::vga.setCursor(x + dlg_Objects[curObject].posx * OSD_FONT_W, y + dlg_Objects[curObject].posy * OSD_FONT_H);
                    if (dlg_Objects[curObject].objType == DLG_OBJ_BUTTON) {
                        VIDEO::vga.print(dlg_Objects[curObject].Label[Config::lang].c_str());
                    } else {
                        VIDEO::vga.print(dlgValues[curObject].c_str());
                    }

                    curObject = dlg_Objects[curObject].objLeft;

                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
                    VIDEO::vga.setCursor(x + dlg_Objects[curObject].posx * OSD_FONT_W, y + dlg_Objects[curObject].posy * OSD_FONT_H);
                    if (dlg_Objects[curObject].objType == DLG_OBJ_BUTTON) {
                        VIDEO::vga.print(dlg_Objects[curObject].Label[Config::lang].c_str());
                    } else {
                        VIDEO::vga.print(dlgValues[curObject].c_str());
                    }

                    click();

                }

            } else
            if (Nextkey.vk == fabgl::VK_RIGHT || Nextkey.vk == fabgl::VK_JOY1RIGHT || Nextkey.vk == fabgl::VK_JOY2RIGHT) {

                if (dlg_Objects[curObject].objRight >= 0) {

                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
                    VIDEO::vga.setCursor(x + dlg_Objects[curObject].posx * OSD_FONT_W, y + dlg_Objects[curObject].posy * OSD_FONT_H);
                    if (dlg_Objects[curObject].objType == DLG_OBJ_BUTTON) {
                        VIDEO::vga.print(dlg_Objects[curObject].Label[Config::lang].c_str());
                    } else {
                        VIDEO::vga.print(dlgValues[curObject].c_str());
                    }

                    curObject = dlg_Objects[curObject].objRight;

                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
                    VIDEO::vga.setCursor(x + dlg_Objects[curObject].posx * OSD_FONT_W, y + dlg_Objects[curObject].posy * OSD_FONT_H);
                    if (dlg_Objects[curObject].objType == DLG_OBJ_BUTTON) {
                        VIDEO::vga.print(dlg_Objects[curObject].Label[Config::lang].c_str());
                    } else {
                        VIDEO::vga.print(dlgValues[curObject].c_str());
                    }

                    click();

                }

            } else
            if (Nextkey.vk == fabgl::VK_UP || Nextkey.vk == fabgl::VK_JOY1UP || Nextkey.vk == fabgl::VK_JOY2UP) {

                if (dlg_Objects[curObject].objTop >= 0) {

                    // Input values validation
                    bool validated = true;

                    if (dlg_Objects[curObject].Name == "Address") {

                        string val = dlgValues[1];
                        trim(val);
                        if (val!="") {
                            // Check value
                            if (dlgValues[0] == "   -   ") {
                                if (stoi(val) < 16384 || stoi(val) > 65535) {
                                    osdCenteredMsg(POKE_ERR_ADDR1[Config::lang], LEVEL_WARN, 1000);
                                    validated = false;
                                }
                            } else {
                                if (stoi(val) > 16383) {
                                    osdCenteredMsg(POKE_ERR_ADDR2[Config::lang], LEVEL_WARN, 1000);
                                    validated = false;
                                }
                            }

                        } else {
                            dlgValues[1]= dlgValues[0] == "   -   " ? "16384" : "0";
                        }
                    } else
                    if (dlg_Objects[curObject].Name == "Value") {
                        // Input values validation
                        string val = dlgValues[2];
                        trim(val);
                        if (val!="") {
                            // Check value
                            if (stoi(val) > 255) {
                                osdCenteredMsg(POKE_ERR_VALUE[Config::lang], LEVEL_WARN, 1000);
                                validated = false;
                            }
                        } else {
                            dlgValues[2]="0";
                        }
                    }

                    if (validated) {

                        VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
                        VIDEO::vga.setCursor(x + dlg_Objects[curObject].posx * OSD_FONT_W, y + dlg_Objects[curObject].posy * OSD_FONT_H);
                        if (dlg_Objects[curObject].objType == DLG_OBJ_BUTTON) {
                            VIDEO::vga.print(dlg_Objects[curObject].Label[Config::lang].c_str());
                        } else {
                            VIDEO::vga.print(dlgValues[curObject].c_str());
                            if (dlg_Objects[curObject].objType == DLG_OBJ_INPUT) VIDEO::vga.print(" "); // Clear K cursor
                        }

                        curObject = dlg_Objects[curObject].objTop;

                        VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
                        VIDEO::vga.setCursor(x + dlg_Objects[curObject].posx * OSD_FONT_W, y + dlg_Objects[curObject].posy * OSD_FONT_H);
                        if (dlg_Objects[curObject].objType == DLG_OBJ_BUTTON) {
                            VIDEO::vga.print(dlg_Objects[curObject].Label[Config::lang].c_str());
                        } else {
                            VIDEO::vga.print(dlgValues[curObject].c_str());
                        }

                        click();

                    }

                }

            } else
            if (Nextkey.vk == fabgl::VK_DOWN || Nextkey.vk == fabgl::VK_JOY1DOWN || Nextkey.vk == fabgl::VK_JOY2DOWN) {

                if (dlg_Objects[curObject].objDown >= 0) {

                    // Input values validation
                    bool validated = true;

                    if (dlg_Objects[curObject].Name == "Address") {

                        string val = dlgValues[1];
                        trim(val);
                        if (val!="") {
                            // Check value
                            if (dlgValues[0] == "   -   ") {
                                if (stoi(val) < 16384 || stoi(val) > 65535) {
                                    osdCenteredMsg(POKE_ERR_ADDR1[Config::lang], LEVEL_WARN, 1000);
                                    validated = false;
                                }
                            } else {
                                if (stoi(val) > 16383) {
                                    osdCenteredMsg(POKE_ERR_ADDR2[Config::lang], LEVEL_WARN, 1000);
                                    validated = false;
                                }
                            }

                        } else {
                            dlgValues[1]= dlgValues[0] == "   -   " ? "16384" : "0";
                        }
                    } else
                    if (dlg_Objects[curObject].Name == "Value") {
                        // Input values validation
                        string val = dlgValues[2];
                        trim(val);
                        if (val!="") {
                            // Check value
                            if (stoi(val) > 255) {
                                osdCenteredMsg(POKE_ERR_VALUE[Config::lang], LEVEL_WARN, 1000);
                                validated = false;
                            }
                        } else {
                            dlgValues[2]="0";
                        }
                    }

                    if (validated) {

                        VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
                        VIDEO::vga.setCursor(x + dlg_Objects[curObject].posx * OSD_FONT_W, y + dlg_Objects[curObject].posy * OSD_FONT_H);
                        if (dlg_Objects[curObject].objType == DLG_OBJ_BUTTON) {
                            VIDEO::vga.print(dlg_Objects[curObject].Label[Config::lang].c_str());
                        } else {
                            VIDEO::vga.print(dlgValues[curObject].c_str());
                            if (dlg_Objects[curObject].objType == DLG_OBJ_INPUT) VIDEO::vga.print(" "); // Clear K cursor
                        }

                        curObject = dlg_Objects[curObject].objDown;

                        VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
                        VIDEO::vga.setCursor(x + dlg_Objects[curObject].posx * OSD_FONT_W, y + dlg_Objects[curObject].posy * OSD_FONT_H);
                        if (dlg_Objects[curObject].objType == DLG_OBJ_BUTTON) {
                            VIDEO::vga.print(dlg_Objects[curObject].Label[Config::lang].c_str());
                        } else {
                            VIDEO::vga.print(dlgValues[curObject].c_str());
                        }

                        click();

                    }

                }

            } else
            if (Nextkey.vk == fabgl::VK_BACKSPACE) {

                if (dlg_Objects[curObject].objType == DLG_OBJ_INPUT) {
                    if (dlgValues[curObject] != "") dlgValues[curObject].pop_back();
                }

                click();

            } else
            if (Nextkey.vk == fabgl::VK_RETURN || Nextkey.vk == fabgl::VK_JOY1B || Nextkey.vk == fabgl::VK_JOY2B) {

                if (dlg_Objects[curObject].Name == "Bank" && !Z80Ops::is48) {

                    click();

                    // Launch bank menu
                    menu_curopt = 1;
                    while (1) {
                        menu_level = 0;
                        menu_saverect = true;
                        uint8_t opt = simpleMenuRun( Bankmenu, x + dlg_Objects[curObject].posx * OSD_FONT_W,y + dlg_Objects[curObject].posy * OSD_FONT_H, 10, 9);
                        if(opt!=0) {
                            if (BankCombo[opt -1] != dlgValues[curObject]) {
                                dlgValues[curObject] = BankCombo[opt - 1];

                                VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
                                VIDEO::vga.setCursor(x + dlg_Objects[curObject].posx * OSD_FONT_W, y + dlg_Objects[curObject].posy * OSD_FONT_H);
                                VIDEO::vga.print(dlgValues[curObject].c_str());

                                if (dlgValues[curObject]==BankCombo[0]) {
                                    dlgValues[1] = "16384";
                                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
                                    VIDEO::vga.setCursor(x + dlg_Objects[1].posx * OSD_FONT_W, y + dlg_Objects[1].posy * OSD_FONT_H);
                                    VIDEO::vga.print("16384");
                                } else {
                                    string val = dlgValues[1];
                                    trim(val);
                                    if(stoi(val) > 16383) {
                                        dlgValues[1] = "0";
                                        VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
                                        VIDEO::vga.setCursor(x + dlg_Objects[1].posx * OSD_FONT_W, y + dlg_Objects[1].posy * OSD_FONT_H);
                                        VIDEO::vga.print("0    ");
                                    }
                                }
                            }
                        }
                        break;
                    }
                } else
                if (dlg_Objects[curObject].Name == "Ok") {

                    string addr = dlgValues[1];
                    string val = dlgValues[2];
                    trim(addr);
                    trim(val);
                    int address = stoi(addr);
                    int value = stoi(val);

                    // Apply poke
                    if (dlgValues[0]=="   -   ") {
                        // Poke address between 16384 and 65535
                        uint8_t page = address >> 14;
                        MemESP::ramCurrent[page][address & 0x3fff] = value;
                    } else {
                        // Poke address in bank
                        string bank = dlgValues[0];
                        trim(bank);
                        MemESP::ram[stoi(bank)][address] = value;
                    }
                    click();
                    break;

                } else
                if (dlg_Objects[curObject].Name == "Cancel") {
                    click();
                    break;
                }

            } else
            if (Nextkey.vk == fabgl::VK_ESCAPE || Nextkey.vk == fabgl::VK_JOY1A || Nextkey.vk == fabgl::VK_JOY2A) {
                click();
                break;
            }

        }

        if (dlg_Objects[curObject].objType == DLG_OBJ_INPUT) {

            if ((++CursorFlash & 0xF) == 0) {

                VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
                VIDEO::vga.setCursor(x + dlg_Objects[curObject].posx * OSD_FONT_W, y + dlg_Objects[curObject].posy * OSD_FONT_H);
                VIDEO::vga.print(dlgValues[curObject].c_str());

                if (CursorFlash > 63) {
                    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(0, 1));
                    if (CursorFlash == 128) CursorFlash = 0;
                } else {
                    VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
                }
                VIDEO::vga.print("K");

                VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
                VIDEO::vga.print(" ");

            }

        }

        vTaskDelay(5 / portTICK_PERIOD_MS);

    }

}

int OSD::VirtualKey2ASCII(fabgl::VirtualKeyItem Nextkey, bool * mode_E ) {
    int ascii = 0;
    if ( ( Nextkey.CTRL  || ESPeccy::PS2Controller.keyboard()->isVKDown(fabgl::VK_LCTRL) || ESPeccy::PS2Controller.keyboard()->isVKDown(fabgl::VK_RCTRL) ) &&
         ( Nextkey.SHIFT || ESPeccy::PS2Controller.keyboard()->isVKDown(fabgl::VK_LSHIFT) || ESPeccy::PS2Controller.keyboard()->isVKDown(fabgl::VK_RSHIFT) )
       ) {
        *mode_E = !*mode_E;
    }

    switch (Nextkey.vk) {
        //case fabgl::VK_GRAVEACCENT:     ascii = '`'; break;     /**< Grave accent: ` */
        //case fabgl::VK_ACUTEACCENT:     ascii = '´'; break;     /**< Acute accent: ´ */
        case fabgl::VK_QUOTE:           ascii = '\''; break;    /**< Quote: ' */
        case fabgl::VK_QUOTEDBL:        ascii = '"'; break;     /**< Double quote: " */
        case fabgl::VK_EQUALS:          ascii = '='; break;     /**< Equals: = */
        case fabgl::VK_MINUS:                                   /**< Minus: - */
        case fabgl::VK_KP_MINUS:        ascii = '-'; break;     /**< Keypad minus: - */
        case fabgl::VK_PLUS:                                    /**< Plus: + */
        case fabgl::VK_KP_PLUS:         ascii = '+'; break;     /**< Keypad plus: + */
        case fabgl::VK_KP_MULTIPLY:                             /**< Keypad multiply: * */
        case fabgl::VK_ASTERISK:        ascii = '*'; break;     /**< Asterisk: * */
        case fabgl::VK_BACKSLASH:       ascii = '\\'; break;    /**< Backslash: \ */
        case fabgl::VK_KP_DIVIDE:                               /**< Keypad divide: / */
        case fabgl::VK_SLASH:           ascii = '/'; break;     /**< Slash: / */
        case fabgl::VK_KP_PERIOD:                               /**< Keypad period: . */
        case fabgl::VK_PERIOD:          ascii = '.'; break;     /**< Period: . */
        case fabgl::VK_COLON:           ascii = ':'; break;     /**< Colon: : */
        case fabgl::VK_COMMA:           ascii = ','; break;     /**< Comma: , */
        case fabgl::VK_SEMICOLON:       ascii = ';'; break;     /**< Semicolon: ; */
        case fabgl::VK_AMPERSAND:       ascii = '&'; break;     /**< Ampersand: & */
        case fabgl::VK_VERTICALBAR:     ascii = '|'; break;     /**< Vertical bar: | */
        case fabgl::VK_HASH:            ascii = '#'; break;     /**< Hash: # */
        case fabgl::VK_AT:              ascii = '@'; break;     /**< At: @ */
        case fabgl::VK_CARET:           ascii = '^'; break;     /**< ↑ > */
        case fabgl::VK_DOLLAR:          ascii = '$'; break;     /**< Dollar: $ */
        // case fabgl::VK_POUND:           ascii = '£'; break;    /**< Pound: £ */
        case fabgl::VK_PERCENT:         ascii = '%'; break;     /**< Percent: % */
        case fabgl::VK_EXCLAIM:         ascii = '!'; break;     /**< Exclamation mark: ! */
        case fabgl::VK_QUESTION:        ascii = '?'; break;     /**< Question mark: ? */
        case fabgl::VK_LEFTBRACE:       ascii = '{'; break;     /**< Left brace: { */
        case fabgl::VK_RIGHTBRACE:      ascii = '}'; break;     /**< Right brace: } */
        case fabgl::VK_LEFTBRACKET:     ascii = '['; break;     /**< Left bracket: [ */
        case fabgl::VK_RIGHTBRACKET:    ascii = ']'; break;     /**< Right bracket: ] */
        case fabgl::VK_LEFTPAREN:       ascii = '('; break;     /**< Left parenthesis: ( */
        case fabgl::VK_RIGHTPAREN:      ascii = ')'; break;     /**< Right parenthesis: ) */
        case fabgl::VK_LESS:            ascii = '<'; break;     /**< Less: < */
        case fabgl::VK_GREATER:         ascii = '>'; break;     /**< Greater: > */
        case fabgl::VK_UNDERSCORE:      ascii = '_'; break;     /**< Underscore: _ */
        //case fabgl::VK_DEGREE:          ascii = '°'; break;     /**< Degree: ° */
        case fabgl::VK_TILDE:           ascii = '~'; break;     /**< Tilde: ~ */
    }

      if ( Nextkey.CTRL || ESPeccy::PS2Controller.keyboard()->isVKDown(fabgl::VK_LCTRL) || ESPeccy::PS2Controller.keyboard()->isVKDown(fabgl::VK_RCTRL) ) {
        if ( !*mode_E ) {
            switch (Nextkey.vk) {
                case fabgl::VK_1        : ascii = '!'; break; /**< Exclamation mark: ! */
                case fabgl::VK_2        : ascii = '@'; break; /**< At: @ */
                case fabgl::VK_3        : ascii = '#'; break; /**< Hash: # */
                case fabgl::VK_4        : ascii = '$'; break; /**< Dollar: $ */
                case fabgl::VK_5        : ascii = '%'; break; /**< Percent: % */
                case fabgl::VK_6        : ascii = '&'; break; /**< Ampersand: & */
                case fabgl::VK_7        : ascii = '\''; break; /**< Quote: ' */
                case fabgl::VK_8        : ascii = '('; break; /**< Left parenthesis: ( */
                case fabgl::VK_9        : ascii = ')'; break; /**< Right parenthesis: ) */
                case fabgl::VK_0        : ascii = '_'; break; /**< Underscore: _ */

                case fabgl::VK_r        :
                case fabgl::VK_R        : ascii = '<'; break; /**< Less: < */

                case fabgl::VK_t        :
                case fabgl::VK_T        : ascii = '>'; break; /**< Greater: > */

                case fabgl::VK_o        :
                case fabgl::VK_O        : ascii = ';'; break; /**< Semicolon: ; */

                case fabgl::VK_p        :
                case fabgl::VK_P        : ascii = '"'; break; /**< Double quote: " */

                case fabgl::VK_h        :
                case fabgl::VK_H        : ascii = '^'; break; /**< ↑ > */

                case fabgl::VK_j        :
                case fabgl::VK_J        : ascii = '-'; break; /**< Minus: - */

                case fabgl::VK_k        :
                case fabgl::VK_K        : ascii = '+'; break; /**< Plus: + */

                case fabgl::VK_l        :
                case fabgl::VK_L        : ascii = '='; break; /**< Equals: = */

                case fabgl::VK_z        :
                case fabgl::VK_Z        : ascii = ':'; break; /**< Colon: : */

                case fabgl::VK_x        :
                case fabgl::VK_X        : ascii = '`'; break; /**< Pound: £ */

                case fabgl::VK_c        :
                case fabgl::VK_C        : ascii = '?'; break; /**< Question mark: ? */

                case fabgl::VK_v        :
                case fabgl::VK_V        : ascii = '/'; break; /**< Slash: / */

                case fabgl::VK_b        :
                case fabgl::VK_B        : ascii = '*'; break; /**< Asterisk: * */

                case fabgl::VK_n        :
                case fabgl::VK_N        : ascii = ','; break; /**< Comma: , */

                case fabgl::VK_m        :
                case fabgl::VK_M        : ascii = '.'; break; /**< Period: . */

            }

        } else {
            switch (Nextkey.vk) {
                case fabgl::VK_a        :
                case fabgl::VK_A        : ascii = '~'; break; /**< Tilde: ~ */

                case fabgl::VK_s        :
                case fabgl::VK_S        : ascii = '|'; break; /**< Vertical bar: | */

                case fabgl::VK_d        :
                case fabgl::VK_D        : ascii = '\\'; break; /**< Backslash: \ */

                case fabgl::VK_f        :
                case fabgl::VK_F        : ascii = '{'; break; /**< Left brace: { */

                case fabgl::VK_g        :
                case fabgl::VK_G        : ascii = '}'; break; /**< Right brace: } */

                case fabgl::VK_y        :
                case fabgl::VK_Y        : ascii = '['; break; /**< Left bracket: [ */

                case fabgl::VK_u        :
                case fabgl::VK_U        : ascii = ']'; break; /**< Right bracket: ] */

                case fabgl::VK_p        :
                case fabgl::VK_P        : ascii = 0x7f; break; /**< copyright symbol > */

            }
        }
    } else {
        if (Nextkey.vk >= fabgl::VK_0 && Nextkey.vk <= fabgl::VK_9)
            ascii = Nextkey.vk + 46;
        else if (Nextkey.vk >= fabgl::VK_a && Nextkey.vk <= fabgl::VK_z)
            ascii = Nextkey.vk + 75;
        else if (Nextkey.vk >= fabgl::VK_A && Nextkey.vk <= fabgl::VK_Z)
            ascii = Nextkey.vk + 17;
    }

    if ( !ascii && Nextkey.vk == fabgl::VK_SPACE ) {
        ascii = ASCII_SPC;
    }

    return ascii;
}

string OSD::input(int x, int y, string inputLabel, int maxSize, int maxDisplaySize, uint16_t ink_color, uint16_t paper_color, const string& default_value, const string& filterchars, uint8_t * result_flags, int filterbehavior ) {

    if (result_flags) *result_flags &= ~INPUT_CANCELED;

    int curObject = 0;

    click();

    // Set font
    VIDEO::vga.setFont(SystemFont);

    // Wait for key
    fabgl::VirtualKeyItem Nextkey;

    uint8_t CursorFlash = 0;

    string inputValue = default_value;

    bool mode_E = false;

    int displayLimit = maxSize > maxDisplaySize ? maxDisplaySize : maxSize;

    int cursor_pos = inputValue.size();

    size_t pos_begin = 0;

    while (1) {

        if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead(KBDREAD_MODEINPUT);

        ESPeccy::readKbdJoy();

        if (ESPeccy::PS2Controller.keyboard()->virtualKeyAvailable()) {

            ESPeccy::readKbd(&Nextkey, KBDREAD_MODEINPUT);

            if(!Nextkey.down) continue;

            if (Nextkey.vk == fabgl::VK_LEFT) {
                if (cursor_pos > 0) cursor_pos--;
                continue;
            } else
            if (Nextkey.vk == fabgl::VK_RIGHT) {
                if (cursor_pos < inputValue.size()) cursor_pos++;
                continue;
            } else
            if (Nextkey.vk == fabgl::VK_HOME) {
                cursor_pos = 0;
                continue;
            } else
            if (Nextkey.vk == fabgl::VK_END) {
                cursor_pos = inputValue.size();
                continue;
            } else {
                int ascii = VirtualKey2ASCII(Nextkey, &mode_E);

                size_t charfilterpos = filterchars.find(ascii);

                if ( ascii &&
                        (
                            ( filterbehavior == FILTER_FORBIDDEN && charfilterpos != std::string::npos ) ||
                            ( filterbehavior == FILTER_ALLOWED && charfilterpos == std::string::npos )
                        )
                    )
                {
    //                OSD::osdCenteredMsg(OSD_INVALIDCHAR[Config::lang], LEVEL_WARN);
                    ascii = 0;
                }

                if ( ascii && inputValue.length() < maxSize ) {
                    inputValue = inputValue.substr(0,cursor_pos) + char(ascii) + inputValue.substr(cursor_pos);
                    cursor_pos++;
                    click();
                    mode_E = false;

                } else
                if (Nextkey.vk == fabgl::VK_BACKSPACE) {
                    if (cursor_pos > 0) {
                        inputValue = inputValue.substr(0,cursor_pos - 1) + inputValue.substr(cursor_pos);
                        cursor_pos--;
                    }
                    click();

                } else
                if (Nextkey.vk == fabgl::VK_RETURN) {
                    click();
                    ESPeccy::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_LCTRL, false, false);
                    ESPeccy::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_LSHIFT, false, false);
                    return inputValue;

                } else
                if (Nextkey.vk == fabgl::VK_ESCAPE) {
                    if ( result_flags ) *result_flags |= INPUT_CANCELED;
                    click();
                    break;

                }
            }
        }

        if ((++CursorFlash & 0xF) == 0) {
            // Determina el texto visible basado en la posición del cursor
            if (cursor_pos < pos_begin) {
                // Si el cursor se mueve antes del inicio actual, ajusta el inicio
                pos_begin = cursor_pos;
            } else
            if (cursor_pos - pos_begin >= displayLimit) {
                // Si el cursor está más allá del límite visible, ajusta el inicio
                pos_begin = cursor_pos - displayLimit;
            }

            // Calcula el segmento visible del texto
            std::string visibleText = inputValue.substr(pos_begin, displayLimit);

            // Imprime la etiqueta
            menuAt(y, x);
            VIDEO::vga.setTextColor(ink_color, paper_color);
            VIDEO::vga.print(inputLabel.c_str());

            // Imprime el primer segmento del texto hasta el cursor
            size_t relativeCursorPos = cursor_pos - pos_begin;
            if (relativeCursorPos > 0) {
                VIDEO::vga.print(visibleText.substr(0, relativeCursorPos).c_str());
            }

            // Configura el color del cursor si está activo
            if (CursorFlash > 63) VIDEO::vga.setTextColor(paper_color, ink_color);
            if (CursorFlash == 128) CursorFlash = 0;

            // Imprime el cursor
            VIDEO::vga.print(mode_E ? "E" : "L");

            // Restaura color texto
            VIDEO::vga.setTextColor(ink_color, paper_color);

            // Imprime el resto del texto después del cursor
            if (relativeCursorPos < visibleText.size()) {
                VIDEO::vga.print(visibleText.substr(relativeCursorPos).c_str());
            }

            // Rellena el espacio restante si el texto visible es menor al límite
            if (visibleText.size() < displayLimit) {
                VIDEO::vga.print(std::string(displayLimit - visibleText.size(), ' ').c_str());
            }

        }

        vTaskDelay(5 / portTICK_PERIOD_MS);

    }

    ESPeccy::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_LCTRL, false, false);
    ESPeccy::PS2Controller.keyboard()->injectVirtualKey(fabgl::VK_LSHIFT, false, false);

    return inputValue;

}

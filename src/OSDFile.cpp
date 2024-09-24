/*

ESPectrum, a Sinclair ZX Spectrum emulator for Espressif ESP32 SoC

Copyright (c) 2023, 2024 Víctor Iborra [Eremus] and 2023 David Crespo [dcrespo3d]
https://github.com/EremusOne/ZX-ESPectrum-IDF

Based on ZX-ESPectrum-Wiimote
Copyright (c) 2020, 2022 David Crespo [dcrespo3d]
https://github.com/dcrespo3d/ZX-ESPectrum-Wiimote

Based on previous work by Ramón Martinez and Jorge Fuertes
https://github.com/rampa069/ZX-ESPectrum

Original project by Pete Todd
https://github.com/retrogubbins/paseVGA

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

To Contact the dev team you can write to zxespectrum@gmail.com or
visit https://zxespectrum.speccy.org/contacto

*/

#include <string>
#include <algorithm>
#include <sys/stat.h>
#include "errno.h"

#include "esp_vfs.h"
#include "esp_vfs_fat.h"

using namespace std;

#include "OSDMain.h"
#include "FileUtils.h"
#include "Config.h"
#include "ESPectrum.h"
#include "cpuESP.h"
#include "Video.h"
#include "messages.h"
#include <math.h>
#include "ZXKeyb.h"
#include "pwm_audio.h"
#include "Z80_JLS/z80.h"
#include "Tape.h"

FILE *dirfile;
unsigned int OSD::elements;
unsigned int OSD::fdSearchElements;
unsigned int OSD::ndirs;
int8_t OSD::fdScrollPos;
int8_t OSD::fdScrollStatus;
int OSD::timeStartScroll;
int OSD::timeScroll;
uint8_t OSD::fdCursorFlash;
bool OSD::fdSearchRefresh;

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

// Función para convertir una cadena de dígitos en un número
// se agrega esta funcion porque atoul crashea si no hay digitos en el buffer
unsigned long getLong(char *buffer) {
    unsigned long result = 0;
    char * p = buffer;

    while (p && isdigit(*p)) {
        result = result * 10 + (*p - '0');
        ++p;
    }

    return result;
}

// Run a new file menu
string OSD::fileDialog(string &fdir, string title, uint8_t ftype, uint8_t mfcols, uint8_t mfrows) {

    // struct stat stat_buf;
    long dirfilesize;
    bool reIndex;

    // Columns and Rows
    cols = mfcols;
    mf_rows = mfrows + (Config::aspect_16_9 ? 0 : 1);

    // CRT Overscan compensation
    if (Config::videomode == 2) {
        x = 18;
        if (menu_level == 0) {
            if (Config::arch[0] == 'T' && Config::ALUTK == 2) {
                y = 4;
            } else {
                y = 12;
            }
        }
    } else {
        x = 0;
        if (menu_level == 0) y = 0;
    }

    // Position
    if (menu_level == 0) {
        x += (Config::aspect_16_9 ? 24 : 4);
        y += (Config::aspect_16_9 ? 4 : 8);
    } else {
        x += (Config::aspect_16_9 ? 24 : 4) + (60 * menu_level);
        y += (Config::aspect_16_9 ? 4 : 8) + (8 * (menu_level - 1));
    }

    // Size
    // w = (cols * OSD_FONT_W) + 2;
    // h = ((mf_rows + 1) * OSD_FONT_H) + 2;
    // // Check window boundaries
    // if ( x + mfcols * OSD_FONT_W > (Config::aspect_16_9 ? 24 : 4) + 52 * OSD_FONT_W ) x = (Config::aspect_16_9 ? 24 : 4) + ( 51 - mfcols ) * OSD_FONT_W;
    // if ( y + mfrows > (Config::aspect_16_9 ? 200 : 240) - 2 * OSD_FONT_H ) y = (Config::aspect_16_9 ? 200 : 240) - ( mfrows + 2 ) * OSD_FONT_H;

    // Adjust dialog size if needed
    w = (cols * OSD_FONT_W) + 2;
    printf("X: %d w: %d Cols: %d scrW: %d\n",x,w,cols,scrW);
    while ( x + w >= OSD::scrW - OSD_FONT_W) {
        cols--;
        w = (cols * OSD_FONT_W) + 2;
        printf("X: %d w: %d Cols: %d scrW: %d\n",x,w,cols,scrW);
    };

    h = ((mf_rows + 1) * OSD_FONT_H) + 2;
    printf("Y: %d h: %d mf_rows: %d scrH: %d\n",y,h,mf_rows,scrH);
    while ( y + h >= OSD::scrH - OSD_FONT_H) {
        mf_rows--;
        h = ((mf_rows + 1) * OSD_FONT_H) + 2;
        printf("Y: %d h: %d mf_rows: %d scrH: %d\n",y,h,mf_rows,scrH);
    };

    // Adjust begin_row & focus in case of values doesn't fit in current dialog size
    // printf("Focus: %d, Begin_row: %d, mf_rows: %d\n",(int) FileUtils::fileTypes[ftype].focus,(int) FileUtils::fileTypes[ftype].begin_row,(int) mf_rows);
    if (FileUtils::fileTypes[ftype].focus > mf_rows - 1) {
        FileUtils::fileTypes[ftype].begin_row += FileUtils::fileTypes[ftype].focus - (mf_rows - 1);
        FileUtils::fileTypes[ftype].focus = mf_rows - 1;
    } else
    if (FileUtils::fileTypes[ftype].focus + (FileUtils::fileTypes[ftype].begin_row - 2) < mf_rows) {
        FileUtils::fileTypes[ftype].focus += FileUtils::fileTypes[ftype].begin_row - 2;
        FileUtils::fileTypes[ftype].begin_row = 2;
    }

    // menu = title + "\n" + fdir + "\n";
    menu = title + "\n" + ( fdir.length() == 1 ? fdir : fdir.substr(0,fdir.length()-1)) + "\n";
    WindowDraw(); // Draw menu outline
    fd_PrintRow(1, IS_INFO);    // Path

reset:

    // Draw blank rows
    uint8_t row = 2;
    for (; row < mf_rows; row++) {
        VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
        menuAt(row, 0);
        VIDEO::vga.print(std::string(cols, ' ').c_str());
    }

    // Draw shortcut help
    string StatusBar = " ";
    if ( ftype == DISK_TAPFILE || ftype == DISK_SNAFILE ) // Dirty hack
        StatusBar += Config::lang == 0 ? "F2 New | " :
                     Config::lang == 1 ? "F2 Nuevo | " :
                                         "F2 Novo | " ;

    StatusBar += Config::lang == 0 ? "F3 Search | F8 Delete" :
                 Config::lang == 1 ? "F3 Buscar | F8 Borrar" :
                                     "F3 Procurar | F8 Excluir" ;

    if ( cols <= StatusBar.length() + 11 + 2 ) {
        StatusBar = " ";
        if ( ftype == DISK_TAPFILE || ftype == DISK_SNAFILE ) // Dirty hack
            StatusBar += "F2 | ";

        StatusBar += "F3 | F8";
    }


    if (cols > StatusBar.length() + 11 + 2 ) { // 11 from elements counter + 2 from borders
        StatusBar += std::string(cols - StatusBar.length() - 12, ' ');
    } else {
        StatusBar = std::string(cols - 12, ' ');
    }

    // Print status bar
    menuAt(row, 0);
    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));

    if (FileUtils::fileTypes[ftype].fdMode)
        VIDEO::vga.print(std::string(cols, ' ').c_str());
    else {
        VIDEO::vga.print(StatusBar.c_str());
        VIDEO::vga.print(std::string(12, ' ').c_str());
    }


    // fdSearchRefresh = true;

    while(1) {

//        ESPectrum::showMemInfo("file dialog: before checking dir");

        fdCursorFlash = 0;

        reIndex = false;
        string filedir = FileUtils::MountPoint + fdir;

        std::vector<std::string> filexts;
        size_t pos = 0;
        string ss = FileUtils::fileTypes[ftype].fileExts;
        while ((pos = ss.find(",")) != std::string::npos) {
            filexts.push_back(ss.substr(0, pos));
            ss.erase(0, pos + 1);
        }
        filexts.push_back(ss.substr(0));

        unsigned long hash = 0; // Name checksum variables

        // Get Dir Stats
        int result = FileUtils::getDirStats(filedir, filexts, &hash, &elements, &ndirs);

        filexts.clear(); // Clear vector
        std::vector<std::string>().swap(filexts); // free memory

        if ( result == -1 ) {

            printf("Error opening %s\n",filedir.c_str());

            FileUtils::unmountSDCard();

            OSD::restoreBackbufferData();

            click();
            return "";

        }

        // Open dir file for read
        printf("Checking existence of index file %s\n",(filedir + FileUtils::fileTypes[ftype].indexFilename).c_str());
        dirfile = fopen((filedir + FileUtils::fileTypes[ftype].indexFilename).c_str(), "r");
        if (dirfile == NULL) {
            printf("No dir file found: reindexing\n");
            reIndex = true;
        } else {
            // stat((filedir + FileUtils::fileTypes[ftype].indexFilename).c_str(), &stat_buf);
            fseek(dirfile,0,SEEK_END);
            dirfilesize = ftell(dirfile);

            fseek(dirfile, dirfilesize - 20, SEEK_SET);

            char fhash[21];
            memset( fhash, '\0', sizeof(fhash));
            fgets(fhash, sizeof(fhash), dirfile);
            // printf("File Hash: %s\n",fhash);

            // If calc hash and file hash are different refresh dir index
            if ( getLong(fhash) != hash ||
                dirfilesize - 20 != FILENAMELEN * ( ndirs+elements + ( filedir != ( FileUtils::MountPoint + "/" ) ? 1 : 0 ) ) ) {
                reIndex = true;
            }
        }

//        ESPectrum::showMemInfo("file dialog: after checking dir");

        // Force reindex (for testing)
        // reIndex = ESPectrum::ESPtestvar ? true : reIndex;

        // There was no index or hashes are different: reIndex
        if (reIndex) {

//            ESPectrum::showMemInfo("file dialog: before reindex");

            if ( dirfile ) {
                fclose(dirfile);
                dirfile = nullptr;
            }

#if 0
            multi_heap_info_t info;
            size_t ram_consumption;

            heap_caps_get_info(&info, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); // internal RAM, memory capable to store data or to create new task

            printf("\n=======================================================\n");
            printf("ORDENANDO CARPETA\n");
            printf("=======================================================\n");
            printf("\nTotal free bytes          : %d\n", info.total_free_bytes);
            printf("Minimum free ever         : %d\n", info.minimum_free_bytes);

            size_t minimum_before = info.minimum_free_bytes;

            uint32_t time_start = esp_timer_get_time();
#endif

            FileUtils::DirToFile(filedir, ftype, hash, ndirs + elements ); // Prepare filelist

#if 0
            uint32_t time_elapsed = esp_timer_get_time() - time_start;

            heap_caps_get_info(&info, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); // internal RAM, memory capable to store data or to create new task

            printf("TIEMPO DE ORDENACION      : %6.2f segundos\n", (float)time_elapsed / 1000000);
            printf("Total free bytes  despues : %d\n", info.total_free_bytes);
            printf("Minimum free ever despues : %d\n", info.minimum_free_bytes);
            printf("Consumo RAM               : %d\n", minimum_before - info.minimum_free_bytes);

            printf("\n=======================================================\n");
#endif
            // stat((filedir + FileUtils::fileTypes[ftype].indexFilename).c_str(), &stat_buf);

            dirfile = fopen((filedir + FileUtils::fileTypes[ftype].indexFilename).c_str(), "r");
            if (dirfile == NULL) {
                printf("Error opening index file\n");

                FileUtils::unmountSDCard();

                OSD::restoreBackbufferData();
                click();

                return "";
            }

            fseek(dirfile,0,SEEK_END);
            dirfilesize = ftell(dirfile);

            // Reset position
            FileUtils::fileTypes[ftype].begin_row = FileUtils::fileTypes[ftype].focus = 2;

//            ESPectrum::showMemInfo("file dialog: after reindex");

        }

        if (FileUtils::fileTypes[ftype].fdMode) {

            // Clean Status Bar
            menuAt(row, 0);
            VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
            VIDEO::vga.print(std::string(StatusBar.length(), ' ').c_str());

            // Recalc items number
            long prevpos = ftell(dirfile);

            unsigned int foundcount = 0;
            fdSearchElements = 0;
            rewind(dirfile);
            char buf[FILENAMELEN+1];
            string search = FileUtils::fileTypes[ftype].fileSearch;
            std::transform(search.begin(), search.end(), search.begin(), ::toupper);
            while(1) {
                fgets(buf, sizeof(buf), dirfile);
                if (feof(dirfile)) break;
                if (buf[0] == ASCII_SPC) {
                    foundcount++;
                    // printf("%s",buf);
                }else {
                    char *p = buf; while(*p) *p++ = toupper(*p);
                    char *pch = strstr(buf, search.c_str());
                    if (pch != NULL) {
                        foundcount++;
                        fdSearchElements++;
                        // printf("%s",buf);
                    }
                }
            }

            if (foundcount) {
                // Redraw rows
                real_rows = foundcount + 2; // Add 2 for title and status bar
                virtual_rows = (real_rows > mf_rows ? mf_rows : real_rows);
                last_begin_row = last_focus = 0;
                // FileUtils::fileTypes[ftype].focus = 2;
                // FileUtils::fileTypes[ftype].begin_row = 2;
                // fd_Redraw(title, fdir, ftype);
            } else {
                fseek(dirfile,prevpos,SEEK_SET);
            }

            fdSearchRefresh = false;

        } else {

            // real_rows = (stat_buf.st_size / FILENAMELEN) + 2; // Add 2 for title and status bar
            real_rows = (dirfilesize / FILENAMELEN) + 2; // Add 2 for title and status bar
            virtual_rows = (real_rows > mf_rows ? mf_rows : real_rows);
            // printf("Real rows: %d; st_size: %d; Virtual rows: %d\n",real_rows,stat_buf.st_size,virtual_rows);

            last_begin_row = last_focus = 0;

            fdSearchElements = elements;

        }

        // printf("Focus: %d, Begin_row: %d, real_rows: %d, mf_rows: %d\n",(int) FileUtils::fileTypes[ftype].focus,(int) FileUtils::fileTypes[ftype].begin_row,(int) real_rows, (int) mf_rows);
        if ((real_rows > mf_rows) && ((FileUtils::fileTypes[ftype].begin_row + mf_rows - 2) > real_rows)) {
            FileUtils::fileTypes[ftype].focus += (FileUtils::fileTypes[ftype].begin_row + mf_rows - 2) - real_rows;
            FileUtils::fileTypes[ftype].begin_row = real_rows - (mf_rows - 2);
            // printf("Focus: %d, BeginRow: %d\n",FileUtils::fileTypes[ftype].focus,FileUtils::fileTypes[ftype].begin_row);
        }

        fd_Redraw(title, fdir, ftype); // Draw content

        // Focus line scroll position
        fdScrollPos = 0;
        timeStartScroll = 0;
        timeScroll = 0;
        fdScrollStatus = 0;

        bool mode_E = false;

        fabgl::VirtualKeyItem Menukey;
        const string fat32forbidden="\\/:*?\"<>|\x7F"; // Characters not valid for FAT32 filenames

        while (1) {

            if (ZXKeyb::Exists) ZXKeyb::ZXKbdRead();

            ESPectrum::readKbdJoy();

            // Process external keyboard
            if (ESPectrum::PS2Controller.keyboard()->virtualKeyAvailable()) {

                timeStartScroll = 0;
                timeScroll = 0;
                fdScrollPos = 0;
                fdScrollStatus = 0;

                // Print elements
                VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));

                unsigned int elem = FileUtils::fileTypes[ftype].fdMode ? fdSearchElements : elements;
                if (elem) {
                    // menuAt(mf_rows, cols - (real_rows > virtual_rows ? 13 : 12));
                    menuAt(mf_rows, cols - 12);
                    char elements_txt[13];
                    int nitem = (FileUtils::fileTypes[ftype].begin_row + FileUtils::fileTypes[ftype].focus ) - (4 + ndirs) + (fdir.length() == 1);
                    snprintf(elements_txt, sizeof(elements_txt), "%d/%d ", nitem > 0 ? nitem : 0 , elem);
                    VIDEO::vga.print(std::string(12 - strlen(elements_txt), ' ').c_str());
                    VIDEO::vga.print(elements_txt);
                } else {
                    menuAt(mf_rows, cols - 12);
                    VIDEO::vga.print(std::string(12,' ').c_str());
                }

                if (ESPectrum::readKbd(&Menukey)) {

                    if (!Menukey.down) continue;

                    int fsearch = VirtualKey2ASCII(Menukey, &mode_E);

                    // Search first ocurrence of letter if we're not on that letter yet
                    if ( fsearch /*((Menukey.vk >= fabgl::VK_a) && (Menukey.vk <= fabgl::VK_Z)) || Menukey.vk == fabgl::VK_SPACE || ((Menukey.vk >= fabgl::VK_0) && (Menukey.vk <= fabgl::VK_9))*/) {

                        // fat32 filter
                        if (fat32forbidden.find(fsearch) != std::string::npos) continue;

                        /*
                        if (Menukey.vk==fabgl::VK_SPACE && FileUtils::fileTypes[ftype].fdMode)
                            fsearch = ASCII_SPC;
                        else if (Menukey.vk<=fabgl::VK_9)
                            fsearch = Menukey.vk + 46;
                        else if (Menukey.vk<=fabgl::VK_z)
                            fsearch = Menukey.vk + 75;
                        else if (Menukey.vk<=fabgl::VK_Z)
                            fsearch = Menukey.vk + 17;
                        */

                        if (FileUtils::fileTypes[ftype].fdMode) {

                            if (fsearch && FileUtils::fileTypes[ftype].fileSearch.length() < FILENAMELEN /*MAXSEARCHLEN*/) {
                                FileUtils::fileTypes[ftype].fileSearch += char(fsearch);
                                fdSearchRefresh = true;
                                mode_E = false;
                                click();
                            }

                        } else {
                            uint8_t letra = rowGet(menu,FileUtils::fileTypes[ftype].focus).at(0);
                            // printf("%d %d\n",(int)letra,fsearch);
                            if (toupper(letra) != toupper(fsearch)) {
                                // Seek first ocurrence of letter/number
                                long prevpos = ftell(dirfile);
                                char buf[FILENAMELEN+1];
                                int cnt = 0;
                                fseek(dirfile,0,SEEK_SET);
                                while(!feof(dirfile)) {
                                    fgets(buf, sizeof(buf), dirfile);
                                    // printf("%c %d\n",buf[0],int(buf[0]));
                                    if (toupper(buf[0]) == toupper(char(fsearch))) break;
                                    cnt++;
                                }
                                // printf("Cnt: %d Letra: %d\n",cnt,int(letra));
                                if (!feof(dirfile)) {
                                    last_begin_row = FileUtils::fileTypes[ftype].begin_row;
                                    last_focus = FileUtils::fileTypes[ftype].focus;
                                    if (real_rows > virtual_rows) {
                                        int m = cnt + virtual_rows - real_rows;
                                        if (m > 0) {
                                            FileUtils::fileTypes[ftype].focus = m + 2;
                                            FileUtils::fileTypes[ftype].begin_row = cnt - m + 2;
                                        } else {
                                            FileUtils::fileTypes[ftype].focus = 2;
                                            FileUtils::fileTypes[ftype].begin_row = cnt + 2;
                                        }
                                    } else {
                                        FileUtils::fileTypes[ftype].focus = cnt + 2;
                                        FileUtils::fileTypes[ftype].begin_row = 2;
                                    }
                                    // printf("Real rows: %d; Virtual rows: %d\n",real_rows,virtual_rows);
                                    // printf("Focus: %d, Begin_row: %d\n",(int) FileUtils::fileTypes[ftype].focus,(int) FileUtils::fileTypes[ftype].begin_row);
                                    fd_Redraw(title,fdir,ftype);
                                    click();
                                } else
                                    fseek(dirfile,prevpos,SEEK_SET);

                            }
                        }

                    } else if (Menukey.vk == fabgl::VK_F2 && ( ftype == DISK_TAPFILE || ftype == DISK_SNAFILE ) ) {  // Dirty hack

                        bool save_withrom = Menukey.CTRL;

                        string new_tap = OSD::input( 1, mf_rows, Config::lang == 0 ? "Name: " :
                                                                 Config::lang == 1 ? "Nomb: " :
                                                                                     "Nome: "
                                                                , 30
                                                                , cols - 3 - 6
                                                                , zxColor(7,1), zxColor(5,0)
                                                                , ""
                                                                , fat32forbidden );

                        if ( new_tap != "" ) {

                            fclose(dirfile);
                            dirfile = NULL;

                            FileUtils::fileTypes[ftype].begin_row = FileUtils::fileTypes[ftype].focus = 2;

                            string ext;
                            if ( ftype == DISK_TAPFILE ) ext = ".tap";
                            else if ( FileUtils::hasZ80extension(new_tap) || FileUtils::hasSNAextension(new_tap) || FileUtils::hasExtension(new_tap, "sp") ) {
                                ext = "";
                            } else
                                ext = ".sna";

                            return ( save_withrom ? "*" : "N" ) + new_tap + ext;

                        } else {

                            menuAt(mf_rows, 0);
                            VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
                            if (FileUtils::fileTypes[ftype].fdMode) {
                                VIDEO::vga.print(string(cols - 1, ' ').c_str());
                            } else {
                                VIDEO::vga.print(StatusBar.substr(0,cols - 12).c_str());
                                if ( StatusBar.size() < cols - 12 ) VIDEO::vga.print(string(cols - 12 - StatusBar.size(), ' ').c_str());
                            }

                        }

//                        fd_Redraw(title, fdir, ftype);

                    } else if (Menukey.vk == fabgl::VK_F3) {

                        FileUtils::fileTypes[ftype].fdMode ^= 1;

                        // status bard position & color
                        menuAt(mf_rows, 0);
                        VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));

                        if (FileUtils::fileTypes[ftype].fdMode) {

                            // Clean status bar

                            VIDEO::vga.print(std::string(StatusBar.length(), ' ').c_str());

                            fdCursorFlash = 63;

                            // menuAt(mfrows + (Config::aspect_letterbox ? 0 : 1), 1);
                            // VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
                            // VIDEO::vga.print(Config::lang ? "B\xA3sq: " : "Find: ");
                            // VIDEO::vga.print(FileUtils::fileTypes[ftype].fileSearch.c_str());
                            // VIDEO::vga.setTextColor(zxColor(5, 0), zxColor(7, 1));
                            // VIDEO::vga.print("K");
                            // VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
                            // VIDEO::vga.print(std::string(16 - FileUtils::fileTypes[ftype].fileSearch.size(), ' ').c_str());

                            fdSearchRefresh = FileUtils::fileTypes[ftype].fileSearch != "";

                        } else {

                            // Restore status bar
                            VIDEO::vga.print(StatusBar.substr(0,cols - 12).c_str());
                            if ( StatusBar.size() < cols - 12 ) VIDEO::vga.print(string(cols - 12 - StatusBar.size(), ' ').c_str());

                            if (FileUtils::fileTypes[ftype].fileSearch != "") {
                                // FileUtils::fileTypes[ftype].fileSearch="";
                                real_rows = (dirfilesize / FILENAMELEN) + 2; // Add 2 for title and status bar
                                virtual_rows = (real_rows > mf_rows ? mf_rows : real_rows);
                                last_begin_row = last_focus = 0;
                                FileUtils::fileTypes[ftype].focus = 2;
                                FileUtils::fileTypes[ftype].begin_row = 2;
                                fd_Redraw(title, fdir, ftype);
                            }

                        }

                        click();

                    } else if (Menukey.vk == fabgl::VK_F8) {
                        click();

                        filedir = rowGet(menu,FileUtils::fileTypes[ftype].focus);
                        // printf("%s\n",filedir.c_str());

                        if ( filedir[0] != ASCII_SPC ) {
                            rtrim(filedir);

                            if ( !access(( FileUtils::MountPoint + fdir + filedir ).c_str(), W_OK) ) {
                                string title = MENU_DELETE_CURRENT_FILE[Config::lang];
                                string msg = OSD_DLG_SURE[Config::lang];
                                uint8_t res = msgDialog(title,msg);
                                menu_saverect = true;

                                if (res == DLG_YES) {
                                    if ( FileUtils::getResolvedPath( FileUtils::MountPoint + fdir + filedir ) == FileUtils::getResolvedPath( Tape::tapeSaveName ) ) Tape::tapeEject();
                                    remove(( FileUtils::MountPoint + fdir + filedir ).c_str());
                                    fd_Redraw(title, fdir, ftype);
                                    menu_saverect = true;
                                    goto reset;
                                }
                            } else {
                                OSD::osdCenteredMsg(OSD_READONLY_FILE_WARN, LEVEL_WARN);
                            }
                            click();
                        }
                    } else if (Menukey.vk == fabgl::VK_UP || Menukey.vk == fabgl::VK_JOY1UP || Menukey.vk == fabgl::VK_JOY2UP) {
                        if (FileUtils::fileTypes[ftype].focus == 2 && FileUtils::fileTypes[ftype].begin_row > 2) {
                            last_begin_row = FileUtils::fileTypes[ftype].begin_row;
                            FileUtils::fileTypes[ftype].begin_row--;
                            fd_Redraw(title, fdir, ftype);
                        } else if (FileUtils::fileTypes[ftype].focus > 2) {
                            last_focus = FileUtils::fileTypes[ftype].focus;
                            fd_PrintRow(FileUtils::fileTypes[ftype].focus--, IS_NORMAL);
                            fd_PrintRow(FileUtils::fileTypes[ftype].focus, IS_FOCUSED);
                            // printf("Focus: %d, Lastfocus: %d\n",FileUtils::fileTypes[ftype].focus,(int) last_focus);
                        }
                        click();
                    } else if (Menukey.vk == fabgl::VK_DOWN || Menukey.vk == fabgl::VK_JOY1DOWN || Menukey.vk == fabgl::VK_JOY2DOWN) {
                        if (FileUtils::fileTypes[ftype].focus == virtual_rows - 1 && FileUtils::fileTypes[ftype].begin_row + virtual_rows - 2 < real_rows) {
                            last_begin_row = FileUtils::fileTypes[ftype].begin_row;
                            FileUtils::fileTypes[ftype].begin_row++;
                            fd_Redraw(title, fdir, ftype);
                        } else if (FileUtils::fileTypes[ftype].focus < virtual_rows - 1) {
                            last_focus = FileUtils::fileTypes[ftype].focus;
                            fd_PrintRow(FileUtils::fileTypes[ftype].focus++, IS_NORMAL);
                            fd_PrintRow(FileUtils::fileTypes[ftype].focus, IS_FOCUSED);
                            // printf("Focus: %d, Lastfocus: %d\n",FileUtils::fileTypes[ftype].focus,(int) last_focus);
                        }
                        click();
                    } else if (Menukey.vk == fabgl::VK_PAGEUP || ((Menukey.vk == fabgl::VK_LEFT) && (Config::osd_LRNav == 0)) || Menukey.vk == fabgl::VK_JOY1LEFT || Menukey.vk == fabgl::VK_JOY2LEFT) {
                        if (FileUtils::fileTypes[ftype].begin_row > virtual_rows) {
                            FileUtils::fileTypes[ftype].focus = 2;
                            FileUtils::fileTypes[ftype].begin_row -= virtual_rows - 2;
                        } else {
                            FileUtils::fileTypes[ftype].focus = 2;
                            FileUtils::fileTypes[ftype].begin_row = 2;
                        }
                        fd_Redraw(title, fdir, ftype);
                        click();
                    } else if (Menukey.vk == fabgl::VK_PAGEDOWN || ((Menukey.vk == fabgl::VK_RIGHT) && (Config::osd_LRNav == 0)) || Menukey.vk == fabgl::VK_JOY1RIGHT || Menukey.vk == fabgl::VK_JOY2RIGHT) {
                        if (real_rows - FileUtils::fileTypes[ftype].begin_row  - virtual_rows > virtual_rows) {
                            FileUtils::fileTypes[ftype].focus = 2;
                            FileUtils::fileTypes[ftype].begin_row += virtual_rows - 2;
                        } else {
                            FileUtils::fileTypes[ftype].focus = virtual_rows - 1;
                            FileUtils::fileTypes[ftype].begin_row = real_rows - virtual_rows + 2;
                        }
                        fd_Redraw(title, fdir, ftype);
                        click();
                    } else if (Menukey.vk == fabgl::VK_HOME) {
                        last_focus = FileUtils::fileTypes[ftype].focus;
                        last_begin_row = FileUtils::fileTypes[ftype].begin_row;
                        FileUtils::fileTypes[ftype].focus = 2;
                        FileUtils::fileTypes[ftype].begin_row = 2;
                        fd_Redraw(title, fdir, ftype);
                        click();
                    } else if (Menukey.vk == fabgl::VK_END) {
                        last_focus = FileUtils::fileTypes[ftype].focus;
                        last_begin_row = FileUtils::fileTypes[ftype].begin_row;
                        FileUtils::fileTypes[ftype].focus = virtual_rows - 1;
                        FileUtils::fileTypes[ftype].begin_row = real_rows - virtual_rows + 2;
                        // printf("Focus: %d, Lastfocus: %d\n",FileUtils::fileTypes[ftype].focus,(int) last_focus);
                        fd_Redraw(title, fdir, ftype);
                        click();
                    } else if (Menukey.vk == fabgl::VK_BACKSPACE) {
                        if (FileUtils::fileTypes[ftype].fdMode) {
                            if (FileUtils::fileTypes[ftype].fileSearch.length()) {
                                FileUtils::fileTypes[ftype].fileSearch.pop_back();
                                fdSearchRefresh = true;
                                click();
                            }
                        } else {
                            if (fdir != "/") {

                                fclose(dirfile);
                                dirfile = NULL;

                                fdir.pop_back();
                                fdir = fdir.substr(0,fdir.find_last_of("/") + 1);

                                FileUtils::fileTypes[ftype].begin_row = FileUtils::fileTypes[ftype].focus = 2;
                                // printf("Fdir: %s\n",fdir.c_str());

                                click();

                                break;

                            }
                        }
                    } else if (Menukey.vk == fabgl::VK_RETURN || ((Menukey.vk == fabgl::VK_RIGHT) && (Config::osd_LRNav == 1)) || Menukey.vk == fabgl::VK_JOY1B || Menukey.vk == fabgl::VK_JOY2B || Menukey.vk == fabgl::VK_JOY1C || Menukey.vk == fabgl::VK_JOY2C) {

                        fclose(dirfile);
                        dirfile = NULL;

                        filedir = rowGet(menu,FileUtils::fileTypes[ftype].focus);
                        // printf("%s\n",filedir.c_str());
                        if (filedir[0] == ASCII_SPC) {
                            if (filedir[1] == ASCII_SPC) {
                                fdir.pop_back();
                                fdir = fdir.substr(0,fdir.find_last_of("/") + 1);
                            } else {
                                filedir.erase(0,1);
                                trim(filedir);
                                fdir = fdir + filedir + "/";
                            }
                            FileUtils::fileTypes[ftype].begin_row = FileUtils::fileTypes[ftype].focus = 2;
                            // printf("Fdir: %s\n",fdir.c_str());
                            break;
                        } else {

                            OSD::restoreBackbufferData();

                            rtrim(filedir);
                            click();

                            if ((Menukey.CTRL && Menukey.vk == fabgl::VK_RETURN) || Menukey.vk == fabgl::VK_JOY1C || Menukey.vk == fabgl::VK_JOY2C)
                                return "S" + filedir;
                            else
                                return "R" + filedir;

                            // return (Menukey.vk == fabgl::VK_RETURN || Menukey.vk == fabgl::VK_JOY1B || Menukey.vk == fabgl::VK_JOY2B ? "R" : "S") + filedir;

                        }

                    } else if (Menukey.vk == fabgl::VK_ESCAPE || ((Menukey.vk == fabgl::VK_LEFT) && (Config::osd_LRNav == 1)) || Menukey.vk == fabgl::VK_JOY1A || Menukey.vk == fabgl::VK_JOY2A) {

                        OSD::restoreBackbufferData();

                        fclose(dirfile);
                        dirfile = NULL;
                        click();
                        return "";
                    }
                }

            } else {

                if (timeStartScroll < 125) timeStartScroll++;

            }

            // Scroll focused line if signaled
            if (timeStartScroll == 125) {
                timeScroll++;
                if (timeScroll == 25) {
                    if (fdScrollStatus==0) fdScrollPos++;
                    else                   fdScrollPos--;
                    fd_PrintRow(FileUtils::fileTypes[ftype].focus, IS_FOCUSED);
                    timeScroll = 0;
                }
            }

            if (FileUtils::fileTypes[ftype].fdMode) {

                if ((++fdCursorFlash & 0xf) == 0) {
                    menuAt(mf_rows, 1);
                    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
                    VIDEO::vga.print(Config::lang == 0 ? "Find: " :
                                     Config::lang == 1 ? "B\xA3sq: " :
                                                         "Proc:"
                                    );
                    VIDEO::vga.print( FileUtils::fileTypes[ftype].fileSearch.size() > MAXSEARCHLEN ? FileUtils::fileTypes[ftype].fileSearch.substr( FileUtils::fileTypes[ftype].fileSearch.size() - MAXSEARCHLEN).c_str() : FileUtils::fileTypes[ftype].fileSearch.c_str());
                    if (fdCursorFlash > 63) {
                        VIDEO::vga.setTextColor(zxColor(5, 0), zxColor(7, 1));
                        if (fdCursorFlash == 128) fdCursorFlash = 0;
                    }
                    VIDEO::vga.print(mode_E?"E":"L");
                    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
                    VIDEO::vga.print(std::string(FileUtils::fileTypes[ftype].fileSearch.size() > MAXSEARCHLEN ? MAXSEARCHLEN : MAXSEARCHLEN - FileUtils::fileTypes[ftype].fileSearch.size(), ' ').c_str());
                }

                if (fdSearchRefresh) {

                    // Recalc items number
                    long prevpos = ftell(dirfile);

                    unsigned int foundcount = 0;
                    fdSearchElements = 0;
                    rewind(dirfile);
                    char buf[FILENAMELEN+1];
                    string search = FileUtils::fileTypes[ftype].fileSearch;
                    std::transform(search.begin(), search.end(), search.begin(), ::toupper);
                    while(1) {
                        fgets(buf, sizeof(buf), dirfile);
                        if (feof(dirfile)) break;
                        if (buf[0] == ASCII_SPC) {
                            foundcount++;
                            // printf("%s",buf);
                        }else {
                            char *p = buf; while(*p) *p++ = toupper(*p);
                            char *pch = strstr(buf, search.c_str());
                            if (pch != NULL) {
                                foundcount++;
                                fdSearchElements++;
                                // printf("%s",buf);
                            }
                        }
                    }

                    if (foundcount) {
                        // Redraw rows
                        real_rows = foundcount + 2; // Add 2 for title and status bar
                        virtual_rows = (real_rows > mf_rows ? mf_rows : real_rows);
                        last_begin_row = last_focus = 0;
                        FileUtils::fileTypes[ftype].focus = 2;
                        FileUtils::fileTypes[ftype].begin_row = 2;
                        fd_Redraw(title, fdir, ftype);
                    } else {
                        fseek(dirfile,prevpos,SEEK_SET);
                    }

                    fdSearchRefresh = false;
                }

            } /*else {

                if (cols > (Config::lang ? 32 : 28) + 14) {

                    menuAt(mfrows + (Config::aspect_16_9 ? 0 : 1), 1);
                    VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
                    if ( ftype == DISK_TAPFILE ) { // Dirty hack
                        VIDEO::vga.print(Config::lang ? "F2 Nuevo | " : "F2 New | " );
                    }
                    VIDEO::vga.print(Config::lang ? "F3 Buscar | F8 Borrar" : "F3 Find | F8 Delete" );

                }

            }*/

            vTaskDelay(5 / portTICK_PERIOD_MS);

        }

    }

}

// Redraw inside rows
void OSD::fd_Redraw(string title, string fdir, uint8_t ftype) {

    if ((FileUtils::fileTypes[ftype].focus != last_focus) || (FileUtils::fileTypes[ftype].begin_row != last_begin_row)) {

        // printf("fd_Redraw\n");

        // Read bunch of rows
        menu = title + "\n" + ( fdir.length() == 1 ? fdir : fdir.substr(0,fdir.length()-1)) + "\n";
        char buf[FILENAMELEN+1];
        if (FileUtils::fileTypes[ftype].fdMode == 0 || FileUtils::fileTypes[ftype].fileSearch == "") {
            fseek(dirfile, (FileUtils::fileTypes[ftype].begin_row - 2) * FILENAMELEN, SEEK_SET);
            for (int i = 2; i < virtual_rows; i++) {
                fgets(buf, sizeof(buf), dirfile);
                if (feof(dirfile)) break;
                menu += buf;
            }
        } else {
            rewind(dirfile);
            int i = 2;
            int count = 2;
            string search = FileUtils::fileTypes[ftype].fileSearch;
            std::transform(search.begin(), search.end(), search.begin(), ::toupper);
            char upperbuf[FILENAMELEN+1];
            while (1) {
                fgets(buf, sizeof(buf), dirfile);
                if (feof(dirfile)) break;
                if (buf[0] == ASCII_SPC) {
                    if (i >= FileUtils::fileTypes[ftype].begin_row) {
                        menu += buf;
                        if (++count == virtual_rows) break;
                    }
                    i++;
                } else {
                    for(int i=0; buf[i]; i++) upperbuf[i] = toupper(buf[i]);
                    char *pch = strstr(upperbuf, search.c_str());
                    if (pch != NULL) {
                        if (i >= FileUtils::fileTypes[ftype].begin_row) {
                            menu += buf;
                            if (++count == virtual_rows) break;
                        }
                        i++;
                    }
                }
            }
        }

        fd_PrintRow(1, IS_INFO); // Print status bar

        uint8_t row = 2;
        for (; row < virtual_rows; row++) {
            if (row == FileUtils::fileTypes[ftype].focus) {
                fd_PrintRow(row, IS_FOCUSED);
            } else {
                fd_PrintRow(row, IS_NORMAL);
            }
        }

        if (real_rows > virtual_rows) {
            menuScrollBar(FileUtils::fileTypes[ftype].begin_row - 1);
        } else {
            for (; row < mf_rows; row++) {
                VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
                menuAt(row, 0);
                VIDEO::vga.print(std::string(cols, ' ').c_str());
            }
        }

        last_focus = FileUtils::fileTypes[ftype].focus;
        last_begin_row = FileUtils::fileTypes[ftype].begin_row;
    }

}

// Print a virtual row
void OSD::fd_PrintRow(uint8_t virtual_row_num, uint8_t line_type) {

    uint8_t margin;

    string line = rowGet(menu, virtual_row_num);

    bool isDir = (line[0] == ASCII_SPC);

    trim(line);

    switch (line_type) {
    case IS_TITLE:
        VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(0, 0));
        margin = 2;
        break;
    case IS_INFO:
        VIDEO::vga.setTextColor(zxColor(7, 1), zxColor(5, 0));
        margin = (real_rows > virtual_rows ? 3 : 2);
        break;
    case IS_FOCUSED:
        VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(5, 1));
        margin = (real_rows > virtual_rows ? 3 : 2);
        break;
    default:
        VIDEO::vga.setTextColor(zxColor(0, 1), zxColor(7, 1));
        margin = (real_rows > virtual_rows ? 3 : 2);
    }

    menuAt(virtual_row_num, 0);

    VIDEO::vga.print(" ");

    const string extra_line = isDir ? " <DIR>" : "";
    size_t extra_margin = extra_line.length();

    if (line.length() <= cols - margin - extra_margin) {
        line = line + std::string(cols - margin - extra_margin - line.length(), ' ');
    } else {
        if ( !isDir && line_type == IS_INFO ) {
            line = ".." + line.substr(line.length() - (cols - margin) + 2);
        } else {
            if (line_type == IS_FOCUSED) {
                string full_line = line;

                if ( fdScrollPos >= (int) line.length() ) {
                    fdScrollPos = - ( cols - margin - extra_margin );
                    line = string(-fdScrollPos, '\x07') + line;
                } else if ( fdScrollPos < 0 ) {
                    line = string(-fdScrollPos, '\x07') + line;
                } else {
                    line = line.substr(fdScrollPos);
                    if (!Config::osd_AltRot && line.length() <= cols - margin - extra_margin && full_line.length() > cols - margin - extra_margin) {
                        line += string(cols - margin - extra_margin, '\x07') + full_line;
                    }
                }

                if (fdScrollStatus==0)
                {
                    if (fdScrollPos >= 0 && line.length() <= cols - margin - extra_margin) {
                        if (Config::osd_AltRot == 1) {
                            timeStartScroll = 0;
                            fdScrollStatus = 1;
                        }
                    }
                }
                else
                {
                    if (fdScrollPos == 0) {
                        fdScrollPos = -1;
                        timeStartScroll = 0;
                        fdScrollStatus = 0;
                    }
                }
            }
        }
    }

    line = line.substr(0,cols - margin - extra_margin) + extra_line;

    VIDEO::vga.print(line.c_str());

    VIDEO::vga.print(" ");

}

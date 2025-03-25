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

About keymap.cpp
-----------------------------------------------
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

#include "Keymap.h"
#include "FileUtils.h"

using namespace std;

// Keyboard map layout
fabgl::KeyboardLayout   Keymap::kbdcustom;

// File with custom layout
char                    Keymap::keymapfilename[25]  = "/key.map";

// Base path for key.map configuration file
string                  Keymap::keymapfile_path = MOUNT_POINT_SD;
bool                    Keymap::keymap_enable = false;

// -----------------------------------------------------------------------------------------
// Auxiliary methods
// -----------------------------------------------------------------------------------------
std::string trim(const std::string &source) 
{
    std::string s(source);
    // Removing comments
    if(s.find_first_of("/") != string::npos)
    { s.erase(s.find_first_of("/"));  }

    // Removing white spaces and special chars
    s.erase(0,s.find_first_not_of(" \n\r\t"));
    s.erase(s.find_last_not_of(" \n\r\t")+1);
    return s;
}

int searchScanCodeFromVK(std::string vk)
{

    char const * VKTOSTR[] = { "VK_NONE", "VK_SPACE", "VK_0", "VK_1", "VK_2", "VK_3", "VK_4", "VK_5", "VK_6", "VK_7", "VK_8", "VK_9", "VK_KP_0", "VK_KP_1", "VK_KP_2",
                                "VK_KP_3", "VK_KP_4", "VK_KP_5", "VK_KP_6", "VK_KP_7", "VK_KP_8", "VK_KP_9", "VK_a", "VK_b", "VK_c", "VK_d", "VK_e", "VK_f", "VK_g", "VK_h",
                                "VK_i", "VK_j", "VK_k", "VK_l", "VK_m", "VK_n", "VK_o", "VK_p", "VK_q", "VK_r", "VK_s", "VK_t", "VK_u", "VK_v", "VK_w", "VK_x", "VK_y", "VK_z",
                                "VK_A", "VK_B", "VK_C", "VK_D", "VK_E", "VK_F", "VK_G", "VK_H", "VK_I", "VK_J", "VK_K", "VK_L", "VK_M", "VK_N", "VK_O", "VK_P", "VK_Q", "VK_R",
                                "VK_S", "VK_T", "VK_U", "VK_V", "VK_W", "VK_X", "VK_Y", "VK_Z", "VK_GRAVEACCENT", "VK_ACUTEACCENT", "VK_QUOTE", "VK_QUOTEDBL", "VK_EQUALS", "VK_MINUS", "VK_KP_MINUS",
                                "VK_PLUS", "VK_KP_PLUS", "VK_KP_MULTIPLY", "VK_ASTERISK", "VK_BACKSLASH", "VK_KP_DIVIDE", "VK_SLASH", "VK_KP_PERIOD", "VK_PERIOD", "VK_COLON",
                                "VK_COMMA", "VK_SEMICOLON", "VK_AMPERSAND", "VK_VERTICALBAR", "VK_HASH", "VK_AT", "VK_CARET", "VK_DOLLAR", "VK_POUND", "VK_EURO", "VK_PERCENT",
                                "VK_EXCLAIM", "VK_QUESTION", "VK_LEFTBRACE", "VK_RIGHTBRACE", "VK_LEFTBRACKET", "VK_RIGHTBRACKET", "VK_LEFTPAREN", "VK_RIGHTPAREN", "VK_LESS",
                                "VK_GREATER", "VK_UNDERSCORE", "VK_DEGREE", "VK_SECTION", "VK_TILDE", "VK_NEGATION", "VK_LSHIFT", "VK_RSHIFT", "VK_LALT", "VK_RALT", "VK_LCTRL", "VK_RCTRL",
                                "VK_LGUI", "VK_RGUI", "VK_ESCAPE", "VK_PRINTSCREEN", "VK_SYSREQ", "VK_INSERT", "VK_KP_INSERT", "VK_DELETE", "VK_KP_DELETE", "VK_BACKSPACE", "VK_HOME", "VK_KP_HOME", "VK_END", "VK_KP_END", "VK_PAUSE", "VK_BREAK",
                                "VK_SCROLLLOCK", "VK_NUMLOCK", "VK_CAPSLOCK", "VK_TAB", "VK_RETURN", "VK_KP_ENTER", "VK_APPLICATION", "VK_PAGEUP", "VK_KP_PAGEUP", "VK_PAGEDOWN", "VK_KP_PAGEDOWN", "VK_UP", "VK_KP_UP",
                                "VK_DOWN", "VK_KP_DOWN", "VK_LEFT", "VK_KP_LEFT", "VK_RIGHT", "VK_KP_RIGHT", "VK_KP_CENTER", "VK_F1", "VK_F2", "VK_F3", "VK_F4", "VK_F5", "VK_F6", "VK_F7", "VK_F8", "VK_F9", "VK_F10", "VK_F11", "VK_F12",
                                "VK_GRAVE_a", "VK_GRAVE_e", "VK_ACUTE_e", "VK_GRAVE_i", "VK_GRAVE_o", "VK_GRAVE_u", "VK_CEDILLA_c", "VK_ESZETT", "VK_UMLAUT_u",
                                "VK_UMLAUT_o", "VK_UMLAUT_a", "VK_CEDILLA_C", "VK_TILDE_n", "VK_TILDE_N", "VK_UPPER_a", "VK_ACUTE_a", "VK_ACUTE_i", "VK_ACUTE_o", "VK_ACUTE_u", "VK_UMLAUT_i", "VK_EXCLAIM_INV", "VK_QUESTION_INV",
                                "VK_ACUTE_A","VK_ACUTE_E","VK_ACUTE_I","VK_ACUTE_O","VK_ACUTE_U", "VK_GRAVE_A","VK_GRAVE_E","VK_GRAVE_I","VK_GRAVE_O","VK_GRAVE_U", "VK_INTERPUNCT", "VK_DIAERESIS",
                                "VK_UMLAUT_e", "VK_UMLAUT_A", "VK_UMLAUT_E", "VK_UMLAUT_I", "VK_UMLAUT_O", "VK_UMLAUT_U", "VK_CARET_a", "VK_CARET_e", "VK_CARET_i", "VK_CARET_o", "VK_CARET_u", "VK_CARET_A", "VK_CARET_E",
                                "VK_CARET_I", "VK_CARET_O", "VK_CARET_U", "VK_ASCII"};

    // Buscamos ahora el VK_
    int numitems = sizeof(VKTOSTR) / sizeof(VKTOSTR[0]);

    for (int i=0;i < numitems ;i++)
    {
        if (VKTOSTR[i] == trim(vk.c_str()))
        {
            return i;
            break;
        }
    }

    return 0;     
}

//------------------------------------------------------------------------------------------
// Keyboard mapping methods
//------------------------------------------------------------------------------------------
bool Keymap::keymapFileExists()
{
    FILE* fkeymapd = nullptr;
    // Open key.map file
    if (FileUtils::isSDReady())
    {   
        printf("Keymap file path: %s\n",(Keymap::keymapfile_path + Keymap::keymapfilename).c_str());
        std::string fkeymapd_path_str = Keymap::keymapfile_path + Keymap::keymapfilename;
        fkeymapd = fopen(fkeymapd_path_str.c_str(),"r");

        if (fkeymapd)
        {
            printf("> [%s] File found.\n",(Keymap::keymapfile_path + Keymap::keymapfilename).c_str());
            fclose(fkeymapd);
            return true;
        }
        printf("> [%s] File not found\n",(Keymap::keymapfile_path + Keymap::keymapfilename).c_str());
        return false;
    }
    else
    {
        printf("> [key.map] SD card not ready\n");
        return false;
    }
}

bool Keymap::getKeymapFromFile(fabgl::KeyboardLayout &kbdcustom)
{
    FILE* fkeymapd = nullptr;
    char sline[255];
    string strline = "";
    string strdata = "";
    string sc = "";
    string vk = "";
    string combo = "";
    size_t posdata = 0;

    uint8_t parsingstate = 0;

    // by default put nullptr
    kbdcustom.inherited = nullptr;

    // Get all keymap from configuration key.map file
    if(FileUtils::isSDReady())
    {
        // Open key.map file
        std::string fkeymapd_path_str = Keymap::keymapfile_path + Keymap::keymapfilename;
        fkeymapd = fopen(fkeymapd_path_str.c_str(),"r");

        //Now getting all data to keymap array
        int i=0;
        int j=0;
        int vkcount = 0;
        uint8_t section = 0; 

        while (!feof(fkeymapd))
        {
            // Get line from file to get the keycode
            fgets(sline, 255, fkeymapd);
            strline = sline;
            // Removing comments and other chars not used
            strline = trim(strline);

            //printf("> Trimmed line: %s\n",strline.c_str());
            // Parsing prepared line
            switch (parsingstate)
            {
                case 0:
                    // Get name
                    if (strline.find("name") != string::npos)
                    {
                        // Is the name of layout
                        if (strline.find(":") != string::npos)
                        {
                            posdata = strline.find(":");
                            strdata = trim(strline.substr(posdata+1, strline.length() - 1));
                            trim(strdata);

                            // Pass the name of layout
                            kbdcustom.name = strdata.c_str();
                            printf(" - Name of layout: %s\n",kbdcustom.name);
                        }
                        else
                        {
                            kbdcustom.name = "custom";
                        }
                                
                        parsingstate = 1;
                    }
                    break;
                
                case 1:
                    // Get description
                    if (strline.find("desc") != string::npos)
                    {
                        // Is the description of layout
                        if (strline.find(":") != string::npos)
                        {
                            posdata = strline.find(":");
                            strdata = trim(strline.substr(posdata+1, strline.length() - 1));
                            // Pass the description of layout
                            kbdcustom.desc = strdata.c_str();
                            printf(" - Description of layout: %s\n",kbdcustom.desc);
                        }
                        else
                        {
                            kbdcustom.desc = "custom layout";
                        }                
                        parsingstate = 2;
                    }                
                    break;
                
                case 2:
                    // Getting keyscan codes
                    if (strline.find("base=") != string::npos)
                    {
                        section = 1;
                        parsingstate = 3;
                    }   
                    else if (strline.find("extend=") != string::npos)
                    {
                        section = 2;
                        parsingstate = 3;
                    } 
                    else if (strline.find("extendjoy=") != string::npos) 
                    {
                        section = 3;
                        parsingstate = 3;
                    } 
                    else if (strline.find("virtual=") != string::npos)
                    {
                        section = 4;
                        parsingstate = 4;
                    }  
                    else if (strline.find("special=") != string::npos)
                    {
                        // to scancodetoVKCombo
                        section = 5;
                        parsingstate = 4;
                    } 
                    else
                    {}
                    break;
                
                case 3:
                    //
                    // Base section parsing
                    //
                    if (strline.find("{") == 0)
                    {
                        continue;
                    }
                    else if (strline.find("}") == 0)
                    {
                        // The section reaches end
                        printf(" - Total {%02u} VK read: %02u\n",section,vkcount);
                        vkcount = 0;
                        section = 0;
                        // return to getting another section
                        parsingstate = 2;
                    }
                    else
                    {
                        // Probably VK_ line?
                        if (strline.find(",") != string::npos)
                        {
                            size_t pos = strline.find(",");

                            vk = trim(strline.substr(pos+1));                           
                            sc = trim(strline.substr(0,pos));

                            if (section == 1)
                            {
                                kbdcustom.scancodeToVK[vkcount].scancode = std::stoul(sc.c_str(), nullptr, 16);      
                                kbdcustom.scancodeToVK[vkcount].virtualKey = fabgl::VirtualKey(searchScanCodeFromVK(vk.c_str()));
                            }
                            else if (section == 2)
                            {
                                kbdcustom.exScancodeToVK[vkcount].scancode = std::stoul(sc.c_str(), nullptr, 16);      
                                kbdcustom.exScancodeToVK[vkcount].virtualKey = fabgl::VirtualKey(searchScanCodeFromVK(vk.c_str()));
                            }
                            else if (section == 3)
                            {
                                kbdcustom.exJoyScancodeToVK[vkcount].scancode = std::stoul(sc.c_str(), nullptr, 16);      
                                kbdcustom.exJoyScancodeToVK[vkcount].virtualKey = fabgl::VirtualKey(searchScanCodeFromVK(vk.c_str()));
                            }

                            //printf(" -- VK: %s - %s\n",vk.c_str(),sc.c_str());           
                            vkcount++;
                        }
                    }                  
                    break;

                case 4:
                    // Combo keys (Virtual and Special)
                    //
                    //
                    if (strline.find("{") == 0)
                    {
                        continue;
                    }
                    else if (strline.find("}") == 0)
                    {
                        // The section reaches end
                        printf(" - Total {%02u} VK read: %02u\n",section,vkcount);
                        vkcount = 0;
                        section = 0;
                        // return to getting another section
                        parsingstate = 2;
                    }
                    else
                    {
                        // Probably VK_ line?
                        if (strline.find(",") != string::npos)
                        {
                            size_t pos = strline.find(",");
                            if (section == 4)
                            {
                                // Alternate section
                                // Get source VK
                                vk = trim(strline.substr(0,pos));
                                kbdcustom.alternateVK[vkcount].reqVirtualKey =fabgl::VirtualKey(searchScanCodeFromVK(vk.c_str()));
                            }
                            else if (section ==5)
                            {
                                // Special section
                                // Get source scancode
                                sc = trim(strline.substr(0,pos));
                                kbdcustom.scancodeToVKCombo[vkcount].scancode = std::stoul(sc.c_str(), nullptr, 16);
                            }

                            // Get combination special keys CTRL,LALT,RALT and SHIFT
                            size_t pos2 = strline.find_last_of(",");
                            combo = trim(strline.substr(pos+1,pos2-pos-1));

                            // Get destination VK_
                            vk = trim(strline.substr(pos2+1));                          
                            if (section == 4)
                            {
                                // Alternate section
                                kbdcustom.alternateVK[vkcount].virtualKey = fabgl::VirtualKey(searchScanCodeFromVK(vk.c_str()));
                            }
                            else if (section ==5)
                            {
                                // Special section
                                kbdcustom.scancodeToVKCombo[vkcount].virtualKey = fabgl::VirtualKey(searchScanCodeFromVK(vk.c_str()));
                            }


                            // -------------------------------------------------
                            //           Now parsing combo parameters
                            // -------------------------------------------------
                            pos = 0;
                            size_t lastpos = 0;
                            int combokey = 0;
                            string strcombokey = "";
                            
                            while (combokey < 4)
                            {
                                pos = combo.find_first_of(",",lastpos);
                                
                                if (pos != string::npos)
                                {
                                    strcombokey = trim(combo.substr(lastpos, pos-lastpos));
                                    lastpos = pos+1;

                                    switch (combokey)                                    
                                    {
                                        case 0:
                                            // CTRL
                                            if (section==4)
                                            {
                                                kbdcustom.alternateVK[vkcount].ctrl = atoi(strcombokey.c_str());                                    
                                            }
                                            else if (section==5)
                                            {
                                                kbdcustom.scancodeToVKCombo[vkcount].ctrl = atoi(strcombokey.c_str());                                    
                                            }
                                            break;
                                        case 1:
                                            // L-ALT
                                            if (section==4)
                                            {
                                                kbdcustom.alternateVK[vkcount].lalt = atoi(strcombokey.c_str());
                                            }
                                            else if (section==5)
                                            {
                                                kbdcustom.scancodeToVKCombo[vkcount].lalt = atoi(strcombokey.c_str());                                                
                                            }
                                            break;
                                        case 2:
                                            // R-ALT
                                            if (section==4)
                                            {
                                                kbdcustom.alternateVK[vkcount].ralt = atoi(strcombokey.c_str());
                                            }
                                            else if (section==5)
                                            {
                                                kbdcustom.scancodeToVKCombo[vkcount].ralt = atoi(strcombokey.c_str());                                                
                                            }
                                            break;                                                                                                                   
                                        default:
                                        break;
                                    }
                                    combokey++;
                                }
                                else
                                {
                                    // SHIFT
                                    strcombokey = trim(combo.substr(lastpos, combo.length()-lastpos));
                                    if (section==4)
                                    {
                                        kbdcustom.alternateVK[vkcount].shift = atoi(strcombokey.c_str());
                                    }
                                    else if (section==5)
                                    {
                                        kbdcustom.scancodeToVKCombo[vkcount].shift = atoi(strcombokey.c_str());                                                
                                    }
                                    
                                    //printf("SHIF %s\n",strcombokey.c_str());
                                    break;
                                }
                            }

                            //printf("Virtual [%002u]: %s - %s - %s\n",vkcount,sc.c_str(),combo.c_str(),vk.c_str());
                            vkcount++;
                        }
                    }      
                    break;

                default:
                    break;
            }
        
            j++;
        }

        fclose(fkeymapd);
        return true;
    }
    else
    {
        printf("> [key.map] SD card not ready\n");
        return false;
    }
}

void Keymap::activeKeyboardLayout(fabgl::KeyboardLayout &kbdcustom)
{
    printf("Base layout loaded\n");
    ESPeccy::PS2Controller.keyboard()->setLayout(&kbdcustom);
}

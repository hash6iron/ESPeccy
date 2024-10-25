"""

ESPeccy, a Sinclair ZX Spectrum emulator for Espressif ESP32 SoC

Copyright (c) 2024 Juan José Ponteprino [SplinterGU]
https://github.com/SplinterGU/ESPeccy

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

"""

import sys
import re

def text_to_bin(input_file, output_file):
    with open(input_file, "r") as f:
        # Leer el contenido del archivo
        content = f.read()

    # Extraer los números hexadecimales usando una expresión regular
    hex_numbers = re.findall(r'0x([0-9A-Fa-f]+)', content)

    with open(output_file, "wb") as f:
        for hex_number in hex_numbers:
            # Convertir cada número hexadecimal a un byte y escribirlo en el archivo
            f.write(int(hex_number, 16).to_bytes(1, byteorder='big'))

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Uso: python text_to_bin.py input_file.txt output_file.bin")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]

    text_to_bin(input_file, output_file)
    print(f"Archivo {output_file} creado con éxito.")


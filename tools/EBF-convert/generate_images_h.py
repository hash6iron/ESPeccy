import os
import subprocess
import binascii

# Configuración de archivos y nombres de variables
variables = [
    ("ESPectrum_logo", "espectrum-logo-canary-cropped.png", "espectrum-logo-cropped.png"),
    ("Layout_ZX", "Layout_ZX-canary.png", "Layout_ZX.png"),
    ("Layout_TK", "Layout_TK-canary.png", "Layout_TK.png"),
    ("ZX_Kbd", "ZX_Kbd-canary.png", "ZX_Kbd.png"),
    ("PS2_Kbd", "PS2_Kbd-canary.png", "PS2_Kbd.png"),
    ("Layout_ZX81", "Layout_ZX81-canary.png", "Layout_ZX81.png"),
]

# Función para ejecutar EBF-convert.py y generar archivos .ebf8
def generate_ebf8_files(png_file):
    subprocess.run(["python", "EBF-convert.py", png_file], check=True)

# Función para obtener el volcado hexadecimal del archivo .ebf8
def hex_dump(ebf8_file):
    with open(ebf8_file, "rb") as f:
        content = f.read()
    hex_data = binascii.hexlify(content).decode("utf-8")

    # Formatear la salida en bloques de 2 dígitos (bytes)
    byte_list = [f"0x{hex_data[i:i+2]}" for i in range(0, len(hex_data), 2)]

    # Insertar saltos de línea cada 12 bytes
    lines = []
    for i in range(0, len(byte_list), 12):
        line = ", ".join(byte_list[i:i + 12])
        lines.append(line)

    formatted_hex = ",\n    ".join(lines)  # Indentación opcional para alineación bonita
    return formatted_hex

# Generar el bloque de código C para cada variable
def generate_include(variable_name, canary_file, normal_file):
    # Generar los archivos .ebf8 para ambas versiones
    generate_ebf8_files(canary_file)
    generate_ebf8_files(normal_file)

    # Obtener el volcado hexadecimal para ambas versiones
    canary_hex = hex_dump(canary_file.replace(".png", ".ebf8"))
    normal_hex = hex_dump(normal_file.replace(".png", ".ebf8"))

    # Generar el bloque de código C
    return f"""
const uint8_t {variable_name}[] = {{
#ifdef CANARY_VERSION
    {canary_hex}
#else
    {normal_hex}
#endif
}};
"""

# Construir todo el include completo
def main():
    include = "#ifndef __IMAGES_H\n#define __IMAGES_H\n\n"

    for variable_name, canary_file, normal_file in variables:
        include += generate_include(variable_name, canary_file, normal_file)

    include += "\n#endif // __IMAGES_H\n"

    # Guardar el include generado en un archivo
    with open("images.h", "w") as f:
        f.write(include)

    print("Include generado exitosamente en 'images.h'.")

if __name__ == "__main__":
    main()

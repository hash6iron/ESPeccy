#include "Cheat.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Constructor por defecto
CheatParser::CheatParser() {}

// Destructor
CheatParser::~CheatParser() {
    clearData(); // Liberar memoria
}

// Función para cargar un archivo .pok
bool CheatParser::loadCheatFile(const std::string& filename) {
    FILE* file = fopen(filename.c_str(), "rb");
    if (!file) {
        printf("Error: Could not open file %s\n", filename.c_str());
        return false;
    }

    clearData(); // Limpiar datos antes de cargar un nuevo archivo
    parseCheatFile(file);

    fclose(file);
    return true;
}

// Función para liberar todos los datos
void CheatParser::clearData() {
    cheats.clear(); // Limpiar el vector de entrenadores
}

// Función para dividir la línea y obtener los tokens
std::vector<std::string> splitLine(const std::string& line) {
    std::vector<std::string> tokens;
    std::string currentToken;

    for (char ch : line) {
        if (std::isspace(ch)) { // Si encontramos un espacio
            if (!currentToken.empty()) { // Si el token actual no está vacío
                tokens.push_back(currentToken);
                currentToken.clear(); // Limpiar el token actual
            }
        } else {
            currentToken += ch; // Agregar el carácter al token actual
        }
    }

    // Agregar el último token si existe
    if (!currentToken.empty()) {
        tokens.push_back(currentToken);
    }

    return tokens;
}

// Función para parsear el archivo .pok
void CheatParser::parseCheatFile(FILE* file) {
    char line[100]; // Buffer para la línea
    Cheat currentCheat;

    while (fgets(line, sizeof(line), file)) {

        char *p = line; while( *p ) if ( *p == '\r' || *p == '\n' ) *p = 0; else p++;

        if (line[0] == 'N') {
            if (!currentCheat.name.empty()) {
                cheats.push_back(currentCheat);
                currentCheat = Cheat(); // Reiniciar el actual
            }
            currentCheat.name = std::string(line + 1);
            currentCheat.enabled = false;
            currentCheat.inputCount = 0; // Reiniciar contador
        } else if (line[0] == 'M' || line[0] == 'Z') {

            // Dividir la línea en tokens usando nuestra función
            std::vector<std::string> tokens = splitLine(line);

            Poke poke;
            poke.bank = std::stoi(tokens[1]); // Convertir el segundo token a int para el bank
            poke.address = std::stoi(tokens[2]); // Convertir el tercer token a int para la dirección
            poke.value = std::stoi(tokens[3]); // Convertir el cuarto token a int para el value
            poke.original = std::stoi(tokens[4]); // Convertir el quinto token a int para el original
            poke.userDefinedValue = (poke.value == 256) ? poke.original : poke.value; // Valor definido por el usuario

            currentCheat.pokes.push_back(poke);

            if (poke.value == 256) {
                currentCheat.inputCount++; // Incrementar contador si value es 256
            }
        } else if (line[0] == 'Y') {
            if (!currentCheat.name.empty()) {
                cheats.push_back(currentCheat);
            }
            break; // Salir del loop al encontrar 'Y'
        }
    }
}

// Función para obtener un puntero a un Cheat
Cheat* CheatParser::getCheat(int index) {
    if (index < 0 || index >= cheats.size()) {
        return nullptr; // Indice inválido
    }
    return &cheats[index]; // Retornar puntero al Cheat
}

// Función para obtener la cantidad de Cheats
int CheatParser::getCheatCount() const {
    return static_cast<int>(cheats.size());
}

// Método para obtener puntero a Cheate por índice
Poke* CheatParser::getPokeForInput(Cheat *cheat, size_t inputIndex) {
    size_t count = 0;

    for (auto& poke : cheat->pokes) {
        if (poke.value == 256) {
            if (count == inputIndex) {
                return &poke; // Retornar puntero al Cheate
            }
            count++;
        }
    }

    return nullptr; // No se encontró
}

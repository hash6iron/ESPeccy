#ifndef CHEAT_H
#define CHEAT_H

#include <string>
#include <vector>

// Estructura para almacenar un POKE
struct Poke {
    int bank;               // Banco
    int address;            // Dirección
    int value;              // Valor a modificar (POKE)
    int original;           // Valor original
    int userDefinedValue;   // Valor definido por el usuario (para casos donde value = 256)
};

// Estructura para almacenar un Entrenador (Cheat)
struct Cheat {
    std::string name;        // Nombre/Descripción del entrenador
    std::vector<Poke> pokes; // Lista de POKEs asociados
    bool enabled;            // Indicador de si el Cheat está habilitado
    int inputCount;          // Contador de POKEs con value = 256
};

// Clase para parsear archivos .pok
class CheatMngr {
public:
    static bool loadCheatFile(const std::string& filename);
    static void clearData();
    static Cheat* getCheat(int index);
    static int getCheatCount();
    static Poke* getPokeForInput(Cheat* cheat, size_t inputIndex);

    static inline int getInputCount(Cheat* cheat) {
        if ( !cheat ) return 0;
        return cheat->inputCount;
    }

private:
    static std::vector<Cheat> cheats; // Lista de entrenadores
    static void parseCheatFile(FILE* file);  // Método para parsear el archivo .pok
};

#endif // CheatMngr_H

#ifndef CHEAT_H
#define CHEAT_H

#include <string>
#include <vector>
#include <cstdint>

// Estructura para almacenar un POKE
#pragma pack(push, 1)
typedef struct Poke {
    uint8_t bank;               // Banco
    uint16_t address;           // Dirección
    uint8_t value;              // Valor a modificar (POKE)
    uint8_t original;           // Valor original
    union {
        struct {
            bool is_input : 1;  // Si el valor debe ser ingresado por el usuario
            bool orig_mem : 1;  // Indica si el valor original fue recuperado de la memoria
            bool orig_file : 1; // Indica si el valor original provino del archivo .pok
            uint8_t : 5;        // Bits sin usar para completar el byte
        };
        uint8_t flags;          // Acceso directo a los flags
    };
    uint8_t padding[2];         // Relleno manual para alinear a 8 bytes
};
#pragma pack(pop)

// Estructura para almacenar un Cheat
#pragma pack(push, 1)
typedef struct Cheat {
    uint32_t nameOffset;        // Offset del nombre (4 u 8 bytes según la plataforma)
    uint16_t pokeStartIdx;      // Índice de inicio de los POKEs en memoria alineada
    uint16_t pokeCount;         // Cantidad de POKEs asociados
    bool enabled;               // Cheat activado o no
    uint8_t inputCount;         // Cantidad de entradas necesarias
    uint8_t padding[2];         // Relleno para alinear a 32 bits
};
#pragma pack(pop)


// Clase para gestionar los Cheats
class CheatMngr {
public:
    static bool loadCheatFile(const std::string& filename);
    static void clearData();
    static void closeCheatFile();

    static const Cheat getCheat(int index);
    static const Cheat toggleCheat(int index);
    static std::string getCheatName(const Cheat& cheat);
    static std::string getCheatFilename();
    static uint16_t getCheatCount();
    static const Poke getPoke(const Cheat& cheat, size_t pokeIndex);
    static const Poke getInputPoke(const Cheat& cheat, size_t inputIndex);
    static const Poke setPokeValue(const Cheat& cheat, size_t pokeIndex, uint8_t value);

    static const Poke setPokeOriginal(const Cheat& cheat, size_t pokeIndex, uint8_t value);
    static const Poke setPokeOrigMemFetched(const Cheat& cheat, size_t pokeIndex, bool value);
    static const Poke setPokeOrigFileLoaded(const Cheat& cheat, size_t pokeIndex, bool value);
    static void clearAllPokeOrigMem();
    static void fetchCheatOriginalValuesFromMem();
    static void applyCheats();

private:
    static std::string cheatFilename;
    static FILE* cheatFileFP;

    static Cheat* cheats;         // Array de Cheats en memoria alineada
    static Poke* pokes;           // Array de POKEs en memoria alineada
    static uint16_t cheatCount;   // Total de Cheats
    static uint32_t pokeCount;    // Total de POKEs

    static inline void copyPoke(const Poke* src, Poke* dest) {
        const uint32_t* s = reinterpret_cast<const uint32_t*>(src);
        uint32_t* d = reinterpret_cast<uint32_t*>(dest);

        *d++ = *s++;  // Copia los primeros 4 bytes
        *d++ = *s++;  // Copia los segundos 4 bytes
    }

    static inline void copyCheat(const Cheat* src, Cheat* dest) {
        const uint32_t* s = reinterpret_cast<const uint32_t*>(src);
        uint32_t* d = reinterpret_cast<uint32_t*>(dest);

        *d++ = *s++;  // Copia los primeros 4 bytes (nameOffset)
        *d++ = *s++;  // Copia los siguientes 4 bytes (pokeStartIdx y pokeCount)
        *d++ = *s++;  // Copia los últimos 4 bytes (enabled, inputCount, padding)
    }

};

#endif // CheatMngr_H

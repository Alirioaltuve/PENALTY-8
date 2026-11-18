// Sin cambios funcionales: las 99 palabras siguen siendo válidas

#ifndef PROGRAMA_STRIKER_H
#define PROGRAMA_STRIKER_H

#include <cstdint>
#include <cstddef>


//  Programa maquina: 99 palabras de 16 bits.
static const uint16_t programa_striker[] = {
    /* 00: */ 0x5000, 0x5101, 0x5202, 0x5303, 0x46EA, 0x3078, 0x680A, 0x40E7,
    /* 08: */ 0x41E8, 0x42E9, 0x47E0, 0x301C, 0x900E, 0x08E0, 0x47E1, 0x303C,
    /* 16: */ 0x9012, 0x09E0, 0x47E2, 0x305C, 0x9016, 0x0AE0, 0x0C40, 0x1484,
    /* 24: */ 0x47E2, 0x309C, 0x901C, 0x0CE0, 0x42E3, 0x3008, 0x7822, 0x0E40,
    /* 32: */ 0x1EC0, 0x6024, 0x0E00, 0x1EC8, 0x42E4, 0x3028, 0x782A, 0x0F40,
    /* 40: */ 0x1FE4, 0x602C, 0x0F20, 0x1FE8, 0x45E5, 0x15B8, 0x15BC, 0x42E2,
    /* 48: */ 0x1A50, 0x15A8, 0x4810, 0x4911, 0x4D12, 0x4C13, 0x4E14, 0x4F15,
    /* 56: */ 0x42E6, 0x4A16, 0x3A00, 0x4A38, 0x42E6, 0x5A19, 0x581A, 0x591B,
    /* 64: */ 0x5D1C, 0x5C1D, 0x3A01, 0x5A10, 0x3B40, 0x3E00, 0x5211, 0x3058,
    /* 72: */ 0x704F, 0x3E01, 0x1B78, 0x3E00, 0x3078, 0x7046, 0x6062, 0x521E,
    /* 80: */ 0x4A30, 0x521F, 0x4A31, 0x5220, 0x4A32, 0x5221, 0x4A33, 0x5222,
    /* 88: */ 0x4A34, 0x5223, 0x4A35, 0x5217, 0x4A36, 0x5218, 0x4A37, 0x3A01,
    /* 96: */ 0x4A38, 0x5A11, 0x9800
};

/// Numero de palabras del programa.
static const size_t TAMANO_PROGRAMA_STRIKER =
    sizeof(programa_striker) / sizeof(programa_striker[0]);

/// Direcciones de los resultados dentro de la memoria de datos.
enum DireccionResultadoStriker {
    RES_X  = 0x10, ///< coordenada X final
    RES_Y  = 0x11, ///< coordenada Y final
    RES_T  = 0x12, ///< tiempo de vuelo
    RES_P  = 0x13, ///< potencia final
    RES_DX = 0x14, ///< distancia horizontal al centro
    RES_DY = 0x15, ///< distancia vertical al centro
    RES_ID = 0x16  ///< identificador del mensaje (0xA1)
};

/// Direcciones donde el programa guarda la respuesta del portero.
enum DireccionRespuestaStriker {
    RES_RESPUESTA_0 = 0x30, ///< respuesta byte 0
    RES_RESPUESTA_1 = 0x31, ///< respuesta byte 1
    RES_RESPUESTA_2 = 0x32, ///< respuesta byte 2
    RES_RESPUESTA_3 = 0x33, ///< respuesta byte 3
    RES_RESPUESTA_4 = 0x34, ///< respuesta byte 4
    RES_RESPUESTA_5 = 0x35, ///< respuesta byte 5
    RES_TICKS_L     = 0x36, ///< reaction_ticks, byte bajo
    RES_TICKS_H     = 0x37, ///< reaction_ticks, byte alto
    RES_ESTADO      = 0x38  ///< 0 = timeout, 1 = respuesta recibida
};

/// Identificador del mensaje STRIKER-8 -> GOALKEEPER-8.
static const unsigned ID_MENSAJE_STRIKER = 0xA1;

/// Tamano de la peticion y de la respuesta del protocolo.
static const unsigned TAMANO_PETICION_STRIKER  = 5;
static const unsigned TAMANO_RESPUESTA_STRIKER = 6;

/// Puertos de entrada del usuario.
enum PuertoEntradaStriker {
    PUERTO_X        = 0x00,
    PUERTO_Y        = 0x01,
    PUERTO_POTENCIA = 0x02,
    PUERTO_MODO     = 0x03
};

/// Puertos del buzon (0x10..0x25).
enum PuertoMailboxStriker {
    MB_REQ      = 0x10, ///< W: 1 inicia la peticion.  R: transaccion activa
    MB_ACK      = 0x11, ///< R: respuesta completa.  W: 1 confirma y limpia ACK
    MB_STATE    = 0x12, ///< R: bit0 busy, bit1 pendiente, bit2 listo, bit3 error
    MB_REQ_IDX  = 0x13, ///< W/R: indice 0..4 para REQ_DATA
    MB_REQ_DATA = 0x14, ///< W/R: byte de peticion seleccionado
    MB_RESP_IDX = 0x15, ///< W/R: indice 0..5 para RESP_DATA
    MB_RESP_DATA= 0x16, ///< R: byte de respuesta seleccionado
    MB_TICKS_L  = 0x17, ///< R: reaction_ticks, byte bajo
    MB_TICKS_H  = 0x18, ///< R: reaction_ticks, byte alto
    MB_REQ_0    = 0x19, ///< W/R: peticion byte 0
    MB_REQ_1    = 0x1A, ///< W/R: peticion byte 1
    MB_REQ_2    = 0x1B, ///< W/R: peticion byte 2
    MB_REQ_3    = 0x1C, ///< W/R: peticion byte 3
    MB_REQ_4    = 0x1D, ///< W/R: peticion byte 4
    MB_RESP_0   = 0x1E, ///< R: respuesta byte 0
    MB_RESP_1   = 0x1F, ///< R: respuesta byte 1
    MB_RESP_2   = 0x20, ///< R: respuesta byte 2
    MB_RESP_3   = 0x21, ///< R: respuesta byte 3
    MB_RESP_4   = 0x22, ///< R: respuesta byte 4
    MB_RESP_5   = 0x23, ///< R: respuesta byte 5
    MB_CLEAR    = 0x24, ///< W: 1 limpia todo el buzon
    MB_IRQ      = 0x25  ///< R: 1 si hay respuesta lista
};

/// Constante del banco de constantes del programa.
struct ConstanteStriker {
    uint8_t direccion;
    uint8_t valor;
};

/// Constantes que el programa espera encontrar en la memoria de datos.
static const ConstanteStriker constantes_striker[] = {
    { 0xE0, 31  }, // MAX_X
    { 0xE1, 15  }, // MAX_Y
    { 0xE2, 100 }, // MAX_P
    { 0xE3, 16  }, // CENTRO_X
    { 0xE4, 8   }, // CENTRO_Y
    { 0xE5, 20  }, // BASE_T
    { 0xE6, 0xA1}, // ID del mensaje
    { 0xE7, 28  }, // AUTO_X (esquina automatica)
    { 0xE8, 12  }, // AUTO_Y
    { 0xE9, 90  }, // AUTO_P
    { 0xEA, 0   }  // CERO
};

/// Numero de constantes del banco.
static const size_t NUM_CONSTANTES_STRIKER =
    sizeof(constantes_striker) / sizeof(constantes_striker[0]);

#endif // PROGRAMA_STRIKER_H

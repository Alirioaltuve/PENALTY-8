#ifndef GOALKEEPER_PROGRAM_H
#define GOALKEEPER_PROGRAM_H

#include <systemc.h>
#include <vector>
#include "isa.h"

// ============================================================
// Programa de GOALKEEPER-8
// Construye el programa que ejecuta el portero. La logica del
// portero vive aqui (en forma de instrucciones), NO dentro de la
// Unidad de Control ni de la ALU.
//
// Estrategia del portero:
//   - Se queda esperando a que REQ (0x15) se active.
//   - Lee los cinco bytes del disparo (0x10..0x14).
//   - Guarda el shot_id en memoria de datos (0x00).
//   - dive_x = target_x, dive_y = target_y.
//   - reach_x = reach_y = 64 (alcance fijo).
//   - Realiza operaciones aritmeticas y de comparacion de ejemplo.
//   - Escribe la respuesta en 0x20..0x25.
//   - Activa ACK escribiendo en 0x16.
//   - Espera a que REQ baje y vuelve a empezar.
//
// reaction_ticks (0x23) lo sobrescribe el CommunicationMailbox con
// el tiempo medido real; el programa solo escribe un marcador.
// ============================================================

namespace goalkeeper_program
{

// Direcciones del mapa de E/S (lado GOALKEEPER)
const unsigned int IO_REQ       = 0x15;
const unsigned int IO_ID        = 0x10;
const unsigned int IO_TARGET_X  = 0x11;
const unsigned int IO_TARGET_Y  = 0x12;
const unsigned int IO_POWER     = 0x13;
const unsigned int IO_FLIGHT    = 0x14;

const unsigned int IO_ACK       = 0x16;
const unsigned int IO_OUT_ID    = 0x20;
const unsigned int IO_DIVE_X    = 0x21;
const unsigned int IO_DIVE_Y    = 0x22;
const unsigned int IO_REACTION  = 0x23;
const unsigned int IO_REACH_X   = 0x24;
const unsigned int IO_REACH_Y   = 0x25;

// Direcciones internas de memoria de datos
const unsigned int MEM_ID       = 0x00;
const unsigned int MEM_SUMA     = 0x01;

// Etiquetas (indices absolutos de instruccion)
enum Etiquetas
{
    L_INICIO       = 0,
    L_ESPERAR      = 1,
    L_SALTAR       = 19,
    L_ESPERAR_BAJO = 27
};

inline std::vector<sc_uint<16>> construir()
{
    using namespace isa;
    std::vector<sc_uint<16>> p;

    // 0: R7 = 0 (registro cero del programa)
    p.push_back(formato_i(LDI, 7, 0));

    // 1: esperar: R0 = REQ
    p.push_back(formato_i(IN, 0, IO_REQ));
    // 2: CMP R0, R7  (R0 - 0)
    p.push_back(formato_r(CMP, 0, 0, 7));
    // 3: JEQ esperar  (mientras REQ == 0)
    p.push_back(formato_j(JEQ, L_ESPERAR));

    // 4-8: leer los cinco bytes del disparo
    p.push_back(formato_i(IN, 0, IO_ID));       // 4  R0 = shot_id
    p.push_back(formato_i(IN, 1, IO_TARGET_X)); // 5  R1 = target_x
    p.push_back(formato_i(IN, 2, IO_TARGET_Y)); // 6  R2 = target_y
    p.push_back(formato_i(IN, 3, IO_POWER));    // 7  R3 = power
    p.push_back(formato_i(IN, 4, IO_FLIGHT));   // 8  R4 = flight_ticks

    // 9: guardar shot_id en memoria de datos
    p.push_back(formato_i(ST, 0, MEM_ID));      // 9  mem[0] = R0

    // 10-11: dive = target
    p.push_back(formato_r(MOV, 5, 1, 0));       // 10 R5 = R1 (dive_x)
    p.push_back(formato_r(MOV, 6, 2, 0));       // 11 R6 = R2 (dive_y)

    // 12-13: alcance fijo
    p.push_back(formato_i(LDI, 3, 64));         // 12 R3 = 64 (reach_x)
    p.push_back(formato_i(LDI, 4, 64));         // 13 R4 = 64 (reach_y)

    // 14: operacion aritmetica de ejemplo
    p.push_back(formato_r(ADD, 0, 1, 2));       // 14 R0 = R1 + R2
    // 15: escritura de ejemplo en memoria de datos
    p.push_back(formato_i(ST, 0, MEM_SUMA));    // 15 mem[1] = R0

    // 16-18: comparacion y salto condicional de ejemplo
    p.push_back(formato_r(CMP, 0, 0, 7));       // 16 CMP R0, R7
    p.push_back(formato_j(JGE, L_SALTAR));      // 17 JGE saltar
    p.push_back(formato_i(LDI, 0, 0xFF));       // 18 (se omite si salta)

    // 19: saltar: recuperar shot_id de memoria
    p.push_back(formato_i(LD, 0, MEM_ID));      // 19 R0 = mem[0]

    // 20-26: escribir la respuesta
    p.push_back(formato_i(OUT, 0, IO_OUT_ID));  // 20 shot_id
    p.push_back(formato_i(OUT, 5, IO_DIVE_X));  // 21 dive_x
    p.push_back(formato_i(OUT, 6, IO_DIVE_Y));  // 22 dive_y
    p.push_back(formato_i(OUT, 7, IO_REACTION));// 23 reaction (lo sobrescribe mailbox)
    p.push_back(formato_i(OUT, 3, IO_REACH_X)); // 24 reach_x
    p.push_back(formato_i(OUT, 4, IO_REACH_Y)); // 25 reach_y
    p.push_back(formato_i(OUT, 7, IO_ACK));     // 26 ACK (evento de escritura)

    // 27: esperar_bajo: R0 = REQ
    p.push_back(formato_i(IN, 0, IO_REQ));
    // 28: CMP R0, R7
    p.push_back(formato_r(CMP, 0, 0, 7));
    // 29: JNE esperar_bajo  (mientras REQ != 0)
    p.push_back(formato_j(JNE, L_ESPERAR_BAJO));

    // 30: volver al inicio
    p.push_back(formato_j(JMP, L_INICIO));

    return p;
}

// Programa minimo para probar HALT: R0 = 42; HALT
inline std::vector<sc_uint<16>> construir_halt()
{
    using namespace isa;
    std::vector<sc_uint<16>> p;
    p.push_back(formato_i(LDI, 0, 42));
    p.push_back(formato_i(HALT, 0, 0));
    return p;
}

} // namespace goalkeeper_program

#endif // GOALKEEPER_PROGRAM_H

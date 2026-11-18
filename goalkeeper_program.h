#ifndef GOALKEEPER_PROGRAM_H
#define GOALKEEPER_PROGRAM_H

#include <systemc.h>
#include <vector>
#include <string>
#include <map>
#include <utility>
#include "isa.h"

// ============================================================
// Programa de GOALKEEPER-8
//
// Estrategia determinista, solo con instrucciones de la ISA:
//   - dive_x = target_x + 16 si shot_id es impar; target_x - 16 si par.
//   - dive_y = target_y + 16 si flight_ticks < 128; target_y si >= 128.
//   - reach  = 16 + 8 si power >= 128 - 8 si flight_ticks < 128.
//   - reach_x = reach_y = reach.
//   - shot_id se conserva en memoria de datos y se devuelve como eco.
//   - reaction_ticks y ACK los gestiona el CommunicationMailbox.
//
// Coordenadas validas de porteria: target_x/y en [32,223]; el sesgo
// de +-16 no desborda el byte en ese rango.
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

inline std::vector<sc_uint<16>> construir()
{
    using namespace isa;

    std::vector<sc_uint<16>> p;
    std::map<std::string, int> etiquetas;
    std::vector<std::pair<int, std::string>> parches;

    auto marcar = [&](const std::string& nombre) {
        etiquetas[nombre] = (int)p.size();
    };

    auto emitir = [&](sc_uint<16> instr) {
        p.push_back(instr);
    };

    auto saltar = [&](Opcode op, const std::string& destino) {
        int idx = (int)p.size();
        p.push_back(formato_j(op, 0));
        parches.push_back(std::make_pair(idx, destino));
    };

    // 0: R7 = 0 (registro cero del programa)
    emitir(formato_i(LDI, 7, 0));

    // 1: esperar: R0 = REQ
    marcar("INICIO");
    emitir(formato_i(IN, 0, IO_REQ));
    // 2: CMP R0, R7
    emitir(formato_r(CMP, 0, 0, 7));
    // 3: JEQ esperar
    saltar(JEQ, "INICIO");

    // 4-8: leer los cinco bytes de la solicitud
    emitir(formato_i(IN, 0, IO_ID));
    emitir(formato_i(IN, 1, IO_TARGET_X));
    emitir(formato_i(IN, 2, IO_TARGET_Y));
    emitir(formato_i(IN, 3, IO_POWER));
    emitir(formato_i(IN, 4, IO_FLIGHT));

    // 9: guardar shot_id en memoria de datos
    emitir(formato_i(ST, 0, MEM_ID));

    // 10-18: dive_x = target_x + 16 (id impar) / target_x - 16 (id par)
    emitir(formato_i(LDI, 5, 1));
    emitir(formato_r(AND, 0, 0, 5));      // R0 = shot_id & 1
    emitir(formato_r(CMP, 0, 0, 7));
    saltar(JEQ, "DX_PAR");
    emitir(formato_i(LDI, 5, 16));
    emitir(formato_r(ADD, 5, 1, 5));      // R5 = target_x + 16
    saltar(JMP, "DX_FIN");
    marcar("DX_PAR");
    emitir(formato_i(LDI, 5, 16));
    emitir(formato_r(SUB, 5, 1, 5));      // R5 = target_x - 16
    marcar("DX_FIN");

    // 19-25: dive_y = target_y + 16 si flight < 128; si no target_y
    emitir(formato_i(LDI, 0, 128));
    emitir(formato_r(CMP, 0, 4, 0));      // flight vs 128
    saltar(JGE, "DY_LENTO");
    emitir(formato_i(LDI, 6, 16));
    emitir(formato_r(ADD, 6, 2, 6));      // R6 = target_y + 16
    saltar(JMP, "DY_FIN");
    marcar("DY_LENTO");
    emitir(formato_r(MOV, 6, 2, 0));      // R6 = target_y
    marcar("DY_FIN");

    // 26-37: reach = 16; +8 si power >= 128; -8 si flight < 128
    emitir(formato_i(LDI, 0, 16));        // R0 = reach
    emitir(formato_i(LDI, 7, 128));
    emitir(formato_r(CMP, 0, 3, 7));      // power vs 128
    saltar(JLT, "POW_BAJO");
    emitir(formato_i(LDI, 7, 8));
    emitir(formato_r(ADD, 0, 0, 7));      // reach += 8
    marcar("POW_BAJO");
    emitir(formato_i(LDI, 7, 128));
    emitir(formato_r(CMP, 0, 4, 7));      // flight vs 128
    saltar(JGE, "FLIGHT_OK");
    emitir(formato_i(LDI, 7, 8));
    emitir(formato_r(SUB, 0, 0, 7));      // reach -= 8
    marcar("FLIGHT_OK");
    emitir(formato_i(LDI, 7, 0));         // restaurar cero

    // 38-45: respuesta por OUT (el mailbox completa reaction/ACK)
    emitir(formato_i(OUT, 0, IO_REACH_X));
    emitir(formato_i(OUT, 0, IO_REACH_Y));
    emitir(formato_i(LD, 0, MEM_ID));     // shot_id
    emitir(formato_i(OUT, 0, IO_OUT_ID));
    emitir(formato_i(OUT, 5, IO_DIVE_X));
    emitir(formato_i(OUT, 6, IO_DIVE_Y));
    emitir(formato_i(OUT, 7, IO_REACTION)); // marcador; mailbox lo ignora
    emitir(formato_i(OUT, 7, IO_ACK));      // evento ACK

    // 46-49: esperar a que REQ baje y volver al inicio
    marcar("ESPERAR_BAJO");
    emitir(formato_i(IN, 0, IO_REQ));
    emitir(formato_r(CMP, 0, 0, 7));
    saltar(JNE, "ESPERAR_BAJO");
    saltar(JMP, "INICIO");

    // Resolver destinos de los saltos con las etiquetas reales.
    for (const auto& par : parches)
    {
        int destino = etiquetas.at(par.second);
        p[par.first].range(10, 3) = destino & 0xFF;
    }

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

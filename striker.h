// Actualizado con registros de pipeline y decodificación compartida

#ifndef STRIKER_H
#define STRIKER_H

#include <systemc.h>
#include <cstdint>
#include <cstddef>

//  Definiciones de la ISA (5 bits de opcode, instrucciones de 16 bits)
namespace isa8 {

/// Codigos de operacion de 5 bits.
enum CodigoOperacion {
    OP_NOP = 0x00, ///< sin operacion
    OP_MOV = 0x01, ///< R: Rd <- Rs1
    OP_ADD = 0x02, ///< R: Rd <- Rs1 + Rs2
    OP_SUB = 0x03, ///< R: Rd <- Rs1 - Rs2
    OP_AND = 0x04, ///< R: Rd <- Rs1 AND Rs2
    OP_OR  = 0x05, ///< R: Rd <- Rs1 OR Rs2
    OP_CMP = 0x06, ///< R: actualiza Z y C con Rs1 - Rs2
    OP_LDI = 0x07, ///< I: Rd <- imm8
    OP_LD  = 0x08, ///< I: Rd <- mem[dir8]
    OP_ST  = 0x09, ///< I: mem[dir8] <- Rd
    OP_IN  = 0x0A, ///< I: Rd <- puerto8
    OP_OUT = 0x0B, ///< I: puerto8 <- Rd
    OP_JMP = 0x0C, ///< J: PC <- dir8
    OP_JEQ = 0x0D, ///< J: si Z, PC <- dir8
    OP_JNE = 0x0E, ///< J: si !Z, PC <- dir8
    OP_JGE = 0x0F, ///< J: si C, PC <- dir8
    OP_JLT = 0x10, ///< J: si !C, PC <- dir8
    OP_JGT = 0x11, ///< J: si C && !Z, PC <- dir8
    OP_JLE = 0x12, ///< J: si !C || Z, PC <- dir8
    OP_HALT = 0x13 ///< detiene la CPU
};

/// Opcode reservado: no es una instruccion valida.
static const unsigned OP_RESERVADO = 0x14;

/// Codigos de operacion de la ALU (3 bits).
enum OperacionALU {
    ALU_ADD = 0, ///< 000: suma
    ALU_SUB = 1, ///< 001: resta
    ALU_AND = 2, ///< 010: AND
    ALU_OR  = 3, ///< 011: OR
    ALU_CMP = 4  ///< 100: comparacion (solo banderas)
};

/// Condiciones de salto (3 bits).
enum Condicion {
    COND_SIEMPRE = 0,
    COND_EQ = 1,
    COND_NE = 2,
    COND_GE = 3,
    COND_LT = 4,
    COND_GT = 5,
    COND_LE = 6
};

//  Se agrupan en una estructura para poder copiarlas completas a los
//  registros de pipeline (ID/EX y EX/MEM) sin olvidar ninguna.
struct ControlSenales {
    unsigned operacion_alu;    ///< codigo de 3 bits para la ALU
    unsigned condicion;        ///< condicion de salto
    bool escribe_registro;     ///< hay escritura en el banco
    bool lee_memoria;          ///< LD
    bool escribe_memoria;      ///< ST
    bool es_entrada;           ///< IN
    bool es_salida;            ///< OUT
    bool es_salto;             ///< JMP / Jcc
    bool es_halt;              ///< HALT
    bool es_mov;               ///< MOV
    bool es_literal;           ///< LDI
    bool actualiza_banderas;   ///< ADD/SUB/AND/OR/CMP
    bool instruccion_invalida; ///< opcode fuera de la ISA
};

// 	Decodificador compartido por la UnidadControl y el pipeline
inline ControlSenales decodificar_control(unsigned op) {
    ControlSenales s;
    s.operacion_alu = ALU_ADD;
    s.condicion = COND_SIEMPRE;
    s.escribe_registro = false;
    s.lee_memoria = false;
    s.escribe_memoria = false;
    s.es_entrada = false;
    s.es_salida = false;
    s.es_salto = false;
    s.es_halt = false;
    s.es_mov = false;
    s.es_literal = false;
    s.actualiza_banderas = false;
    s.instruccion_invalida = false;

    switch (op) {
        case OP_NOP:
            break;

        case OP_MOV:
            s.escribe_registro = true;
            s.es_mov = true;
            break;

        case OP_ADD:
            s.escribe_registro = true;
            s.actualiza_banderas = true;
            s.operacion_alu = ALU_ADD;
            break;

        case OP_SUB:
            s.escribe_registro = true;
            s.actualiza_banderas = true;
            s.operacion_alu = ALU_SUB;
            break;

        case OP_AND:
            s.escribe_registro = true;
            s.actualiza_banderas = true;
            s.operacion_alu = ALU_AND;
            break;

        case OP_OR:
            s.escribe_registro = true;
            s.actualiza_banderas = true;
            s.operacion_alu = ALU_OR;
            break;

        case OP_CMP:
            s.actualiza_banderas = true;
            s.operacion_alu = ALU_CMP;
            break;

        case OP_LDI:
            s.escribe_registro = true;
            s.es_literal = true;
            break;

        case OP_LD:
            s.escribe_registro = true;
            s.lee_memoria = true;
            break;

        case OP_ST:
            s.escribe_memoria = true;
            break;

        case OP_IN:
            s.escribe_registro = true;
            s.es_entrada = true;
            break;

        case OP_OUT:
            s.es_salida = true;
            break;

        case OP_JMP:
            s.es_salto = true;
            s.condicion = COND_SIEMPRE;
            break;

        case OP_JEQ:
            s.es_salto = true;
            s.condicion = COND_EQ;
            break;

        case OP_JNE:
            s.es_salto = true;
            s.condicion = COND_NE;
            break;

        case OP_JGE:
            s.es_salto = true;
            s.condicion = COND_GE;
            break;

        case OP_JLT:
            s.es_salto = true;
            s.condicion = COND_LT;
            break;

        case OP_JGT:
            s.es_salto = true;
            s.condicion = COND_GT;
            break;

        case OP_JLE:
            s.es_salto = true;
            s.condicion = COND_LE;
            break;

        case OP_HALT:
            s.es_halt = true;
            break;

        default:
            s.instruccion_invalida = true;
            break;
    }
    return s;
}

/// Resultado combinacional de la ALU.
struct ResultadoALU {
    unsigned resultado;
    bool cero;
    bool acarreo;
};

/// Evaluacion de la ALU compartida por el modulo ALU8 y por el pipeline.
inline ResultadoALU evaluar_alu(unsigned a, unsigned b, unsigned op) {
    ResultadoALU r;
    r.resultado = 0;
    r.acarreo = false;

    switch (op) {
        case ALU_ADD: {
            const unsigned suma = a + b;
            r.resultado = suma & 0xFFu;
            r.acarreo = (suma > 0xFFu);
            break;
        }
        case ALU_SUB:
            r.resultado = (a - b) & 0xFFu;
            r.acarreo = (a >= b);
            break;
        case ALU_AND:
            r.resultado = a & b;
            r.acarreo = false;
            break;
        case ALU_OR:
            r.resultado = a | b;
            r.acarreo = false;
            break;
        case ALU_CMP:
            r.resultado = (a - b) & 0xFFu;
            r.acarreo = (a >= b);
            break;
        default:
            r.resultado = a;
            r.acarreo = false;
            break;
    }

    r.cero = (r.resultado == 0);
    return r;
}

//  Formato de instruccion de 16 bits

//  Formato R (registro a registro):
//    [15:11] opcode   [10:8] Rd   [7:5] Rs1   [4:2] Rs2   [1:0] 00

//  Formato I (inmediato, memoria o puerto):
//    [15:11] opcode   [10:8] Rd/Rs   [7:0] imm8 / dir8 / puerto8

//  Formato J (salto):
//    [15:11] opcode   [10:8] 000   [7:0] dir8 (direccion de palabra)
inline unsigned codigo(uint16_t instruccion) {
    return (instruccion >> 11) & 0x1Fu;
}
inline unsigned campo_d(uint16_t instruccion) {
    return (instruccion >> 8) & 0x07u;
}
inline unsigned campo_s1(uint16_t instruccion) {
    return (instruccion >> 5) & 0x07u;
}
inline unsigned campo_s2(uint16_t instruccion) {
    return (instruccion >> 2) & 0x07u;
}
inline unsigned inmediato(uint16_t instruccion) {
    return instruccion & 0xFFu;
}

//  Codificadores (utiles para el programa, las pruebas y la documentacion)
/// Formato R: opcode | Rd | Rs1 | Rs2 | 00.
inline uint16_t codificar_r(unsigned op, unsigned rd, unsigned rs1, unsigned rs2) {
    return static_cast<uint16_t>(((op & 0x1Fu) << 11) |
                                 ((rd & 0x07u) << 8) |
                                 ((rs1 & 0x07u) << 5) |
                                 ((rs2 & 0x07u) << 2));
}

/// Formato I: opcode | Rd/Rs | imm8 / dir8 / puerto8.
inline uint16_t codificar_i(unsigned op, unsigned r, unsigned imm) {
    return static_cast<uint16_t>(((op & 0x1Fu) << 11) |
                                 ((r & 0x07u) << 8) |
                                 (imm & 0xFFu));
}

/// Formato J: opcode | 000 | dir8 (direccion de palabra).
inline uint16_t codificar_j(unsigned op, unsigned addr) {
    return static_cast<uint16_t>(((op & 0x1Fu) << 11) |
                                 (addr & 0xFFu));
}

/// Indica si el opcode pertenece a la ISA v1.0.
inline bool es_valida(unsigned op) {
    return op <= OP_HALT;
}

/// Formato R: MOV, ADD, SUB, AND, OR, CMP.
inline bool formato_r(unsigned op) {
    return op >= OP_MOV && op <= OP_CMP;
}

/// Formato I: LDI, LD, ST, IN, OUT.
inline bool formato_i(unsigned op) {
    return op >= OP_LDI && op <= OP_OUT;
}

// Formato J: JMP y saltos condicionales.
inline bool formato_j(unsigned op) {
    return op >= OP_JMP && op <= OP_JLE;
}

/// Nombre legible de un codigo de operacion.
const char* nombre_instruccion(unsigned op);

} // namespace isa8

//  ALU8 - Unidad aritmetico-logica de 8 bits
//  3 bits de operacion: ADD, SUB, AND, OR, CMP.
//  Salidas: resultado de 8 bits, Z (cero) y C (acarreo/prestamo).
SC_MODULE(ALU8) {
    sc_in<sc_uint<8>>  a;          ///< operando A
    sc_in<sc_uint<8>>  b;          ///< operando B
    sc_in<sc_uint<3>>  operacion;  ///< codigo de operacion de la ALU
    sc_out<sc_uint<8>> resultado;  ///< resultado truncado a 8 bits
    sc_out<bool>       cero;       ///< Z: resultado igual a cero
    sc_out<bool>       acarreo;    ///< C: acarreo (ADD) o A>=B (SUB/CMP)

    SC_CTOR(ALU8) {
        SC_METHOD(calcular);
        sensitive << a << b << operacion;
    }

    /// Evaluacion combinacional de la ALU.
    void calcular();

    /// Evaluacion compartida con el pipeline (sin senales SystemC).
    static isa8::ResultadoALU evaluar(unsigned a, unsigned b, unsigned op);
};

//  UnidadControl - decodificador de instrucciones de 5 bits
SC_MODULE(UnidadControl) {
    sc_in<sc_uint<5>>  codigo_operacion;      ///< opcode de la instruccion
    sc_out<sc_uint<3>> operacion_alu;         ///< operacion para la ALU
    sc_out<sc_uint<3>> condicion;             ///< condicion de salto

    sc_out<bool> escribe_registro;      ///< hay escritura de registro
    sc_out<bool> lee_memoria;           ///< LD
    sc_out<bool> escribe_memoria;       ///< ST
    sc_out<bool> es_entrada;            ///< IN
    sc_out<bool> es_salida;             ///< OUT
    sc_out<bool> es_salto;              ///< JMP / Jcc
    sc_out<bool> es_halt;               ///< HALT
    sc_out<bool> es_mov;                ///< MOV
    sc_out<bool> es_literal;            ///< LDI
    sc_out<bool> actualiza_banderas;    ///< ADD/SUB/AND/OR/CMP
    sc_out<bool> instruccion_invalida;  ///< opcode reservado o no valido

    SC_CTOR(UnidadControl) {
        SC_METHOD(decodificar);
        sensitive << codigo_operacion;
    }

    /// Decodificacion combinacional.
    void decodificar();
};

//  BancoRegistros - 8 registros de 8 bits (R0..R7)
//  Escritura sincrona; lectura mediante valor() para el secuenciador.
SC_MODULE(BancoRegistros) {
    sc_in<bool>        clk;
    sc_in<bool>        escritura;            ///< habilitacion de escritura
    sc_in<sc_uint<3>>  direccion_escritura;  ///< registro destino (0..7)
    sc_in<sc_uint<8>>  dato_escritura;       ///< dato a guardar

    SC_CTOR(BancoRegistros) {
        for (int i = 0; i < 8; ++i) registros[i] = 0;
        SC_METHOD(escribir);
        sensitive << clk.pos();
        dont_initialize();
    }

    /// Escritura sincrona en el flanco de subida.
    void escribir();

    /// Consulta directa del registro (observabilidad y decodificacion).
    /// El indice se enmascara a 3 bits: el hardware solo tiene 8 registros.
    unsigned valor(unsigned indice) const {
        return registros[indice & 0x07u].to_uint();
    }

    /// Precarga directa (pruebas y reset).
    void fijar(unsigned indice, unsigned dato) {
        registros[indice & 0x07u] = dato & 0xFFu;
    }

    /// Deja los 8 registros a cero.
    void reiniciar() {
        for (int i = 0; i < 8; ++i) registros[i] = 0;
    }

    /// Escritura directa usada por la etapa WB del pipeline.
    ///
    /// Se ejecuta al comienzo del ciclo, antes de que la etapa ID lea los
    /// registros.  Ese orden "write-first" permite que una instruccion vea
    /// el resultado de la instruccion que se retira en ese mismo ciclo.
    void escribir_directo(unsigned indice, unsigned dato) {
        registros[indice & 0x07u] = dato & 0xFFu;
    }

private:
    sc_uint<8> registros[8];
};

//  MemoriaInstrucciones - 256 palabras de 16 bits
//  Lectura combinacional: la palabra direccionada por el PC.
SC_MODULE(MemoriaInstrucciones) {
    sc_in<sc_uint<8>>  direccion;  ///< direccion de palabra del PC
    sc_out<sc_uint<16>> dato;      ///< palabra de instruccion

    SC_CTOR(MemoriaInstrucciones) {
        for (int i = 0; i < 256; ++i) memoria[i] = 0;
        SC_METHOD(leer);
        sensitive << direccion;
    }

    /// Lectura combinacional.
    void leer();

    /// Carga un programa de palabras de 16 bits.
    /// Devuelve true si cupo completo; si supera 256 palabras se trunca
    /// de forma segura y devuelve false.
    bool cargar(const uint16_t* programa, size_t tamano);

    /// Consulta directa de una palabra (observabilidad).
    unsigned contenido(unsigned dir) const {
        return memoria[dir & 0xFFu];
    }

    /// Borra toda la memoria de instrucciones.
    void limpiar();

private:
    uint16_t memoria[256];
};

//  MemoriaDatos - 256 bytes de datos
//  Escritura sincrona (ST); lectura inmediata mediante contenido().
SC_MODULE(MemoriaDatos) {
    sc_in<bool>        clk;
    sc_in<sc_uint<8>>  direccion;    ///< direccion de acceso
    sc_in<sc_uint<8>>  dato_entrada; ///< dato a escribir
    sc_in<bool>        escritura;    ///< habilitacion de escritura

    SC_CTOR(MemoriaDatos) {
        for (int i = 0; i < 256; ++i) memoria[i] = 0;
        SC_METHOD(escribir);
        sensitive << clk.pos();
        dont_initialize();
    }

    /// Escritura sincrona en el flanco de subida.
    void escribir();

    /// Consulta directa de un byte (observabilidad y fase de ejecucion).
    unsigned contenido(unsigned dir) const {
        return memoria[dir & 0xFFu];
    }

    /// Escritura directa (precarga de constantes y pruebas).
    void escribir_directo(unsigned dir, unsigned dato) {
        memoria[dir & 0xFFu] = static_cast<uint8_t>(dato & 0xFFu);
    }

    /// Borra toda la memoria de datos.
    void limpiar();

private:
    uint8_t memoria[256];
};

//  CommunicationMailbox - buzon REQ/ACK entre STRIKER-8 y GOALKEEPER-8
//
//  Mapa de puertos (0x10..0x25):
//    0x10 REQ        W: 1 inicia la peticion; R: 1 si hay transaccion
//    0x11 ACK        R: 1 si la respuesta de 6 bytes esta completa
//                    W: 1 confirma/limpia ACK (los bytes se conservan)
//    0x12 STATE      R: bit0 busy, bit1 pendiente, bit2 listo, bit3 error
//    0x13 REQ_IDX    W: indice 0..4 para REQ_DATA; R: indice actual
//    0x14 REQ_DATA   W: byte de peticion; R: byte seleccionado
//    0x15 RESP_IDX   W: indice 0..5 para RESP_DATA; R: indice actual
//    0x16 RESP_DATA  R: byte de respuesta seleccionado
//    0x17 TICKS_L    R: parte baja de reaction_ticks
//    0x18 TICKS_H    R: parte alta de reaction_ticks
//    0x19..0x1D      W/R: bytes 0..4 de la peticion
//    0x1E..0x23      R: bytes 0..5 de la respuesta
//    0x24 CLEAR      W: 1 limpia todo el buzon
//    0x25 IRQ        R: 1 si hay respuesta lista (sin limpiar ACK)
//
//  Protocolo:
//    1. La CPU escribe los 5 bytes de peticion (0x19..0x1D o indice/dato).
//    2. La CPU escribe REQ=1. El buzon activa req_valid durante un ciclo
//       y pone busy=1 si los 5 bytes estan presentes.
//    3. GOALKEEPER-8 devuelve 6 bytes mediante resp_valid/resp_indice/
//       resp_dato, en orden 0..5.
//    4. Al recibir el sexto byte se activa resp_ready/ACK y busy=0.
//    5. La CPU lee los 6 bytes (0x1E..0x23 o indice/dato) y reaction_ticks,
//       y escribe ACK=1 para confirmar.
//
//  reaction_ticks cuenta ciclos de reloj desde la aceptacion de REQ hasta
//  la llegada del primer byte de respuesta; despues queda congelado.
SC_MODULE(CommunicationMailbox) {
    sc_in<bool> clk;
    sc_in<bool> reset;

    // ---- Lado CPU ----
    sc_in<bool>        escribe;         ///< la CPU escribe un puerto
    sc_in<sc_uint<8>>  puerto;          ///< numero de puerto
    sc_in<sc_uint<8>>  dato_escritura;  ///< byte escrito por la CPU
    sc_out<sc_uint<8>> dato_lectura;    ///< lectura combinacional (observabilidad)

    // ---- Lado GOALKEEPER-8 ----
    sc_out<bool>        req_valid;      ///< pulso: peticion de 5 bytes disponible
    sc_out<sc_uint<8>>  req_byte[5];    ///< bytes 0..4 de la peticion
    sc_in<bool>         resp_valid;     ///< GOALKEEPER-8 entrega un byte
    sc_in<sc_uint<3>>   resp_indice;    ///< indice 0..5 del byte entregado
    sc_in<sc_uint<8>>   resp_dato;      ///< byte de respuesta
    sc_out<sc_uint<8>>  resp_byte[6];   ///< bytes 0..5 de la respuesta
    sc_out<bool>        busy;           ///< transaccion en curso
    sc_out<bool>        resp_ready;     ///< respuesta completa (ACK)
    sc_out<bool>        error;          ///< error de protocolo
    sc_out<sc_uint<16>> reaction_ticks; ///< ciclos hasta la primera respuesta

    SC_CTOR(CommunicationMailbox) {
        limpiar_estado();
        SC_METHOD(procesar);
        sensitive << clk.pos();
        dont_initialize();
    }

    /// Procesa escrituras de la CPU, respuestas y el contador de ciclos.
    void procesar();

    /// Lectura combinacional de un puerto del buzon.
    unsigned leer(unsigned p) const;

    /// Byte i de la peticion (0..4).
    unsigned contenido_solicitud(unsigned i) const {
        return (i < 5) ? req[i] : 0u;
    }

    /// Byte i de la respuesta (0..5).
    unsigned contenido_respuesta(unsigned i) const {
        return (i < 6) ? resp[i] : 0u;
    }

    /// Indica si los 5 bytes de peticion fueron escritos.
    bool solicitud_completa() const { return req_mask == 0x1Fu; }

    /// Indica si la respuesta de 6 bytes esta completa.
    bool respuesta_completa() const { return listo_reg; }

    /// Indica si hubo un error de protocolo.
    bool hay_error() const { return error_reg; }

    /// Valor actual del contador de reaccion.
    unsigned valor_ticks() const { return ticks_reg; }

private:
    uint8_t  req[5];
    uint8_t  resp[6];
    unsigned req_mask;        ///< bit i = byte i de peticion escrito
    unsigned req_idx;         ///< indice para REQ_DATA
    unsigned resp_idx;        ///< indice para RESP_DATA
    unsigned resp_count;      ///< bytes de respuesta recibidos (0..6)
    bool     busy_reg;
    bool     pendiente;       ///< REQ aceptada, esperando respuesta
    bool     listo_reg;       ///< respuesta completa
    bool     error_reg;
    bool     ticks_congelados;
    uint16_t ticks_reg;

    /// Deja el buzon en su estado inicial.
    void limpiar_estado();
};

//  Striker8 - computador lanzador segmentado
//
//  Pipeline de 5 etapas:
//      IF  -> busqueda de la instruccion (PC)
//      ID  -> decodificacion, lectura de registros y resolucion de saltos
//      EX  -> ALU y calculo de direcciones
//      MEM -> memoria de datos, puertos de E/S y buzon
//      WB  -> escritura del banco de registros
//
//  Los registros IF/ID, ID/EX, EX/MEM y MEM/WB permiten que varias
//  instrucciones esten presentes al mismo tiempo en distintas etapas.
SC_MODULE(Striker8) {

    // ---------------- Entradas ----------------
    sc_in<bool>        clk;
    sc_in<bool>        reset;             ///< reset sincrono activo alto
    sc_in<bool>        lanzar;            ///< orden de iniciar el lanzamiento
    sc_in<sc_uint<8>>  entrada_x;         ///< parametro del usuario
    sc_in<sc_uint<8>>  entrada_y;         ///< parametro del usuario
    sc_in<sc_uint<8>>  entrada_potencia;  ///< parametro del usuario
    sc_in<sc_uint<8>>  entrada_modo;      ///< 0 manual / distinto de 0 automatico

    // ---------------- Resultados ----------------
    sc_out<sc_uint<8>> resultado_x;
    sc_out<sc_uint<8>> resultado_y;
    sc_out<sc_uint<8>> resultado_tiempo;
    sc_out<sc_uint<8>> resultado_potencia;
    sc_out<sc_uint<8>> resultado_id;

    // ---------------- Observabilidad de la CPU ----------------
    sc_out<sc_uint<8>>  pc_actual;
    sc_out<sc_uint<16>> ir_actual;
    sc_out<bool>        bandera_cero;
    sc_out<bool>        bandera_acarreo;
    sc_out<bool>        ocupado;
    sc_out<bool>        detenido;
    sc_out<bool>        error_instruccion;
    sc_out<bool>        error_comunicacion;

    // ---------------- Lado GOALKEEPER-8 (buzon) ----------------
    sc_out<bool>        req_valid;
    sc_out<sc_uint<8>>  req_byte[5];
    sc_in<bool>         resp_valid;
    sc_in<sc_uint<3>>   resp_indice;
    sc_in<sc_uint<8>>   resp_dato;
    sc_out<sc_uint<8>>  resp_byte[6];
    sc_out<bool>        solicitud_enviada;
    sc_out<bool>        respuesta_lista;
    sc_out<sc_uint<16>> reaction_ticks;

    // ---------------- Unidades ----------------
    ALU8*                 alu;
    UnidadControl*        control;
    BancoRegistros*       banco;
    MemoriaInstrucciones* memoria_programa;
    MemoriaDatos*         memoria_datos;
    CommunicationMailbox* comunicacion;

    SC_CTOR(Striker8) {
        alu              = new ALU8("ALU");
        control          = new UnidadControl("UNIDAD_CONTROL");
        banco            = new BancoRegistros("BANCO_REGISTROS");
        memoria_programa = new MemoriaInstrucciones("MEMORIA_INSTRUCCIONES");
        memoria_datos    = new MemoriaDatos("MEMORIA_DATOS");
        comunicacion     = new CommunicationMailbox("COMMUNICATION_MAILBOX");

        // ---- ALU ----
        alu->a(sig_alu_a);
        alu->b(sig_alu_b);
        alu->operacion(sig_operacion_alu);
        alu->resultado(sig_alu_resultado);
        alu->cero(sig_alu_cero);
        alu->acarreo(sig_alu_acarreo);

        // ---- Unidad de control ----
        control->codigo_operacion(sig_codigo_operacion);
        control->operacion_alu(sig_operacion_alu);
        control->condicion(sig_condicion);
        control->escribe_registro(sig_cu_escribe_reg);
        control->lee_memoria(sig_cu_lee_mem);
        control->escribe_memoria(sig_cu_escribe_mem);
        control->es_entrada(sig_cu_entrada);
        control->es_salida(sig_cu_salida);
        control->es_salto(sig_cu_salto);
        control->es_halt(sig_cu_halt);
        control->es_mov(sig_cu_mov);
        control->es_literal(sig_cu_literal);
        control->actualiza_banderas(sig_cu_banderas);
        control->instruccion_invalida(sig_cu_invalida);

        // ---- Banco de registros ----
        banco->clk(clk);
        banco->escritura(sig_banco_escritura);
        banco->direccion_escritura(sig_banco_direccion);
        banco->dato_escritura(sig_banco_dato);

        // ---- Memoria de instrucciones ----
        memoria_programa->direccion(sig_pc);
        memoria_programa->dato(sig_instruccion);

        // ---- Memoria de datos ----
        memoria_datos->clk(clk);
        memoria_datos->direccion(sig_dir_datos);
        memoria_datos->dato_entrada(sig_dato_escritura_mem);
        memoria_datos->escritura(sig_escribe_mem);

        // ---- Buzon de comunicacion ----
        comunicacion->clk(clk);
        comunicacion->reset(reset);
        comunicacion->escribe(sig_com_escribir);
        comunicacion->puerto(sig_com_puerto);
        comunicacion->dato_escritura(sig_com_dato);
        comunicacion->dato_lectura(sig_com_lectura);

        // Los puertos del buzon se enlazan jerarquicamente a los puertos del
        // computador. Asi el pulso req_valid y los bytes de respuesta llegan
        // al exterior sin perder ciclos por copias intermedias.
        comunicacion->req_valid(req_valid);
        for (int i = 0; i < 5; ++i) comunicacion->req_byte[i](req_byte[i]);
        comunicacion->resp_valid(resp_valid);
        comunicacion->resp_indice(resp_indice);
        comunicacion->resp_dato(resp_dato);
        for (int i = 0; i < 6; ++i) comunicacion->resp_byte[i](resp_byte[i]);
        comunicacion->busy(solicitud_enviada);
        comunicacion->resp_ready(respuesta_lista);
        comunicacion->error(error_comunicacion);
        comunicacion->reaction_ticks(reaction_ticks);

        // ---- Secuenciador ----
        estado = REPOSO;
        z = false;
        c = false;
        error_instr = false;
        error_com = false;
        ciclos = 0;
        pc_reg = 0;
        ir_reg = 0;
        parada_pendiente = false;
        inicializar_pipeline();

        SC_METHOD(procesar);
        sensitive << clk.pos();
        dont_initialize();

        cargar_programa_predeterminado();
    }

    /// Ciclo del pipeline IF -> ID -> EX -> MEM -> WB.
    void procesar();

    /// Carga el programa y las constantes del lanzamiento.
    void cargar_programa_predeterminado();

    // ----- Utilidades para pruebas e integracion -----
    unsigned long ciclos_ejecutados() const { return ciclos; }
    bool esta_detenido() const { return estado == DETENIDO; }
    unsigned valor_registro(unsigned i) const { return banco->valor(i); }
    unsigned valor_dato(unsigned dir) const { return memoria_datos->contenido(dir); }
    unsigned valor_pc() const { return pc_reg; }
    unsigned valor_ir() const { return ir_reg; }

private:
    enum Estado { REPOSO, ACTIVO, DETENIDO };

    //  Registros de pipeline

    /// Registro IF/ID: instruccion buscada y su direccion.
    struct EtapaIF_ID {
        bool     valido;
        uint16_t instruccion;
        unsigned pc;

        EtapaIF_ID() : valido(false), instruccion(0), pc(0) {}
    };

    /// Registro ID/EX: control decodificado y operandos leidos.
    struct EtapaID_EX {
        bool                  valido;
        uint16_t              instruccion;
        unsigned              pc;
        isa8::ControlSenales  control;
        unsigned              rd;
        unsigned              fuente1;
        unsigned              fuente2;
        bool                  usa_fuente1;
        bool                  usa_fuente2;
        unsigned              valor1;
        unsigned              valor2;
        unsigned              inmediato;

        EtapaID_EX()
            : valido(false), instruccion(0), pc(0), control(), rd(0),
              fuente1(0), fuente2(0), usa_fuente1(false), usa_fuente2(false),
              valor1(0), valor2(0), inmediato(0) {}
    };

    /// Registro EX/MEM: resultado de la ALU y control para memoria/puertos.
    struct EtapaEX_MEM {
        bool                  valido;
        uint16_t              instruccion;
        unsigned              pc;
        isa8::ControlSenales  control;
        unsigned              rd;
        unsigned              resultado;
        unsigned              dato_guardar;
        unsigned              direccion;

        EtapaEX_MEM()
            : valido(false), instruccion(0), pc(0), control(), rd(0),
              resultado(0), dato_guardar(0), direccion(0) {}
    };

    /// Registro MEM/WB: dato final y habilitacion de escritura.
    struct EtapaMEM_WB {
        bool     valido;
        uint16_t instruccion;
        unsigned pc;
        bool     escribe_registro;
        unsigned rd;
        unsigned dato;
        bool     es_halt;
        bool     invalida;

        EtapaMEM_WB()
            : valido(false), instruccion(0), pc(0), escribe_registro(false),
              rd(0), dato(0), es_halt(false), invalida(false) {}
    };

    Estado        estado;
    bool          z;                 ///< bandera de cero
    bool          c;                 ///< bandera de acarreo/prestamo
    bool          error_instr;
    bool          error_com;
    unsigned long ciclos;

    unsigned      pc_reg;            ///< PC de la etapa IF
    unsigned      ir_reg;            ///< instruccion retirada en WB
    bool          parada_pendiente;  ///< HALT/invalida en camino: no buscar mas

    EtapaIF_ID    if_id;
    EtapaID_EX    id_ex;
    EtapaEX_MEM   ex_mem;
    EtapaMEM_WB   mem_wb;

    // Senales internas para conectar los submodulos
    sc_signal<sc_uint<8>>  sig_pc;
    sc_signal<sc_uint<16>> sig_ir;
    sc_signal<sc_uint<16>> sig_instruccion;

    sc_signal<sc_uint<5>> sig_codigo_operacion;
    sc_signal<sc_uint<3>> sig_operacion_alu;
    sc_signal<sc_uint<3>> sig_condicion;
    sc_signal<bool> sig_cu_escribe_reg, sig_cu_lee_mem, sig_cu_escribe_mem;
    sc_signal<bool> sig_cu_entrada, sig_cu_salida, sig_cu_salto, sig_cu_halt;
    sc_signal<bool> sig_cu_mov, sig_cu_literal, sig_cu_banderas, sig_cu_invalida;

    sc_signal<sc_uint<8>> sig_alu_a, sig_alu_b, sig_alu_resultado;
    sc_signal<bool>       sig_alu_cero, sig_alu_acarreo;

    sc_signal<bool>       sig_banco_escritura;
    sc_signal<sc_uint<3>> sig_banco_direccion;
    sc_signal<sc_uint<8>> sig_banco_dato;

    sc_signal<sc_uint<8>> sig_dir_datos, sig_dato_escritura_mem;
    sc_signal<bool>       sig_escribe_mem;

    sc_signal<bool>       sig_com_escribir;
    sc_signal<sc_uint<8>> sig_com_puerto, sig_com_dato, sig_com_lectura;

    // Ayudantes
    void reiniciar();
    void inicializar_pipeline();
    void copiar_resultados();
    unsigned leer_puerto(unsigned puerto) const;
};

#endif // STRIKER_H

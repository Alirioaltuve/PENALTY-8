// Pruebas existentes intactas + soporte para ejecutar programas .asm

#include <systemc.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cctype>
#include <cstdlib>

#include "striker.h"
#include "programa_striker.h"

//  Contadores globales de la bateria de pruebas
static int total_comprobaciones = 0;
static int total_fallos = 0;

static void verificar(const std::string& nombre, long esperado, long obtenido) {
    ++total_comprobaciones;
    if (esperado == obtenido) {
        std::cout << "  [OK]    " << std::left << std::setw(46) << nombre
                  << " = " << obtenido << std::endl;
    } else {
        ++total_fallos;
        std::cout << "  [FALLO] " << std::left << std::setw(46) << nombre
                  << " esperado " << esperado << ", obtenido " << obtenido
                  << std::endl;
    }
}

static void verificar_bool(const std::string& nombre, bool esperado, bool obtenido) {
    verificar(nombre, esperado ? 1 : 0, obtenido ? 1 : 0);
}

static void verificar_texto(const std::string& nombre,
                            const std::string& esperado,
                            const std::string& obtenido) {
    ++total_comprobaciones;
    if (esperado == obtenido) {
        std::cout << "  [OK]    " << std::left << std::setw(46) << nombre
                  << " = " << obtenido << std::endl;
    } else {
        ++total_fallos;
        std::cout << "  [FALLO] " << std::left << std::setw(46) << nombre
                  << " esperado " << esperado << ", obtenido " << obtenido
                  << std::endl;
    }
}

//  Ensamblador sencillo de .asm hacia palabras de 16 bits de STRIKER-8
namespace asm_striker {

struct ResultadoEnsamblado {
    bool ok;
    std::string error;
    std::vector<uint16_t> palabras;
};

static std::string recortar(const std::string& s) {
    size_t ini = 0;
    while (ini < s.size() && std::isspace(static_cast<unsigned char>(s[ini]))) {
        ++ini;
    }
    size_t fin = s.size();
    while (fin > ini && std::isspace(static_cast<unsigned char>(s[fin - 1]))) {
        --fin;
    }
    return s.substr(ini, fin - ini);
}

static std::string a_mayusculas(std::string s) {
    for (size_t i = 0; i < s.size(); ++i) {
        s[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[i])));
    }
    return s;
}

static bool parsear_registro(const std::string& tok, unsigned& reg) {
    std::string t = a_mayusculas(recortar(tok));
    if (t.size() == 2 && t[0] == 'R' && t[1] >= '0' && t[1] <= '7') {
        reg = static_cast<unsigned>(t[1] - '0');
        return true;
    }
    return false;
}

static bool parsear_numero(const std::string& tok, unsigned& val) {
    std::string t = recortar(tok);
    if (t.empty()) return false;
    char* fin = nullptr;
    errno = 0;
    unsigned long n = std::strtoul(t.c_str(), &fin, 0);
    if (errno != 0 || fin == t.c_str() || *fin != '\0') return false;
    val = static_cast<unsigned>(n);
    return true;
}

static std::vector<std::string> separar_operandos(const std::string& resto) {
    std::vector<std::string> ops;
    std::stringstream ss(resto);
    std::string item;
    while (std::getline(ss, item, ',')) {
        std::string limpio = recortar(item);
        if (!limpio.empty()) ops.push_back(limpio);
    }
    return ops;
}

static ResultadoEnsamblado ensamblar_archivo(const std::string& ruta) {
    ResultadoEnsamblado res;
    res.ok = false;

    std::ifstream archivo(ruta.c_str());
    if (!archivo) {
        res.error = "No se pudo abrir el archivo: " + ruta;
        return res;
    }

    std::string linea;
    unsigned num_linea = 0;

    while (std::getline(archivo, linea)) {
        ++num_linea;

        size_t pos_pycom = linea.find(';');
        if (pos_pycom != std::string::npos) linea = linea.substr(0, pos_pycom);
        size_t pos_alm = linea.find('#');
        if (pos_alm != std::string::npos) linea = linea.substr(0, pos_alm);

        std::string limpia = recortar(linea);
        if (limpia.empty()) continue;

        std::istringstream iss(limpia);
        std::string mnem;
        iss >> mnem;
        mnem = a_mayusculas(mnem);

        std::string resto;
        std::getline(iss, resto);
        std::vector<std::string> ops = separar_operandos(resto);

        unsigned a = 0, b = 0, c = 0;
        uint16_t palabra = 0;

        if (mnem == "NOP") {
            if (!ops.empty()) {
                res.error = "Linea " + std::to_string(num_linea) + ": NOP no acepta operandos";
                return res;
            }
            palabra = isa8::codificar_j(isa8::OP_NOP, 0);
        } else if (mnem == "HALT") {
            if (!ops.empty()) {
                res.error = "Linea " + std::to_string(num_linea) + ": HALT no acepta operandos";
                return res;
            }
            palabra = isa8::codificar_j(isa8::OP_HALT, 0);
        } else if (mnem == "MOV") {
            if (ops.size() != 2 || !parsear_registro(ops[0], a) || !parsear_registro(ops[1], b)) {
                res.error = "Linea " + std::to_string(num_linea) + ": sintaxis esperada MOV Rd, Rs1";
                return res;
            }
            palabra = isa8::codificar_r(isa8::OP_MOV, a, b, 0);
        } else if (mnem == "ADD" || mnem == "SUB" || mnem == "AND" || mnem == "OR" || mnem == "CMP") {
            if (ops.size() != 3 || !parsear_registro(ops[0], a) ||
                !parsear_registro(ops[1], b) || !parsear_registro(ops[2], c)) {
                res.error = "Linea " + std::to_string(num_linea) + ": sintaxis esperada " + mnem + " Rd, Rs1, Rs2";
                return res;
            }
            unsigned op_code = isa8::OP_ADD;
            if (mnem == "SUB") op_code = isa8::OP_SUB;
            else if (mnem == "AND") op_code = isa8::OP_AND;
            else if (mnem == "OR") op_code = isa8::OP_OR;
            else if (mnem == "CMP") op_code = isa8::OP_CMP;
            palabra = isa8::codificar_r(op_code, a, b, c);
        } else if (mnem == "LDI" || mnem == "LD" || mnem == "ST" || mnem == "IN" || mnem == "OUT") {
            if (ops.size() != 2 || !parsear_registro(ops[0], a) || !parsear_numero(ops[1], b)) {
                res.error = "Linea " + std::to_string(num_linea) + ": sintaxis esperada " + mnem + " R, imm8/dir8/puerto8";
                return res;
            }
            unsigned op_code = isa8::OP_LDI;
            if (mnem == "LD") op_code = isa8::OP_LD;
            else if (mnem == "ST") op_code = isa8::OP_ST;
            else if (mnem == "IN") op_code = isa8::OP_IN;
            else if (mnem == "OUT") op_code = isa8::OP_OUT;
            palabra = isa8::codificar_i(op_code, a, b);
        } else if (mnem == "JMP" || mnem == "JEQ" || mnem == "JNE" ||
                   mnem == "JGE" || mnem == "JLT" || mnem == "JGT" || mnem == "JLE") {
            if (ops.size() != 1 || !parsear_numero(ops[0], a)) {
                res.error = "Linea " + std::to_string(num_linea) + ": sintaxis esperada " + mnem + " dir8";
                return res;
            }
            unsigned op_code = isa8::OP_JMP;
            if (mnem == "JEQ") op_code = isa8::OP_JEQ;
            else if (mnem == "JNE") op_code = isa8::OP_JNE;
            else if (mnem == "JGE") op_code = isa8::OP_JGE;
            else if (mnem == "JLT") op_code = isa8::OP_JLT;
            else if (mnem == "JGT") op_code = isa8::OP_JGT;
            else if (mnem == "JLE") op_code = isa8::OP_JLE;
            palabra = isa8::codificar_j(op_code, a);
        } else {
            res.error = "Linea " + std::to_string(num_linea) + ": instruccion desconocida '" + mnem + "'";
            return res;
        }

        res.palabras.push_back(palabra);
    }

    res.ok = true;
    return res;
}

static std::string nombre_base(const std::string& ruta) {
    size_t p = ruta.find_last_of("/\\");
    return (p == std::string::npos) ? ruta : ruta.substr(p + 1);
}

} // namespace asm_striker

//  RespondedorPortero - GOALKEEPER-8 simulado para las pruebas
//  Detecta el pulso req_valid y entrega 6 bytes con el indice 0..5.
//  Permite configurar los bytes, el retardo hasta el primer byte y si el
//  portero esta habilitado (para la prueba de timeout autonomo).

SC_MODULE(RespondedorPortero) {
    sc_in<bool>       clk;
    sc_in<bool>       reset;
    sc_in<bool>       req_valid;

    sc_out<bool>      resp_valid;
    sc_out<sc_uint<3>> resp_indice;
    sc_out<sc_uint<8>> resp_dato;

    uint8_t datos[6];
    int     retardo;
    bool    habilitado;
    bool    arrancar;
    bool    activo;
    int     espera;
    int     indice;

    SC_CTOR(RespondedorPortero) {
        for (int i = 0; i < 6; ++i) datos[i] = 0;
        retardo = 2;
        habilitado = true;
        arrancar = false;
        activo = false;
        espera = 0;
        indice = 0;

        SC_METHOD(despertar);
        sensitive << req_valid.pos();
        dont_initialize();

        SC_METHOD(procesar);
        sensitive << clk.pos();
        dont_initialize();
    }

    /// Configura la respuesta y el retardo hasta el primer byte.
    void configurar(const uint8_t* d, int r, bool habilitar_portero) {
        for (int i = 0; i < 6; ++i) datos[i] = d[i];
        retardo = r;
        habilitado = habilitar_portero;
        activo = false;
        arrancar = false;
    }

    void habilitar(bool valor) {
        habilitado = valor;
        if (!habilitado) {
            activo = false;
            arrancar = false;
        }
    }

    void despertar() {
        if (habilitado) arrancar = true;
    }

    void procesar() {
        if (reset.read()) {
            activo = false;
            arrancar = false;
            espera = 0;
            indice = 0;
            resp_valid.write(false);
            resp_indice.write(0);
            resp_dato.write(0);
            return;
        }

        resp_valid.write(false);

        if (arrancar && !activo) {
            arrancar = false;
            activo = true;
            espera = retardo;
            indice = 0;
        }

        if (!activo) return;

        if (espera > 0) {
            --espera;
            return;
        }

        if (indice < 6) {
            resp_indice.write(indice);
            resp_dato.write(datos[indice]);
            resp_valid.write(true);
            ++indice;
            if (indice == 6) activo = false;
        }
    }
};

//  Banco de pruebas
SC_MODULE(BancoPruebas) {

    // ---- Reloj ----
    sc_clock* reloj;

    // ---- Computador bajo prueba ----
    Striker8* dut;

    sc_signal<bool>       reset;
    sc_signal<bool>       lanzar;
    sc_signal<sc_uint<8>> in_x, in_y, in_p, in_modo;

    sc_signal<sc_uint<8>> res_x, res_y, res_t, res_p, res_id;
    sc_signal<sc_uint<8>> dbg_pc;
    sc_signal<sc_uint<16>> dbg_ir;
    sc_signal<bool>       dbg_z, dbg_c;
    sc_signal<bool>       ocupado, detenido, err_instr, err_com;

    // ---- Buzon visto desde el exterior ----
    sc_signal<bool>        s_req_valid;
    sc_signal<sc_uint<8>>  s_req_byte[5];
    sc_signal<bool>        s_resp_valid;
    sc_signal<sc_uint<3>>  s_resp_indice;
    sc_signal<sc_uint<8>>  s_resp_dato;
    sc_signal<sc_uint<8>>  s_resp_byte[6];
    sc_signal<bool>        s_solicitud_enviada;
    sc_signal<bool>        s_respuesta_lista;
    sc_signal<sc_uint<16>> s_ticks;

    // ---- Portero simulado ----
    RespondedorPortero* portero;

    // ---- ALU suelta ----
    ALU8* alu;
    sc_signal<sc_uint<8>> alu_a, alu_b, alu_r;
    sc_signal<sc_uint<3>> alu_op;
    sc_signal<bool>       alu_z, alu_c;

    // ---- Banco de registros suelto ----
    BancoRegistros* banco;
    sc_signal<bool>       banco_we;
    sc_signal<sc_uint<3>> banco_dir;
    sc_signal<sc_uint<8>> banco_dato;

    // ---- Memoria de instrucciones suelta ----
    MemoriaInstrucciones* mem_i;
    sc_signal<sc_uint<8>>  mem_i_dir;
    sc_signal<sc_uint<16>> mem_i_dato;

    // ---- Memoria de datos suelta ----
    MemoriaDatos* mem_d;
    sc_signal<sc_uint<8>> mem_d_dir, mem_d_in;
    sc_signal<bool>       mem_d_we;

    // ---- Buzon aislado ----
    CommunicationMailbox* com;
    sc_signal<bool>        com_reset;
    sc_signal<bool>        com_escribe;
    sc_signal<sc_uint<8>>  com_puerto, com_dato, com_lectura;
    sc_signal<bool>        com_req_valid;
    sc_signal<sc_uint<8>>  com_req_byte[5];
    sc_signal<bool>        com_resp_valid;
    sc_signal<sc_uint<3>>  com_resp_indice;
    sc_signal<sc_uint<8>>  com_resp_dato;
    sc_signal<sc_uint<8>>  com_resp_byte[6];
    sc_signal<bool>        com_busy, com_resp_ready, com_error;
    sc_signal<sc_uint<16>> com_ticks;

    int peticiones_vistas;

    // Constructor: crea y conecta el DUT y el portero
    SC_CTOR(BancoPruebas) {
        reloj = new sc_clock("reloj", 10, SC_NS);

        // ---------------- Computador ----------------
        dut = new Striker8("STRIKER8");
        dut->clk(*reloj);
        dut->reset(reset);
        dut->lanzar(lanzar);
        dut->entrada_x(in_x);
        dut->entrada_y(in_y);
        dut->entrada_potencia(in_p);
        dut->entrada_modo(in_modo);
        dut->resultado_x(res_x);
        dut->resultado_y(res_y);
        dut->resultado_tiempo(res_t);
        dut->resultado_potencia(res_p);
        dut->resultado_id(res_id);
        dut->pc_actual(dbg_pc);
        dut->ir_actual(dbg_ir);
        dut->bandera_cero(dbg_z);
        dut->bandera_acarreo(dbg_c);
        dut->ocupado(ocupado);
        dut->detenido(detenido);
        dut->error_instruccion(err_instr);
        dut->error_comunicacion(err_com);

        dut->req_valid(s_req_valid);
        for (int i = 0; i < 5; ++i) dut->req_byte[i](s_req_byte[i]);
        dut->resp_valid(s_resp_valid);
        dut->resp_indice(s_resp_indice);
        dut->resp_dato(s_resp_dato);
        for (int i = 0; i < 6; ++i) dut->resp_byte[i](s_resp_byte[i]);
        dut->solicitud_enviada(s_solicitud_enviada);
        dut->respuesta_lista(s_respuesta_lista);
        dut->reaction_ticks(s_ticks);

        // ---------------- Portero simulado ----------------
        portero = new RespondedorPortero("PORTERO");
        portero->clk(*reloj);
        portero->reset(reset);
        portero->req_valid(s_req_valid);
        portero->resp_valid(s_resp_valid);
        portero->resp_indice(s_resp_indice);
        portero->resp_dato(s_resp_dato);

        // ---------------- ALU suelta ----------------
        alu = new ALU8("ALU_PRUEBA");
        alu->a(alu_a);
        alu->b(alu_b);
        alu->operacion(alu_op);
        alu->resultado(alu_r);
        alu->cero(alu_z);
        alu->acarreo(alu_c);

        // ---------------- Banco de registros suelto ----------------
        banco = new BancoRegistros("BANCO_PRUEBA");
        banco->clk(*reloj);
        banco->escritura(banco_we);
        banco->direccion_escritura(banco_dir);
        banco->dato_escritura(banco_dato);

        // ---------------- Memoria de instrucciones suelta ----------------
        mem_i = new MemoriaInstrucciones("MEM_INSTRUCCIONES_PRUEBA");
        mem_i->direccion(mem_i_dir);
        mem_i->dato(mem_i_dato);

        // ---------------- Memoria de datos suelta ----------------
        mem_d = new MemoriaDatos("MEM_DATOS_PRUEBA");
        mem_d->clk(*reloj);
        mem_d->direccion(mem_d_dir);
        mem_d->dato_entrada(mem_d_in);
        mem_d->escritura(mem_d_we);

        // ---------------- Buzon aislado ----------------
        com = new CommunicationMailbox("MAILBOX_PRUEBA");
        com->clk(*reloj);
        com->reset(com_reset);
        com->escribe(com_escribe);
        com->puerto(com_puerto);
        com->dato_escritura(com_dato);
        com->dato_lectura(com_lectura);
        com->req_valid(com_req_valid);
        for (int i = 0; i < 5; ++i) com->req_byte[i](com_req_byte[i]);
        com->resp_valid(com_resp_valid);
        com->resp_indice(com_resp_indice);
        com->resp_dato(com_resp_dato);
        for (int i = 0; i < 6; ++i) com->resp_byte[i](com_resp_byte[i]);
        com->busy(com_busy);
        com->resp_ready(com_resp_ready);
        com->error(com_error);
        com->reaction_ticks(com_ticks);

        // ---------------- Estimulos iniciales ----------------
        reset.write(true);
        lanzar.write(false);
        in_x.write(0);
        in_y.write(0);
        in_p.write(0);
        in_modo.write(0);

        banco_we.write(false);
        banco_dir.write(0);
        banco_dato.write(0);
        mem_i_dir.write(0);
        mem_d_we.write(false);
        mem_d_dir.write(0);
        mem_d_in.write(0);

        com_reset.write(false);
        com_escribe.write(false);
        com_puerto.write(0);
        com_dato.write(0);
        com_resp_valid.write(false);
        com_resp_indice.write(0);
        com_resp_dato.write(0);

        alu_op.write(0);
        alu_a.write(0);
        alu_b.write(0);

        peticiones_vistas = 0;

        // Monitor del pulso req_valid (una peticion por lanzamiento).
        SC_METHOD(contar_peticion);
        sensitive << s_req_valid.posedge_event();
        dont_initialize();
    }

    //  Utilidades
    void avanzar(int ciclos = 1) {
        // avanza ciclos de reloj
        for (int i = 0; i < ciclos; ++i) sc_start(10, SC_NS);
    }

    void contar_peticion() {
        ++peticiones_vistas;
    }

    void preparar_lanzamiento(unsigned x, unsigned y, unsigned p, unsigned modo) {
        // aplica reset y entradas
        lanzar.write(false);
        reset.write(true);
        in_x.write(x);
        in_y.write(y);
        in_p.write(p);
        in_modo.write(modo);
        peticiones_vistas = 0;
        avanzar(2);
        reset.write(false);
        avanzar(1);
    }

    bool ejecutar_hasta_halt(int maximo = 6000) {
        // espera a que la CPU termine
        for (int i = 0; i < maximo; ++i) {
            avanzar(1);
            if (detenido.read()) return true;
        }
        return false;
    }

    void ejecutar_lanzamiento(unsigned x, unsigned y, unsigned p, unsigned modo) {
        preparar_lanzamiento(x, y, p, modo);
        lanzar.write(true);
        avanzar(1);
        lanzar.write(false);
        if (!ejecutar_hasta_halt()) {
            std::cout << "  [FALLO] el programa no alcanzo HALT" << std::endl;
            ++total_comprobaciones;
            ++total_fallos;
        }
    }

    void ejecutar_programa(const uint16_t* prog, size_t n) {
        dut->memoria_programa->cargar(prog, n);
        preparar_lanzamiento(0, 0, 0, 0);
        lanzar.write(true);
        avanzar(1);
        lanzar.write(false);
        if (!ejecutar_hasta_halt(2000)) {
            std::cout << "  [FALLO] el programa de prueba no alcanzo HALT"
                      << std::endl;
            ++total_comprobaciones;
            ++total_fallos;
        }
    }

    void ejecutar_asm(const std::string& ruta,
                      unsigned int max_ciclos = 100000,
                      unsigned int mem_volcado = 32) {
        asm_striker::ResultadoEnsamblado ens = asm_striker::ensamblar_archivo(ruta);
        if (!ens.ok) {
            std::cerr << "Error de ensamblado: " << ens.error << "\n";
            ++total_comprobaciones;
            ++total_fallos;
            return;
        }

        std::cout << "Programa: " << asm_striker::nombre_base(ruta) << "\n";
        std::cout << "Instrucciones: " << ens.palabras.size() << "\n";
        std::cout << "Palabras de 16 bits:\n";
        for (size_t i = 0; i < ens.palabras.size(); ++i) {
            std::cout << "  [" << std::setw(3) << i << "] 0x"
                      << std::hex << std::uppercase << std::setw(4)
                      << std::setfill('0') << ens.palabras[i]
                      << std::dec << std::setfill(' ') << "\n";
        }

        portero->habilitar(false);
        dut->memoria_programa->cargar(ens.palabras.data(), ens.palabras.size());
        preparar_lanzamiento(0, 0, 0, 0);
        lanzar.write(true);
        avanzar(1);
        lanzar.write(false);

        const bool fin_halt = ejecutar_hasta_halt(static_cast<int>(max_ciclos));

        std::cout << "\nCiclos ejecutados: " << dut->ciclos_ejecutados() << "\n";
        std::cout << "Estado final: " << (fin_halt ? "HALT" : "LIMITE DE CICLOS") << "\n";
        std::cout << "PC final: " << dut->valor_pc() << "  IR final: 0x"
                  << std::hex << std::uppercase << std::setw(4)
                  << std::setfill('0') << dut->valor_ir()
                  << std::dec << std::setfill(' ') << "\n";
        std::cout << "Banderas: Z = " << (dbg_z.read() ? 1 : 0)
                  << "  C = " << (dbg_c.read() ? 1 : 0) << "\n";

        std::cout << "\nRegistros:\n";
        for (int i = 0; i < 8; ++i) {
            std::cout << "  R" << i << " = " << std::dec
                      << dut->valor_registro(static_cast<unsigned>(i)) << "\n";
        }

        std::cout << "\nMemoria de datos (primeras " << mem_volcado << " posiciones):\n";
        for (unsigned int i = 0; i < mem_volcado && i < 256; ++i) {
            unsigned int v = dut->valor_dato(i);
            if (v != 0) {
                std::cout << "  [" << std::hex << std::setw(2) << std::setfill('0')
                          << i << "] = " << std::dec << v << "\n";
            }
        }
        std::cout << "  (el resto es 0)\n";

        dut->cargar_programa_predeterminado();
        portero->habilitar(true);
    }

    // ---- Utilidades del buzon aislado ----
    void com_resetear() {
        com_reset.write(true);
        avanzar(1);
        com_reset.write(false);
        avanzar(1);
    }

    void com_escribir(unsigned puerto, unsigned dato) {
        com_puerto.write(puerto & 0xFFu);
        com_dato.write(dato & 0xFFu);
        com_escribe.write(true);
        avanzar(1);
        com_escribe.write(false);
        avanzar(1);
    }

    void com_responder(unsigned indice, unsigned dato) {
        com_resp_indice.write(indice & 0x7u);
        com_resp_dato.write(dato & 0xFFu);
        com_resp_valid.write(true);
        avanzar(1);
        com_resp_valid.write(false);
        avanzar(1);
    }

    void com_peticion_completa(const uint8_t pet[5]) {
        for (unsigned i = 0; i < 5; ++i) {
            com_escribir(0x19u + i, pet[i]);
        }
    }

    //  1. PRUEBAS de la ISA
    void probar_isa() {
        std::cout << "\n== 1. Codificacion de la ISA de 16 bits ==" << std::endl;

        verificar("palabras del programa", 99,
                  static_cast<long>(TAMANO_PROGRAMA_STRIKER));
        verificar("primera palabra = IN R0,0x00", 0x5000,
                  programa_striker[0]);
        verificar("ultima palabra = HALT", 0x9800,
                  programa_striker[TAMANO_PROGRAMA_STRIKER - 1]);
        verificar("opcode de la primera palabra", isa8::OP_IN,
                  isa8::codigo(programa_striker[0]));
        verificar("Rd de IN R0", 0, isa8::campo_d(programa_striker[0]));
        verificar("puerto de IN R0", 0, isa8::inmediato(programa_striker[0]));

        // Codificadores y decodificadores.
        verificar("codificar_r(ADD,4,4,1)", 0x1484,
                  isa8::codificar_r(isa8::OP_ADD, 4, 4, 1));
        verificar("codificar_i(LD,7,0xE0)", 0x47E0,
                  isa8::codificar_i(isa8::OP_LD, 7, 0xE0));
        verificar("codificar_j(JMP,36)", 0x6024,
                  isa8::codificar_j(isa8::OP_JMP, 36));
        verificar("decodificar Rd de MOV R4,R2", 4,
                  isa8::campo_d(isa8::codificar_r(isa8::OP_MOV, 4, 2, 0)));
        verificar("decodificar Rs1 de MOV R4,R2", 2,
                  isa8::campo_s1(isa8::codificar_r(isa8::OP_MOV, 4, 2, 0)));

        // Instrucciones concretas del programa.
        verificar("programa[22] = MOV R4,R2", 0x0C40, programa_striker[22]);
        verificar("programa[50] = ST R0,0x10", 0x4810, programa_striker[50]);
        verificar("programa[67] = OUT R2,0x10 (REQ)", 0x5A10,
                  programa_striker[67]);
        verificar("programa[70] = IN R2,0x11 (ACK)", 0x5211,
                  programa_striker[70]);
        verificar("programa[97] = OUT R2,0x11 (ACK)", 0x5A11,
                  programa_striker[97]);

        // Formatos.
        verificar_bool("ADD es formato R", true,
                       isa8::formato_r(isa8::OP_ADD));
        verificar_bool("CMP es formato R", true,
                       isa8::formato_r(isa8::OP_CMP));
        verificar_bool("LDI es formato I", true,
                       isa8::formato_i(isa8::OP_LDI));
        verificar_bool("OUT es formato I", true,
                       isa8::formato_i(isa8::OP_OUT));
        verificar_bool("JMP es formato J", true,
                       isa8::formato_j(isa8::OP_JMP));
        verificar_bool("JLE es formato J", true,
                       isa8::formato_j(isa8::OP_JLE));
        verificar_bool("NOP no es formato R/I/J", false,
                       isa8::formato_r(isa8::OP_NOP) ||
                       isa8::formato_i(isa8::OP_NOP) ||
                       isa8::formato_j(isa8::OP_NOP));

        // Todos los opcodes 0x00..0x13 son validos; 0x14..0x1F no.
        int validos = 0;
        for (unsigned op = 0x00; op <= 0x13; ++op) {
            if (isa8::es_valida(op)) ++validos;
        }
        verificar("opcodes validos 0x00..0x13", 20, validos);
        int invalidos = 0;
        for (unsigned op = 0x14; op <= 0x1F; ++op) {
            if (!isa8::es_valida(op)) ++invalidos;
        }
        verificar("opcodes invalidos 0x14..0x1F", 12, invalidos);
        verificar_bool("0x14 no es valido", false,
                       isa8::es_valida(isa8::OP_RESERVADO));

        // Mnemonicos.
        verificar_texto("mnemonico ADD", "ADD",
                        isa8::nombre_instruccion(isa8::OP_ADD));
        verificar_texto("mnemonico LDI", "LDI",
                        isa8::nombre_instruccion(isa8::OP_LDI));
        verificar_texto("mnemonico JLT", "JLT",
                        isa8::nombre_instruccion(isa8::OP_JLT));
        verificar_texto("mnemonico HALT", "HALT",
                        isa8::nombre_instruccion(isa8::OP_HALT));
        verificar_texto("mnemonico reservado", "RESERVADA",
                        isa8::nombre_instruccion(isa8::OP_RESERVADO));
    }

    //  2. PRUEBAS de la ALU
    void probar_alu() {
        std::cout << "\n== 2. ALU de 3 bits y banderas Z/C ==" << std::endl;

        unsigned r = 0;
        bool z = false, c = false;

        auto evaluar = [&](unsigned va, unsigned vb, unsigned op) {
            alu_a.write(va);
            alu_b.write(vb);
            alu_op.write(op);
            sc_start(1, SC_NS);
            r = alu_r.read().to_uint();
            z = alu_z.read();
            c = alu_c.read();
        };

        evaluar(20, 8, isa8::ALU_ADD);
        verificar("ADD 20+8", 28, r);
        verificar_bool("ADD 20+8 Z", false, z);
        verificar_bool("ADD 20+8 C", false, c);

        evaluar(0, 0, isa8::ALU_ADD);
        verificar("ADD 0+0", 0, r);
        verificar_bool("ADD 0+0 Z", true, z);
        verificar_bool("ADD 0+0 C", false, c);

        evaluar(255, 1, isa8::ALU_ADD);
        verificar("ADD 255+1 (8 bits)", 0, r);
        verificar_bool("ADD 255+1 Z", true, z);
        verificar_bool("ADD 255+1 C", true, c);

        evaluar(200, 100, isa8::ALU_ADD);
        verificar("ADD 200+100 (8 bits)", 44, r);
        verificar_bool("ADD 200+100 Z", false, z);
        verificar_bool("ADD 200+100 C", true, c);

        evaluar(20, 8, isa8::ALU_SUB);
        verificar("SUB 20-8", 12, r);
        verificar_bool("SUB 20-8 Z", false, z);
        verificar_bool("SUB 20-8 C (A>=B)", true, c);

        evaluar(8, 20, isa8::ALU_SUB);
        verificar("SUB 8-20 (8 bits)", 244, r);
        verificar_bool("SUB 8-20 Z", false, z);
        verificar_bool("SUB 8-20 C (A<B)", false, c);

        evaluar(255, 255, isa8::ALU_SUB);
        verificar("SUB 255-255", 0, r);
        verificar_bool("SUB 255-255 Z", true, z);
        verificar_bool("SUB 255-255 C (A>=B)", true, c);

        evaluar(0, 1, isa8::ALU_SUB);
        verificar("SUB 0-1 (8 bits)", 255, r);
        verificar_bool("SUB 0-1 C (A<B)", false, c);

        evaluar(0xF0, 0x0F, isa8::ALU_AND);
        verificar("AND 0xF0 & 0x0F", 0x00, r);
        verificar_bool("AND Z", true, z);
        verificar_bool("AND C", false, c);

        evaluar(0xF0, 0x0F, isa8::ALU_OR);
        verificar("OR 0xF0 | 0x0F", 0xFF, r);
        verificar_bool("OR Z", false, z);
        verificar_bool("OR C", false, c);

        evaluar(20, 8, isa8::ALU_CMP);
        verificar_bool("CMP 20,8 Z", false, z);
        verificar_bool("CMP 20,8 C (A>=B)", true, c);

        evaluar(8, 20, isa8::ALU_CMP);
        verificar_bool("CMP 8,20 Z", false, z);
        verificar_bool("CMP 8,20 C (A<B)", false, c);

        evaluar(8, 8, isa8::ALU_CMP);
        verificar_bool("CMP 8,8 Z", true, z);
        verificar_bool("CMP 8,8 C (A>=B)", true, c);

        // Codigo de ALU no definido: comportamiento seguro.
        evaluar(0x5A, 0x00, 5);
        verificar("ALU op 5 devuelve A", 0x5A, r);
        verificar_bool("ALU op 5 C", false, c);
    }

    //  3. PRUEBAS del Banco de registros
    void probar_banco() {
        std::cout << "\n== 3. Banco de 8 registros ==" << std::endl;

        banco->reiniciar();
        verificar("R0 tras reiniciar", 0, banco->valor(0));
        verificar("R7 tras reiniciar", 0, banco->valor(7));

        banco_dir.write(5);
        banco_dato.write(0xA5);
        banco_we.write(true);
        avanzar(1);
        banco_we.write(false);

        verificar("escritura de R5", 0xA5, banco->valor(5));
        verificar("R4 no se altera", 0, banco->valor(4));

        banco_dir.write(7);
        banco_dato.write(0x5A);
        banco_we.write(true);
        avanzar(1);
        banco_we.write(false);

        verificar("escritura de R7", 0x5A, banco->valor(7));
        verificar("R5 conserva su valor", 0xA5, banco->valor(5));

        banco_dir.write(6);
        banco_dato.write(0xFF);
        banco_we.write(false);
        avanzar(1);
        verificar("sin WE no se escribe R6", 0, banco->valor(6));

        for (unsigned i = 0; i < 8; ++i) banco->fijar(i, i + 1);
        banco->reiniciar();
        unsigned suma = 0;
        for (unsigned i = 0; i < 8; ++i) suma += banco->valor(i);
        verificar("reinicio de los 8 registros", 0, suma);

        // El indice se enmascara a 3 bits: 0x0B direcciona R3.
        banco->fijar(0x0B, 0x33);
        verificar("indice 0x0B accede a R3", 0x33, banco->valor(3));
    }

    //  4. PRUEBAS de las Memorias
    void probar_memorias() {
        std::cout << "\n== 4. Memorias ==" << std::endl;

        // --- Memoria de instrucciones ---
        mem_i->limpiar();
        verificar_bool("carga del programa completo", true,
                       mem_i->cargar(programa_striker,
                                     TAMANO_PROGRAMA_STRIKER));
        verificar("mem. instrucciones [0]", 0x5000, mem_i->contenido(0));
        verificar("mem. instrucciones [98]", 0x9800, mem_i->contenido(98));

        mem_i_dir.write(0);
        sc_start(1, SC_NS);
        verificar("lectura combinacional [0]", 0x5000,
                  mem_i_dato.read().to_uint());

        mem_i_dir.write(98);
        sc_start(1, SC_NS);
        verificar("lectura combinacional [98]", 0x9800,
                  mem_i_dato.read().to_uint());

        // Programa mayor que 256 palabras: truncado seguro.
        uint16_t grande[300];
        for (unsigned i = 0; i < 300; ++i) {
            grande[i] = static_cast<uint16_t>(0x1000u + i);
        }
        verificar_bool("programa > 256 se trunca", false,
                       mem_i->cargar(grande, 300));
        verificar("carga truncada [0]", 0x1000, mem_i->contenido(0));
        verificar("carga truncada [255]", 0x1000 + 255,
                  mem_i->contenido(255));
        verificar_bool("cargar puntero nulo", false,
                       mem_i->cargar(nullptr, 8));

        uint16_t exacto[256];
        for (unsigned i = 0; i < 256; ++i) {
            exacto[i] = static_cast<uint16_t>(i);
        }
        verificar_bool("programa de 256 palabras cabe", true,
                       mem_i->cargar(exacto, 256));

        // Un programa corto cargado despues de uno largo no debe dejar
        // instrucciones residuales: las palabras sobrantes quedan a NOP.
        const uint16_t corto[2] = {
            isa8::codificar_i(isa8::OP_LDI, 0, 0x2A),
            isa8::codificar_j(isa8::OP_HALT, 0)
        };
        verificar_bool("programa corto sobre uno largo", true,
                       mem_i->cargar(corto, 2));
        verificar("palabra copiada [0]",
                  static_cast<long>(isa8::codificar_i(isa8::OP_LDI, 0, 0x2A)),
                  mem_i->contenido(0));
        verificar("sin residuos en [2]", 0, mem_i->contenido(2));
        verificar("sin residuos en [255]", 0, mem_i->contenido(255));

        mem_i->limpiar();
        verificar("limpieza de la memoria de instrucciones", 0,
                  mem_i->contenido(0xFF));

        // --- Memoria de datos ---
        mem_d->limpiar();
        mem_d_dir.write(0x40);
        mem_d_in.write(0x7E);
        mem_d_we.write(true);
        avanzar(1);
        mem_d_we.write(false);

        verificar("mem. datos [0x40]", 0x7E, mem_d->contenido(0x40));
        verificar("mem. datos [0x41] intacta", 0, mem_d->contenido(0x41));

        mem_d_dir.write(0xFE);
        mem_d_in.write(0x11);
        mem_d_we.write(true);
        avanzar(1);
        mem_d_we.write(false);

        verificar("mem. datos [0xFE]", 0x11, mem_d->contenido(0xFE));
        verificar("mem. datos [0x40] conserva", 0x7E,
                  mem_d->contenido(0x40));

        mem_d->escribir_directo(0x20, 0x99);
        verificar("escritura directa [0x20]", 0x99,
                  mem_d->contenido(0x20));

        mem_d->limpiar();
        verificar("limpieza de la memoria de datos", 0,
                  mem_d->contenido(0xFE));

        // Restaurar el banco de constantes del programa.
        for (size_t i = 0; i < NUM_CONSTANTES_STRIKER; ++i) {
            mem_d->escribir_directo(constantes_striker[i].direccion,
                                    constantes_striker[i].valor);
        }
    }

    //  COMPRUEBA Lanzamientos completos
    void comprobar_lanzamiento(const std::string& nombre,
                               unsigned ex, unsigned ey,
                               unsigned et, unsigned ep,
                               unsigned edx, unsigned edy,
                               const uint8_t pet[5],
                               const uint8_t resp[6],
                               bool con_respuesta) {
        std::cout << "  -- " << nombre << " --" << std::endl;

        verificar("resultado X", ex, res_x.read().to_uint());
        verificar("resultado Y", ey, res_y.read().to_uint());
        verificar("tiempo de vuelo", et, res_t.read().to_uint());
        verificar("potencia final", ep, res_p.read().to_uint());
        verificar("identificador", ID_MENSAJE_STRIKER,
                  res_id.read().to_uint());

        verificar("memoria RES_X", ex, dut->valor_dato(RES_X));
        verificar("memoria RES_Y", ey, dut->valor_dato(RES_Y));
        verificar("memoria RES_T", et, dut->valor_dato(RES_T));
        verificar("memoria RES_P", ep, dut->valor_dato(RES_P));
        verificar("memoria RES_DX", edx, dut->valor_dato(RES_DX));
        verificar("memoria RES_DY", edy, dut->valor_dato(RES_DY));
        verificar("memoria RES_ID", ID_MENSAJE_STRIKER,
                  dut->valor_dato(RES_ID));

        for (unsigned i = 0; i < 5; ++i) {
            verificar("peticion[" + std::to_string(i) + "]", pet[i],
                      s_req_byte[i].read().to_uint());
            verificar("mailbox peticion[" + std::to_string(i) + "]", pet[i],
                      dut->comunicacion->contenido_solicitud(i));
        }
        verificar("un pulso REQ por lanzamiento", 1, peticiones_vistas);

        verificar_bool("CPU detenida (HALT)", true, detenido.read());
        verificar_bool("CPU ya no esta ocupada", false, ocupado.read());
        verificar_bool("sin error de instruccion", false, err_instr.read());
        verificar_bool("sin error de comunicacion", false, err_com.read());
        verificar("PC tras HALT", 99, dbg_pc.read().to_uint());
        verificar("IR = HALT", 0x9800, dbg_ir.read().to_uint());

        verificar("registro R0 = X", ex, dut->valor_registro(0));
        verificar("registro R1 = Y", ey, dut->valor_registro(1));
        verificar("registro R4 = Pf", ep, dut->valor_registro(4));
        verificar("registro R5 = T", et, dut->valor_registro(5));
        // R2, R3 y R6 se reutilizan como temporales durante la espera de
        // ACK; dx se comprueba en la memoria de datos (RES_DX).
        verificar("registro R7 = dy", edy, dut->valor_registro(7));

        if (con_respuesta) {
            for (unsigned i = 0; i < 6; ++i) {
                verificar("respuesta[" + std::to_string(i) + "]", resp[i],
                          s_resp_byte[i].read().to_uint());
                verificar("mailbox respuesta[" + std::to_string(i) + "]",
                          resp[i], dut->comunicacion->contenido_respuesta(i));
                verificar("memoria respuesta[" + std::to_string(i) + "]",
                          resp[i],
                          dut->valor_dato(RES_RESPUESTA_0 + i));
            }
            verificar("estado de respuesta", 1,
                      dut->valor_dato(RES_ESTADO));

            const unsigned ticks = dut->comunicacion->valor_ticks();
            verificar_bool("reaction_ticks > 0", true, ticks > 0);
            verificar("ticks guardados (bajo)", ticks & 0xFFu,
                      dut->valor_dato(RES_TICKS_L));
            verificar("ticks guardados (alto)", (ticks >> 8) & 0xFFu,
                      dut->valor_dato(RES_TICKS_H));
            verificar("salida reaction_ticks", ticks,
                      s_ticks.read().to_uint());
            verificar_bool("ACK limpio tras confirmar", false,
                           s_respuesta_lista.read());
            verificar_bool("buzon ya no ocupado", false,
                           s_solicitud_enviada.read());
        } else {
            verificar("estado de respuesta (timeout)", 0,
                      dut->valor_dato(RES_ESTADO));
            verificar_bool("sin ACK en timeout", false,
                           s_respuesta_lista.read());
        }
    }

    // Lanzamientos manual
    void probar_lanzamiento_manual() {
        std::cout << "\n== 5. Lanzamiento manual (20, 8, 85, 0) =="
                  << std::endl;
        const uint8_t resp[6] = {0xB2, 0x01, 0x02, 0x03, 0x04, 0x05};
        const uint8_t pet[5]  = {0xA1, 0x14, 0x08, 0x1F, 0x5D};
        portero->configurar(resp, 3, true);
        ejecutar_lanzamiento(20, 8, 85, 0);
        comprobar_lanzamiento("manual", 20, 8, 31, 93, 4, 0,
                              pet, resp, true);
    }

    // Lanzamientos automaticos
    void probar_lanzamiento_automatico() {
        std::cout << "\n== 6. Lanzamiento automatico (modo = 1) =="
                  << std::endl;
        const uint8_t resp[6] = {0xB2, 0x11, 0x22, 0x33, 0x44, 0x55};
        const uint8_t pet[5]  = {0xA1, 0x1C, 0x0C, 0x24, 0x64};
        portero->configurar(resp, 3, true);
        ejecutar_lanzamiento(5, 3, 50, 1);
        comprobar_lanzamiento("automatico", 28, 12, 36, 100, 12, 4,
                              pet, resp, true);
    }

    // Lanzamientos saturado
    void probar_lanzamiento_saturado() {
        std::cout << "\n== 7. Saturacion de parametros ==" << std::endl;
        const uint8_t resp[6] = {0xB2, 0x06, 0x07, 0x08, 0x09, 0x0A};
        const uint8_t pet[5]  = {0xA1, 0x1F, 0x0F, 0x2A, 0x64};
        portero->configurar(resp, 3, true);
        ejecutar_lanzamiento(200, 200, 200, 0);
        comprobar_lanzamiento("saturado", 31, 15, 42, 100, 15, 7,
                              pet, resp, true);
    }

    // Lanzamientos extremo
    void probar_lanzamiento_extremos() {
        std::cout << "\n== 8. Caso extremo (0, 0, 0, 0) ==" << std::endl;
        const uint8_t resp[6] = {0xB2, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
        const uint8_t pet[5]  = {0xA1, 0x00, 0x00, 0x90, 0x00};
        portero->configurar(resp, 3, true);
        ejecutar_lanzamiento(0, 0, 0, 0);
        comprobar_lanzamiento("extremo", 0, 0, 144, 0, 16, 8,
                              pet, resp, true);
    }

    // Lanzamientos centro
    void probar_lanzamiento_centro() {
        std::cout << "\n== 9. Tiro por encima del centro (5, 12, 50, 0) =="
                  << std::endl;
        const uint8_t resp[6] = {0xB2, 0x10, 0x20, 0x30, 0x40, 0x50};
        const uint8_t pet[5]  = {0xA1, 0x05, 0x0C, 0x49, 0x3E};
        portero->configurar(resp, 3, true);
        ejecutar_lanzamiento(5, 12, 50, 0);
        comprobar_lanzamiento("centro", 5, 12, 73, 62, 11, 4,
                              pet, resp, true);
    }

    //  10. Funcionamiento autonomo sin GOALKEEPER-8
    void probar_sin_portero() {
        std::cout << "\n== 10. Timeout autonomo sin GOALKEEPER-8 =="
                  << std::endl;

        const uint8_t vacio[6] = {0, 0, 0, 0, 0, 0};
        const uint8_t pet[5]   = {0xA1, 0x14, 0x08, 0x1F, 0x5D};
        portero->configurar(vacio, 0, false);

        ejecutar_lanzamiento(20, 8, 85, 0);

        verificar("X calculada sin portero", 20, res_x.read().to_uint());
        verificar("Y calculada sin portero", 8, res_y.read().to_uint());
        verificar("T calculada sin portero", 31, res_t.read().to_uint());
        verificar("Pf calculada sin portero", 93, res_p.read().to_uint());
        verificar("estado de respuesta (timeout)", 0,
                  dut->valor_dato(RES_ESTADO));
        verificar_bool("CPU detenida por timeout", true, detenido.read());
        verificar_bool("sin error de instruccion", false, err_instr.read());
        verificar_bool("sin error de comunicacion", false, err_com.read());
        verificar("un pulso REQ", 1, peticiones_vistas);
        verificar_bool("peticion completa antes de REQ", true,
                       dut->comunicacion->solicitud_completa());
        verificar_bool("buzon sigue pendiente", true,
                       s_solicitud_enviada.read());
        verificar_bool("sin respuesta lista", false,
                       s_respuesta_lista.read());
        for (unsigned i = 0; i < 5; ++i) {
            verificar("peticion[" + std::to_string(i) + "]", pet[i],
                      dut->comunicacion->contenido_solicitud(i));
        }
    }

    //  11. Pruebas del buzón aislado

    void probar_mailbox_aislado() {
        std::cout << "\n== 11. CommunicationMailbox aislado ==" << std::endl;

        const uint8_t pet[5] = {0xA1, 0x14, 0x08, 0x1F, 0x5D};

        // ---- Estado inicial ----
        com_resetear();
        verificar_bool("com: sin busy", false, com_busy.read());
        verificar_bool("com: sin ready", false, com_resp_ready.read());
        verificar_bool("com: sin error", false, com_error.read());
        verificar("com: ticks iniciales", 0, com_ticks.read().to_uint());
        verificar_bool("com: solicitud incompleta", false,
                       com->solicitud_completa());
        verificar_bool("com: respuesta incompleta", false,
                       com->respuesta_completa());
        verificar("com: STATE inicial", 0, com->leer(0x12));

        // ---- Peticion incompleta: REQ debe fallar ----
        com_escribir(0x19, 0xA1);
        com_escribir(0x1A, 0x14);
        com_escribir(0x1B, 0x08);
        verificar_bool("com: aun incompleta", false,
                       com->solicitud_completa());
        com_escribir(0x10, 1);
        verificar_bool("com: REQ incompleta -> error", true,
                       com_error.read());
        verificar_bool("com: REQ incompleta -> no busy", false,
                       com_busy.read());
        verificar_bool("com: REQ incompleta -> no ready", false,
                       com_resp_ready.read());

        // ---- Peticion completa y REQ ----
        com_resetear();
        com_peticion_completa(pet);
        verificar_bool("com: peticion completa", true,
                       com->solicitud_completa());
        for (unsigned i = 0; i < 5; ++i) {
            verificar("com: peticion[" + std::to_string(i) + "]", pet[i],
                      com->contenido_solicitud(i));
        }

        com_puerto.write(0x10);
        com_dato.write(1);
        com_escribe.write(true);
        avanzar(1);
        verificar_bool("com: req_valid activo", true,
                       com_req_valid.read());
        verificar_bool("com: busy tras REQ", true, com_busy.read());
        verificar("com: STATE busy+pendiente", 0x03, com->leer(0x12));
        com_escribe.write(false);
        avanzar(1);
        verificar_bool("com: req_valid es un pulso", false,
                       com_req_valid.read());

        // ---- reaction_ticks avanza y se congela ----
        const unsigned t0 = com->valor_ticks();
        avanzar(4);
        const unsigned t1 = com->valor_ticks();
        verificar_bool("com: ticks avanza", true, t1 > t0);

        com_resp_indice.write(0);
        com_resp_dato.write(0x60);
        com_resp_valid.write(true);
        avanzar(1);
        com_resp_valid.write(false);
        avanzar(1);

        const unsigned t2 = com->valor_ticks();
        avanzar(4);
        const unsigned t3 = com->valor_ticks();
        verificar_bool("com: ticks se congela", true, t2 == t3);
        verificar("com: byte 0 recibido", 0x60,
                  com->contenido_respuesta(0));
        verificar("com: byte 0 en puerto 0x1E", 0x60, com->leer(0x1E));

        // ---- Resto de la respuesta en orden ----
        for (unsigned i = 1; i < 6; ++i) {
            com_responder(i, 0x60u + i);
        }
        verificar_bool("com: respuesta completa", true,
                       com_resp_ready.read());
        verificar_bool("com: busy al terminar", false, com_busy.read());
        verificar_bool("com: respuesta_completa()", true,
                       com->respuesta_completa());
        verificar("com: STATE listo", 0x04, com->leer(0x12));
        verificar("com: IRQ activo", 1, com->leer(0x25));
        verificar("com: ACK activo", 1, com->leer(0x11));
        for (unsigned i = 0; i < 6; ++i) {
            verificar("com: respuesta[" + std::to_string(i) + "]",
                      0x60u + i, com->contenido_respuesta(i));
        }

        // ---- ACK limpia ready pero conserva los bytes ----
        com_escribir(0x11, 1);
        verificar_bool("com: ACK limpia ready", false,
                       com_resp_ready.read());
        verificar_bool("com: ACK conserva bytes", true,
                       com->contenido_respuesta(0) == 0x60);
        verificar("com: IRQ tras ACK", 0, com->leer(0x25));
        verificar_bool("com: respuesta_completa() tras ACK", false,
                       com->respuesta_completa());

        // ---- Respuesta fuera de orden ----
        com_resetear();
        com_peticion_completa(pet);
        com_escribir(0x10, 1);
        com_responder(1, 0xAA);
        verificar_bool("com: fuera de orden -> error", true,
                       com_error.read());
        verificar("com: fuera de orden no acepta byte", 0,
                  com->contenido_respuesta(1));
        verificar_bool("com: fuera de orden no ready", false,
                       com_resp_ready.read());

        // ---- Indices de respuesta invalidos ----
        com_resetear();
        com_peticion_completa(pet);
        com_escribir(0x10, 1);
        com_responder(6, 0xAA);
        verificar_bool("com: indice 6 -> error", true, com_error.read());
        com_responder(7, 0xAA);
        verificar_bool("com: indice 7 -> error", true, com_error.read());

        // ---- CLEAR ----
        com_resetear();
        com_peticion_completa(pet);
        com_escribir(0x10, 1);
        com_responder(0, 0x11);
        com_escribir(0x24, 1);
        verificar_bool("com: CLEAR no busy", false, com_busy.read());
        verificar_bool("com: CLEAR sin solicitud", false,
                       com->solicitud_completa());
        verificar_bool("com: CLEAR sin error", false, com_error.read());
        verificar("com: CLEAR ticks", 0, com->valor_ticks());
        verificar("com: CLEAR respuesta", 0,
                  com->contenido_respuesta(0));

        // ---- Puertos de indice y dato ----
        com_resetear();
        com_escribir(0x13, 3);
        com_escribir(0x14, 0x77);
        verificar("com: REQ_IDX lee 3", 3, com->leer(0x13));
        verificar("com: REQ_DATA lee 0x77", 0x77, com->leer(0x14));
        verificar("com: byte 3 = 0x77", 0x77,
                  com->contenido_solicitud(3));
        verificar_bool("com: mascara parcial", false,
                       com->solicitud_completa());

        com_escribir(0x13, 7);
        verificar_bool("com: REQ_IDX 7 -> error", true,
                       com_error.read());
        com_escribir(0x15, 6);
        verificar_bool("com: RESP_IDX 6 -> error", true,
                       com_error.read());

        // ---- Reset durante una transaccion ----
        com_resetear();
        com_peticion_completa(pet);
        com_escribir(0x10, 1);
        verificar_bool("com: transaccion en curso", true,
                       com_busy.read());
        com_reset.write(true);
        avanzar(1);
        com_reset.write(false);
        avanzar(1);
        verificar_bool("com: reset no busy", false, com_busy.read());
        verificar_bool("com: reset sin error", false, com_error.read());
        verificar("com: reset ticks", 0, com->valor_ticks());
        verificar_bool("com: reset sin solicitud", false,
                       com->solicitud_completa());
    }

    //  12. Pruebas de banderas y saltos
    void probar_banderas_cpu() {
        std::cout << "\n== 12. Banderas Z/C y saltos JLT/JGT =="
                  << std::endl;

        portero->habilitar(false);

        // ADD 255 + 1 = 0, Z = 1, C = 1.
        const uint16_t p_add[] = {
            isa8::codificar_i(isa8::OP_LDI, 0, 255),
            isa8::codificar_i(isa8::OP_LDI, 1, 1),
            isa8::codificar_r(isa8::OP_ADD, 2, 0, 1),
            isa8::codificar_j(isa8::OP_HALT, 0)
        };
        ejecutar_programa(p_add, 4);
        verificar("ADD CPU: R2", 0, dut->valor_registro(2));
        verificar_bool("ADD CPU: Z", true, dbg_z.read());
        verificar_bool("ADD CPU: C", true, dbg_c.read());

        // SUB 3 - 5 = 254, Z = 0, C = 0.
        const uint16_t p_sub1[] = {
            isa8::codificar_i(isa8::OP_LDI, 0, 3),
            isa8::codificar_i(isa8::OP_LDI, 1, 5),
            isa8::codificar_r(isa8::OP_SUB, 2, 0, 1),
            isa8::codificar_j(isa8::OP_HALT, 0)
        };
        ejecutar_programa(p_sub1, 4);
        verificar("SUB CPU: R2", 254, dut->valor_registro(2));
        verificar_bool("SUB CPU: Z", false, dbg_z.read());
        verificar_bool("SUB CPU: C (A<B)", false, dbg_c.read());

        // SUB 5 - 5 = 0, Z = 1, C = 1.
        const uint16_t p_sub2[] = {
            isa8::codificar_i(isa8::OP_LDI, 0, 5),
            isa8::codificar_i(isa8::OP_LDI, 1, 5),
            isa8::codificar_r(isa8::OP_SUB, 2, 0, 1),
            isa8::codificar_j(isa8::OP_HALT, 0)
        };
        ejecutar_programa(p_sub2, 4);
        verificar("SUB iguales: R2", 0, dut->valor_registro(2));
        verificar_bool("SUB iguales: Z", true, dbg_z.read());
        verificar_bool("SUB iguales: C", true, dbg_c.read());

        // CMP 5, 9: Z = 0, C = 0.
        const uint16_t p_cmp1[] = {
            isa8::codificar_i(isa8::OP_LDI, 0, 5),
            isa8::codificar_i(isa8::OP_LDI, 1, 9),
            isa8::codificar_r(isa8::OP_CMP, 0, 0, 1),
            isa8::codificar_j(isa8::OP_HALT, 0)
        };
        ejecutar_programa(p_cmp1, 4);
        verificar_bool("CMP 5,9: Z", false, dbg_z.read());
        verificar_bool("CMP 5,9: C", false, dbg_c.read());

        // CMP 9, 5: Z = 0, C = 1.
        const uint16_t p_cmp2[] = {
            isa8::codificar_i(isa8::OP_LDI, 0, 9),
            isa8::codificar_i(isa8::OP_LDI, 1, 5),
            isa8::codificar_r(isa8::OP_CMP, 0, 0, 1),
            isa8::codificar_j(isa8::OP_HALT, 0)
        };
        ejecutar_programa(p_cmp2, 4);
        verificar_bool("CMP 9,5: Z", false, dbg_z.read());
        verificar_bool("CMP 9,5: C", true, dbg_c.read());

        // JLT tomado: 5 < 9.
        const uint16_t p_jlt[] = {
            isa8::codificar_i(isa8::OP_LDI, 0, 5),
            isa8::codificar_i(isa8::OP_LDI, 1, 9),
            isa8::codificar_r(isa8::OP_CMP, 0, 0, 1),
            isa8::codificar_j(isa8::OP_JLT, 6),
            isa8::codificar_i(isa8::OP_LDI, 2, 0),
            isa8::codificar_j(isa8::OP_HALT, 0),
            isa8::codificar_i(isa8::OP_LDI, 2, 1),
            isa8::codificar_j(isa8::OP_HALT, 0)
        };
        ejecutar_programa(p_jlt, 8);
        verificar("JLT tomado: R2", 1, dut->valor_registro(2));
        verificar("JLT tomado: PC", 8, dbg_pc.read().to_uint());

        // JLT no tomado: 9 < 5 es falso.
        const uint16_t p_jlt_no[] = {
            isa8::codificar_i(isa8::OP_LDI, 0, 9),
            isa8::codificar_i(isa8::OP_LDI, 1, 5),
            isa8::codificar_r(isa8::OP_CMP, 0, 0, 1),
            isa8::codificar_j(isa8::OP_JLT, 6),
            isa8::codificar_i(isa8::OP_LDI, 2, 7),
            isa8::codificar_j(isa8::OP_HALT, 0),
            isa8::codificar_i(isa8::OP_LDI, 2, 1),
            isa8::codificar_j(isa8::OP_HALT, 0)
        };
        ejecutar_programa(p_jlt_no, 8);
        verificar("JLT no tomado: R2", 7, dut->valor_registro(2));

        // JGT tomado: 9 > 5.
        const uint16_t p_jgt[] = {
            isa8::codificar_i(isa8::OP_LDI, 0, 9),
            isa8::codificar_i(isa8::OP_LDI, 1, 5),
            isa8::codificar_r(isa8::OP_CMP, 0, 0, 1),
            isa8::codificar_j(isa8::OP_JGT, 6),
            isa8::codificar_i(isa8::OP_LDI, 3, 0),
            isa8::codificar_j(isa8::OP_HALT, 0),
            isa8::codificar_i(isa8::OP_LDI, 3, 1),
            isa8::codificar_j(isa8::OP_HALT, 0)
        };
        ejecutar_programa(p_jgt, 8);
        verificar("JGT tomado: R3", 1, dut->valor_registro(3));
        verificar("JGT tomado: PC", 8, dbg_pc.read().to_uint());

        // JGT no tomado: 5 > 9 es falso.
        const uint16_t p_jgt_no[] = {
            isa8::codificar_i(isa8::OP_LDI, 0, 5),
            isa8::codificar_i(isa8::OP_LDI, 1, 9),
            isa8::codificar_r(isa8::OP_CMP, 0, 0, 1),
            isa8::codificar_j(isa8::OP_JGT, 6),
            isa8::codificar_i(isa8::OP_LDI, 3, 7),
            isa8::codificar_j(isa8::OP_HALT, 0),
            isa8::codificar_i(isa8::OP_LDI, 3, 1),
            isa8::codificar_j(isa8::OP_HALT, 0)
        };
        ejecutar_programa(p_jgt_no, 8);
        verificar("JGT no tomado: R3", 7, dut->valor_registro(3));

        // Restaurar el programa y las constantes del entregable.
        dut->cargar_programa_predeterminado();
        portero->habilitar(true);
    }

    //  13. Modo fuera de 0/1
    void probar_modo_fuera_de_rango() {
        std::cout << "\n== 13. Modo fuera de rango (modo = 2) =="
                  << std::endl;
        const uint8_t resp[6] = {0xB2, 0x21, 0x22, 0x23, 0x24, 0x25};
        const uint8_t pet[5]  = {0xA1, 0x1C, 0x0C, 0x24, 0x64};
        portero->configurar(resp, 3, true);
        ejecutar_lanzamiento(5, 3, 50, 2);
        comprobar_lanzamiento("modo=2 (automatico)", 28, 12, 36, 100,
                              12, 4, pet, resp, true);
    }

    //  14. Instruccion reservada y NOP + HALT
    void probar_instruccion_reservada() {
        std::cout << "\n== 14. Instruccion reservada y NOP + HALT =="
                  << std::endl;

        portero->habilitar(false);

        // Opcode 0x14 seguido de HALT: la CPU debe detenerse con error.
        const uint16_t prog_reservado[] = {
            static_cast<uint16_t>(isa8::OP_RESERVADO << 11),
            isa8::codificar_j(isa8::OP_HALT, 0)
        };
        ejecutar_programa(prog_reservado, 2);
        verificar_bool("la CPU se detiene ante 0x14", true,
                       detenido.read());
        verificar_bool("error_instruccion activo", true,
                       err_instr.read());
        verificar("PC no consume la palabra siguiente", 1,
                  dbg_pc.read().to_uint());
        verificar("IR conserva la instruccion invalida",
                  static_cast<long>(isa8::OP_RESERVADO << 11),
                  dbg_ir.read().to_uint());

        // NOP + HALT: termina normalmente.
        const uint16_t prog_nop[] = {
            isa8::codificar_i(isa8::OP_NOP, 0, 0),
            isa8::codificar_j(isa8::OP_HALT, 0)
        };
        ejecutar_programa(prog_nop, 2);
        verificar_bool("NOP + HALT termina", true, detenido.read());
        verificar_bool("NOP no marca error", false, err_instr.read());
        verificar("PC tras NOP + HALT", 2, dbg_pc.read().to_uint());

        // Restaurar el entregable.
        dut->cargar_programa_predeterminado();
        portero->habilitar(true);
    }

    //  15. Pipeline: solapamiento de instrucciones
    void probar_pipeline() {
        std::cout << "\n== 15. Pipeline: solapamiento de instrucciones =="
                  << std::endl;

        portero->habilitar(false);

        // 20 LDI independientes + HALT.  En el modelo multiciclo anterior
        // serian 21 x 3 = 63 ciclos.  Con pipeline deben ser muchos menos.
        const uint16_t prog[] = {
            isa8::codificar_i(isa8::OP_LDI, 0, 1),
            isa8::codificar_i(isa8::OP_LDI, 1, 2),
            isa8::codificar_i(isa8::OP_LDI, 2, 3),
            isa8::codificar_i(isa8::OP_LDI, 3, 4),
            isa8::codificar_i(isa8::OP_LDI, 4, 5),
            isa8::codificar_i(isa8::OP_LDI, 5, 6),
            isa8::codificar_i(isa8::OP_LDI, 6, 7),
            isa8::codificar_i(isa8::OP_LDI, 7, 8),
            isa8::codificar_i(isa8::OP_LDI, 0, 9),
            isa8::codificar_i(isa8::OP_LDI, 1, 10),
            isa8::codificar_i(isa8::OP_LDI, 2, 11),
            isa8::codificar_i(isa8::OP_LDI, 3, 12),
            isa8::codificar_i(isa8::OP_LDI, 4, 13),
            isa8::codificar_i(isa8::OP_LDI, 5, 14),
            isa8::codificar_i(isa8::OP_LDI, 6, 15),
            isa8::codificar_i(isa8::OP_LDI, 7, 16),
            isa8::codificar_i(isa8::OP_LDI, 0, 17),
            isa8::codificar_i(isa8::OP_LDI, 1, 18),
            isa8::codificar_i(isa8::OP_LDI, 2, 19),
            isa8::codificar_i(isa8::OP_LDI, 3, 20),
            isa8::codificar_j(isa8::OP_HALT, 0)
        };

        ejecutar_programa(prog, 21);

        const unsigned long ciclos = dut->ciclos_ejecutados();
        std::cout << "  ciclos del pipeline: " << ciclos << std::endl;

        verificar_bool("el pipeline solapa instrucciones", true,
                       ciclos < 63);
        verificar_bool("latencia menor que 40 ciclos", true, ciclos < 40);
        verificar("ultimo LDI R3 = 20", 20, dut->valor_registro(3));
        verificar_bool("pipeline termina en HALT", true, detenido.read());

        // Restaurar el entregable.
        dut->cargar_programa_predeterminado();
        portero->habilitar(true);
    }

    //  Ejecucion completa
    void ejecutar_todo() {
        std::cout << "\n======================================================"
                  << std::endl;
        std::cout << "   PENALTY-8 : PRUEBAS DE STRIKER-8 (v1.0)"
                  << std::endl;
        std::cout << "======================================================"
                  << std::endl;

        probar_isa();
        probar_alu();
        probar_banco();
        probar_memorias();
        probar_lanzamiento_manual();
        probar_lanzamiento_automatico();
        probar_lanzamiento_saturado();
        probar_lanzamiento_extremos();
        probar_lanzamiento_centro();
        probar_sin_portero();
        probar_mailbox_aislado();
        probar_banderas_cpu();
        probar_modo_fuera_de_rango();
        probar_instruccion_reservada();
        probar_pipeline();

        std::cout << "\n======================================================"
                  << std::endl;
        std::cout << "  Comprobaciones: " << total_comprobaciones
                  << "   Fallos: " << total_fallos << std::endl;
        std::cout << (total_fallos == 0
                          ? "  RESULTADO: TODAS LAS PRUEBAS PASARON"
                          : "  RESULTADO: HAY PRUEBAS FALLIDAS")
                  << std::endl;
        std::cout << "======================================================"
                  << std::endl;
    }

    int fallos_totales() const { return total_fallos; }
};

//  Programa principal
int sc_main(int argc, char* argv[]) {
    BancoPruebas banco_pruebas("BANCO_PRUEBAS");

    std::string archivo_asm;
    unsigned int max_ciclos = 100000;
    unsigned int mem_volcado = 32;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--max-ciclos" && i + 1 < argc) {
            max_ciclos = static_cast<unsigned int>(std::atoi(argv[++i]));
        } else if (a == "--mem" && i + 1 < argc) {
            mem_volcado = static_cast<unsigned int>(std::atoi(argv[++i]));
        } else if (archivo_asm.empty()) {
            archivo_asm = a;
        }
    }

    if (!archivo_asm.empty()) {
        banco_pruebas.ejecutar_asm(archivo_asm, max_ciclos, mem_volcado);
        return (total_fallos == 0) ? 0 : 1;
    }

    banco_pruebas.ejecutar_todo();
    return banco_pruebas.fallos_totales() == 0 ? 0 : 1;
}
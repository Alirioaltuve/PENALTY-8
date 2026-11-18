#include <systemc.h>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cstdlib>
#include <unistd.h>

#include "cpu_goalkeeper.h"
#include "communication_mailbox.h"
#include "goalkeeper_program.h"
#include "arbitro.h"
#include "isa.h"

// ============================================================
// Demostracion interactiva de GOALKEEPER-8
// ------------------------------------------------------------
// Observa el computador RISC pipeline de cinco etapas ejecutando
// su programa real, comunicandose por el CommunicationMailbox y
// siendo arbitrado con el modulo real Arbitro. No sustituye ningun
// componente: la CPU, el Mailbox, el programa y el arbitro son los
// mismos modulos del proyecto.
//
// El generador de solicitudes (StrikerDemo) solo usa las senales
// externas del Mailbox (st_we/st_addr/st_wdata/st_re/st_rdata) y no
// escribe en registros, PC, pipeline ni memoria de la CPU.
// ============================================================

static bool g_color = true;

static std::string col(const char* code)
{
    return g_color ? std::string("\x1b[") + code + "m" : "";
}
static std::string esc(const char* code)
{
    return g_color ? std::string("\x1b[") + code : "";
}
static std::string rst() { return g_color ? "\x1b[0m" : ""; }

// ---------- Estado compartido entre sc_main y el hilo ----------
struct Peticion
{
    bool activa;
    unsigned int id, tx, ty, power, flight;
};

struct Respuesta
{
    bool lista;
    bool error;
    unsigned int resp_id, dive_x, dive_y, reaction, reach_x, reach_y;
    int arbitro;
};

static Peticion  g_peticion  = {false, 0, 0, 0, 0, 0};
static Respuesta g_respuesta = {false, false, 0, 0, 0, 0, 0, 0, -1};

// ---------- Estadisticas de la sesion (capa de presentacion) ----------
struct Estadisticas
{
    unsigned int total, atajadas, goles, afuera;
    Estadisticas() : total(0), atajadas(0), goles(0), afuera(0) {}
};
static Estadisticas g_stats;

// ---------- Decodificador de instrucciones (solo presentacion) ----------
static std::string hex2(unsigned int v)
{
    std::ostringstream os;
    os << "0x" << std::hex << std::uppercase << std::setw(2)
       << std::setfill('0') << (v & 0xFF);
    return os.str();
}

static std::string mnem(sc_uint<16> i)
{
    unsigned int op = i.range(15, 11).to_uint();
    unsigned int rd = i.range(10, 8).to_uint();
    unsigned int ra = i.range(7, 5).to_uint();
    unsigned int rb = i.range(4, 2).to_uint();
    unsigned int imm = i.range(7, 0).to_uint();
    unsigned int dir = i.range(10, 3).to_uint();

    std::ostringstream os;
    switch (op)
    {
        case isa::NOP:  return "NOP";
        case isa::LDI:  os << "LDI R" << rd << "," << imm; return os.str();
        case isa::MOV:  os << "MOV R" << rd << ",R" << ra; return os.str();
        case isa::ADD:  os << "ADD R" << rd << ",R" << ra << ",R" << rb; return os.str();
        case isa::SUB:  os << "SUB R" << rd << ",R" << ra << ",R" << rb; return os.str();
        case isa::AND:  os << "AND R" << rd << ",R" << ra << ",R" << rb; return os.str();
        case isa::OR:   os << "OR R" << rd << ",R" << ra << ",R" << rb; return os.str();
        case isa::CMP:  os << "CMP R" << ra << ",R" << rb; return os.str();
        case isa::JMP:  os << "JMP " << dir; return os.str();
        case isa::JEQ:  os << "JEQ " << dir; return os.str();
        case isa::JNE:  os << "JNE " << dir; return os.str();
        case isa::JGE:  os << "JGE " << dir; return os.str();
        case isa::JLT:  os << "JLT " << dir; return os.str();
        case isa::JGT:  os << "JGT " << dir; return os.str();
        case isa::JLE:  os << "JLE " << dir; return os.str();
        case isa::LD:   os << "LD R" << rd << "," << hex2(imm); return os.str();
        case isa::ST:   os << "ST R" << rd << "," << hex2(imm); return os.str();
        case isa::IN:   os << "IN R" << rd << "," << hex2(imm); return os.str();
        case isa::OUT:  os << "OUT R" << rd << "," << hex2(imm); return os.str();
        case isa::HALT: return "HALT";
        default:        os << hex2(op); return os.str();
    }
}

// Deteccion del ciclo de espera REQ (IN R0,0x15), que se colapsa en la UI.
static bool es_poll_req(sc_uint<16> i)
{
    return i.range(15, 11).to_uint() == isa::IN && i.range(7, 0).to_uint() == 0x15;
}

// ---------- Nombres de puertos de E/S ----------
static std::string nombre_puerto(unsigned int a)
{
    switch (a)
    {
        case 0x10: return "shot_id";
        case 0x11: return "target_x";
        case 0x12: return "target_y";
        case 0x13: return "power";
        case 0x14: return "flight_ticks";
        case 0x15: return "REQ";
        case 0x16: return "ACK";
        case 0x20: return "shot_id";
        case 0x21: return "dive_x";
        case 0x22: return "dive_y";
        case 0x23: return "reaction_ticks";
        case 0x24: return "reach_x";
        case 0x25: return "reach_y";
        default:   return "?";
    }
}

// ---------- Resultado del arbitro ----------
static const char* texto_resultado(int a)
{
    if (a == ARB_SAVE) return "ATAJADA";
    if (a == ARB_GOAL) return "GOL";
    return "DISPARO AFUERA";
}

static const char* color_resultado(int a)
{
    if (a == ARB_SAVE) return "1;32";   // verde brillante
    if (a == ARB_GOAL) return "1;31";   // rojo brillante
    return "1;33";                      // amarillo
}

// ============================================================
// StrikerDemo: generador de solicitudes por protocolo externo.
// ============================================================
SC_MODULE(StrikerDemo)
{
    sc_in<bool> clk;
    sc_in<bool> ack;

    sc_out<bool> st_we;
    sc_out<sc_uint<8>> st_addr;
    sc_out<sc_uint<8>> st_wdata;
    sc_out<bool> st_re;
    sc_in<sc_uint<8>> st_rdata;

    sc_out<sc_uint<8>> o_tx, o_ty, o_pw, o_ft;
    sc_out<sc_uint<8>> o_dx, o_dy, o_rt, o_rx, o_ry;
    sc_in<sc_uint<2>> arbitro_res;

    void st_write(unsigned int a, unsigned int v)
    {
        st_we.write(false);
        st_re.write(false);
        wait(clk.negedge_event());
        st_addr.write(a);
        st_wdata.write(v);
        st_we.write(true);
        wait(clk.posedge_event());
        wait(clk.negedge_event());
        st_we.write(false);
    }

    unsigned int st_read(unsigned int a)
    {
        st_we.write(false);
        wait(clk.negedge_event());
        st_re.write(true);
        st_addr.write(a);
        wait(1, SC_NS);
        unsigned int v = st_rdata.read().to_uint();
        st_re.write(false);
        return v;
    }

    void run()
    {
        st_we.write(false);
        st_re.write(false);

        while (true)
        {
            while (!g_peticion.activa) wait(clk.posedge_event());
            g_peticion.activa = false;

            Peticion s = g_peticion;
            g_respuesta.lista = false;
            g_respuesta.error = false;

            // Publicar el disparo al arbitro (datos originales).
            o_tx.write(s.tx);
            o_ty.write(s.ty);
            o_pw.write(s.power);
            o_ft.write(s.flight);

            // Escribir los cinco campos y activar REQ.
            st_write(0x10, s.id);
            st_write(0x11, s.tx);
            st_write(0x12, s.ty);
            st_write(0x13, s.power);
            st_write(0x14, s.flight);
            st_write(0x15, 1);

            // Esperar ACK (con limite de seguridad).
            unsigned int guarda = 0;
            while (st_read(0x16) == 0)
            {
                if (++guarda > 5000) { g_respuesta.error = true; break; }
                wait(clk.posedge_event());
            }

            if (!g_respuesta.error)
            {
                g_respuesta.resp_id = st_read(0x20);
                g_respuesta.dive_x  = st_read(0x21);
                g_respuesta.dive_y  = st_read(0x22);
                g_respuesta.reaction = st_read(0x23);
                g_respuesta.reach_x = st_read(0x24);
                g_respuesta.reach_y = st_read(0x25);

                o_dx.write(g_respuesta.dive_x);
                o_dy.write(g_respuesta.dive_y);
                o_rt.write(g_respuesta.reaction);
                o_rx.write(g_respuesta.reach_x);
                o_ry.write(g_respuesta.reach_y);

                wait(2, SC_NS);
                g_respuesta.arbitro = arbitro_res.read().to_uint();

                // Liberar el protocolo.
                st_write(0x15, 0);
                guarda = 0;
                while (st_read(0x16) != 0)
                {
                    if (++guarda > 5000) { g_respuesta.error = true; break; }
                    wait(clk.posedge_event());
                }
            }

            g_respuesta.lista = true;
        }
    }

    SC_CTOR(StrikerDemo)
    {
        SC_THREAD(run);
    }
};

// ============================================================
// Presentacion
// ============================================================
static void linea_sep(const char* c = "36")
{
    std::cout << col(c) << std::string(62, '-') << rst() << "\n";
}

static void cabecera()
{
    std::cout << esc("2J") << esc("H");
    std::cout << col("1;36")
              << "  GOALKEEPER-8" << rst() << "\n";
    std::cout << col("36")
              << "  Computador RISC de 8 bits  |  Pipeline IF-ID-EX-MEM-WB  |  "
                 "CommunicationMailbox" << rst() << "\n";
    linea_sep();
}

static void mostrar_recepcion(const Peticion& p)
{
    std::cout << col("1;34") << "\n[A] RECEPCION DEL DISPARO (protocolo externo)"
              << rst() << "\n";
    std::cout << "  Los datos llegan por el CommunicationMailbox "
                 "(STRIKER -> GOALKEEPER).\n\n";
    std::cout << "  " << std::left
              << std::setw(16) << "shot_id"     << "= " << p.id     << "\n"
              << "  " << std::setw(16) << "target_x"    << "= " << p.tx     << "\n"
              << "  " << std::setw(16) << "target_y"    << "= " << p.ty     << "\n"
              << "  " << std::setw(16) << "power"       << "= " << p.power  << "\n"
              << "  " << std::setw(16) << "flight_ticks"<< "= " << p.flight << "\n";
}

// Imprime un resumen del pipeline: ciclos donde EX es IN u OUT.
static void mostrar_procesamiento(const std::vector<CPU_Goalkeeper::TrazaCiclo>& t)
{
    std::cout << col("1;34") << "\n[B] PROCESAMIENTO EN LA CPU (programa RISC real)"
              << rst() << "\n";
    std::cout << "  La CPU ejecuta su programa y emite eventos de E/S. "
                 "Se muestran los\n  ciclos donde la etapa EX contiene un IN o un OUT.\n\n";

    std::cout << "  " << std::left
              << std::setw(6) << "CICLO" << " "
              << std::setw(14) << "IF" << " "
              << std::setw(14) << "ID" << " "
              << std::setw(14) << "EX" << " "
              << std::setw(14) << "MEM" << " "
              << std::setw(14) << "WB" << "\n";

    int mostrados = 0;
    bool aviso_espera = false;
    for (const auto& c : t)
    {
        if (!c.idex_valid) continue;
        unsigned int op = c.idex_instr.range(15, 11).to_uint();
        if (op != isa::IN && op != isa::OUT) continue;

        if (es_poll_req(c.idex_instr))
        {
            if (!aviso_espera)
            {
                std::cout << "  ... la CPU consulta REQ (IN R0,0x15) en bucle hasta "
                             "recibir o liberar la solicitud ...\n";
                aviso_espera = true;
            }
            continue;
        }

        if (++mostrados > 22) { std::cout << "  ... (mas ciclos omitidos)\n"; break; }

        std::string if_s  = mnem(c.if_instr);
        std::string id_s  = c.ifid_valid ? mnem(c.ifid_instr) : "-";
        std::string ex_s  = c.idex_valid ? mnem(c.idex_instr) : "-";
        std::string mem_s = c.exmem_valid ? mnem(c.exmem_instr) : "-";
        std::string wb_s  = c.memwb_valid ? mnem(c.memwb_instr) : "-";

        std::cout << "  " << std::left
                  << std::setw(6) << c.ciclo << " "
                  << std::setw(14) << if_s << " "
                  << std::setw(14) << id_s << " "
                  << std::setw(14) << ex_s << " "
                  << std::setw(14) << mem_s << " "
                  << std::setw(14) << wb_s << "\n";
    }
}

// Lista los eventos de E/S (IN/OUT) con direccion y valor.
static void mostrar_es(const std::vector<CPU_Goalkeeper::TrazaCiclo>& t)
{
    std::cout << col("1;34") << "\n[C] COMUNICACION (eventos de E/S del bus GOALKEEPER)"
              << rst() << "\n";
    std::cout << "  Lecturas (IN) y escrituras (OUT) reales emitidas por la CPU.\n\n";

    std::cout << "  " << std::left
              << std::setw(6) << "CICLO" << " "
              << std::setw(9) << "EVENTO" << " "
              << std::setw(15) << "INSTRUCCION" << " "
              << std::setw(22) << "PUERTO" << " "
              << "VALOR\n";

    int mostrados = 0;
    bool aviso_espera = false;
    for (const auto& c : t)
    {
        if (!c.idex_valid) continue;
        unsigned int op = c.idex_instr.range(15, 11).to_uint();
        if (op != isa::IN && op != isa::OUT) continue;

        if (es_poll_req(c.idex_instr))
        {
            if (!aviso_espera)
            {
                std::cout << "  ... consultas de REQ (IN R0,0x15) omitidas ...\n";
                aviso_espera = true;
            }
            continue;
        }

        if (++mostrados > 40) { std::cout << "  ... (mas eventos omitidos)\n"; break; }

        unsigned int imm = c.idex_instr.range(7, 0).to_uint();

        if (op == isa::OUT)
        {
            unsigned int val = c.ex_a.to_uint();
            std::cout << "  " << std::left
                      << std::setw(6) << c.ciclo << " "
                      << std::setw(9) << "OUT" << " "
                      << std::setw(15) << mnem(c.idex_instr) << " "
                      << std::setw(22) << (hex2(imm) + " (" + nombre_puerto(imm) + ")")
                      << " " << val << "\n";
        }
        else
        {
            std::cout << "  " << std::left
                      << std::setw(6) << c.ciclo << " "
                      << std::setw(9) << "IN" << " "
                      << std::setw(15) << mnem(c.idex_instr) << " "
                      << std::setw(22) << (hex2(imm) + " (" + nombre_puerto(imm) + ")")
                      << " -" << "\n";
        }
    }
}

// Muestra el estado observable del banco de registros tras la ejecucion.
static void mostrar_registros(CPU_Goalkeeper& cpu)
{
    static const char* nom[8] = {
        "shot_id / reach / REQ (reutilizado)",
        "target_x",
        "target_y",
        "power",
        "flight_ticks",
        "dive_x",
        "dive_y",
        "constante 0 / auxiliar",
    };
    std::cout << col("1;34") << "\n    Registros del banco (observados tras ejecutar el programa)"
              << rst() << "\n";
    for (int i = 0; i < 8; i++)
    {
        std::cout << "      R" << i << " = "
                  << std::setw(3) << cpu.valor_registro(i).to_uint()
                  << "   (" << nom[i] << ")\n";
    }
}

static void mostrar_respuesta(const Respuesta& r)
{
    std::cout << col("1;34") << "\n[D] RESPUESTA DEL PORTERO (leida del Mailbox)"
              << rst() << "\n";
    std::cout << "  " << std::left
              << std::setw(16) << "shot_id"       << "= " << r.resp_id  << "\n"
              << "  " << std::setw(16) << "dive_x"        << "= " << r.dive_x   << "\n"
              << "  " << std::setw(16) << "dive_y"        << "= " << r.dive_y   << "\n"
              << "  " << std::setw(16) << "reaction_ticks"<< "= " << r.reaction << "\n"
              << "  " << std::setw(16) << "reach_x"       << "= " << r.reach_x  << "\n"
              << "  " << std::setw(16) << "reach_y"       << "= " << r.reach_y  << "\n";
}

static void mostrar_resultado(const Respuesta& r)
{
    std::cout << col("1;34") << "\n[E] RESULTADO DEL ARBITRO" << rst() << "\n\n";

    std::cout << "    " << col(color_resultado(r.arbitro))
              << "  " << texto_resultado(r.arbitro)
              << rst() << "\n\n";

    if (r.arbitro == ARB_SAVE)
        std::cout << "  El portero llego a tiempo y cubrio el angulo.\n";
    else if (r.arbitro == ARB_GOAL)
        std::cout << "  El portero no alcanzo el balon o llego tarde.\n";
    else
        std::cout << "  El disparo quedo fuera del area de porteria [32,223].\n";
}

static void registrar_estadistica(const Respuesta& r)
{
    g_stats.total++;
    if (r.arbitro == ARB_SAVE) g_stats.atajadas++;
    else if (r.arbitro == ARB_GOAL) g_stats.goles++;
    else g_stats.afuera++;
}

static void mostrar_estadisticas()
{
    cabecera();
    std::cout << col("1;34") << "ESTADISTICAS DE LA SESION" << rst() << "\n\n";
    std::cout << "  " << std::left
              << std::setw(22) << "Total de disparos" << g_stats.total << "\n"
              << std::setw(22) << "Atajadas" << g_stats.atajadas << "\n"
              << std::setw(22) << "Goles" << g_stats.goles << "\n"
              << std::setw(22) << "Disparos afuera" << g_stats.afuera << "\n";

    double pct = (g_stats.total == 0) ? 0.0
               : (100.0 * g_stats.atajadas / g_stats.total);
    std::cout << "  " << std::setw(22) << "Porcentaje de atajadas"
              << std::fixed << std::setprecision(1) << pct << "%\n\n";
}

static void mostrar_info_tecnica()
{
    cabecera();
    std::cout << col("1;34") << "INFORMACION TECNICA" << rst() << "\n\n";

    std::cout << col("1;36") << "  CPU RISC de 8 bits" << rst() << "\n";
    std::cout << "  - Datapath de 8 bits, 8 registros generales R0-R7.\n";
    std::cout << "  - PC de 8 bits (256 instrucciones direccionables).\n";
    std::cout << "  - Instrucciones de 16 bits (opcode de 5 bits).\n";
    std::cout << "  - Pipeline de cinco etapas: IF -> ID -> EX -> MEM -> WB.\n\n";

    std::cout << col("1;36") << "  Unidades" << rst() << "\n";
    std::cout << "  - ALU: ADD, SUB, AND, OR, CMP; banderas C (carry) y Z (zero).\n";
    std::cout << "  - Banco de registros de 8 x 8 bits con reset sincrono.\n";
    std::cout << "  - Memoria de instrucciones 256 x 16; memoria de datos 256 x 8.\n";
    std::cout << "  - IN/OUT para E/S mapeada en memoria.\n\n";

    std::cout << col("1;36") << "  CommunicationMailbox" << rst() << "\n";
    std::cout << "  - Protocolo REQ/ACK entre STRIKER y GOALKEEPER.\n";
    std::cout << "  - Campos de solicitud en 0x10..0x15; respuesta en 0x20..0x25 y ACK en 0x16.\n";
    std::cout << "  - Mide reaction_ticks desde REQ hasta ACK (maximo 255).\n\n";

    std::cout << col("1;36") << "  Programa RISC del portero" << rst() << "\n";
    std::cout << "  - Espera REQ, lee los cinco campos y calcula dive_x, dive_y y reach.\n";
    std::cout << "  - Responde los cinco campos y activa ACK; luego espera REQ=0.\n";
    std::cout << "  - Usa LDI, MOV, ADD, SUB, AND, CMP, JMP, JEQ, JNE, JGE, JLT, LD, ST, IN, OUT.\n\n";
}

// ============================================================
// Ejecucion de un disparo
// ============================================================
static void ejecutar_disparo(CPU_Goalkeeper& cpu, const Peticion& p, bool detalle)
{
    g_peticion = p;
    g_peticion.activa = true;
    g_respuesta.lista = false;

    cpu.iniciar_traza();
    sc_start(5000, SC_NS);
    cpu.detener_traza();

    if (!g_respuesta.lista || g_respuesta.error)
    {
        std::cout << col("1;31") << "  [ERROR] No se completo el protocolo."
                  << rst() << "\n";
        return;
    }

    if (detalle)
    {
        mostrar_recepcion(p);
        mostrar_procesamiento(cpu.traza);
        mostrar_es(cpu.traza);
        mostrar_registros(cpu);
        mostrar_respuesta(g_respuesta);
    }
    mostrar_resultado(g_respuesta);
    registrar_estadistica(g_respuesta);
}

// ============================================================
// sc_main
// ============================================================
int sc_main(int argc, char* argv[])
{
    g_color = isatty(STDOUT_FILENO) != 0;
    if (const char* nc = std::getenv("NO_COLOR")) g_color = false;

    sc_clock clk("clk", 10, SC_NS);
    sc_signal<bool> reset;

    sc_signal<bool> cpu_io_write, cpu_io_read;
    sc_signal<sc_uint<8>> cpu_io_addr, cpu_io_wdata, cpu_io_rdata;
    sc_signal<bool> cpu_halted;

    sc_signal<bool> st_we, st_re;
    sc_signal<sc_uint<8>> st_addr, st_wdata, st_rdata;

    sc_signal<bool> req, ack;
    sc_signal<sc_uint<8>> reaction;

    sc_signal<sc_uint<8>> a_tx, a_ty, a_pw, a_ft;
    sc_signal<sc_uint<8>> a_dx, a_dy, a_rt, a_rx, a_ry;
    sc_signal<sc_uint<2>> a_res;

    CPU_Goalkeeper cpu("cpu");
    cpu.clk(clk);
    cpu.reset(reset);
    cpu.io_write(cpu_io_write);
    cpu.io_read(cpu_io_read);
    cpu.io_addr(cpu_io_addr);
    cpu.io_wdata(cpu_io_wdata);
    cpu.io_rdata(cpu_io_rdata);
    cpu.halted(cpu_halted);

    CommunicationMailbox mailbox("mailbox");
    mailbox.clk(clk);
    mailbox.reset(reset);
    mailbox.st_we(st_we);
    mailbox.st_addr(st_addr);
    mailbox.st_wdata(st_wdata);
    mailbox.st_re(st_re);
    mailbox.st_rdata(st_rdata);
    mailbox.gk_we(cpu_io_write);
    mailbox.gk_addr(cpu_io_addr);
    mailbox.gk_wdata(cpu_io_wdata);
    mailbox.gk_re(cpu_io_read);
    mailbox.gk_rdata(cpu_io_rdata);
    mailbox.req(req);
    mailbox.ack(ack);
    mailbox.reaction_ticks(reaction);

    Arbitro arbitro("arbitro");
    arbitro.target_x(a_tx);
    arbitro.target_y(a_ty);
    arbitro.power(a_pw);
    arbitro.flight_ticks(a_ft);
    arbitro.dive_x(a_dx);
    arbitro.dive_y(a_dy);
    arbitro.reaction_ticks(a_rt);
    arbitro.reach_x(a_rx);
    arbitro.reach_y(a_ry);
    arbitro.resultado(a_res);

    StrikerDemo striker("striker");
    striker.clk(clk);
    striker.ack(ack);
    striker.st_we(st_we);
    striker.st_addr(st_addr);
    striker.st_wdata(st_wdata);
    striker.st_re(st_re);
    striker.st_rdata(st_rdata);
    striker.o_tx(a_tx);
    striker.o_ty(a_ty);
    striker.o_pw(a_pw);
    striker.o_ft(a_ft);
    striker.o_dx(a_dx);
    striker.o_dy(a_dy);
    striker.o_rt(a_rt);
    striker.o_rx(a_rx);
    striker.o_ry(a_ry);
    striker.arbitro_res(a_res);

    cpu.cargar_programa(goalkeeper_program::construir());

    reset.write(true);
    sc_start(35, SC_NS);
    reset.write(false);
    sc_start(10, SC_NS);

    while (true)
    {
        cabecera();
        std::cout << col("1;33") << "  MENU PRINCIPAL" << rst() << "\n\n";
        std::cout << "    1. Simular un disparo de prueba\n";
        std::cout << "    2. Simular un disparo personalizado\n";
        std::cout << "    3. Ejecutar secuencia automatica\n";
        std::cout << "    4. Mostrar estadisticas de la sesion\n";
        std::cout << "    5. Mostrar informacion tecnica\n";
        std::cout << "    6. Salir\n\n";
        std::cout << "  Opcion: " << std::flush;

        std::string linea;
        if (!std::getline(std::cin, linea))
            break;
        if (linea.empty()) continue;

        int opcion = std::atoi(linea.c_str());

        if (opcion == 1)
        {
            cabecera();
            Peticion p = {false, 1, 128, 128, 100, 200};
            ejecutar_disparo(cpu, p, true);
            std::cout << "\n  Presione ENTER para continuar..." << std::flush;
            std::string in; std::getline(std::cin, in);
        }
        else if (opcion == 2)
        {
            cabecera();
            std::cout << col("1;34") << "DISPARO PERSONALIZADO (0-255)" << rst() << "\n\n";

            unsigned int v[5];
            const char* nombres[5] = {"shot_id", "target_x", "target_y", "power", "flight_ticks"};
            bool ok = true;
            for (int i = 0; i < 5; i++)
            {
                std::cout << "  " << nombres[i] << " (0-255): " << std::flush;
                std::string in;
                std::getline(std::cin, in);
                long val = std::strtol(in.c_str(), nullptr, 10);
                if (in.empty() || val < 0 || val > 255)
                {
                    std::cout << col("1;31") << "  Valor invalido (debe ser 0-255)."
                              << rst() << "\n";
                    ok = false;
                    break;
                }
                v[i] = (unsigned int)val;
            }
            if (ok)
            {
                Peticion p = {false, v[0], v[1], v[2], v[3], v[4]};
                ejecutar_disparo(cpu, p, true);
            }
            std::cout << "\n  Presione ENTER para continuar..." << std::flush;
            std::string in; std::getline(std::cin, in);
        }
        else if (opcion == 3)
        {
            cabecera();
            std::cout << col("1;34") << "SECUENCIA AUTOMATICA" << rst() << "\n\n";

            struct Esc { const char* nombre; Peticion p; };
            Esc escs[] = {
                {"Atajada clara",            {false, 1, 128, 128, 100, 200}},
                {"Gol (portero no alcanza)", {false, 5, 180, 100, 50, 127}},
                {"Disparo fuera del area",   {false, 3, 10, 128, 50, 200}},
                {"Limite reaccion vs vuelo", {false, 2, 90, 150, 180, 1}},
            };

            for (const Esc& e : escs)
            {
                std::cout << col("36") << "  >> " << e.nombre << rst() << "\n";
                ejecutar_disparo(cpu, e.p, false);
                std::cout << "\n";
            }

            std::cout << "\n  Presione ENTER para continuar..." << std::flush;
            std::string in; std::getline(std::cin, in);
        }
        else if (opcion == 4)
        {
            mostrar_estadisticas();
            std::cout << "  Presione ENTER para continuar..." << std::flush;
            std::string in; std::getline(std::cin, in);
        }
        else if (opcion == 5)
        {
            mostrar_info_tecnica();
            std::cout << "  Presione ENTER para continuar..." << std::flush;
            std::string in; std::getline(std::cin, in);
        }
        else if (opcion == 6)
        {
            std::cout << col("1;36") << "  Fin de la demostracion." << rst() << "\n";
            break;
        }
        else
        {
            std::cout << col("1;31") << "  Opcion no reconocida." << rst() << "\n";
        }
    }

    return 0;
}

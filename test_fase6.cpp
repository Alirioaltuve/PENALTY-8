#include <systemc.h>
#include <vector>
#include <string>
#include <iostream>
#include "cpu_goalkeeper.h"
#include "communication_mailbox.h"
#include "goalkeeper_program.h"

// ============================================================
// Fase 6: validacion individual final de GOALKEEPER-8.
//
// Testbench autonomo: emula al STRIKER externo y se comunica
// exclusivamente por el protocolo REQ/ACK del CommunicationMailbox.
// No escribe en el banco de registros, PC, registros del pipeline
// ni memoria interna de la CPU. La unica interaccion con la CPU es
// cargar el programa (equivalente a una ROM precargada) y liberar el
// reset; toda la ejecucion se dispara por REQ.
//
// El oraculo es independiente: tabla de valores esperados escritos
// como literales, NO una reimplementacion de la estrategia.
// ============================================================

static int fallos = 0;

static void comprobar(const char* nombre, bool ok, const char* detalle = "")
{
    std::cout << (ok ? "[OK]   " : "[FALLO] ") << nombre;
    if (detalle && detalle[0]) std::cout << " -> " << detalle;
    std::cout << std::endl;
    if (!ok) fallos++;
}

// ---------- Solicitud y respuesta esperada (oraculo) ----------
struct Solicitud
{
    unsigned int id;
    unsigned int tx;
    unsigned int ty;
    unsigned int power;
    unsigned int flight;
};

struct RespuestaEsperada
{
    unsigned int resp_id;
    unsigned int dive_x;
    unsigned int dive_y;
    unsigned int reach_x;
    unsigned int reach_y;
};

struct Caso
{
    const char* nombre;
    Solicitud    req;
    RespuestaEsperada esp;
};

// Valores esperados calculados a mano (oraculo independiente).
// Estrategia aprobada:
//   dive_x = tx + 16 (id impar) / tx - 16 (id par)
//   dive_y = ty + 16 (flight < 128) / ty (flight >= 128)
//   reach  = 16 + (8 si power >= 128) - (8 si flight < 128)
static const Caso casos[] = {
    {"base id1 (SAVE)",           {1, 128, 128, 50, 200}, {1, 144, 128, 16, 16}},
    {"par id2 (power=128)",       {2, 100, 120, 128, 200}, {2, 84, 120, 24, 24}},
    {"power=127 id3",             {3, 140, 130, 127, 200}, {3, 156, 130, 16, 16}},
    {"power=129 id4",             {4, 160, 140, 129, 200}, {4, 144, 140, 24, 24}},
    {"flight=127 id5",            {5, 180, 100, 50, 127}, {5, 196, 116, 8, 8}},
    {"flight=128 id6",            {6, 200, 60, 50, 128}, {6, 184, 60, 16, 16}},
    {"flight=129 id7",            {7, 90, 80, 150, 129}, {7, 106, 80, 24, 24}},
    {"no-duplicados id8",         {8, 128, 128, 50, 200}, {8, 112, 128, 16, 16}},
    {"pre-reset id9",             {9, 100, 100, 128, 100}, {9, 116, 116, 16, 16}},
    {"post-reset id10",           {10, 200, 150, 127, 200}, {10, 184, 150, 16, 16}},
};

static const unsigned int NUM_CASOS = sizeof(casos) / sizeof(casos[0]);

// ---------- Monitor pasivo del bus de salida de GOALKEEPER ----------
// Registra unicamente las escrituras OUT que la CPU emite hacia el
// mailbox (gk_we). No escribe nada en la CPU. Sirve para demostrar,
// al nivel del protocolo, que por solicitud se emiten exactamente 7
// escrituras (5 campos + marcador + ACK) sin duplicados.
struct OutGk
{
    unsigned int addr;
    unsigned int data;
};

SC_MODULE(MonitorGk)
{
    sc_in<bool> clk;
    sc_in<bool> gk_we;
    sc_in<sc_uint<8>> gk_addr;
    sc_in<sc_uint<8>> gk_wdata;

    std::vector<OutGk> outs;

    void capturar()
    {
        if (gk_we.read())
        {
            OutGk o;
            o.addr = gk_addr.read().to_uint();
            o.data = gk_wdata.read().to_uint();
            outs.push_back(o);
        }
    }

    SC_CTOR(MonitorGk)
    {
        SC_METHOD(capturar);
        sensitive << clk.pos();
        dont_initialize();
    }
};

// ---------- Emulador de STRIKER (solo protocolo) ----------
SC_MODULE(EmuladorStriker)
{
    sc_in<bool> clk;
    sc_out<bool> reset;

    sc_out<bool> st_we;
    sc_out<sc_uint<8>> st_addr;
    sc_out<sc_uint<8>> st_wdata;
    sc_out<bool> st_re;
    sc_in<sc_uint<8>> st_rdata;

    sc_in<bool> req;
    sc_in<bool> ack;

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

    void enviar(const Solicitud& s)
    {
        st_write(0x10, s.id);
        st_write(0x11, s.tx);
        st_write(0x12, s.ty);
        st_write(0x13, s.power);
        st_write(0x14, s.flight);
        st_write(0x15, 1);
    }

    bool esperar_ack(unsigned int limite = 5000)
    {
        unsigned int n = 0;
        while (st_read(0x16) == 0)
        {
            if (++n > limite) return false;
            wait(clk.posedge_event());
        }
        return true;
    }

    void leer_respuesta(RespuestaEsperada& r, unsigned int& reac)
    {
        r.resp_id = st_read(0x20);
        r.dive_x  = st_read(0x21);
        r.dive_y  = st_read(0x22);
        reac      = st_read(0x23);
        r.reach_x = st_read(0x24);
        r.reach_y = st_read(0x25);
    }

    bool bajar_req(unsigned int limite = 5000)
    {
        st_write(0x15, 0);
        unsigned int n = 0;
        while (st_read(0x16) != 0)
        {
            if (++n > limite) return false;
            wait(clk.posedge_event());
        }
        return true;
    }

    void verificar(const Caso& c, const RespuestaEsperada& r,
                   unsigned int reac1, unsigned int reac2)
    {
        std::cout << "  " << c.nombre << ": resp=("
                  << r.resp_id << "," << r.dive_x << "," << r.dive_y
                  << ",rx=" << r.reach_x << ",ry=" << r.reach_y
                  << ",reaction=" << reac1 << ")" << std::endl;

        bool ok = r.resp_id == c.esp.resp_id &&
                  r.dive_x  == c.esp.dive_x  &&
                  r.dive_y  == c.esp.dive_y  &&
                  r.reach_x == c.esp.reach_x &&
                  r.reach_y == c.esp.reach_y;

        comprobar((std::string(c.nombre) + " -> respuesta").c_str(), ok, "");

        bool ok_reac = reac1 > 0 && reac1 <= 255 && reac1 == reac2;
        comprobar((std::string(c.nombre) + " -> reaction_ticks").c_str(),
                  ok_reac, "");
    }

    void procesar(const Caso& c)
    {
        enviar(c.req);

        bool req_ok = req.read() && !ack.read();
        comprobar((std::string(c.nombre) + " -> REQ detectado").c_str(),
                  req_ok, "");

        if (!esperar_ack())
        {
            comprobar((std::string(c.nombre) + " -> ACK").c_str(), false, "timeout");
            return;
        }

        RespuestaEsperada r;
        unsigned int reac1 = 0;
        leer_respuesta(r, reac1);

        // El temporizador debe quedar congelado en ACK.
        wait(clk.posedge_event());
        wait(clk.posedge_event());
        unsigned int reac2 = st_read(0x23);

        verificar(c, r, reac1, reac2);

        if (!bajar_req())
            comprobar((std::string(c.nombre) + " -> REQ=0").c_str(), false, "timeout");
    }

    void procesar_sin_duplicados(const Caso& c)
    {
        enviar(c.req);

        if (!esperar_ack())
        {
            comprobar("no-duplicados -> ACK", false, "timeout");
            return;
        }

        RespuestaEsperada r1;
        unsigned int reac1 = 0;
        leer_respuesta(r1, reac1);

        // Mantener REQ alto varias cicles: el GOALKEEPER debe esperar REQ=0.
        for (int i = 0; i < 5; i++) wait(clk.posedge_event());

        RespuestaEsperada r2;
        unsigned int reac2 = 0;
        leer_respuesta(r2, reac2);

        // Un segundo REQ=1 debe ignorarse (ya hay una solicitud pendiente).
        st_write(0x15, 1);

        RespuestaEsperada r3;
        unsigned int reac3 = 0;
        leer_respuesta(r3, reac3);

        bool estable = r1.resp_id == r2.resp_id && r1.resp_id == r3.resp_id &&
                       r1.dive_x  == r2.dive_x  && r1.dive_x  == r3.dive_x  &&
                       r1.dive_y  == r2.dive_y  && r1.dive_y  == r3.dive_y  &&
                       r1.reach_x == r2.reach_x && r1.reach_x == r3.reach_x &&
                       r1.reach_y == r2.reach_y && r1.reach_y == r3.reach_y &&
                       reac1 == reac2 && reac2 == reac3;

        comprobar("no-duplicados -> respuesta estable con REQ alto", estable, "");

        unsigned int ack_alto = st_read(0x16);
        comprobar("no-duplicados -> ACK permanece alto hasta REQ=0", ack_alto == 1, "");

        verificar(c, r1, reac1, reac2);

        if (!bajar_req())
            comprobar("no-duplicados -> REQ=0", false, "timeout");
    }

    void run()
    {
        st_we.write(false);
        st_re.write(false);
        reset.write(false);

        // Reset inicial.
        reset.write(true);
        wait(20, SC_NS);
        reset.write(false);
        wait(clk.negedge_event());

        unsigned int r0 = st_read(0x23);
        comprobar("reaction_ticks = 0 antes de la primera solicitud", r0 == 0, "");

        // Fase 1: solicitudes consecutivas + fronteras (casos 0..6).
        for (unsigned int i = 0; i < 7; i++)
            procesar(casos[i]);

        // Fase 2: ausencia de respuestas duplicadas y espera de REQ=0.
        procesar_sin_duplicados(casos[7]);

        // Fase 3: reset en caliente y recuperacion.
        procesar(casos[8]);

        reset.write(true);
        wait(20, SC_NS);
        reset.write(false);
        wait(clk.negedge_event());

        unsigned int r_reset = st_read(0x23);
        comprobar("reaction_ticks = 0 tras reset", r_reset == 0, "");

        procesar(casos[9]);

        std::cout << "\n===== RESULTADO FINAL =====" << std::endl;
        std::cout << "Fallos: " << fallos << std::endl;
        sc_stop();
    }

    SC_CTOR(EmuladorStriker)
    {
        SC_THREAD(run);
    }
};

int sc_main(int argc, char* argv[])
{
    sc_clock clk("clk", 10, SC_NS);

    sc_signal<bool> reset;

    // Bus de E/S de la CPU (lado GOALKEEPER).
    sc_signal<bool> cpu_io_write;
    sc_signal<bool> cpu_io_read;
    sc_signal<sc_uint<8>> cpu_io_addr;
    sc_signal<sc_uint<8>> cpu_io_wdata;
    sc_signal<sc_uint<8>> cpu_io_rdata;
    sc_signal<bool> cpu_halted;

    // Lado STRIKER del mailbox.
    sc_signal<bool> st_we;
    sc_signal<sc_uint<8>> st_addr;
    sc_signal<sc_uint<8>> st_wdata;
    sc_signal<bool> st_re;
    sc_signal<sc_uint<8>> st_rdata;

    // Estado observable del mailbox.
    sc_signal<bool> req;
    sc_signal<bool> ack;
    sc_signal<sc_uint<8>> reaction;

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

    MonitorGk monitor("monitor");
    monitor.clk(clk);
    monitor.gk_we(cpu_io_write);
    monitor.gk_addr(cpu_io_addr);
    monitor.gk_wdata(cpu_io_wdata);

    EmuladorStriker emulador("emulador");
    emulador.clk(clk);
    emulador.reset(reset);
    emulador.st_we(st_we);
    emulador.st_addr(st_addr);
    emulador.st_wdata(st_wdata);
    emulador.st_re(st_re);
    emulador.st_rdata(st_rdata);
    emulador.req(req);
    emulador.ack(ack);

    // Cargar el programa una sola vez (como ROM). No se manipulan
    // registros internos para provocar la ejecucion.
    cpu.cargar_programa(goalkeeper_program::construir());

    sc_start(100000, SC_NS);

    // ---- Verificacion a nivel del bus GOALKEEPER (monitor) ----
    std::cout << "\n===== VERIFICACION DE OUT (bus GOALKEEPER) =====" << std::endl;

    const unsigned int escrituras_por_solicitud = 7; // 5 campos + 0x23 + 0x16
    const unsigned int esperado_total = NUM_CASOS * escrituras_por_solicitud;

    std::cout << "  OUT capturados por el monitor: " << monitor.outs.size()
              << " (esperados " << esperado_total << ")" << std::endl;

    comprobar("numero exacto de OUT (sin duplicados)",
              monitor.outs.size() == esperado_total, "");

    if (monitor.outs.size() == esperado_total)
    {
        for (unsigned int i = 0; i < NUM_CASOS; i++)
        {
            const Caso& c = casos[i];
            unsigned int b = i * escrituras_por_solicitud;

            const OutGk& o0 = monitor.outs[b + 0];
            const OutGk& o1 = monitor.outs[b + 1];
            const OutGk& o2 = monitor.outs[b + 2];
            const OutGk& o3 = monitor.outs[b + 3];
            const OutGk& o4 = monitor.outs[b + 4];
            const OutGk& o5 = monitor.outs[b + 5];
            const OutGk& o6 = monitor.outs[b + 6];

            bool ok = o0.addr == 0x24 && o0.data == c.esp.reach_x &&
                      o1.addr == 0x25 && o1.data == c.esp.reach_y &&
                      o2.addr == 0x20 && o2.data == c.esp.resp_id &&
                      o3.addr == 0x21 && o3.data == c.esp.dive_x  &&
                      o4.addr == 0x22 && o4.data == c.esp.dive_y  &&
                      o5.addr == 0x23 && o5.data == 0 &&
                      o6.addr == 0x16 && o6.data == 0;

            comprobar((std::string(c.nombre) + " -> OUT orden y datos").c_str(),
                      ok, "");
        }
    }

    std::cout << "\n===== RESUMEN =====" << std::endl;
    std::cout << "Fallos totales: " << fallos << std::endl;
    return fallos == 0 ? 0 : 1;
}

#ifndef COMMUNICATION_MAILBOX_H
#define COMMUNICATION_MAILBOX_H

#include <systemc.h>

// ============================================================
// CommunicationMailbox
// Medio de comunicacion independiente entre STRIKER-8 y
// GOALKEEPER-8. Ninguna CPU accede a los registros internos de
// la otra: cada una usa su propio bus de E/S.
//
// Mapa de E/S (acordado como referencia):
//   STRIKER escribe / GOALKEEPER lee:
//     0x10 shot_id
//     0x11 target_x
//     0x12 target_y
//     0x13 power
//     0x14 flight_ticks
//     0x15 REQ
//   GOALKEEPER escribe / STRIKER lee:
//     0x16 ACK
//     0x20 shot_id (eco)
//     0x21 dive_x
//     0x22 dive_y
//     0x23 reaction_ticks (lo llena el mailbox con el tiempo medido)
//     0x24 reach_x
//     0x25 reach_y
//
// Protocolo REQ/ACK:
//   1. STRIKER escribe los datos.
//   2. STRIKER escribe REQ=1  -> comienza la solicitud y el temporizador.
//   3. GOALKEEPER lee los datos y ejecuta su programa.
//   4. GOALKEEPER escribe la respuesta.
//   5. GOALKEEPER escribe ACK  -> se detiene el temporizador.
//   6. STRIKER lee la respuesta y escribe REQ=0.
//   7. Al bajar REQ se desactiva ACK y se permite otra solicitud.
//
// Solo se permite una solicitud pendiente a la vez.
// ============================================================

SC_MODULE(CommunicationMailbox)
{
    // -------------------- Reloj y reset ----------------
    sc_in<bool> clk;
    sc_in<bool> reset;

    // -------------------- Lado STRIKER -----------------
    sc_in<bool> st_we;
    sc_in<sc_uint<8>> st_addr;
    sc_in<sc_uint<8>> st_wdata;
    sc_in<bool> st_re;
    sc_out<sc_uint<8>> st_rdata;

    // -------------------- Lado GOALKEEPER --------------
    sc_in<bool> gk_we;
    sc_in<sc_uint<8>> gk_addr;
    sc_in<sc_uint<8>> gk_wdata;
    sc_in<bool> gk_re;
    sc_out<sc_uint<8>> gk_rdata;

    // -------------------- Estado observable ------------
    sc_out<bool> req;                     // visible al portero
    sc_out<bool> ack;                     // visible al striker
    sc_out<sc_uint<8>> reaction_ticks;    // tiempo medido

    // -------------------- Registros internos -----------
    sc_signal<sc_uint<8>> r_shot_id;
    sc_signal<sc_uint<8>> r_target_x;
    sc_signal<sc_uint<8>> r_target_y;
    sc_signal<sc_uint<8>> r_power;
    sc_signal<sc_uint<8>> r_flight;

    sc_signal<sc_uint<8>> r_resp_id;
    sc_signal<sc_uint<8>> r_dive_x;
    sc_signal<sc_uint<8>> r_dive_y;
    sc_signal<sc_uint<8>> r_reach_x;
    sc_signal<sc_uint<8>> r_reach_y;

    sc_signal<bool> r_req;
    sc_signal<bool> r_ack;
    sc_signal<sc_uint<8>> r_ticks;

    // Temporizador interno
    bool contando;

    // -------------------- Lectura GOALKEEPER -----------
    void leer_gk()
    {
        switch (gk_addr.read().to_uint())
        {
            case 0x10: gk_rdata.write(r_shot_id.read()); break;
            case 0x11: gk_rdata.write(r_target_x.read()); break;
            case 0x12: gk_rdata.write(r_target_y.read()); break;
            case 0x13: gk_rdata.write(r_power.read()); break;
            case 0x14: gk_rdata.write(r_flight.read()); break;
            case 0x15: gk_rdata.write(r_req.read() ? 1 : 0); break;
            default:   gk_rdata.write(0); break;
        }
    }

    // -------------------- Lectura STRIKER --------------
    void leer_st()
    {
        switch (st_addr.read().to_uint())
        {
            case 0x16: st_rdata.write(r_ack.read() ? 1 : 0); break;
            case 0x20: st_rdata.write(r_resp_id.read()); break;
            case 0x21: st_rdata.write(r_dive_x.read()); break;
            case 0x22: st_rdata.write(r_dive_y.read()); break;
            case 0x23: st_rdata.write(r_ticks.read()); break;
            case 0x24: st_rdata.write(r_reach_x.read()); break;
            case 0x25: st_rdata.write(r_reach_y.read()); break;
            default:   st_rdata.write(0); break;
        }
    }

    // -------------------- Proceso secuencial -----------
    void actualizar()
    {
        if (reset.read() == true)
        {
            r_shot_id.write(0);
            r_target_x.write(0);
            r_target_y.write(0);
            r_power.write(0);
            r_flight.write(0);
            r_resp_id.write(0);
            r_dive_x.write(0);
            r_dive_y.write(0);
            r_reach_x.write(0);
            r_reach_y.write(0);
            r_req.write(false);
            r_ack.write(false);
            r_ticks.write(0);
            contando = false;
            return;
        }

        // --- Temporizador de reaccion ---
        // Se incrementa en cada flanco mientras la solicitud esta
        // pendiente. Se detiene al confirmar ACK. No supera 255.
        if (contando && r_ticks.read() < 255)
        {
            r_ticks.write(r_ticks.read() + 1);
        }

        // --- Escrituras del STRIKER ---
        if (st_we.read() == true)
        {
            unsigned int a = st_addr.read().to_uint();

            if (a >= 0x10 && a <= 0x14)
            {
                // Los campos de la solicitud permanecen estables
                // mientras hay una solicitud pendiente.
                if (!r_req.read())
                {
                    switch (a)
                    {
                        case 0x10: r_shot_id.write(st_wdata.read()); break;
                        case 0x11: r_target_x.write(st_wdata.read()); break;
                        case 0x12: r_target_y.write(st_wdata.read()); break;
                        case 0x13: r_power.write(st_wdata.read()); break;
                        case 0x14: r_flight.write(st_wdata.read()); break;
                        default: break;
                    }
                }
            }
            else if (a == 0x15)
            {
                if (st_wdata.read().to_uint() != 0)
                {
                    // Nueva solicitud solo si no hay una pendiente.
                    if (!r_req.read())
                    {
                        r_req.write(true);
                        r_ack.write(false);
                        r_ticks.write(0);
                        contando = true;
                    }
                }
                else
                {
                    // Fin de la solicitud: bajar REQ y ACK.
                    r_req.write(false);
                    r_ack.write(false);
                    contando = false;
                }
            }
        }

        // --- Escrituras del GOALKEEPER ---
        if (gk_we.read() == true)
        {
            unsigned int a = gk_addr.read().to_uint();

            if (a == 0x16)
            {
                // ACK solo con solicitud pendiente y una sola vez.
                if (r_req.read() && !r_ack.read())
                {
                    r_ack.write(true);
                    contando = false;
                }
            }
            else if (a == 0x23)
            {
                // reaction_ticks lo llena el mailbox.
            }
            else if (a >= 0x20 && a <= 0x25)
            {
                // La respuesta solo se acepta mientras la solicitud
                // esta pendiente y aun no se ha confirmado ACK.
                if (r_req.read() && !r_ack.read())
                {
                    switch (a)
                    {
                        case 0x20: r_resp_id.write(gk_wdata.read()); break;
                        case 0x21: r_dive_x.write(gk_wdata.read()); break;
                        case 0x22: r_dive_y.write(gk_wdata.read()); break;
                        case 0x24: r_reach_x.write(gk_wdata.read()); break;
                        case 0x25: r_reach_y.write(gk_wdata.read()); break;
                        default: break;
                    }
                }
            }
        }
    }

    // Expone el estado interno observable a traves de los puertos.
    void exponer()
    {
        req.write(r_req.read());
        ack.write(r_ack.read());
        reaction_ticks.write(r_ticks.read());
    }

    SC_CTOR(CommunicationMailbox)
    {
        contando = false;

        // Limpieza inicial
        r_shot_id.write(0);
        r_target_x.write(0);
        r_target_y.write(0);
        r_power.write(0);
        r_flight.write(0);
        r_resp_id.write(0);
        r_dive_x.write(0);
        r_dive_y.write(0);
        r_reach_x.write(0);
        r_reach_y.write(0);
        r_req.write(false);
        r_ack.write(false);
        r_ticks.write(0);

        // Exposicion del estado observable
        SC_METHOD(exponer);
        sensitive << r_req;
        sensitive << r_ack;
        sensitive << r_ticks;

        // Lectura combinacional de ambos lados
        SC_METHOD(leer_gk);
        sensitive << gk_addr;
        sensitive << r_shot_id;
        sensitive << r_target_x;
        sensitive << r_target_y;
        sensitive << r_power;
        sensitive << r_flight;
        sensitive << r_req;

        SC_METHOD(leer_st);
        sensitive << st_addr;
        sensitive << r_ack;
        sensitive << r_resp_id;
        sensitive << r_dive_x;
        sensitive << r_dive_y;
        sensitive << r_ticks;
        sensitive << r_reach_x;
        sensitive << r_reach_y;

        // Escrituras y temporizador en el flanco positivo
        SC_METHOD(actualizar);
        sensitive << clk.pos();
        dont_initialize();
    }
};

#endif // COMMUNICATION_MAILBOX_H

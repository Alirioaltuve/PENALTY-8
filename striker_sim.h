#ifndef STRIKER_SIM_H
#define STRIKER_SIM_H

#include <systemc.h>
#include <vector>
#include <iostream>

// ============================================================
// StrikerSim  (NO es el STRIKER-8 real de Rafael)
// ------------------------------------------------------------
// Simulador del lado STRIKER a nivel de protocolo. Sirve para
// validar el CommunicationMailbox y la integracion mientras el
// STRIKER-8 real no esta disponible en el repositorio.
//
// Este modulo NO ejecuta la ISA de STRIKER: solo escribe los
// bytes del disparo, activa REQ, espera ACK, lee la respuesta y
// baja REQ, exactamente como deberia hacerlo STRIKER-8 por su
// bus de E/S.
// ============================================================

SC_MODULE(StrikerSim)
{
    // Reloj de referencia
    sc_in<bool> clk;

    // Bus hacia el CommunicationMailbox
    sc_out<bool> st_we;
    sc_out<sc_uint<8>> st_addr;
    sc_out<sc_uint<8>> st_wdata;
    sc_out<bool> st_re;
    sc_in<sc_uint<8>> st_rdata;
    sc_in<bool> ack;

    // Datos del disparo (hacia el arbitro)
    sc_out<sc_uint<8>> o_target_x;
    sc_out<sc_uint<8>> o_target_y;
    sc_out<sc_uint<8>> o_power;
    sc_out<sc_uint<8>> o_flight;

    // Respuesta del portero (hacia el arbitro)
    sc_out<sc_uint<8>> o_dive_x;
    sc_out<sc_uint<8>> o_dive_y;
    sc_out<sc_uint<8>> o_reaction;
    sc_out<sc_uint<8>> o_reach_x;
    sc_out<sc_uint<8>> o_reach_y;

    // Resultado del arbitro (entrada)
    sc_in<sc_uint<2>> arbitro_res;

    struct Disparo
    {
        unsigned int id;
        unsigned int tx;
        unsigned int ty;
        unsigned int power;
        unsigned int flight;
    };

    struct Resultado
    {
        unsigned int id;
        unsigned int resp_id;
        unsigned int dive_x;
        unsigned int dive_y;
        unsigned int reaction;
        unsigned int reach_x;
        unsigned int reach_y;
        unsigned int arbitro;
    };

    std::vector<Disparo> disparos;
    std::vector<Resultado> resultados;

    // ---------------- Helpers de bus -------------------
    void escribir(unsigned int addr, unsigned int valor)
    {
        st_we.write(false);
        st_re.write(false);
        wait(clk.negedge_event());

        st_addr.write(addr);
        st_wdata.write(valor);
        st_we.write(true);

        // El mailbox captura en el flanco positivo
        wait(clk.posedge_event());

        // Soltar la escritura tras el flanco
        wait(clk.negedge_event());
        st_we.write(false);
    }

    unsigned int leer(unsigned int addr)
    {
        st_we.write(false);
        wait(clk.negedge_event());

        st_re.write(true);
        st_addr.write(addr);

        // Dar tiempo a que la lectura combinacional se estabilice
        wait(1, SC_NS);

        return st_rdata.read().to_uint();
    }

    // ---------------- Hilo principal -------------------
    void ejecutar()
    {
        // Estado inicial del bus (despues de la elaboracion, cuando
        // los puertos ya estan enlazados).
        st_we.write(false);
        st_re.write(false);

        // Esperar a que termine el reset inicial
        wait(50, SC_NS);

        for (unsigned int i = 0; i < disparos.size(); i++)
        {
            const Disparo& d = disparos[i];

            // Fijar el disparo para el arbitro
            o_target_x.write(d.tx);
            o_target_y.write(d.ty);
            o_power.write(d.power);
            o_flight.write(d.flight);

            // Escribir los cinco bytes del disparo
            escribir(0x10, d.id);
            escribir(0x11, d.tx);
            escribir(0x12, d.ty);
            escribir(0x13, d.power);
            escribir(0x14, d.flight);

            // Activar REQ
            escribir(0x15, 1);

            // Esperar ACK
            while (leer(0x16) == 0)
            {
                wait(clk.posedge_event());
            }

            // Leer la respuesta
            Resultado r;
            r.id = d.id;
            r.resp_id = leer(0x20);
            r.dive_x = leer(0x21);
            r.dive_y = leer(0x22);
            r.reaction = leer(0x23);
            r.reach_x = leer(0x24);
            r.reach_y = leer(0x25);

            // Publicar respuesta al arbitro
            o_dive_x.write(r.dive_x);
            o_dive_y.write(r.dive_y);
            o_reaction.write(r.reaction);
            o_reach_x.write(r.reach_x);
            o_reach_y.write(r.reach_y);

            // Esperar a que el arbitro (combinacional) se estabilice
            wait(2, SC_NS);
            r.arbitro = arbitro_res.read().to_uint();

            resultados.push_back(r);

            // Bajar REQ para cerrar la solicitud
            escribir(0x15, 0);

            // Esperar a que ACK se desactive
            while (leer(0x16) != 0)
            {
                wait(clk.posedge_event());
            }

            std::cout << "[STRIKER-SIM] Disparo " << d.id
                      << " -> resp_id=" << r.resp_id
                      << " dive=(" << r.dive_x << "," << r.dive_y << ")"
                      << " reach=(" << r.reach_x << "," << r.reach_y << ")"
                      << " reaction=" << r.reaction
                      << " arbitro=" << r.arbitro << std::endl;
        }

        sc_stop();
    }

    SC_CTOR(StrikerSim)
    {
        // Escenarios de prueba (SAVE, GOAL por reaccion tardia, MISS, SAVE)
        disparos.push_back({1, 128, 128, 100, 200}); // SAVE
        disparos.push_back({2, 90, 150, 180, 1});    // GOAL (tarde)
        disparos.push_back({3, 10, 128, 50, 200});   // MISS (fuera de X)
        disparos.push_back({4, 200, 60, 120, 200});  // SAVE

        SC_THREAD(ejecutar);
    }
};

#endif // STRIKER_SIM_H

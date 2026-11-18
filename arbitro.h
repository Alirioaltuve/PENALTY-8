#ifndef ARBITRO_H
#define ARBITRO_H

#include <systemc.h>

// ============================================================
// Arbitro de PENALTY-8
// Modulo independiente, totalmente combinacional.
// Decide el resultado de un penalti a partir del disparo
// (target, power, flight_ticks) y de la respuesta del portero
// (dive, reaction_ticks, reach).
//
// Reglas:
//   - Balon fuera de la porteria (X o Y fuera de [32,223]): MISS.
//   - Potencia cero: tiro invalido (MISS).
//   - Dentro y portero alcanza y llega a tiempo: SAVE.
//   - Dentro y el portero no cumple: GOAL.
//
// Condiciones de SAVE:
//   reaction_ticks <= flight_ticks
//   |target_x - dive_x| <= reach_x
//   |target_y - dive_y| <= reach_y
// ============================================================

#define ARB_MISS 0
#define ARB_GOAL 1
#define ARB_SAVE 2

SC_MODULE(Arbitro)
{
    // Disparo
    sc_in<sc_uint<8>> target_x;
    sc_in<sc_uint<8>> target_y;
    sc_in<sc_uint<8>> power;
    sc_in<sc_uint<8>> flight_ticks;

    // Respuesta del portero
    sc_in<sc_uint<8>> dive_x;
    sc_in<sc_uint<8>> dive_y;
    sc_in<sc_uint<8>> reaction_ticks;
    sc_in<sc_uint<8>> reach_x;
    sc_in<sc_uint<8>> reach_y;

    // Resultado (ARB_MISS / ARB_GOAL / ARB_SAVE)
    sc_out<sc_uint<2>> resultado;

    static unsigned int absoluto(int a)
    {
        return (a < 0) ? (unsigned int)(-a) : (unsigned int)a;
    }

    void decidir()
    {
        unsigned int tx = target_x.read().to_uint();
        unsigned int ty = target_y.read().to_uint();
        unsigned int pw = power.read().to_uint();
        unsigned int ft = flight_ticks.read().to_uint();

        unsigned int dx = dive_x.read().to_uint();
        unsigned int dy = dive_y.read().to_uint();
        unsigned int rt = reaction_ticks.read().to_uint();
        unsigned int rx = reach_x.read().to_uint();
        unsigned int ry = reach_y.read().to_uint();

        // Fuera de la porteria: X e Y validos en [32, 223]
        bool dentro_x = (tx >= 32) && (tx <= 223);
        bool dentro_y = (ty >= 32) && (ty <= 223);

        if (!dentro_x || !dentro_y)
        {
            resultado.write(ARB_MISS);
            return;
        }

        // Potencia cero es invalida
        if (pw == 0)
        {
            resultado.write(ARB_MISS);
            return;
        }

        // Condiciones de atajada
        bool tiempo_ok = (rt <= ft);
        bool alcanza_x = (absoluto((int)tx - (int)dx) <= rx);
        bool alcanza_y = (absoluto((int)ty - (int)dy) <= ry);

        if (tiempo_ok && alcanza_x && alcanza_y)
        {
            resultado.write(ARB_SAVE);
        }
        else
        {
            resultado.write(ARB_GOAL);
        }
    }

    SC_CTOR(Arbitro)
    {
        SC_METHOD(decidir);
        sensitive << target_x;
        sensitive << target_y;
        sensitive << power;
        sensitive << flight_ticks;
        sensitive << dive_x;
        sensitive << dive_y;
        sensitive << reaction_ticks;
        sensitive << reach_x;
        sensitive << reach_y;
    }
};

#endif // ARBITRO_H

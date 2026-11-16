#include <systemc.h>
#include <iostream>
#include "arbitro.h"

static int fallos = 0;

static void comprobar(const char* nombre, int obtenido, int esperado)
{
    bool ok = (obtenido == esperado);
    std::cout << (ok ? "[OK]   " : "[FALLO] ") << nombre
              << ": obtenido=" << obtenido
              << " esperado=" << esperado << std::endl;
    if (!ok) fallos++;
}

int sc_main(int argc, char* argv[])
{
    sc_signal<sc_uint<8>> tx, ty, power, flight;
    sc_signal<sc_uint<8>> dx, dy, reaction, rx, ry;
    sc_signal<sc_uint<2>> res;

    Arbitro arb("arb");
    arb.target_x(tx);
    arb.target_y(ty);
    arb.power(power);
    arb.flight_ticks(flight);
    arb.dive_x(dx);
    arb.dive_y(dy);
    arb.reaction_ticks(reaction);
    arb.reach_x(rx);
    arb.reach_y(ry);
    arb.resultado(res);

    auto evaluar = [&](unsigned int a_tx, unsigned int a_ty, unsigned int a_pw,
                       unsigned int a_ft, unsigned int a_dx, unsigned int a_dy,
                       unsigned int a_rt, unsigned int a_rx, unsigned int a_ry) {
        tx.write(a_tx); ty.write(a_ty); power.write(a_pw); flight.write(a_ft);
        dx.write(a_dx); dy.write(a_dy); reaction.write(a_rt);
        rx.write(a_rx); ry.write(a_ry);
        sc_start(1, SC_NS);
        return res.read().to_uint();
    };

    // Atajada clara
    comprobar("SAVE", evaluar(128, 128, 100, 100, 128, 128, 50, 10, 10), ARB_SAVE);

    // Llega a tiempo pero no alcanza (coordenadas)
    comprobar("GOAL por alcance", evaluar(200, 200, 100, 100, 100, 100, 50, 10, 10), ARB_GOAL);

    // Reacciona demasiado tarde
    comprobar("GOAL por tiempo", evaluar(128, 128, 100, 100, 128, 128, 200, 10, 10), ARB_GOAL);

    // Fuera de porteria en X
    comprobar("MISS fuera X", evaluar(10, 128, 100, 100, 10, 128, 10, 64, 64), ARB_MISS);

    // Fuera de porteria en Y
    comprobar("MISS fuera Y", evaluar(128, 250, 100, 100, 128, 250, 10, 64, 64), ARB_MISS);

    // Potencia cero invalida
    comprobar("MISS potencia 0", evaluar(128, 128, 0, 100, 128, 128, 10, 64, 64), ARB_MISS);

    // Limites de la porteria: X=32,Y=32 es dentro
    comprobar("SAVE limite inferior", evaluar(32, 32, 100, 100, 32, 32, 10, 0, 0), ARB_SAVE);

    // Limites de la porteria: X=224 es fuera
    comprobar("MISS limite superior", evaluar(224, 128, 100, 100, 224, 128, 10, 64, 64), ARB_MISS);

    // Alcance exacto en el limite
    comprobar("SAVE alcance exacto", evaluar(138, 128, 100, 100, 128, 128, 10, 10, 10), ARB_SAVE);

    // Alcance excedido por 1
    comprobar("GOAL alcance por 1", evaluar(139, 128, 100, 100, 128, 128, 10, 10, 10), ARB_GOAL);

    // Reaccion exacta en el tiempo limite
    comprobar("SAVE reaccion exacta", evaluar(128, 128, 100, 50, 128, 128, 50, 10, 10), ARB_SAVE);

    std::cout << "Fallos: " << fallos << std::endl;
    return fallos == 0 ? 0 : 1;
}

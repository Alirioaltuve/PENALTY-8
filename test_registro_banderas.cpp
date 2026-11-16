#include <systemc.h>
#include <iostream>
#include "registro_banderas.h"

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
    sc_signal<bool> clk, reset, actualizar, carry_in, zero_in;
    sc_signal<bool> carry_out, zero_out;

    RegistroBanderas flags("flags");
    flags.clk(clk);
    flags.reset(reset);
    flags.actualizar(actualizar);
    flags.carry_in(carry_in);
    flags.zero_in(zero_in);
    flags.carry_out(carry_out);
    flags.zero_out(zero_out);

    auto pulso = [&]() {
        clk.write(false); sc_start(1, SC_NS);
        clk.write(true);  sc_start(1, SC_NS);
        clk.write(false); sc_start(1, SC_NS);
    };

    clk.write(false);
    reset.write(true);
    actualizar.write(false);
    carry_in.write(false);
    zero_in.write(false);
    pulso();
    reset.write(false);

    // Reset deja C=0, Z=0
    comprobar("Reset C", carry_out.read() ? 1 : 0, 0);
    comprobar("Reset Z", zero_out.read() ? 1 : 0, 0);

    // Actualizar con C=1, Z=0
    actualizar.write(true);
    carry_in.write(true);
    zero_in.write(false);
    pulso();
    comprobar("Actualizar C=1", carry_out.read() ? 1 : 0, 1);
    comprobar("Actualizar Z=0", zero_out.read() ? 1 : 0, 0);

    // Sin actualizar, con entradas distintas: conservar valores
    actualizar.write(false);
    carry_in.write(false);
    zero_in.write(true);
    pulso();
    comprobar("Conserva C", carry_out.read() ? 1 : 0, 1);
    comprobar("Conserva Z", zero_out.read() ? 1 : 0, 0);

    // Actualizar con C=0, Z=1
    actualizar.write(true);
    carry_in.write(false);
    zero_in.write(true);
    pulso();
    comprobar("Actualizar C=0", carry_out.read() ? 1 : 0, 0);
    comprobar("Actualizar Z=1", zero_out.read() ? 1 : 0, 1);

    std::cout << "Fallos: " << fallos << std::endl;
    return fallos == 0 ? 0 : 1;
}

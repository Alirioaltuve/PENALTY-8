#include <systemc.h>
#include <iostream>
#include "memoria_datos.h"

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
    sc_signal<bool> clk, reset, write_enable;
    sc_signal<sc_uint<8>> direccion, write_data, read_data;

    MemoriaDatos mem("mem");
    mem.clk(clk);
    mem.reset(reset);
    mem.write_enable(write_enable);
    mem.direccion(direccion);
    mem.write_data(write_data);
    mem.read_data(read_data);

    auto pulso = [&]() {
        clk.write(false); sc_start(1, SC_NS);
        clk.write(true);  sc_start(1, SC_NS);
        clk.write(false); sc_start(1, SC_NS);
    };

    clk.write(false);
    reset.write(true);
    write_enable.write(false);
    direccion.write(0);
    write_data.write(0);
    pulso();
    reset.write(false);

    // Escritura en direccion 5
    write_enable.write(true);
    direccion.write(5);
    write_data.write(0xAA);
    pulso();
    write_enable.write(false);

    direccion.write(5);
    sc_start(1, SC_NS);
    comprobar("Lectura tras escribir 0xAA en 5", read_data.read().to_uint(), 0xAA);

    // Sobrescritura
    write_enable.write(true);
    direccion.write(5);
    write_data.write(0x55);
    pulso();
    write_enable.write(false);
    direccion.write(5);
    sc_start(1, SC_NS);
    comprobar("Sobrescritura en 5", read_data.read().to_uint(), 0x55);

    // Direccion no escrita
    direccion.write(9);
    sc_start(1, SC_NS);
    comprobar("Direccion 9 sin escribir", read_data.read().to_uint(), 0);

    // Reset limpia la memoria
    reset.write(true);
    pulso();
    reset.write(false);
    direccion.write(5);
    sc_start(1, SC_NS);
    comprobar("Reset limpia direccion 5", read_data.read().to_uint(), 0);

    std::cout << "Fallos: " << fallos << std::endl;
    return fallos == 0 ? 0 : 1;
}

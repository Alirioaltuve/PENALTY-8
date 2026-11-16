#include <systemc.h>
#include "memoria_instrucciones.h"

int sc_main(int argc, char* argv[])
{
    // Señales de conexión
    sc_signal<sc_uint<8>> direccion;
    sc_signal<sc_uint<16>> instruccion;

    // Instanciamos la memoria
    MemoriaInstrucciones memoria("memoria");

    // Conectamos los puertos
    memoria.direccion(direccion);
    memoria.instruccion(instruccion);

    // PRUEBA 1: Dirección 5
    direccion.write(5);
    sc_start(1, SC_NS);

    cout << "Direccion 5: 0x"
         << hex << instruccion.read() << endl;

    // PRUEBA 2: Dirección 6
    direccion.write(6);
    sc_start(1, SC_NS);

    cout << "Direccion 6: 0x"
         << hex << instruccion.read() << endl;

    // PRUEBA 3: Dirección 0
    direccion.write(0);
    sc_start(1, SC_NS);

    cout << "Direccion 0: 0x"
         << hex << instruccion.read() << endl;

    return 0;
}

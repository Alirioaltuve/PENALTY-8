
#ifndef MEMORIA_INSTRUCCIONES_H
#define MEMORIA_INSTRUCCIONES_H

#include <systemc.h>
#include <vector>

SC_MODULE(MemoriaInstrucciones)
{
    // Entrada: dirección proporcionada por el PC
    sc_in<sc_uint<8>> direccion;

    // Salida: instrucción de 16 bits
    sc_out<sc_uint<16>> instruccion;

    // Memoria de 256 instrucciones de 16 bits
    sc_uint<16> memoria[256];

    // Lectura combinacional
    void leer()
    {
        instruccion.write(memoria[direccion.read()]);
    }

    // Carga un programa completo en la memoria. Se debe llamar
    // antes de iniciar la simulacion (sc_start). No depende de
    // instrucciones fijas escritas en el constructor.
    void cargar_programa(const std::vector<sc_uint<16>>& programa)
    {
        for (unsigned int i = 0; i < 256; i++)
        {
            if (i < programa.size())
                memoria[i] = programa[i];
            else
                memoria[i] = 0;
        }
    }

    SC_CTOR(MemoriaInstrucciones)
    {
        // Inicializamos toda la memoria en cero (sin programa fijo)
        for (int i = 0; i < 256; i++)
        {
            memoria[i] = 0;
        }

        // Configuramos la lectura
        SC_METHOD(leer);
        sensitive << direccion;
    }
};

#endif

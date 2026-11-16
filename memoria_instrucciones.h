
#ifndef MEMORIA_INSTRUCCIONES_H
#define MEMORIA_INSTRUCCIONES_H

#include <systemc.h>

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

    SC_CTOR(MemoriaInstrucciones)
    {
        // Inicializamos toda la memoria
        for (int i = 0; i < 256; i++)
        {
            memoria[i] = 0;
        }

        // Instrucción de prueba
        memoria[5] = 0x1234;

        // Configuramos la lectura
        SC_METHOD(leer);
        sensitive << direccion;
    }
};

#endif

#ifndef MEMORIA_DATOS_H
#define MEMORIA_DATOS_H

#include <systemc.h>

// ============================================================
// Memoria de datos de GOALKEEPER-8
// - 256 posiciones de 8 bits.
// - Lectura combinacional (se actualiza al cambiar la direccion
//   o el contenido de la memoria).
// - Escritura en flanco positivo del reloj.
// - Reset sincrono que limpia toda la memoria.
// Esta memoria es independiente del espacio de entrada/salida.
// ============================================================

SC_MODULE(MemoriaDatos)
{
    // -------------------- Entradas --------------------
    sc_in<bool> clk;
    sc_in<bool> reset;

    // Habilitacion de escritura (ST)
    sc_in<bool> write_enable;

    // Direccion de acceso
    sc_in<sc_uint<8>> direccion;

    // Dato que se desea almacenar
    sc_in<sc_uint<8>> write_data;

    // -------------------- Salidas ---------------------
    sc_out<sc_uint<8>> read_data;

    // -------------------- Estado interno --------------
    // Se usa sc_signal para que la lectura combinacional
    // reaccione tambien cuando el contenido cambia.
    sc_signal<sc_uint<8>> memoria[256];

    // Lectura combinacional
    void leer()
    {
        read_data.write(memoria[direccion.read()].read());
    }

    // Escritura secuencial y reset sincrono
    void escribir()
    {
        if (reset.read() == true)
        {
            for (int i = 0; i < 256; i++)
            {
                memoria[i].write(0);
            }
            return;
        }

        if (write_enable.read() == true)
        {
            memoria[direccion.read()].write(write_data.read());
        }
    }

    SC_CTOR(MemoriaDatos)
    {
        // Limpieza inicial
        for (int i = 0; i < 256; i++)
        {
            memoria[i].write(0);
        }

        // Proceso de lectura combinacional
        SC_METHOD(leer);
        sensitive << direccion;
        for (int i = 0; i < 256; i++)
        {
            sensitive << memoria[i];
        }

        // Proceso de escritura en el flanco positivo
        SC_METHOD(escribir);
        sensitive << clk.pos();
        dont_initialize();
    }
};

#endif // MEMORIA_DATOS_H

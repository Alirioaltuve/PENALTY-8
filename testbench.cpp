#include <systemc.h>
#include "banco_registros.h"

int sc_main(int argc, char* argv[])
{

    // ==========================================
    // 1. DECLARACION DE LAS SEÑALES
    // ==========================================

    sc_signal<bool> clk;
    sc_signal<bool> write_enable;

    sc_signal<sc_uint<3>> read_register_1;
    sc_signal<sc_uint<3>> read_register_2;
    sc_signal<sc_uint<3>> write_register;

    sc_signal<sc_uint<8>> write_data;

    sc_signal<sc_uint<8>> read_data_1;
    sc_signal<sc_uint<8>> read_data_2;

    // ==========================================
    // 2. INSTANCIAMOS EL BANCO DE REGISTROS
    // ==========================================

    BancoRegistros banco("BancoRegistros");

    // Conectamos las entradas
    banco.clk(clk);
    banco.write_enable(write_enable);

    banco.read_register_1(read_register_1);
    banco.read_register_2(read_register_2);
    banco.write_register(write_register);

    banco.write_data(write_data);

    // Conectamos las salidas
    banco.read_data_1(read_data_1);
    banco.read_data_2(read_data_2);

    // ==========================================
    // 3. ESTADO INICIAL
    // ==========================================

    clk.write(false);
    write_enable.write(false);

    read_register_1.write(0);
    read_register_2.write(0);

    write_register.write(0);
    write_data.write(0);

    //Dejamos que SystemC procese las señales
    sc_start(1, SC_NS);

    // ==========================================
    // 4. PRUEBA DE ESCRITURA
    // ==========================================

    // Escribimos 25 en R0
    write_enable.write(true);
    write_register.write(0);
    write_data.write(25);

    // Damos tiempo para actualizar las entradas
    sc_start(1, SC_NS);

    // Generamos un flanco positivo
    clk.write(true);
    sc_start(1, SC_NS);

    // Regresamos el reloj a cero
    clk.write(false);
    sc_start(1, SC_NS);

    // Escribimos 12 en R1
    write_register.write(1);
    write_data.write(12);

    sc_start(1, SC_NS);

    clk.write(true);
    sc_start(1, SC_NS);

    clk.write(false);
    sc_start(1, SC_NS);

    // ==========================================
    // 5. PRUEBA DE LECTURA
    // =========================================

    // Seleccionamos R0 y R1
    read_register_1.write(0);
    read_register_2.write(1);

    // Dejamos que se actualicen las salidas

    sc_start(1, SC_NS);

    cout << "Lectura de R0: "
        << read_data_1.read() << endl;
    
    cout << "Lectura de R1: "
        << read_data_2.read() << endl;    

    // ==========================================
    // 6. ESCRIBIMOS 37 EN R2
    // ==========================================

    write_register.write(2);
    write_data.write(37);

    sc_start(1, SC_NS);

    clk.write(true);
    sc_start(1, SC_NS);

    clk.write(false);
    sc_start(1, SC_NS);

    // Seleccionamos R2 para leerlo
    read_register_1.write(2);

    sc_start(1, SC_NS);

    cout << "Lectura de R2: "
        << read_data_1.read() << endl;

    // ==========================================
    // 7. PRUEBA DE ESCRITURA DESHABILITADA
    // ==========================================

    write_enable.write(false);

    write_register.write(2);
    write_data.write(50);

    sc_start(1, SC_NS);

    // Generamos otro flanco positivo
    clk.write(true);
    sc_start(1, SC_NS);

    clk.write(false);
    sc_start(1, SC_NS);

    //Volvemos a leer R2
    sc_start(1, SC_NS);

    cout << "R2 despues de intentar escribir 50: "
        << read_data_1.read() << endl;
    
    // ==========================================
    // 8. FINALIZAMOS LA SIMULACION
    // ==========================================

    
    sc_stop();

    return 0;

}
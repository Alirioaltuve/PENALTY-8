#include <systemc.h>
#include "pc.h" 

int sc_main(int argc, char* argv[])
{

    // ==========================================
    // 1. DECLARACION DE LAS SEÑALES
    // ==========================================

    sc_signal<bool> clk;

    sc_signal<bool> reset;

    sc_signal<bool> salto;

    sc_signal<sc_uint<8>> direccion_salto;

    sc_signal<sc_uint<8>> direccion_actual;

    // ==========================================
    // 2. INSTANCIAR EL MODULO PC
    // ==========================================

    PC pc("pc");

    // ==========================================
    // 3. CONECTAR LAS SEÑALES AL PC
    // ==========================================

    pc.clk(clk);

    pc.reset(reset);

    pc.salto(salto);

    pc.direccion_salto(direccion_salto);

    pc.direccion_actual(direccion_actual);

    // ==========================================
    // 4. INICIALIZAR LAS SEÑALES
    // ==========================================

    clk.write(false);

    reset.write(true);

    salto.write(false);

    direccion_salto.write(0);

    // ==========================================
    // 5. GENERAR EL PRIMER FLANCO DE RELOJ
    // ==========================================

    //Esperar 1 nanosegundo
    sc_start(1, SC_NS);

    // Generar flanco positivo
    clk.write(true);

    sc_start(1, SC_NS);

    cout << "========== PRUEBA DEL REINICIO DEL PC ==========" << endl;

    cout << "PC actual = " << direccion_actual.read() << endl ;

    // ==========================================
    // 6. PRUEBA DEL INCREMENTO SECUENCIAL
    // ==========================================

    // Bajar el reloj para preparar el siguiente flanco positivo
    clk.write(false);

    // Desactivar el reset
    reset.write(false);

    // No realizar ningún salto
    salto.write(false);

    // Permitir que las señales se actualicen
    sc_start(1, SC_NS);

    // Generar un nuevo flanco positivo
    clk.write(true);

    // Ejecutar la simulación
    sc_start(1, SC_NS);

    // Mostrar el resultado
    cout << "========== PRUEBA DEL INCREMENTO ==========" << endl;
    cout << "PC actual = " << direccion_actual.read() << endl;


    // ==========================================
    // 7. PRUEBA DE SALTO
    // ==========================================

    // Bajar el reloj
    clk.write(false);

    // Activar la señal de salto
    salto.write(true);

    // Establecer la dirección de destino
    direccion_salto.write(50);

    // Permitir que las señales se actualicen
    sc_start(1, SC_NS);

    // Generar flanco positivo
    clk.write(true);

    // Ejecutar la simulación
    sc_start(1, SC_NS);

    // Mostrar el resultado
    cout << "========== PRUEBA DEL SALTO ==========" << endl;
    cout << "PC actual = " << direccion_actual.read() << endl;

    // ==========================================
    // 8. PRUEBA DE PRIORIDAD DEL RESET
    // ==========================================

    // Bajar el reloj
    clk.write(false);

    // Activar reset y salto simultáneamente
    reset.write(true);
    salto.write(true);

    // Establecer una dirección de destino diferente de cero
    direccion_salto.write(120);

    // Permitir que las señales se actualicen
    sc_start(1, SC_NS);

    // Generar flanco positivo
    clk.write(true);

    // Ejecutar la simulación
    sc_start(1, SC_NS);

    // Mostrar el resultado
    cout << "========== PRUEBA DE PRIORIDAD DEL RESET ==========" << endl;
    cout << "PC actual = " << direccion_actual.read() << endl;

    // ==========================================
    // 9. PRUEBA DE DESBORDAMIENTO
    // ==========================================

    // Bajar el reloj
    clk.write(false);

    // Desactivar reset y salto
    reset.write(false);
    salto.write(true);

    // Cargar la dirección 255
    direccion_salto.write(255);

    sc_start(1, SC_NS);

    // Generar flanco positivo para cargar 255
    clk.write(true);

    sc_start(1, SC_NS);

    // Bajar nuevamente el reloj
    clk.write(false);

    // Desactivar el salto para permitir el incremento
    salto.write(false);

    sc_start(1, SC_NS);

    // Generar otro flanco positivo
    clk.write(true);

    sc_start(1, SC_NS);

    // Mostrar el resultado
    cout << "========== PRUEBA DE DESBORDAMIENTO ==========" << endl;
    cout << "PC actual = " << direccion_actual.read() << endl;
    
    return 0;
} 

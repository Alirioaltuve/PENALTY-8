#include <systemc.h>
#include "alu.h"

int sc_main(int argc, char* argv[])
{

    // ==========================================
    // 1. DECLARACION DE LAS SEÑALES
    // ==========================================

    sc_signal<sc_uint<8>> A;
    sc_signal<sc_uint<8>> B;

    sc_signal<sc_uint<3>> operacion;

    sc_signal<sc_uint<8>> resultado;

    sc_signal<bool> carry;
    sc_signal<bool> zero;

    // ==========================================
    // 2. INSTANCIAR LA ALU
    // ==========================================

    ALU alu("alu");

    // ==========================================
    // 3. CONECTAR LAS SEÑALES A LA ALU
    // ==========================================

    alu.A(A);
    alu.B(B);
    alu.operacion(operacion);

    alu.resultado(resultado);
    alu.carry(carry);
    alu.zero(zero);

    // ==========================================
    // 4. PRIMERA PRUEBA: OPERACION ADD
    // ==========================================

    // Introducir los operandos
    A.write(25);
    B.write(12);

    // Seleccionar ADD (codigo 000)
    operacion.write(0);

    // Ejecutar la simulacion durante 1 nanosegundo
    sc_start(1, SC_NS);

    // Mostrar los resultados
    cout << "========== PRUEBA ADD ==========" << endl;

    cout << "A: " << A.read() << endl;
    cout << "B: " << B.read() << endl;

    cout << "Resultado: " << resultado.read() << endl;
    cout << "Carry: " << carry.read() << endl;
    cout << "Zero: " << zero.read() << endl;

    // ==========================================
    // 5. SEGUNDA PRUEBA: ADD CON ACARREO
    // ==========================================

    A.write(200);
    B.write(100);

    operacion.write(0);

    sc_start(1, SC_NS);

    cout << "\n========== ADD CON ACARREO ==========" << endl;

    cout << "A: " << A.read() << endl;
    cout << "B: " << B.read() << endl;

    cout << "Resultado: " << resultado.read() << endl;
    cout << "Carry: " << carry.read() << endl;
    cout << "Zero: " << zero.read() << endl;

    // ==========================================
    // 6. TERCERA PRUEBA: SUB CON RESULTADO CERO
    // ==========================================

    A.write(50);
    B.write(50);

    operacion.write(1);

    sc_start(1, SC_NS);

    cout << "\n========== PRUEBA SUB ==========" << endl;

    cout << "A: " << A.read() << endl;
    cout << "B: " << B.read() << endl;

    cout << "Resultado: " << resultado.read() << endl;
    cout << "Carry: " << carry.read() << endl;
    cout << "Zero: " << zero.read() << endl;

    // ==========================================
    // 7. CUARTA PRUEBA: OPERACION AND
    // ==========================================

    A.write(8);
    B.write(4);

    operacion.write(2);

    sc_start(1, SC_NS);

    cout << "\n========== PRUEBA AND ==========" << endl;

    cout << "A: " << A.read() << endl;
    cout << "B: " << B.read() << endl;

    cout << "Resultado: " << resultado.read() << endl;
    cout << "Carry: " << carry.read() << endl;
    cout << "Zero: " << zero.read() << endl;

    // ==========================================
    // 8. QUINTA PRUEBA: OPERACION OR
    // ==========================================

    A.write(25);
    B.write(12);

    operacion.write(3);

    sc_start(1, SC_NS);

    cout << "\n========== PRUEBA OR ==========" << endl;

    cout << "A: " << A.read() << endl;
    cout << "B: " << B.read() << endl;

    cout << "Resultado: " << resultado.read() << endl;
    cout << "Carry: " << carry.read() << endl;
    cout << "Zero: " << zero.read() << endl;

     // ==========================================
    // 8. SEXTA PRUEBA: OPERACION CMP
    // ==========================================

    // Prueba 1
    A.write(25);
    B.write(25);

    operacion.write(4);

    sc_start(1, SC_NS);

    cout << "\n========== PRUEBA CMP 1 ==========" << endl;

    cout << "A: " << A.read() << endl;
    cout << "B: " << B.read() << endl;

    cout << "Resultado: " << resultado.read() << endl;
    cout << "Carry: " << carry.read() << endl;
    cout << "Zero: " << zero.read() << endl;

    // Prueba 2
    A.write(30);
    B.write(20);

    operacion.write(4);

    sc_start(1, SC_NS);

    cout << "\n========== PRUEBA CMP 2 ==========" << endl;

    cout << "A: " << A.read() << endl;
    cout << "B: " << B.read() << endl;

    cout << "Resultado: " << resultado.read() << endl;
    cout << "Carry: " << carry.read() << endl;
    cout << "Zero: " << zero.read() << endl;

    // ==========================================
    // 6. SEPTIMA PRUEBA: SUB CON RESULTADO CERO - CON ACARREO MODIFICADO
    // ==========================================

    A.write(50);
    B.write(20);

    operacion.write(1);

    sc_start(1, SC_NS);

    cout << "\n========== PRUEBA SUB 1 - NUEVO ACARREO==========" << endl;

    cout << "A: " << A.read() << endl;
    cout << "B: " << B.read() << endl;

    cout << "Resultado: " << resultado.read() << endl;
    cout << "Carry: " << carry.read() << endl;
    cout << "Zero: " << zero.read() << endl;


    A.write(15);
    B.write(15);

    operacion.write(1);

    sc_start(1, SC_NS);

    cout << "\n========== PRUEBA SUB 2 - NUEVO ACARREO==========" << endl;

    cout << "A: " << A.read() << endl;
    cout << "B: " << B.read() << endl;

    cout << "Resultado: " << resultado.read() << endl;
    cout << "Carry: " << carry.read() << endl;
    cout << "Zero: " << zero.read() << endl;

    A.write(10);
    B.write(25);

    operacion.write(1);

    sc_start(1, SC_NS);

    cout << "\n========== PRUEBA SUB 3 - NUEVO ACARREO==========" << endl;

    cout << "A: " << A.read() << endl;
    cout << "B: " << B.read() << endl;

    cout << "Resultado: " << resultado.read() << endl;
    cout << "Carry: " << carry.read() << endl;
    cout << "Zero: " << zero.read() << endl;

    return 0;

}
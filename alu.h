#include <systemc.h>

SC_MODULE(ALU)
{
    // Entradas de la ALU

    sc_in<sc_uint<8>> A;
    sc_in<sc_uint<8>> B;

    sc_in<sc_uint<3>> operacion;

    // Salida de la ALU

    sc_out<sc_uint<8>> resultado;

    // Salida del acarreo
    sc_out<bool> carry;

    sc_out<bool> zero;

    void ejecutar()
    {   
         // Leer los operandos
        sc_uint<8> valor_A = A.read();
        sc_uint<8> valor_B = B.read();

        // Variable temporal para almacenar el resultado
        sc_uint<8> valor_resultado = 0;

        switch (operacion.read())
        {
            case 0:
            {
                //ADD
                
                // Realizar suma utilizando 9 bits
                sc_uint<9> suma = valor_A + valor_B;

                // Extraer los 8 bits del resultado

                valor_resultado = suma.range(7, 0);

                // Extraer el bit de acarreo
                carry.write(suma[8]);

                break;
            }
            
            case 1:
            {
                //SUB

                valor_resultado = valor_A - valor_B;

                // Carry = 1 cuando NO existe prestamo
                carry.write(valor_A >= valor_B); 

                break;
            }
            
            case 2:
            {   //AND
                valor_resultado = valor_A & valor_B;
                carry.write(0);
                break;
            }
            
            case 3:
            {
                //OR
                valor_resultado = valor_A | valor_B;
                carry.write(0);
                break;
            }
            case 4:
            {
                // CMP: comparar los operandos mediante una resta

                valor_resultado = valor_A - valor_B;

                // Carry = 1 cuando A >= B
                carry.write(valor_A >= valor_B); 

                break;
            }
                
            default:
                // Operacion no reconocida
                valor_resultado = 0;
                carry.write(0);
                break;

        }

        // Actualizar las salidas
        resultado.write(valor_resultado);
        zero.write(valor_resultado == 0);
    }

    // Constructor del modulo
    SC_CTOR(ALU)
    {
        SC_METHOD(ejecutar);

        sensitive << A;
        sensitive << B;
        sensitive << operacion;
    }
};


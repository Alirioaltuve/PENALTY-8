
#include <systemc.h>

SC_MODULE(BancoRegistros)
{   
    // Señales y registros del banco de registros
    
      // ==========================================
    // 1. PUERTOS DE ENTRADA
    // ==========================================

    // Reloj del sistema
    sc_in<bool> clk;


    // Control de escritura
    sc_in<bool> write_enable;


    // Seleccion de registros
    // Selectores de los registros que queremos leer
    //¿Por qué utilizamos 3 bits?
    //Porque necesitamos identificar ocho registros diferentes.
    sc_in<sc_uint<3>> read_register_1;
    sc_in<sc_uint<3>> read_register_2;
    sc_in<sc_uint<3>> write_register;

    //Dato que queremos almacenar
    sc_in<sc_uint<8>> write_data;

    // ==========================================
    // 2. PUERTOS DE SALIDA
    // ==========================================

    //Datos obtenidos de las dos lecturas
    sc_out<sc_uint<8>> read_data_1;
    sc_out<sc_uint<8>> read_data_2;

    // ==========================================
    // 3. ALMACENAMIENTO INTERNO
    // ==========================================

    // Ocho registros internos de 8 bits
    sc_signal<sc_uint<8>> registros[8];

    // ==========================================
    // 4. PROCESO DE LECTURA
    // ==========================================

    void leer_registros()
    {
        // Obtenemos el indice del primer registro
        unsigned int indice_1 = 
            read_register_1.read().to_uint();

        // Obtenemos el indice del primer registro
        unsigned int indice_2 = 
            read_register_2.read().to_uint();

        // Leemos el contenido de ambos registros
        read_data_1.write(registros[indice_1].read());
        read_data_2.write(registros[indice_2].read());

    }

    // ==========================================
    // 5. PROCESO DE ESCRITURA
    // ==========================================
    void escribir_registro()
    {
        // Comprobamos si la lectura esta habilitada
        if (write_enable.read())
        {

            //Obtenemos el registro destino
            unsigned int indice =
                write_register.read().to_uint();
            
            // Guardamos el dato en el registro seleccionado
            // Alamcenamos el nuevo dato
            registros[indice].write(write_data.read());

        }


    }

    // ==========================================
    // 6. CONSTRUCTOR
    // ==========================================

    //Constructor del modulo
    SC_CTOR(BancoRegistros)
    {
        // Aquí se registran procesos o comportamiento del módulo

        //Registramos el proceso de lectura
        SC_METHOD(leer_registros);

        // La lectura se actualiza cuando cambia
        // cualquiera de los selectores

        sensitive << read_register_1;
        sensitive << read_register_2;

        // Tambien cuando cambia cualquiera
        // de los ocho registros internos

        for (int i = 0; i < 8; i++)
        {
            sensitive << registros[i];
        }

        // Registramos el proceso de escritura
        SC_METHOD(escribir_registro);

        // La escritura ocurre en el flanco positivo
        sensitive << clk.pos();

        // Evitamos una ejecucion inicial innecesaria
        dont_initialize();

        // Inicializamos los registros
        for (int i = 0; i < 8; i++)
        {
            registros[i].write(0);
        }
    }
};


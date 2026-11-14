
#include <systemc.h>

SC_MODULE(BancoRegistros)
{
    // Señales y registros del banco de registros

    SC_CTOR(BancoRegistros)
    {
        // Aquí se registran procesos o comportamiento del módulo
    }
};

int sc_main(int argc, char* argv[])
{
    BancoRegistros banco("banco");
    cout << "Hola";

    sc_start(1, SC_NS);
    return 0;
}

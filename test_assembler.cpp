#include <iostream>
#include <string>
#include <vector>
#include "assembler.h"

static int fallos = 0;

static void comprobar(const char* nombre, bool ok, const std::string& detalle = "")
{
    std::cout << (ok ? "[OK]   " : "[FALLO] ") << nombre;
    if (!detalle.empty()) std::cout << " -> " << detalle;
    std::cout << std::endl;
    if (!ok) fallos++;
}

using namespace assembler;

int sc_main(int argc, char* argv[])
{
    // 1. Programa basico de suma (compara contra la codificacion isa).
    {
        std::string asm_ =
            "LDI R1, 10\n"
            "LDI R2, 20\n"
            "ADD R3, R1, R2\n"
            "HALT\n";
        Resultado r = ensamblar(asm_);
        comprobar("suma: ok", r.ok, r.error);
        comprobar("suma: 4 instrucciones", r.instrucciones == 4, "");
        if (r.ok)
        {
            comprobar("suma: LDI R1,10", r.palabras[0] == isa::formato_i(isa::LDI, 1, 10), "");
            comprobar("suma: LDI R2,20", r.palabras[1] == isa::formato_i(isa::LDI, 2, 20), "");
            comprobar("suma: ADD R3,R1,R2", r.palabras[2] == isa::formato_r(isa::ADD, 3, 1, 2), "");
            comprobar("suma: HALT", r.palabras[3] == isa::formato_i(isa::HALT, 0, 0), "");
        }
    }

    // 2. Las 20 instrucciones codifican sin error.
    {
        std::string asm_ =
            "NOP\n"
            "LDI R0, 5\n"
            "MOV R1, R0\n"
            "ADD R2, R0, R1\n"
            "SUB R3, R2, R0\n"
            "AND R4, R2, R1\n"
            "OR R5, R2, R1\n"
            "CMP R0, R1\n"
            "JMP fin\n"
            "JEQ fin\n"
            "JNE fin\n"
            "JGE fin\n"
            "JLT fin\n"
            "JGT fin\n"
            "JLE fin\n"
            "LD R6, 0x10\n"
            "ST R6, 0x10\n"
            "IN R6, 0x11\n"
            "OUT R6, 0x30\n"
            "fin: HALT\n";
        Resultado r = ensamblar(asm_);
        comprobar("20 instr: ok", r.ok, r.error);
        comprobar("20 instr: 20 palabras", r.instrucciones == 20, "");
    }

    // 3. Valores hexadecimales y decimales equivalentes.
    {
        Resultado r = ensamblar("LDI R0, 0xFF\nLDI R0, 255\nHALT\n");
        comprobar("hex == decimal", r.ok && r.palabras[0] == r.palabras[1],
                  r.ok ? "" : r.error);
    }

    // 4. Comentarios y lineas vacias.
    {
        Resultado r = ensamblar(
            "; comentario\n"
            "\n"
            "LDI R0, 7   ; valor\n"
            "HALT\n"
            "# otro comentario\n");
        comprobar("comentarios/vacias: ok", r.ok && r.instrucciones == 2,
                  r.ok ? "" : r.error);
    }

    // 5. Etiquetas: salto hacia adelante y hacia atras.
    {
        std::string asm_ =
            "JMP fin\n"
            "LDI R0, 99\n"
            "fin: LDI R0, 1\n"
            "HALT\n";
        Resultado r = ensamblar(asm_);
        comprobar("etiqueta adelante", r.ok && r.palabras[0].range(10, 3).to_uint() == 2,
                  r.ok ? "" : r.error);

        std::string asm2 =
            "inicio: LDI R0, 1\n"
            "LDI R7, 2\n"
            "SUB R0, R0, R7\n"
            "JMP inicio\n";
        Resultado r2 = ensamblar(asm2);
        comprobar("etiqueta atras", r2.ok && r2.palabras[3].range(10, 3).to_uint() == 0,
                  r2.ok ? "" : r2.error);
    }

    // 6. Etiqueta duplicada.
    {
        Resultado r = ensamblar("a: LDI R0,1\na: HALT\n");
        comprobar("etiqueta duplicada: error", !r.ok, "");
    }

    // 7. Etiqueta inexistente.
    {
        Resultado r = ensamblar("JMP noexiste\nHALT\n");
        comprobar("etiqueta inexistente: error", !r.ok, "");
    }

    // 8. Instruccion desconocida.
    {
        Resultado r = ensamblar("FOO R0, R1\n");
        comprobar("instruccion desconocida: error", !r.ok, "");
    }

    // 9. Numero de operandos incorrecto.
    {
        Resultado r = ensamblar("ADD R0, R1\n");
        comprobar("operandos incorrectos: error", !r.ok, "");
    }

    // 10. Registro invalido.
    {
        Resultado r = ensamblar("LDI R8, 1\n");
        comprobar("registro invalido: error", !r.ok, "");
    }

    // 11. Inmediato fuera de rango.
    {
        Resultado r = ensamblar("LDI R0, 256\n");
        comprobar("inmediato >255: error", !r.ok, "");
    }

    // 12. Direccion de salto fuera de rango.
    {
        Resultado r = ensamblar("JMP 300\n");
        comprobar("salto >255: error", !r.ok, "");
    }

    // 13. Mas de 256 instrucciones (se rechaza, no se trunca).
    {
        std::string asm_;
        for (int i = 0; i < 257; i++)
            asm_ += "NOP\n";
        Resultado r = ensamblar(asm_);
        comprobar(">256 instrucciones: error", !r.ok, "");
    }

    // 14. El error indica numero de linea.
    {
        Resultado r = ensamblar("LDI R0, 1\nADD R9, R0, R0\nHALT\n");
        bool tiene_linea = r.error.find("linea 2") != std::string::npos;
        comprobar("error con numero de linea", !r.ok && tiene_linea, r.error);
    }

    // 15. ST y OUT usan el registro fuente en el campo rd.
    {
        Resultado r = ensamblar("ST R3, 0x10\nOUT R3, 0x30\n");
        bool ok = r.ok &&
                  r.palabras[0].range(10, 8).to_uint() == 3 &&
                  r.palabras[1].range(10, 8).to_uint() == 3;
        comprobar("ST/OUT fuente en campo rd", ok, "");
    }

    // 16. CMP usa rd=0 (dos operandos, ra y rb).
    {
        Resultado r = ensamblar("CMP R4, R7\n");
        bool ok = r.ok &&
                  r.palabras[0] == isa::formato_r(isa::CMP, 0, 4, 7);
        comprobar("CMP R4,R7", ok, "");
    }

    std::cout << "\nFallos: " << fallos << std::endl;
    return fallos == 0 ? 0 : 1;
}

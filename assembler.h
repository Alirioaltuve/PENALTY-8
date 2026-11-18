#ifndef ASSEMBLER_H
#define ASSEMBLER_H

// ============================================================
// Ensamblador de GOALKEEPER-8
// ------------------------------------------------------------
// Convierte texto ensamblador a palabras de 16 bits usando
// exclusivamente la ISA existente (isa.h) y sus helpers de
// codificacion. No define una segunda ISA.
//
// Sintaxis soportada:
//   LDI Rd, imm        (imm decimal o 0x.. hexadecimal, 0..255)
//   MOV Rd, Ra
//   ADD/SUB/AND/OR Rd, Ra, Rb
//   CMP Ra, Rb
//   JMP/JEQ/JNE/JGE/JLT/JGT/JLE etiqueta|addr
//   LD Rd, addr
//   ST Rs, addr        (Rs es el registro FUENTE, en el campo rd)
//   IN Rd, addr
//   OUT Rs, addr       (Rs es el registro FUENTE, en el campo rd)
//   HALT
//   NOP
//
// Comentarios con ';' o '#'. Etiquetas con ':' (dos pasadas).
// Direcciones absolutas de 8 bits (formato J existente).
// ============================================================

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <cstdlib>
#include <cctype>
#include "isa.h"

namespace assembler
{

struct Resultado
{
    bool ok;
    std::string error;                 // "linea N: mensaje" cuando !ok
    std::vector<sc_uint<16>> palabras; // instrucciones codificadas
    int instrucciones;                 // cantidad de instrucciones
};

// ---------- Utilidades de texto ----------
static std::string trim(const std::string& s)
{
    size_t a = s.find_first_not_of(" \t\r");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r");
    return s.substr(a, b - a + 1);
}

static std::string mayusculas(const std::string& s)
{
    std::string r = s;
    for (char& c : r) c = (char)std::toupper((unsigned char)c);
    return r;
}

// Quita el comentario (desde ';' o '#') y recorta.
static std::string sin_comentario(const std::string& s)
{
    std::string r;
    for (char c : s)
    {
        if (c == ';' || c == '#') break;
        r.push_back(c);
    }
    return trim(r);
}

// Divide la linea en etiqueta (opcional) + resto.
static void partir_etiqueta(const std::string& linea, std::string& etiqueta,
                            std::string& resto)
{
    etiqueta.clear();
    resto = linea;
    size_t dos_puntos = linea.find(':');
    if (dos_puntos != std::string::npos)
    {
        // Solo es etiqueta si el ':' no pertenece a un '0x' (no aplica aqui,
        // las etiquetas van al inicio). Se trata todo lo anterior como nombre.
        std::string antes = trim(linea.substr(0, dos_puntos));
        std::string despues = trim(linea.substr(dos_puntos + 1));
        // El nombre de etiqueta no debe contener espacios.
        if (antes.find(' ') == std::string::npos &&
            antes.find('\t') == std::string::npos &&
            !antes.empty())
        {
            etiqueta = antes;
            resto = despues;
        }
        // Si no es un nombre valido (p. ej. contiene espacios), se deja
        // el ':' dentro del resto y fallara en el parseo de operandos.
    }
}

// Divide el resto en tokens separados por comas o espacios.
static std::vector<std::string> tokenizar(const std::string& s)
{
    std::vector<std::string> tokens;
    std::string actual;
    for (char c : s)
    {
        if (c == ',' || c == ' ' || c == '\t')
        {
            if (!actual.empty()) { tokens.push_back(actual); actual.clear(); }
        }
        else
        {
            actual.push_back(c);
        }
    }
    if (!actual.empty()) tokens.push_back(actual);
    return tokens;
}

// Convierte un operando numerico (decimal o 0x..) a entero. -1 si invalido.
static int parse_valor(const std::string& tok, bool& ok)
{
    ok = false;
    if (tok.empty()) return 0;
    std::string t = tok;
    int base = 10;
    if (t.size() > 2 && (t[0] == '0' && (t[1] == 'x' || t[1] == 'X')))
    {
        base = 16;
        t = t.substr(2);
        if (t.empty()) return 0;
    }
    else if (t[0] == '$')
    {
        base = 16;
        t = t.substr(1);
        if (t.empty()) return 0;
    }
    for (char c : t)
        if (!std::isxdigit((unsigned char)c)) return 0;
    // isxdigit admite a-f para decimal, por lo que se valida la base.
    if (base == 10)
        for (char c : t)
            if (!std::isdigit((unsigned char)c)) return 0;

    char* fin = nullptr;
    long v = std::strtol(t.c_str(), &fin, base);
    if (fin == t.c_str() || *fin != '\0') return 0;
    ok = true;
    return (int)v;
}

// Convierte un operando de registro ("R0".."R7"). -1 si invalido.
static int parse_registro(const std::string& tok)
{
    std::string t = mayusculas(trim(tok));
    if (t.size() < 2 || t[0] != 'R') return -1;
    if (!std::isdigit((unsigned char)t[1])) return -1;
    int r = t[1] - '0';
    if (r < 0 || r > 7) return -1;
    // No admite "R10" (dos digitos).
    if (t.size() != 2) return -1;
    return r;
}

// Instruccion parseada de una linea (sin etiqueta).
struct Instruccion
{
    int linea;
    std::string mnemonico;
    std::vector<std::string> operandos;
};

static Resultado ensamblar(const std::string& codigo)
{
    Resultado res;
    res.ok = true;
    res.instrucciones = 0;

    // --- Separar en lineas ---
    std::vector<std::string> lineas;
    std::string actual;
    for (char c : codigo)
    {
        if (c == '\n')
        {
            lineas.push_back(actual);
            actual.clear();
        }
        else if (c != '\r')
        {
            actual.push_back(c);
        }
    }
    if (!actual.empty()) lineas.push_back(actual);

    // --- Parsear cada linea en (etiqueta, instruccion) ---
    struct Item
    {
        int linea;
        std::string etiqueta;
        std::string mnemonico;
        std::vector<std::string> operandos;
    };
    std::vector<Item> items;
    items.reserve(lineas.size());

    for (size_t i = 0; i < lineas.size(); i++)
    {
        int n = (int)i + 1;
        std::string limpia = sin_comentario(lineas[i]);
        if (limpia.empty()) continue;

        std::string etiqueta, resto;
        partir_etiqueta(limpia, etiqueta, resto);

        Item it;
        it.linea = n;
        it.etiqueta = etiqueta;
        it.mnemonico.clear();

        std::string instr = trim(resto);
        if (!instr.empty())
        {
            std::vector<std::string> toks = tokenizar(instr);
            if (toks.empty())
            {
                res.ok = false;
                res.error = "linea " + std::to_string(n) + ": instruccion vacia";
                return res;
            }
            it.mnemonico = mayusculas(toks[0]);
            for (size_t k = 1; k < toks.size(); k++)
                it.operandos.push_back(toks[k]);
        }
        items.push_back(it);
    }

    // --- Pasada 1: asignar direcciones y recoger etiquetas ---
    std::map<std::string, int> etiquetas;
    int direccion = 0;
    for (const Item& it : items)
    {
        if (!it.etiqueta.empty())
        {
            if (etiquetas.count(it.etiqueta))
            {
                res.ok = false;
                res.error = "linea " + std::to_string(it.linea) +
                            ": etiqueta duplicada '" + it.etiqueta + "'";
                return res;
            }
            etiquetas[it.etiqueta] = direccion;
        }
        if (!it.mnemonico.empty())
        {
            direccion++;
            if (direccion > 256)
            {
                res.ok = false;
                res.error = "linea " + std::to_string(it.linea) +
                            ": el programa supera 256 instrucciones";
                return res;
            }
        }
    }

    // --- Pasada 2: codificar ---
    for (const Item& it : items)
    {
        if (it.mnemonico.empty()) continue;  // linea solo de etiqueta

        const std::string& m = it.mnemonico;
        const std::vector<std::string>& o = it.operandos;

        auto err = [&](const std::string& msg) -> Resultado {
            res.ok = false;
            res.error = "linea " + std::to_string(it.linea) + ": " + msg;
            return res;
        };

        auto num_ops = [&](size_t esperado) -> bool {
            if (o.size() != esperado)
            {
                err("'" + m + "' espera " + std::to_string(esperado) +
                    " operando(s), recibio " + std::to_string(o.size()));
                return false;
            }
            return true;
        };

        // --- Sin operandos ---
        if (m == "NOP")
        {
            if (!num_ops(0)) return res;
            res.palabras.push_back(isa::formato_r(isa::NOP, 0, 0, 0));
        }
        else if (m == "HALT")
        {
            if (!num_ops(0)) return res;
            res.palabras.push_back(isa::formato_i(isa::HALT, 0, 0));
        }
        // --- Un operando (saltos) ---
        else if (m == "JMP" || m == "JEQ" || m == "JNE" || m == "JGE" ||
                 m == "JLT" || m == "JGT" || m == "JLE")
        {
            if (!num_ops(1)) return res;
            isa::Opcode op;
            if (m == "JMP") op = isa::JMP;
            else if (m == "JEQ") op = isa::JEQ;
            else if (m == "JNE") op = isa::JNE;
            else if (m == "JGE") op = isa::JGE;
            else if (m == "JLT") op = isa::JLT;
            else if (m == "JGT") op = isa::JGT;
            else op = isa::JLE;

            int dir;
            if (etiquetas.count(o[0]))
                dir = etiquetas[o[0]];
            else
            {
                bool oknum = false;
                dir = parse_valor(o[0], oknum);
                if (!oknum || dir < 0 || dir > 255)
                    return err("destino invalido '" + o[0] + "' (etiqueta inexistente o direccion fuera de 0..255)");
            }
            res.palabras.push_back(isa::formato_j(op, dir));
        }
        // --- Dos operandos: registro + valor ---
        else if (m == "LDI" || m == "LD" || m == "ST" || m == "IN" || m == "OUT")
        {
            if (!num_ops(2)) return res;
            int r = parse_registro(o[0]);
            if (r < 0) return err("registro invalido '" + o[0] + "'");

            int v = 0;
            bool oknum = false;
            v = parse_valor(o[1], oknum);
            if (!oknum || v < 0 || v > 255)
                return err("valor/direccion invalido '" + o[1] + "' (0..255)");

            if (m == "LDI") res.palabras.push_back(isa::formato_i(isa::LDI, r, v));
            else if (m == "LD") res.palabras.push_back(isa::formato_i(isa::LD, r, v));
            else if (m == "ST") res.palabras.push_back(isa::formato_i(isa::ST, r, v));
            else if (m == "IN") res.palabras.push_back(isa::formato_i(isa::IN, r, v));
            else res.palabras.push_back(isa::formato_i(isa::OUT, r, v));
        }
        // --- Dos operandos: MOV Rd,Ra y CMP Ra,Rb ---
        else if (m == "MOV")
        {
            if (!num_ops(2)) return res;
            int rd = parse_registro(o[0]);
            int ra = parse_registro(o[1]);
            if (rd < 0) return err("registro invalido '" + o[0] + "'");
            if (ra < 0) return err("registro invalido '" + o[1] + "'");
            res.palabras.push_back(isa::formato_r(isa::MOV, rd, ra, 0));
        }
        else if (m == "CMP")
        {
            if (!num_ops(2)) return res;
            int ra = parse_registro(o[0]);
            int rb = parse_registro(o[1]);
            if (ra < 0) return err("registro invalido '" + o[0] + "'");
            if (rb < 0) return err("registro invalido '" + o[1] + "'");
            res.palabras.push_back(isa::formato_r(isa::CMP, 0, ra, rb));
        }
        // --- Tres operandos: aritmetica/logica ---
        else if (m == "ADD" || m == "SUB" || m == "AND" || m == "OR")
        {
            if (!num_ops(3)) return res;
            int rd = parse_registro(o[0]);
            int ra = parse_registro(o[1]);
            int rb = parse_registro(o[2]);
            if (rd < 0) return err("registro invalido '" + o[0] + "'");
            if (ra < 0) return err("registro invalido '" + o[1] + "'");
            if (rb < 0) return err("registro invalido '" + o[2] + "'");
            isa::Opcode op;
            if (m == "ADD") op = isa::ADD;
            else if (m == "SUB") op = isa::SUB;
            else if (m == "AND") op = isa::AND;
            else op = isa::OR;
            res.palabras.push_back(isa::formato_r(op, rd, ra, rb));
        }
        else
        {
            return err("instruccion desconocida '" + m + "'");
        }
    }

    res.instrucciones = (int)res.palabras.size();
    res.ok = true;
    res.error.clear();
    return res;
}

} // namespace assembler

#endif // ASSEMBLER_H

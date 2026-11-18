// 	Secuenciador reescrito como pipeline IF/ID/EX/MEM/WB

#include "striker.h"
#include "programa_striker.h"

//  Nombres de las instrucciones (documentacion y trazas)
namespace isa8 {

const char* nombre_instruccion(unsigned operacion) {
    switch (operacion) {
        case OP_NOP:  return "NOP";
        case OP_MOV:  return "MOV";
        case OP_ADD:  return "ADD";
        case OP_SUB:  return "SUB";
        case OP_AND:  return "AND";
        case OP_OR:   return "OR";
        case OP_CMP:  return "CMP";
        case OP_LDI:  return "LDI";
        case OP_LD:   return "LD";
        case OP_ST:   return "ST";
        case OP_IN:   return "IN";
        case OP_OUT:  return "OUT";
        case OP_JMP:  return "JMP";
        case OP_JEQ:  return "JEQ";
        case OP_JNE:  return "JNE";
        case OP_JGE:  return "JGE";
        case OP_JLT:  return "JLT";
        case OP_JGT:  return "JGT";
        case OP_JLE:  return "JLE";
        case OP_HALT: return "HALT";
        default:      return "RESERVADA";
    }
}

} // namespace isa8


//  Devuelve ResultadoALU sin usar señales
isa8::ResultadoALU ALU8::evaluar(unsigned a, unsigned b, unsigned op) {
    return isa8::evaluar_alu(a, b, op);
}

void ALU8::calcular() {
    const isa8::ResultadoALU r = evaluar(a.read().to_uint(),
                                         b.read().to_uint(),
                                         operacion.read().to_uint());
    resultado.write(r.resultado);
    cero.write(r.cero);
    acarreo.write(r.acarreo);
}

//  Usa isa8::decodificar_control()
void UnidadControl::decodificar() {
    const isa8::ControlSenales s =
        isa8::decodificar_control(codigo_operacion.read().to_uint());

    operacion_alu.write(s.operacion_alu);
    condicion.write(s.condicion);
    escribe_registro.write(s.escribe_registro);
    lee_memoria.write(s.lee_memoria);
    escribe_memoria.write(s.escribe_memoria);
    es_entrada.write(s.es_entrada);
    es_salida.write(s.es_salida);
    es_salto.write(s.es_salto);
    es_halt.write(s.es_halt);
    es_mov.write(s.es_mov);
    es_literal.write(s.es_literal);
    actualiza_banderas.write(s.actualiza_banderas);
    instruccion_invalida.write(s.instruccion_invalida);
}

//  BancoRegistros
void BancoRegistros::escribir() {
    if (escritura.read()) {
        registros[direccion_escritura.read().to_uint()] = dato_escritura.read();
    }
}

//  MemoriaInstrucciones
void MemoriaInstrucciones::leer() {
    dato.write(memoria[direccion.read().to_uint()]);
}

bool MemoriaInstrucciones::cargar(const uint16_t* programa, size_t tamano) {
    if (programa == nullptr) return false;

    bool completo = true;
    if (tamano > 256u) {
        tamano = 256u;      // truncado seguro, nunca desborda
        completo = false;
    }
    // Limpia las 256 palabras antes de copiar.  Asi un programa corto
    // cargado despues de uno mas largo no deja instrucciones residuales:
    // las palabras sobrantes quedan como OP_NOP (0x0000).
    for (int i = 0; i < 256; ++i) {
        memoria[i] = 0;
    }
    for (size_t i = 0; i < tamano; ++i) {
        memoria[i] = programa[i];
    }

    // Refresca la salida combinacional para que el primer ciclo de
    // busqueda lea la palabra correcta aunque el PC no cambie.
    leer();
    return completo;
}

void MemoriaInstrucciones::limpiar() {
    for (int i = 0; i < 256; ++i) memoria[i] = 0;
    leer();
}

//  MemoriaDatos
void MemoriaDatos::escribir() {
    if (escritura.read()) {
        memoria[direccion.read().to_uint()] =
            static_cast<uint8_t>(dato_entrada.read().to_uint());
    }
}

void MemoriaDatos::limpiar() {
    for (int i = 0; i < 256; ++i) memoria[i] = 0;
}

//  CommunicationMailbox
void CommunicationMailbox::limpiar_estado() {
    for (int i = 0; i < 5; ++i) req[i] = 0;
    for (int i = 0; i < 6; ++i) resp[i] = 0;

    req_mask = 0;
    req_idx = 0;
    resp_idx = 0;
    resp_count = 0;

    busy_reg = false;
    pendiente = false;
    listo_reg = false;
    error_reg = false;

    ticks_congelados = false;
    ticks_reg = 0;
}

void CommunicationMailbox::procesar() {
    // ---- Reset sincrono del buzon ----
    if (reset.read()) {
        limpiar_estado();

        req_valid.write(false);
        for (int i = 0; i < 5; ++i) req_byte[i].write(0);
        for (int i = 0; i < 6; ++i) resp_byte[i].write(0);

        busy.write(false);
        resp_ready.write(false);
        error.write(false);
        reaction_ticks.write(0);
        dato_lectura.write(0);
        return;
    }

    // El pulso de peticion dura exactamente este ciclo.
    req_valid.write(false);

    // ---- Contador de reaccion ----
    // Cuenta ciclos desde la aceptacion de REQ hasta el primer byte de
    // respuesta.  Despues queda congelado.
    if (pendiente && !ticks_congelados && ticks_reg < 0xFFFFu) {
        ++ticks_reg;
    }

    // ---- Escrituras de la CPU ----
    if (escribe.read()) {
        const unsigned p = puerto.read().to_uint();
        const unsigned d = dato_escritura.read().to_uint() & 0xFFu;

        switch (p) {
            case 0x10: // REQ
                if (d != 0) {
                    if (solicitud_completa()) {
                        // Se acepta la peticion: comienza una transaccion.
                        busy_reg = true;
                        pendiente = true;
                        listo_reg = false;
                        error_reg = false;
                        resp_count = 0;
                        for (int i = 0; i < 6; ++i) resp[i] = 0;

                        req_idx = 0;
                        resp_idx = 0;
                        ticks_reg = 0;
                        ticks_congelados = false;

                        // Pulso de un ciclo con la peticion de 5 bytes.
                        req_valid.write(true);
                        for (int i = 0; i < 5; ++i) req_byte[i].write(req[i]);

                        busy.write(true);
                        resp_ready.write(false);
                        for (int i = 0; i < 6; ++i) resp_byte[i].write(0);
                        reaction_ticks.write(0);
                    } else {
                        // No se puede pedir una transaccion sin los 5 bytes.
                        error_reg = true;
                    }
                }
                break;

            case 0x11: // ACK
                if (d != 0) {
                    // Confirma la respuesta.  Los 6 bytes se conservan.
                    listo_reg = false;
                    resp_ready.write(false);
                }
                break;

            case 0x13: // REQ_IDX
                if (d < 5) req_idx = d;
                else error_reg = true;
                break;

            case 0x14: // REQ_DATA
                if (req_idx < 5) {
                    req[req_idx] = static_cast<uint8_t>(d);
                    req_mask |= (1u << req_idx);
                } else {
                    error_reg = true;
                }
                break;

            case 0x15: // RESP_IDX
                if (d < 6) resp_idx = d;
                else error_reg = true;
                break;

            case 0x16: // RESP_DATA (solo lectura)
            case 0x17: // TICKS_L (solo lectura)
            case 0x18: // TICKS_H (solo lectura)
                break;

            case 0x24: // CLEAR
                if (d != 0) {
                    limpiar_estado();
                    req_valid.write(false);
                    for (int i = 0; i < 5; ++i) req_byte[i].write(0);
                    for (int i = 0; i < 6; ++i) resp_byte[i].write(0);
                    busy.write(false);
                    resp_ready.write(false);
                    reaction_ticks.write(0);
                }
                break;

            default:
                if (p >= 0x19 && p <= 0x1D) {
                    const unsigned i = p - 0x19;
                    req[i] = static_cast<uint8_t>(d);
                    req_mask |= (1u << i);
                }
                // Cualquier otro puerto de escritura se ignora.
                break;
        }
    }

    // ---- Recepcion de la respuesta de GOALKEEPER-8 ----
    if (resp_valid.read()) {
        const unsigned j = resp_indice.read().to_uint();

        if (listo_reg && !pendiente) {
            // Respuesta ya completa: se ignora un byte adicional.
        } else if (!pendiente || j >= 6 || j != resp_count) {
            // Indice invalido o byte fuera del orden 0..5.
            error_reg = true;
        } else {
            resp[j] = static_cast<uint8_t>(resp_dato.read().to_uint() & 0xFFu);
            resp_byte[j].write(resp[j]);

            if (resp_count == 0 && !ticks_congelados) {
                // Primer byte: reaction_ticks queda congelado.
                ticks_congelados = true;
            }

            ++resp_count;
            if (resp_count == 6) {
                listo_reg = true;
                busy_reg = false;
                pendiente = false;
            }
        }
    }

    // ---- Salidas de estado ----
    busy.write(busy_reg);
    resp_ready.write(listo_reg);
    error.write(error_reg);
    reaction_ticks.write(ticks_reg);
    dato_lectura.write(leer(puerto.read().to_uint()));
}

unsigned CommunicationMailbox::leer(unsigned p) const {
    switch (p) {
        case 0x10: // REQ
            return (busy_reg || pendiente) ? 1u : 0u;

        case 0x11: // ACK
            return listo_reg ? 1u : 0u;

        case 0x12: // STATE
            return (busy_reg  ? 0x01u : 0u) |
                   (pendiente ? 0x02u : 0u) |
                   (listo_reg ? 0x04u : 0u) |
                   (error_reg ? 0x08u : 0u);

        case 0x13: // REQ_IDX
            return req_idx;

        case 0x14: // REQ_DATA
            return (req_idx < 5) ? req[req_idx] : 0u;

        case 0x15: // RESP_IDX
            return resp_idx;

        case 0x16: // RESP_DATA
            return (resp_idx < 6) ? resp[resp_idx] : 0u;

        case 0x17: // TICKS_L
            return ticks_reg & 0xFFu;

        case 0x18: // TICKS_H
            return (ticks_reg >> 8) & 0xFFu;

        case 0x24: // CLEAR
            return 0u;

        case 0x25: // IRQ
            return listo_reg ? 1u : 0u;

        default:
            if (p >= 0x19 && p <= 0x1D) return req[p - 0x19];
            if (p >= 0x1E && p <= 0x23) return resp[p - 0x1E];
            return 0u;
    }
}

//  Striker8 - secuenciador segmentado (IF / ID / EX / MEM / WB)
//
//  Cada flanco de subida hace lo siguiente:
//    1. WB escribe el banco de registros (write-first).
//    2. EX calcula la ALU con forwarding desde EX/MEM y MEM/WB.
//    3. ID decodifica, lee registros y resuelve los saltos con las
//       banderas reenviadas desde EX.
//    4. MEM lee/escribe la memoria de datos y los puertos de E/S.
//    5. Se actualizan los registros de pipeline y el PC.
//
//  Los saltos se resuelven en ID: si se toma el salto se descarta la
//  instruccion que estaba en IF y se pierde un solo ciclo.
void Striker8::procesar() {
    // Las habilitaciones de escritura duran exactamente un ciclo.
    sig_banco_escritura.write(false);
    sig_escribe_mem.write(false);
    sig_com_escribir.write(false);

    // ---- Reset sincrono ----
    if (reset.read()) {
        reiniciar();
        return;
    }

    // ---- CPU detenida: solo se mantienen las salidas ----
    if (estado == DETENIDO) {
        pc_actual.write(pc_reg);
        ir_actual.write(ir_reg);
        bandera_cero.write(z);
        bandera_acarreo.write(c);
        error_instruccion.write(error_instr);
        return;
    }

    // ---- Espera de la orden de lanzamiento ----
    if (estado == REPOSO) {
        if (lanzar.read()) {
            estado = ACTIVO;
            ocupado.write(true);
            detenido.write(false);
            pc_reg = 0;
            parada_pendiente = false;
            inicializar_pipeline();
        }
        pc_actual.write(pc_reg);
        ir_actual.write(ir_reg);
        bandera_cero.write(z);
        bandera_acarreo.write(c);
        error_instruccion.write(error_instr);
        return;
    }

    ++ciclos;

    // =================================================================
    //  ETAPA WB: escritura del banco de registros.
    //  Se hace antes de ID para que una instruccion pueda leer, en el
    //  mismo ciclo, el resultado de la que se esta retirando.
    // =================================================================
    if (mem_wb.valido && mem_wb.escribe_registro) {
        banco->escribir_directo(mem_wb.rd, mem_wb.dato);
    }

    // =================================================================
    //  ETAPA EX: operandos con forwarding y calculo de la ALU.
    // =================================================================
    unsigned ex_valor1 = id_ex.valor1;
    unsigned ex_valor2 = id_ex.valor2;

    // Forwarding desde la instruccion que esta en MEM (EX/MEM).
    // No se reenvia desde un LD/IN en MEM porque su dato aun no existe;
    // para ese caso la etapa ID inserta un ciclo de espera.
    if (id_ex.usa_fuente1) {
        if (ex_mem.valido && ex_mem.control.escribe_registro &&
            !ex_mem.control.lee_memoria && !ex_mem.control.es_entrada &&
            ex_mem.rd == id_ex.fuente1) {
            ex_valor1 = ex_mem.resultado;
        } else if (mem_wb.valido && mem_wb.escribe_registro &&
                   mem_wb.rd == id_ex.fuente1) {
            ex_valor1 = mem_wb.dato;
        }
    }

    if (id_ex.usa_fuente2) {
        if (ex_mem.valido && ex_mem.control.escribe_registro &&
            !ex_mem.control.lee_memoria && !ex_mem.control.es_entrada &&
            ex_mem.rd == id_ex.fuente2) {
            ex_valor2 = ex_mem.resultado;
        } else if (mem_wb.valido && mem_wb.escribe_registro &&
                   mem_wb.rd == id_ex.fuente2) {
            ex_valor2 = mem_wb.dato;
        }
    }

    // La ALU se evalua siempre; solo se usa si la instruccion lo requiere.
    const isa8::ResultadoALU alu_ex =
        ALU8::evaluar(ex_valor1, ex_valor2, id_ex.control.operacion_alu);

    unsigned ex_resultado = alu_ex.resultado;
    if (id_ex.control.es_mov) {
        // MOV: copia directa de Rs1.
        ex_resultado = ex_valor1;
    } else if (id_ex.control.es_literal) {
        // LDI: carga del inmediato de 8 bits.
        ex_resultado = id_ex.inmediato;
    }

    // Dato que ST guarda en memoria o que OUT envia al puerto.
    const unsigned ex_dato_guardar = ex_valor1;

    // =================================================================
    //  ETAPA MEM: memoria de datos, puertos y buzon.
    // =================================================================
    unsigned mem_dato = ex_mem.resultado;

    if (ex_mem.valido) {
        if (ex_mem.control.lee_memoria) {
            // LD: lectura combinacional de la memoria de datos.
            mem_dato = memoria_datos->contenido(ex_mem.direccion);
        } else if (ex_mem.control.es_entrada) {
            // IN: lectura combinacional del puerto de E/S.
            mem_dato = leer_puerto(ex_mem.direccion);
        }

        if (ex_mem.control.escribe_memoria) {
            // ST: escritura directa en la memoria de datos.
            memoria_datos->escribir_directo(ex_mem.direccion,
                                            ex_mem.dato_guardar);
        }

        if (ex_mem.control.es_salida) {
            // OUT: se dejan listos los cables del buzon.  El buzon los
            // consume en el siguiente flanco, igual que en el diseno
            // multiciclo original.
            sig_com_puerto.write(ex_mem.direccion & 0xFFu);
            sig_com_dato.write(ex_mem.dato_guardar & 0xFFu);
            sig_com_escribir.write(true);
        }
    }

    // =================================================================
    //  ETAPA ID: decodificacion, lectura de registros y saltos.
    // =================================================================
    EtapaID_EX id_ex_siguiente;   // por defecto es una burbuja

    bool stall = false;
    bool salto_tomado = false;
    unsigned pc_salto = 0;
    bool halt_en_id = false;

    if (if_id.valido) {
        const unsigned instr = if_id.instruccion;
        const unsigned op = isa8::codigo(instr);
        const isa8::ControlSenales ctrl = isa8::decodificar_control(op);

        id_ex_siguiente.valido = true;
        id_ex_siguiente.instruccion = if_id.instruccion;
        id_ex_siguiente.pc = if_id.pc;
        id_ex_siguiente.control = ctrl;
        id_ex_siguiente.rd = isa8::campo_d(instr);
        id_ex_siguiente.inmediato = isa8::inmediato(instr);

        // ---- Lectura de los registros fuente ----
        const unsigned rs1 = isa8::campo_s1(instr);
        const unsigned rs2 = isa8::campo_s2(instr);

        if (isa8::formato_r(op)) {
            id_ex_siguiente.fuente1 = rs1;
            id_ex_siguiente.usa_fuente1 = true;
            id_ex_siguiente.valor1 = banco->valor(rs1);

            if (op != isa8::OP_MOV) {
                id_ex_siguiente.fuente2 = rs2;
                id_ex_siguiente.usa_fuente2 = true;
                id_ex_siguiente.valor2 = banco->valor(rs2);
            }
        } else if (op == isa8::OP_ST || op == isa8::OP_OUT) {
            // En formato I el campo Rd identifica el registro fuente.
            id_ex_siguiente.fuente1 = id_ex_siguiente.rd;
            id_ex_siguiente.usa_fuente1 = true;
            id_ex_siguiente.valor1 = banco->valor(id_ex_siguiente.rd);
        }

        // ---- Peligro de datos load-use ----
        // Usa isa8::decodificar_control()
        if (id_ex.valido &&
            (id_ex.control.lee_memoria || id_ex.control.es_entrada) &&
            id_ex.control.escribe_registro &&
            ((id_ex_siguiente.usa_fuente1 &&
              id_ex.rd == id_ex_siguiente.fuente1) ||
             (id_ex_siguiente.usa_fuente2 &&
              id_ex.rd == id_ex_siguiente.fuente2))) {
            stall = true;
        }

        // ---- Saltos resueltos en ID ----
        // Las banderas se reenvian desde EX si la instruccion que esta
        // alli las acaba de calcular (por ejemplo CMP seguido de JLT).
        const bool z_efectivo =
            (id_ex.valido && id_ex.control.actualiza_banderas)
                ? alu_ex.cero : z;
        const bool c_efectivo =
            (id_ex.valido && id_ex.control.actualiza_banderas)
                ? alu_ex.acarreo : c;

        if (ctrl.es_salto && !parada_pendiente) {
            bool tomar = false;
            switch (op) {
                case isa8::OP_JMP: tomar = true;              break;
                case isa8::OP_JEQ: tomar = z_efectivo;        break;
                case isa8::OP_JNE: tomar = !z_efectivo;       break;
                case isa8::OP_JGE: tomar = c_efectivo;        break;
                case isa8::OP_JLT: tomar = !c_efectivo;       break;
                case isa8::OP_JGT: tomar = c_efectivo && !z_efectivo; break;
                case isa8::OP_JLE: tomar = !c_efectivo || z_efectivo; break;
                default:           tomar = false;             break;
            }
            if (tomar) {
                salto_tomado = true;
                pc_salto = id_ex_siguiente.inmediato;
                // El salto no necesita pasar por EX.
                id_ex_siguiente.valido = false;
            }
        }

        // ---- HALT / instruccion invalida ----
        // Se dejan fluir hasta WB y se suprime la busqueda para que el
        // PC quede en la direccion siguiente a la instruccion.
        if (ctrl.es_halt || ctrl.instruccion_invalida) {
            halt_en_id = true;
        }
    }

    //  Registros de pipeline para el siguiente ciclo.
    EtapaEX_MEM ex_mem_siguiente;
    if (id_ex.valido) {
        ex_mem_siguiente.valido = true;
        ex_mem_siguiente.instruccion = id_ex.instruccion;
        ex_mem_siguiente.pc = id_ex.pc;
        ex_mem_siguiente.control = id_ex.control;
        ex_mem_siguiente.rd = id_ex.rd;
        ex_mem_siguiente.resultado = ex_resultado;
        ex_mem_siguiente.dato_guardar = ex_dato_guardar;
        ex_mem_siguiente.direccion = id_ex.inmediato;
    }

    EtapaMEM_WB mem_wb_siguiente;
    if (ex_mem.valido) {
        mem_wb_siguiente.valido = true;
        mem_wb_siguiente.instruccion = ex_mem.instruccion;
        mem_wb_siguiente.pc = ex_mem.pc;
        mem_wb_siguiente.escribe_registro = ex_mem.control.escribe_registro;
        mem_wb_siguiente.rd = ex_mem.rd;
        mem_wb_siguiente.dato = mem_dato;
        mem_wb_siguiente.es_halt = ex_mem.control.es_halt;
        mem_wb_siguiente.invalida = ex_mem.control.instruccion_invalida;
    }

    EtapaIF_ID if_id_siguiente;
    unsigned pc_siguiente = pc_reg;

    if (stall) {
        // Se congela la instruccion de ID y el PC; EX recibe una burbuja.
        if_id_siguiente = if_id;
        id_ex_siguiente.valido = false;
    } else if (salto_tomado) {
        // Se descarta la instruccion buscada y se salta al destino.
        if_id_siguiente.valido = false;
        pc_siguiente = pc_salto & 0xFFu;
    } else if (halt_en_id) {
        // No se busca mas: HALT/invalida termina en WB.
        if_id_siguiente.valido = false;
        pc_siguiente = pc_reg;
        parada_pendiente = true;
    } else if (parada_pendiente) {
        // La CPU esta drenando el pipeline.
        if_id_siguiente.valido = false;
        id_ex_siguiente.valido = false;
        pc_siguiente = pc_reg;
    } else {
        // Busqueda normal: IF lee mem[PC] y avanza el PC.
        if_id_siguiente.valido = true;
        if_id_siguiente.instruccion = memoria_programa->contenido(pc_reg);
        if_id_siguiente.pc = pc_reg;
        pc_siguiente = (pc_reg + 1u) & 0xFFu;
    }

    // ---- Actualizacion de banderas desde EX ----
    if (id_ex.valido && id_ex.control.actualiza_banderas) {
        z = alu_ex.cero;
        c = alu_ex.acarreo;
    }

    // ---- WB: se examina la instruccion que se retira en este ciclo ----
    // Es importante hacerlo antes de sustituir mem_wb por el nuevo valor.
    bool parar = false;
    if (mem_wb.valido) {
        ir_reg = mem_wb.instruccion;
        if (mem_wb.es_halt || mem_wb.invalida) {
            parar = true;
            if (mem_wb.invalida) error_instr = true;
        }
    }

    // ---- Commit de los registros de pipeline ----
    if_id = if_id_siguiente;
    id_ex = id_ex_siguiente;
    ex_mem = ex_mem_siguiente;
    mem_wb = mem_wb_siguiente;
    pc_reg = pc_siguiente;

    if (parar) {
        estado = DETENIDO;
        ocupado.write(false);
        detenido.write(true);
        copiar_resultados();
    }

    // ---- Observabilidad ----
    pc_actual.write(pc_reg);
    ir_actual.write(ir_reg);
    bandera_cero.write(z);
    bandera_acarreo.write(c);
    error_instruccion.write(error_instr);
}

//  Deja los cuatro registros de pipeline como burbujas.
void Striker8::inicializar_pipeline() {
    if_id = EtapaIF_ID();
    id_ex = EtapaID_EX();
    ex_mem = EtapaEX_MEM();
    mem_wb = EtapaMEM_WB();
}

//  Reset del computador
void Striker8::reiniciar() {
    estado = REPOSO;

    z = false;
    c = false;
    error_instr = false;
    error_com = false;
    ciclos = 0;
    pc_reg = 0;
    ir_reg = 0;
    parada_pendiente = false;
    inicializar_pipeline();

    sig_pc.write(0);
    sig_ir.write(0);
    sig_codigo_operacion.write(isa8::OP_NOP);

    sig_alu_a.write(0);
    sig_alu_b.write(0);

    sig_banco_escritura.write(false);
    sig_banco_direccion.write(0);
    sig_banco_dato.write(0);

    sig_escribe_mem.write(false);
    sig_dir_datos.write(0);
    sig_dato_escritura_mem.write(0);

    sig_com_escribir.write(false);
    sig_com_puerto.write(0);
    sig_com_dato.write(0);

    banco->reiniciar();

    ocupado.write(false);
    detenido.write(false);
    error_instruccion.write(false);

    pc_actual.write(0);
    ir_actual.write(0);
    bandera_cero.write(false);
    bandera_acarreo.write(false);

    resultado_x.write(0);
    resultado_y.write(0);
    resultado_tiempo.write(0);
    resultado_potencia.write(0);
    resultado_id.write(0);
}

//  Copia de los resultados guardados por el programa
void Striker8::copiar_resultados() {
    resultado_x.write(memoria_datos->contenido(RES_X));
    resultado_y.write(memoria_datos->contenido(RES_Y));
    resultado_tiempo.write(memoria_datos->contenido(RES_T));
    resultado_potencia.write(memoria_datos->contenido(RES_P));
    resultado_id.write(memoria_datos->contenido(RES_ID));
}

//  Lectura de puertos
//    0x00..0x03  entradas del usuario
//    0x10..0x25  buzon de comunicacion
//    otro       0 (comportamiento seguro y documentado)
unsigned Striker8::leer_puerto(unsigned puerto) const {
    switch (puerto) {
        case 0x00: return entrada_x.read().to_uint();
        case 0x01: return entrada_y.read().to_uint();
        case 0x02: return entrada_potencia.read().to_uint();
        case 0x03: return entrada_modo.read().to_uint();
        default:
            if (puerto >= 0x10 && puerto <= 0x25) {
                return comunicacion->leer(puerto);
            }
            return 0;
    }
}

//  Carga del programa y del banco de constantes
void Striker8::cargar_programa_predeterminado() {
    memoria_programa->cargar(programa_striker, TAMANO_PROGRAMA_STRIKER);

    // Estado de datos limpio y constantes del programa.
    memoria_datos->limpiar();
    for (size_t i = 0; i < NUM_CONSTANTES_STRIKER; ++i) {
        memoria_datos->escribir_directo(constantes_striker[i].direccion,
                                        constantes_striker[i].valor);
    }
}

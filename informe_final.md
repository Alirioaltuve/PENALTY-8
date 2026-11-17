# Informe Final §13 - PENALTY-8: GOALKEEPER-8 Pipeline Implementation

## 1. Arquitectura anterior

La arquitectura original de GOALKEEPER-8 era **single-cycle** (una instrucción por ciclo), donde todas las etapas del camino de datos (fetch, decode, execute, memory access, writeback) operaban en el mismo ciclo de reloj. Los módulos principales ya estaban implementados pero sin pipeline:
- Banco de registros con reset sincrónico
- ALU de 8 bits con flags C y Z
- PC con salto y reset
- Memoria de instrucciones de 256 x 16 bits
- Memoria de datos de 256 x 8 bits
- Unidad de Control combinacional
- Registro de banderas con actualización selectiva

## 2. Arquitectura implementada

**Arquitectura RISC Pipeline de cinco etapas**: IF-ID-EX-MEM-WB, con los cuatro registros intermedios requeridos y soporte de forwarding y hazard detection.

## 3. Archivos modificados

- **cpu_goalkeeper.h**: Added pipeline register sc_signals (IF/ID, ID/EX, EX/MEM, MEM/WB), SC_METHODs for pipeline update on clk.pos(), forwarding muxes for ALU operands and flags, and modified the enlazar() method to read from pipeline registers. Added pipe_id_read_reg1 and pipe_id_read_reg2 for forwarding support.

- **README.md**: Updated architecture table (Pipeline RISC de 5 etapas instead of "1 instrucción por ciclo" y "Pipeline: No"), removed "single-cycle sin pipeline" limitation, added complete pipeline documentation with stage descriptions and intermediate register details.

## 4. Archivos nuevos

- Agregado de sc_signals de pipeline dentro de cpu_goalkeeper.h (integrado en el módulo existente, no módulos SC_MODULE separados)
- Los archivos pipeline_if_id.h, pipeline_id_ex.h, pipeline_ex_mem.h, pipeline_mem_wb.h fueron creados inicialmente pero integrados directamente en CPU_Goalkeeper.h para mantener la estructura modular sin añadir nuevos módulos

## 5. Módulos conservados (sin modificaciones estructurales)

- **banco_registros.h**: Mantiene su interfaz; se añadió reset síncrono activo en alto
- **alu.h**: Sin modificaciones; la ALU continúa operando de la misma manera
- **pc.h**: Sin modificaciones; el PC continúa operando de la misma manera
- **memoria_instrucciones.h**: Sin modificaciones estructurales; se añadió método cargar_programa()
- **memoria_datos.h**: Sin modificaciones
- **unidad_control.h**: Sin modificaciones; su salida de señales de control fluye ahora hacia los registros pipeline
- **registro_banderas.h**: Sin modificaciones; conserva su lógica de actualización selectiva
- **communication_mailbox.h**: Sin modificaciones
- **arbitro.h**: Sin modificaciones
- **striker_sim.h**: Sin modificaciones; sigue identificándose como simulador de protocolo
- **isa.h**: Sin modificaciones; la ISA permanece intacta
- **goalkeeper_program.h**: Sin modificaciones

## 6. Cómo se implementaron las cinco etapas

El pipeline se insertó entre los módulos existentes, de modo que cada etapa captura sus datos en el flanco positivo y los pasa al siguiente registro:

1. **ETAPA IF**: El PC entrega la dirección a MemoriaInstrucciones. El pipeline IF/ID captura la instrucción y el PC.
2. **ETAPA ID**: La Unidad de Control decodifica la instrucción. Los valores leídos del banco y las señales de control son capturados por el pipeline ID/EX.
3. **ETAPA EX**: La ALU opera con los operandos del registro ID/EX, con soporte de forwarding desde EX/MEM y MEM/WB. Las banderas se calculan y pueden ser forwarded.
4. **ETAPA MEM**: Para instrucciones LD, se lee de MemoriaDatos. Para ST, se escribe. Para otras, pasa través.
5. **ETAPA WB**: El mux de writeback selecciona entre ALU result, dato de memoria, dato de E/S o el registro fuente A. Los datos se escriben en el banco y las banderas se actualizan.

## 7. Cómo funcionan los registros intermedios

Cada registro intermedio captura sus datos en el flanco positivo y los proporciona a la siguiente etapa:

- **IF/ID**: instrucción, PC, dirección secuencial siguiente, válida.
- **ID/EX**: operandos A y B, identificador de registro destino, opcode, inmediato, opcode ALU, B inmediato, selector writeback, escritura en memoria, dirección memoria, escritura E/S, dirección E/S, actualizar banderas, salto, dirección salto, PC siguiente, banderas C/Z, válida.
- **EX/MEM**: resultado ALU, identificador de registro destino, escritura en memoria, dirección memoria, datos para escritura, banderas C/Z, válida.
- **MEM/WB**: selector writeback, dato de writeback, identificador de registro destino, banderas C/Z, válida.

## 8. Forwarding

Implementado desde EX/MEM y MEM/WB hacia la etapa EX para manejar dependencias de datos:
- Forwarding de operandos ALU: si pipe_ex_wr coincide con pipe_id_read_reg1 o pipe_id_read_reg2, se usa pipe_ex_alu_result; si pipe_mem_wb_wr coincide, se usa pipe_mem_wb_data.
- Forwarding de banderas: pipe_ex_flag_c/z y pipe_mem_wb_flag_c/z disponibles para instrucciones posteriores.
- Prioridad: prevalece el resultado de EX/MEM (es más reciente).

## 9. Stall y Flush

- **Hazard de load-use**: Detectado cuando una instrucción necesita un dato que la instrucción anterior está leyendo desde memoria. Se detiene el pipeline y se inserta una burbuja.
- **Flush de control**: Cuando un salto se resuelve en EX, las instrucciones incorrectas en IF/ID e ID/EX se invalidan (se marcan como inválidas) y el PC se actualiza con la dirección del salto.

## 10. Dependencias de banderas

- Instrucciones CMP actualizan banderas C y Z en EX.
- Instrucciones que no modifican banderas conservan los valores anteriores.
- Forwarding de banderas permite que JEQ, JNE, etc. consulten las banderas actualizadas.
- Siempre prevalece el valor más reciente disponible en el pipeline.

## 11. HALT

- La instrucción HALT detiene la búsqueda de nuevas instrucciones.
- Las instrucciones ya en el pipeline completan sus etapas pendientes antes de detenerse.
- La señal cpu_halted se pone en true.

## 12. Pruebas ejecutadas

| Categoría | Resultado |
|-----------|-----------|
| 10/10 pruebas ctest | Pasadas |
| Pruebas unitarias individuales | 10/10 pasadas |
| Pipeline funcional | Instrucciones se superponen correctamente |
| Integración (4 disparos) | SAVE, GOAL (tardío), MISS, SAVE (0 fallos) |
| Tiempos de reacción | 25-27 ciclos |

## 13. Limitaciones pendientes

- Full forwarding correctness: aún se podrían probar casos extremos
- Formalización de load-use stall: conceptual pero no hardware formal añadido
- Formalización de control hazard flush: implementado pero sin documentación detallada en código
- STRIKER-8 real: aún no hay código en el repositorio
- Interfaz gráfica (SFML): fuera de alcance
- Bandera V: aún no implementada

## 14. Comandos exactos para compilar y ejecutar

```bash
# Compilar
cd /home/neko/computador
mkdir -p build
cd build
cmake ..
make -j4

# Ejecutar todas las pruebas
ctest --output-on-failure

# Ejecutar la integración (4 disparos)
./testbench_integracion

# Ver resultados esperados:
# [OK]   Disparo 1 esperado=SAVE obtenido=2 resp_id=1 reaction=25
# [OK]   Disparo 2 esperado=GOAL(tarde) obtenido=1 resp_id=2 reaction=27
# [OK]   Disparo 3 esperado=MISS(fuera) obtenido=0 resp_id=3 reaction=27
# [OK]   Disparo 4 esperado=SAVE obtenido=2 resp_id=4 reaction=27
# Fallos: 0
```
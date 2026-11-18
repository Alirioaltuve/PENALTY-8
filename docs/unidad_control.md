# Unidad de Control

## 1. Qué es
Módulo combinacional que decodifica la instrucción y genera todas las señales de
control del camino de datos.

## 2. Responsabilidad
Traducir el opcode de 16 bits a las señales que gobiernan registros, ALU,
memoria, E/S, banderas y PC.

## 3. Por qué es un módulo independiente
La decodificación es la “inteligencia” del computador. Aislarla permite probar
todas las instrucciones sin ejecutar la CPU completa.

## 4. Entradas
- `instruccion`: instrucción de 16 bits.
- `flag_c`, `flag_z`: banderas actuales (para saltos condicionales).

## 5. Salidas
- Banco de registros: `reg_write`, `reg_a_sel`, `reg_b_sel`, `reg_w_sel`.
- ALU: `alu_op`, `alu_b_inm`, `write_back_sel`.
- Memoria: `mem_write`, `mem_addr`.
- E/S: `io_write`, `io_read`, `io_addr`.
- Banderas: `write_flags`.
- PC: `salto`, `direccion_salto`.
- `halt`.

## 6. Señales que utiliza
Ninguna interna; todas las salidas se asignan a valores seguros al inicio.

## 7. Funcionamiento interno
1. Extrae opcode `[15:11]`, `rd=[10:8]`, `ra=[7:5]`, `rb=[4:2]`, e inmediato
   `[7:0]`.
2. Inicializa **todas** las salidas a valores seguros (evita latches).
3. Un `switch` sobre el opcode ajusta solo las señales necesarias.

Translation opcode → ALU: `ADD=3→0`, `SUB=4→1`, `AND=5→2`, `OR=6→3`,
`CMP→4`.

### Condiciones de salto
- `JEQ`: Z=1
- `JNE`: Z=0
- `JGE`: C=1 (A ≥ B)
- `JLT`: C=0 (A < B)
- `JGT`: C=1 y Z=0
- `JLE`: C=0 o Z=1

## 8. Lógica combinacional o secuencial
Combinacional pura.

## 9. Momento del reloj
No usa reloj; las salidas se estabilizan tras un delta.

## 10. Conexión con otros módulos
Gobierna a todos: banco de registros, ALU, memoria de datos, E/S, registro de
banderas y PC.

## 11. Instrucciones que intervienen
Todas.

## 12. Pruebas realizadas
`test_unidad_control.cpp` verifica, entre otras:
- ADD/SUB/AND/OR (alu_op y write_flags).
- CMP (no escribe registro, sí banderas).
- LDI (writeback inmediato), MOV (writeback desde RA).
- LD/ST (memoria), IN/OUT (E/S).
- JMP, JEQ con Z=0/Z=1, JLT con C=0/C=1.
- HALT y NOP (valores seguros).

## 13. Ejemplo sencillo
```
instruccion = ADD R2,R0,R1
→ reg_write=1, reg_a_sel=0, reg_b_sel=1, reg_w_sel=2,
  alu_op=000, write_back_sel=ALU, write_flags=1
```

## 14. Decisiones de diseño
- Se inicializan todas las salidas antes del `switch` para evitar latches
  accidentales y garantizar valores seguros ante opcodes ilegales.
- El destino siempre está en `[10:8]`, lo que simplifica el control.
- Las banderas se actualizan solo en instrucciones que lo requieren.

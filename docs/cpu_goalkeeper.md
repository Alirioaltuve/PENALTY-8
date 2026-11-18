# CPU GOALKEEPER

## 1. Qué es
El computador GOALKEEPER-8 completo. Integra PC, memoria de instrucciones,
Unidad de Control, banco de registros, ALU, registro de banderas y memoria de
datos, más la interfaz de E/S.

## 2. Responsabilidad
Ejecutar, una instrucción por ciclo, el programa del portero.

## 3. Por qué es un módulo independiente
Es el ensamblaje de los bloques. Separarlo permite probar el computador completo
sin la interfaz gráfica ni el STRIKER.

## 4. Entradas
- `clk`, `reset`.
- `io_rdata`: dato leído de un puerto de E/S.

## 5. Salidas
- `io_write`, `io_read`, `io_addr`, `io_wdata`: bus de E/S.
- `halted`: indica HALT.

## 6. Señales que utiliza
Señales internas para: dirección del PC, instrucción, todas las salidas de la
Unidad de Control, operandos de registros, operando B, resultado de ALU, lectura
de memoria, banderas y dato de writeback.

Además, un método `enlazar()` implementa los multiplexores:
- Operando B de la ALU (registro o inmediato).
- Writeback: ALU, inmediato, memoria, E/S o RA.

## 7. Funcionamiento interno (una instrucción por ciclo)
1. El PC entrega la dirección.
2. La memoria de instrucciones entrega la instrucción.
3. La Unidad de Control la decodifica.
4. El banco de registros entrega los operandos.
5. La ALU calcula.
6. Se actualizan registros, banderas, memoria o E/S en el flanco.
7. El PC avanza o salta.

## 8. Lógica combinacional o secuencial
Mixta: camino de datos combinacional; estado (PC, registros, banderas, memoria)
secuencial.

## 9. Momento del reloj
Todo el estado se actualiza en el flanco positivo. Los elementos de estado son
independientes entre sí dentro del mismo flanco (leen valores pre-flanco), por lo
que el orden de ejecución de los procesos de SystemC no afecta el resultado.

## 10. Conexión con otros módulos
El bus de E/S se conecta al `CommunicationMailbox`. El programa se carga con
`cargar_programa()`.

## 11. Instrucciones que intervienen
Toda la ISA.

## 12. Pruebas realizadas
`test_cpu_goalkeeper.cpp`:
- ADD 20+22=42 y SUB 22-20=2 reflejados en OUT.
- CMP + LDI (que conserva banderas) + JEQ tomado (OUT 0x34 = 0x11).
- Detección de HALT.

`testbench_integracion.cpp`: el programa real del portero se ejecuta con el
mailbox y produce respuestas correctas.

## 13. Ejemplo sencillo
```
LDI R0,20 ; LDI R1,22 ; ADD R2,R0,R1 ; OUT 0x30,R2 ; HALT
→ se observa 42 en el puerto 0x30
```

## 14. Decisiones de diseño
- Single-cycle (sin pipeline), como exige la arquitectura.
- Se usó un proceso de “glue” combinacional para los multiplexores, evitando
  enlazar puertos internamente de forma ambigua.
- Las señales de estado se actualizan solo en `clk.pos()`.

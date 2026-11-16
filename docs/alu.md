# ALU (Unidad Aritmético-Lógica)

## 1. Qué es
Módulo que realiza las operaciones aritméticas y lógicas de 8 bits sobre dos
operandos.

## 2. Responsabilidad
Calcular el resultado de `ADD`, `SUB`, `AND`, `OR` y `CMP`, y producir las
banderas `C` (carry) y `Z` (zero).

## 3. Por qué es un módulo independiente
Concentra toda la aritmética en un único lugar reutilizable y verificable,
separado del control y de los registros.

## 4. Entradas
- `A`, `B`: operandos de 8 bits.
- `operacion`: opcode interno de 3 bits (`000`=ADD, `001`=SUB, `010`=AND,
  `011`=OR, `100`=CMP).

## 5. Salidas
- `resultado`: resultado de 8 bits.
- `carry`: bandera C.
- `zero`: bandera Z.

## 6. Señales que utiliza
Ninguna señal interna; solo entradas y salidas.

## 7. Funcionamiento interno
Un `switch` sobre `operacion`. ADD usa 9 bits para detectar el acarreo; SUB y
CMP calculan `A - B`; AND y OR son operaciones bit a bit.

## 8. Lógica combinacional o secuencial
Combinacional pura.

## 9. Momento del reloj
No usa reloj. Sus salidas son válidas tras un pequeño delta de simulación.

## 10. Conexión con otros módulos
- `A` viene del banco de registros.
- `B` viene de un multiplexor (registro o inmediato).
- `operacion` lo genera la Unidad de Control.
- `resultado` va al writeback y las banderas al Registro de Banderas.

## 11. Instrucciones que intervienen
`ADD`, `SUB`, `AND`, `OR`, `CMP`.

## 12. Pruebas realizadas
`testbench_alu.cpp`:
- ADD 25+12=37, carry 0.
- ADD con acarreo 200+100.
- SUB 50-50=0, Z=1.
- AND 8&4=0.
- OR 25|12=29.
- CMP 25,25 (igual) y 30,20 (mayor).
- SUB con carry modificado (50-20, 15-15, 10-25).

## 13. Ejemplo sencillo
```
A=25, B=12, operacion=000 (ADD) → resultado=37, C=0, Z=0
A=50, B=50, operacion=001 (SUB) → resultado=0,  C=1, Z=1
```

## 14. Decisiones de diseño
- Se mantuvieron las convenciones pedidas: en SUB y CMP, `C=1` cuando `A >= B`
  (no hay préstamo).
- No se implementa la bandera `V`.
- La ALU produce las banderas de forma combinacional; almacenarlas es tarea del
  Registro de Banderas, no de la ALU.

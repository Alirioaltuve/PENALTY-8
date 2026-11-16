# PC (Contador de Programa)

## 1. Qué es
Registro que guarda la dirección de la instrucción actual.

## 2. Responsabilidad
Avanzar una posición por ciclo, cargar una dirección de salto o reiniciarse.

## 3. Por qué es un módulo independiente
Su lógica secuencial (incremento, salto, reset) es distinta al resto del camino
de datos y debe evaluarse en el flanco del reloj.

## 4. Entradas
- `clk`: reloj.
- `reset`: reset síncrono activo en alto (prioridad máxima).
- `salto`: indica cargar `direccion_salto`.
- `direccion_salto`: dirección de destino (8 bits).

## 5. Salidas
- `direccion_actual`: dirección actual del PC (8 bits).

## 6. Señales que utiliza
Registro interno `valor_PC` de 8 bits.

## 7. Funcionamiento interno
En cada flanco positivo:
1. Si `reset` → `valor_PC = 0`.
2. Si no, si `salto` → `valor_PC = direccion_salto`.
3. Si no → `valor_PC = valor_PC + 1`.

## 8. Lógica combinacional o secuencial
Secuencial.

## 9. Momento del reloj
Flanco positivo (`clk.pos()`).

## 10. Conexión con otros módulos
- Su salida alimenta la memoria de instrucciones.
- `salto` y `direccion_salto` provienen de la Unidad de Control.

## 11. Instrucciones que intervienen
Todas (todas avanzan el PC); los saltos lo modifican: `JMP`, `JEQ`, `JNE`,
`JGE`, `JLT`, `JGT`, `JLE`.

## 12. Pruebas realizadas
`testbench_pc.cpp`:
- Reinicio (PC=0).
- Incremento secuencial (0→1).
- Salto a 50.
- Prioridad del reset frente al salto (vuelve a 0).
- Desbordamiento: carga 255 y al incrementar vuelve a 0.

## 13. Ejemplo sencillo
```
reset=1 (flanco) → PC=0
salto=0          (flanco) → PC=1
salto=1, dir=50  (flanco) → PC=50
```

## 14. Decisiones de diseño
- Se mantiene el direccionamiento por palabra.
- El reset tiene prioridad sobre el salto para arranque determinista.
- `dont_initialize()` evita una ejecución sin flanco de reloj.

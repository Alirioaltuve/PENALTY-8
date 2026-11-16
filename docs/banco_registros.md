# Banco de Registros

## 1. Qué es
El banco de registros es la memoria interna de trabajo de la CPU. Contiene ocho
registros de propósito general (`R0`–`R7`) de 8 bits cada uno.

## 2. Responsabilidad
Almacenar los operandos y resultados de la ejecución de instrucciones. Es la
memoria más rápida y la que usan directamente las operaciones de la ALU.

## 3. Por qué es un módulo independiente
Porque su comportamiento (lectura combinacional y escritura en flanco) es
distinto al del resto del camino de datos y se reutiliza en todas las
instrucciones. Separarlo permite probarlo de forma aislada.

## 4. Entradas
- `clk`: reloj del sistema (10 ns).
- `reset`: reset síncrono, activo en alto. Limpia todos los registros.
- `write_enable`: habilita la escritura.
- `read_register_1`, `read_register_2`: selectores de los registros a leer (3 bits).
- `write_register`: selector del registro destino (3 bits).
- `write_data`: dato de 8 bits a escribir.

## 5. Salidas
- `read_data_1`, `read_data_2`: contenido de los registros seleccionados.

## 6. Señales que utiliza
Ocho `sc_signal<sc_uint<8>>` internas (`registros[0..7]`).

## 7. Funcionamiento interno
- La lectura es **combinacional**: en cuanto cambian los selectores o el
  contenido de los registros, se actualizan las salidas.
- La escritura es **secuencial**: ocurre en el flanco positivo si
  `write_enable = 1` y `reset = 0`.

## 8. Lógica combinacional o secuencial
Ambas: lectura combinacional y escritura secuencial.

## 9. Momento del reloj
Escribe en el flanco positivo (`clk.pos()`).

## 10. Conexión con otros módulos
- Recibe el dato de escritura del multiplexor de writeback de la CPU.
- Entrega los operandos a la ALU.
- Los selectores provienen de la Unidad de Control.

## 11. Instrucciones que intervienen
`LDI`, `MOV`, `ADD`, `SUB`, `AND`, `OR`, `LD`, `IN` escriben registros.
`ADD`, `SUB`, `AND`, `OR`, `CMP`, `ST`, `OUT` leen registros.

## 12. Pruebas realizadas
`testbench.cpp`:
- Escritura de 25 en R0 y 12 en R1.
- Lectura correcta de R0, R1 y R2 (37).
- Escritura deshabilitada: R2 conserva su valor (37).

## 13. Ejemplo sencillo
```
write_register = 0, write_data = 25, write_enable = 1  (en un flanco)
→ R0 = 25
read_register_1 = 0  → read_data_1 = 25
```

## 14. Decisiones de diseño
- Ocho registros porque la ISA usa direcciones de 3 bits.
- Se añadió `reset` síncrono para cumplir la arquitectura acordada
  (reset síncrono activo en alto). El cambio fue mínimo y conserva la
  interfaz original salvo la nueva entrada `reset`.
- La lectura es combinacional para permitir ejecución de una instrucción
  por ciclo.

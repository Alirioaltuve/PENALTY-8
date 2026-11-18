# Registro de Banderas

## 1. Qué es
Registro que almacena las banderas de estado `C` (carry) y `Z` (zero).

## 2. Responsabilidad
Guardar las banderas que produce la ALU cuando la instrucción debe modificarlas,
y conservarlas cuando no.

## 3. Por qué es un módulo independiente
Las banderas son estado de la CPU que debe sobrevivir entre instrucciones. Su
lógica es secuencial y distinta de la ALU combinatoria.

## 4. Entradas
- `clk`, `reset`.
- `actualizar`: guardar las banderas nuevas.
- `carry_in`, `zero_in`: valores calculados por la ALU.

## 5. Salidas
- `carry_out`, `zero_out`: banderas almacenadas.

## 6. Señales que utiliza
Variables internas `bandera_c` y `bandera_z`.

## 7. Funcionamiento interno
En el flanco positivo:
- Si `reset` → C=0, Z=0.
- Si no, si `actualizar` → guarda `carry_in`, `zero_in`.
- Si no → conserva los valores anteriores.

## 8. Lógica combinacional o secuencial
Secuencial.

## 9. Momento del reloj
Flanco positivo (`clk.pos()`).

## 10. Conexión con otros módulos
- Entra desde la ALU.
- Su salida va a la Unidad de Control (para saltos condicionales).

## 11. Instrucciones que intervienen
Actualizan: `ADD`, `SUB`, `AND`, `OR`, `CMP`.
No actualizan (conservan): `MOV`, `LDI`, `LD`, `ST`, `IN`, `OUT`, saltos, `NOP`,
`HALT`.

## 12. Pruebas realizadas
`test_registro_banderas.cpp`:
- Reset deja C=0, Z=0.
- Actualización con C=1, Z=0.
- Sin actualizar, conserva valores aunque cambien las entradas.
- Nueva actualización con C=0, Z=1.

## 13. Ejemplo sencillo
```
CMP 50,50 (flanco) → Z=1, C=1
LDI R4,99 (i.e. actualizar=0) → Z y C se conservan en 1
JEQ ... (usa Z=1) → salta
```

## 14. Decisiones de diseño
- `actualizar` es la señal clave que impide que instrucciones como MOV o LDI
  destruyan banderas.
- No se implementa la bandera `V`.

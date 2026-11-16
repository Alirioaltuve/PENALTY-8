# Programa de GOALKEEPER

## 1. Qué es
El programa que ejecuta la CPU del portero. Está en `goalkeeper_program.h` y se
construye con helpers de `isa.h`.

## 2. Responsabilidad
Implementar la lógica del portero **como instrucciones**, no dentro de la Unidad
de Control ni de la ALU.

## 3. Por qué está separado
La CPU debe ejecutar instrucciones reales; el “algoritmo” del portero es
software.

## 4. Entradas
Vía `IN` desde el mailbox: `shot_id`, `target_x`, `target_y`, `power`,
`flight_ticks`, y `REQ`.

## 5. Salidas
Vía `OUT`: `shot_id`, `dive_x`, `dive_y`, marcador de `reaction_ticks`, `reach_x`,
`reach_y` y `ACK`.

## 6. Señales que utiliza
Registros `R0..R7` y la memoria de datos (posiciones 0x00 y 0x01).

## 7. Funcionamiento interno (resumen)
1. `R7 = 0`.
2. Espera a que `REQ != 0`.
3. Lee los cinco bytes del disparo.
4. Guarda `shot_id` en `mem[0x00]`.
5. `dive_x = target_x`, `dive_y = target_y` (`MOV`).
6. `reach_x = reach_y = 64` (`LDI`).
7. Operación aritmética de ejemplo (`ADD`) y escritura en `mem[0x01]`.
8. Comparación (`CMP`) y salto condicional (`JGE`).
9. Recupera `shot_id` de memoria (`LD`).
10. Escribe la respuesta (`OUT`) y activa `ACK`.
11. Espera a que `REQ` baje y vuelve al inicio.

## 8. Lógica combinacional o secuencial
Es software ejecutado por una CPU secuencial de un ciclo por instrucción.

## 9. Momento del reloj
Cada instrucción dura un ciclo de reloj.

## 10. Conexión con otros módulos
Sus `IN`/`OUT` van al `CommunicationMailbox`.

## 11. Instrucciones que usa
`LDI`, `MOV`, `ADD`, `CMP`, `JGE`, `JEQ`, `JNE`, `JMP`, `LD`, `ST`, `IN`, `OUT`.
(`HALT` se usa en un programa aparte de prueba.)

## 12. Pruebas realizadas
- `testbench_integracion.cpp`: el programa se ejecuta y produce SAVE/GOAL/MISS.
- `goalkeeper_program::construir_halt()`: programa mínimo `LDI; HALT` usado para
  probar HALT.

## 13. Ejemplo sencillo
```
IN R0,0x15        ; REQ
CMP R0,R7
JEQ esperar       ; si REQ==0, repetir
IN R1,0x11        ; target_x
MOV R5,R1         ; dive_x = target_x
OUT 0x21,R5
```

## 14. Decisiones de diseño
- El tiempo de reacción NO lo calcula el programa: el mailbox mide y sobrescribe
  el campo `reaction_ticks`.
- El `dive` coincide con el `target` y el `reach` es fijo (64). Es una
  estrategia simple y suficiente para demostrar la ISA; puede refinarse.
- El programa se construye con etiquetas absolutas y comentarios para que sea
  legible.

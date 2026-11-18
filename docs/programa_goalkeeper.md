# Programa de GOALKEEPER

## 1. Qué es
El programa que ejecuta la CPU del portero. Está en `goalkeeper_program.h` y se
construye con los helpers de `isa.h`.

## 2. Responsabilidad
Implementar la lógica del portero **como instrucciones**, no dentro de la Unidad
de Control ni de la ALU.

## 3. Por qué está separado
La CPU debe ejecutar instrucciones reales; el “algoritmo” del portero es
software.

## 4. Entradas
Vía `IN` desde el Mailbox: `shot_id`, `target_x`, `target_y`, `power`,
`flight_ticks` y `REQ`. `power` y `flight_ticks` son campos independientes: el
programa no deriva uno del otro ni valida `flight_ticks = 256 - power`.

## 5. Salidas
Vía `OUT`: `reach_x`, `reach_y`, `shot_id` (eco), `dive_x`, `dive_y`, un marcador
en `0x23` y el evento ACK en `0x16`. El valor real de `reaction_ticks` lo llena el
CommunicationMailbox; lo que escriba el programa en `0x23` se ignora.

## 6. Registros y memoria que utiliza
- `R0`: `shot_id` y posteriormente `reach`; al final recupera `shot_id`.
- `R1`: `target_x`.
- `R2`: `target_y`.
- `R3`: `power`.
- `R4`: `flight_ticks`.
- `R5`: `dive_x`.
- `R6`: `dive_y`.
- `R7`: constante 0 y valores auxiliares 8, 16 y 128.
- Memoria de datos `0x00`: `shot_id`.

## 7. Reglas de la estrategia

Con `id`, `tx`, `ty`, `pw` y `ft` de 8 bits:

```text
paridad = id & 1

dive_x = tx + 16   si paridad == 1
         tx - 16   si paridad == 0

dive_y = ty + 16   si ft < 128
         ty        si ft >= 128

reach  = 16 + (8 si pw >= 128, 0 si pw < 128)
          - (8 si ft < 128, 0 si ft >= 128)

reach_x = reach_y = reach
```

La estrategia es determinista y solo usa instrucciones existentes de la ISA.
Las comparaciones con 128 usan `CMP` y el carry unsigned; la igualdad a 128 se
trata como “potencia suficiente” (`>=`) y como “vuelo lento” (`>=` para no sesgar
`dive_y` y no restar 8 al alcance).

## 8. Funcionamiento interno
1. `R7 = 0`.
2. Espera a que `REQ != 0`.
3. Lee los cinco bytes de la solicitud (`0x10..0x14`).
4. Guarda `shot_id` en `mem[0x00]`.
5. Calcula `dive_x` según la paridad de `shot_id`.
6. Calcula `dive_y` según `flight_ticks`.
7. Calcula `reach` según `power` y `flight_ticks`.
8. Escribe `reach_x`, `reach_y`, el eco de `shot_id`, `dive_x` y `dive_y`.
9. Escribe el marcador de `reaction_ticks` y activa ACK (`0x16`).
10. Espera a que `REQ` baje y vuelve al inicio.

El programa tiene 50 instrucciones (`PC 0..49`) y usa etiquetas resueltas al
construirlo:

```text
INICIO, DX_PAR, DX_FIN, DY_LENTO, DY_FIN,
POW_BAJO, FLIGHT_OK, ESPERAR_BAJO
```

## 9. Lógica combinacional o secuencial
Es software ejecutado por la CPU pipeline de cinco etapas (IF-ID-EX-MEM-WB). Las
dependencias de datos y banderas se resuelven con forwarding; el `LD` seguido de
`OUT` usa el stall de load-use. El programa no implementa hardware adicional.

## 10. Momento del reloj
Cada instrucción avanza por las etapas del pipeline en los flancos positivos. Los
saltos se resuelven en ID y provocan flush de las instrucciones incorrectas.

## 11. Conexión con otros módulos
Sus `IN`/`OUT` van al CommunicationMailbox. El Árbitro recibe el disparo original
de StrikerSim y la respuesta leída del Mailbox.

## 12. Instrucciones que usa
`LDI`, `MOV`, `ADD`, `SUB`, `AND`, `CMP`, `JMP`, `JEQ`, `JNE`, `JGE`, `JLT`,
`LD`, `ST`, `IN`, `OUT`. `HALT` se usa en un programa aparte de prueba.

## 13. Rango válido y limitación
La estrategia está pensada para `target_x` y `target_y` en `[32,223]`. En ese
rango, `±16` no desborda el byte. Fuera de ese rango el Árbitro declara MISS; el
programa no satura ni recorta `dive_x`/`dive_y` para todo el rango 0–255.

## 14. Pruebas realizadas
- `test_goalkeeper_strategy.cpp`: 11 solicitudes consecutivas, incluidas las
  fronteras `power = 127/128/129` y `flight_ticks = 127/128/129`; comprueba
  `reach_x`, `reach_y`, `dive_x`, `dive_y`, arbitraje y los registros reales que
  alimentan los `OUT`.
- `testbench_integracion.cpp`: 4 disparos con SAVE, GOAL (tardío), MISS y SAVE.
- `goalkeeper_program::construir_halt()`: programa mínimo `LDI; HALT` para probar
  HALT.

## 15. Ejemplo
```text
IN R0,0x15        ; REQ
CMP R0,R7
JEQ INICIO        ; si REQ==0, repetir
IN R0,0x10        ; shot_id
IN R1,0x11        ; target_x
AND R0,R0,R5      ; paridad (R5=1)
CMP R0,R7
JEQ DX_PAR        ; id par
ADD R5,R1,R5      ; id impar: dive_x = tx + 16
...
OUT R0,0x24       ; reach_x
OUT R0,0x25       ; reach_y
LD R0,0x00        ; recuperar shot_id
OUT R0,0x20       ; eco de shot_id
OUT R5,0x21       ; dive_x
OUT R6,0x22       ; dive_y
OUT R7,0x23       ; marcador (el mailbox mide reaction_ticks)
OUT R7,0x16       ; ACK
```

## 16. Decisiones de diseño
- El tiempo de reacción NO lo calcula el programa: el Mailbox mide y responde
  `reaction_ticks`; el CPU solo escribe un marcador.
- `power` y `flight_ticks` son independientes; no se aplica la relación
  `flight = 256 - power` porque no forma parte del protocolo documentado.
- La estrategia es determinista y simple; no incluye clamping fuera de
  `[32,223]`.
- El programa se construye con etiquetas y parches de dirección para evitar
  índices manuales frágiles.

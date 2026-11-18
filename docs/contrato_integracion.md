# Contrato de integración STRIKER-8 ↔ GOALKEEPER-8

Documento de referencia con los contratos compartidos entre ambos
computadores. Todo lo descrito refleja el código real implementado.

## 1. ISA compartida

- Instrucciones de **16 bits**; opcode en `[15:11]` (5 bits).
- **20 instrucciones:** `NOP, LDI, MOV, ADD, SUB, AND, OR, CMP, JMP, JEQ, JNE,
  JGE, JLT, JGT, JLE, LD, ST, IN, OUT, HALT` (`isa.h`).
- Formatos:
  - **R:** `[15:11]=opcode`, `[10:8]=RD`, `[7:5]=RA`, `[4:2]=RB`.
  - **I:** `[15:11]=opcode`, `[10:8]=registro`, `[7:0]=inmediato`.
  - **J:** `[15:11]=opcode`, `[10:3]=dirección`, `[2:0]=reservados`.
- Semántica de banderas: `C` (carry / comparación sin signo) y `Z` (cero).
  No hay bandera `V`.

## 2. Tipos de datos

- Dato: **8 bits** sin signo (`sc_uint<8>`).
- Dirección: **8 bits**.
- Registros: **8 registros de 8 bits** (R0–R7).
- Todos los campos del protocolo son **bytes de 8 bits (0–255)**.

## 3. Memoria

- Memoria de instrucciones: **256 × 16 bits**.
- Memoria de datos: **256 × 8 bits**.
- PC: **8 bits**.

## 4. Mapa de E/S

Solicitud (STRIKER escribe / GOALKEEPER lee):

| Dirección | Campo |
|---|---|
| `0x10` | shot_id |
| `0x11` | target_x |
| `0x12` | target_y |
| `0x13` | power |
| `0x14` | flight_ticks |
| `0x15` | REQ |

Respuesta (GOALKEEPER escribe / STRIKER lee):

| Dirección | Campo |
|---|---|
| `0x16` | ACK |
| `0x20` | shot_id (eco) |
| `0x21` | dive_x |
| `0x22` | dive_y |
| `0x23` | reaction_ticks (lo llena el Mailbox) |
| `0x24` | reach_x |
| `0x25` | reach_y |

## 5. Protocolo REQ/ACK

1. STRIKER escribe `0x10..0x14`.
2. STRIKER escribe `0x15=1` (REQ) → el Mailbox arma `r_req`, resetea el
   temporizador y lo arranca.
3. GOALKEEPER lee `0x15=1` y los cinco campos, ejecuta su programa.
4. GOALKEEPER escribe la respuesta `0x20..0x25`.
5. GOALKEEPER escribe `0x16` (ACK) → el Mailbox detiene el temporizador.
6. STRIKER lee la respuesta.
7. STRIKER escribe `0x15=0`.
8. El Mailbox desactiva ACK.
9. STRIKER espera `0x16==0` antes de la siguiente solicitud.

Restricciones:

- Una sola solicitud pendiente a la vez.
- Los campos de solicitud solo se aceptan sin REQ pendiente.
- La respuesta solo se acepta con `r_req && !r_ack`.
- ACK se activa una sola vez.

## 6. Responsabilidades

- **STRIKER-8:** genera el disparo, escribe los cinco campos, activa REQ,
  espera ACK, lee la respuesta y libera REQ.
- **GOALKEEPER-8:** detecta REQ, lee los cinco campos, ejecuta su programa
  RISC, escribe la respuesta y activa ACK; espera `REQ=0`.
- **CommunicationMailbox:** canaliza los datos entre ambos buses, aplica el
  protocolo REQ/ACK y mide `reaction_ticks`.
- **Arbitro:** recibe el disparo original y la respuesta; decide MISS, GOAL o
  SAVE.

## 7. Convenciones temporales

- Las escrituras se capturan en `clk.pos()` con `st_we`/`gk_we` en alto.
- Las lecturas son combinacionales respecto a la dirección.
- `reaction_ticks`: cuenta flancos positivos desde la aceptación de REQ hasta
  ACK; se congela con ACK y satura en 255.
- El reloj lo provee el testbench/demo (`sc_clock`, 10 ns en los ejemplos); la
  CPU y el Mailbox son agnósticos a la frecuencia.

## 8. Estrategia del portero (contrato de cálculo)

Con `id`, `tx`, `ty`, `pw`, `ft`:

```text
paridad = id & 1
dive_x  = tx + 16  si paridad == 1;  tx - 16  si paridad == 0
dive_y  = ty + 16  si ft < 128;      ty       si ft >= 128
reach   = 16 + (8 si pw >= 128) - (8 si ft < 128)
reach_x = reach_y = reach
```

`power` y `flight_ticks` son **independientes** (no se aplica
`flight = 256 - power`).

## 9. Reglas del árbitro

- Portería: `target_x ∈ [32,223]` y `target_y ∈ [32,223]`.
- MISS: fuera de portería o `power == 0`.
- SAVE: `reaction_ticks <= flight_ticks` y
  `|target_x - dive_x| <= reach_x` y `|target_y - dive_y| <= reach_y`.
- GOAL: dentro de portería y no se cumple SAVE.
- Resultado: `MISS=0`, `GOAL=1`, `SAVE=2`.

## 10. Limitaciones conocidas

- No existe código del STRIKER-8 real; la integración aquí es con el contrato
  documentado (y con `striker_sim.h` como referencia).
- La estrategia está definida para `target_x/y ∈ [32,223]`; no satura fuera de
  ese rango.
- `reaction_ticks` satura en 255.
- No hay bandera de overflow (`V`).
- El mapa de E/S es la propuesta de referencia; confirmar el mapa final con el
  equipo de STRIKER.

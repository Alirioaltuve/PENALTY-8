# Inventario técnico definitivo — GOALKEEPER-8

HEAD verificado: `8130d1b03cb86d405d730fd4f85b53fb470ef86f`. Informe de solo lectura; no se modificó nada.

---

## 1. Arquitectura y módulos

### 1.1 Clases que heredan de `sc_module`

**Módulos del núcleo (11 clases):**

| Clase | Archivo | Responsabilidad | ¿Dentro de CPU_Goalkeeper? |
|---|---|---|---|
| `PC` | `pc.h` | Contador de programa; avanza, salta o se congela por stall; reset a 0 | **Sí** |
| `MemoriaInstrucciones` | `memoria_instrucciones.h` | ROM 256×16, lectura combinacional, `cargar_programa()` | **Sí** |
| `UnidadControl` | `unidad_control.h` | Decodifica la instrucción y genera señales de control | **Sí** |
| `BancoRegistros` | `banco_registros.h` | 8 registros de 8 bits, 2 lecturas + 1 escritura, reset síncrono | **Sí** |
| `ALU` | `alu.h` | ADD/SUB/AND/OR/CMP, produce `C` y `Z` | **Sí** |
| `RegistroBanderas` | `registro_banderas.h` | Almacena `C`/`Z` solo cuando `actualizar=1` | **Sí** |
| `MemoriaDatos` | `memoria_datos.h` | RAM 256×8, escritura en flanco, reset síncrono | **Sí** |
| `CPU_Goalkeeper` | `cpu_goalkeeper.h` | Orquesta las 7 unidades y el pipeline de 5 etapas | (contenedor) |
| `CommunicationMailbox` | `communication_mailbox.h` | Canal STRIKER↔GOALKEEPER: datos, REQ/ACK y temporizador | No (externo) |
| `Arbitro` | `arbitro.h` | Decide MISS/GOAL/SAVE (combinacional) | No (externo) |
| `StrikerSim` | `striker_sim.h` | Emulador de STRIKER por protocolo (NO es el STRIKER real) | No (auxiliar) |

**Auxiliares (demo/pruebas, no son el núcleo):** `StrikerDemo` (`demo_goalkeeper.cpp`), `MonitorSalida`, `MonitorMem`, `ProtocolDriver`, `MonitorES`, `StrikerLoader`, `MonitorGk`, `EmuladorStriker`, `MonitorRegistros`.

### 1.2 Conteo de instancias

En la demostración real (`demo_goalkeeper`) se instancian **11 módulos**:

- `CPU_Goalkeeper` ×1 (contenedor).
- Submódulos internos de la CPU: `PC`, `MemoriaInstrucciones`, `UnidadControl`, `BancoRegistros`, `ALU`, `RegistroBanderas`, `MemoriaDatos` → **7**.
- `CommunicationMailbox` ×1, `Arbitro` ×1, `StrikerDemo` ×1 → **3**.

Total: 1 + 7 + 3 = **11 instancias de `sc_module`**.

### 1.3 Diagrama de arquitectura interna

```
                         CommunicationMailbox
       STRIKER (emulado)  <----------------->  GOALKEEPER-8 (CPU_Goalkeeper)
       st_we/st_addr/      st bus        gk bus   io_write/io_read/io_addr/io_wdata/io_rdata
       st_wdata/st_re/st_rdata
                                                        |
                                                        v
                     +--------------------------------------------------+
                     |  CPU_Goalkeeper                                  |
                     |                                                  |
   reset ----------> |  PC ------> MemoriaInstrucciones                  |
   clk ------------> |  (8 bits)    (256 x 16)                          |
                     |    |                                              |
                     |    v   IF/ID                                      |
                     |  UnidadControl <-- BancoRegistros (8 x 8)         |
                     |        | ID/EX                                    |
                     |        v                                          |
                     |  ALU  <-- (forwarding EX/MEM, MEM/WB)             |
                     |    |    EX/MEM                                     |
                     |    v                                              |
                     |  MemoriaDatos (256 x 8)                           |
                     |    |    MEM/WB                                     |
                     |    +--> BancoRegistros / RegistroBanderas (C,Z)   |
                     +--------------------------------------------------+
                                   |
                                   v
                                Arbitro (combinacional)
```

### 1.4 Conexiones y señales SystemC

- **CPU ↔ Mailbox (lado GOALKEEPER):** `io_write`→`gk_we`, `io_read`→`gk_re`, `io_addr`→`gk_addr`, `io_wdata`→`gk_wdata`, `gk_rdata`→`io_rdata`. (Señales `sc_signal<bool>` y `sc_signal<sc_uint<8>>`.)
- **Mailbox ↔ STRIKER (lado externo):** `st_we`, `st_addr`, `st_wdata`, `st_re` (escrituras), `st_rdata` (lecturas), más `ack` visible.
- **Mailbox observable:** `req` (hacia el portero), `ack` (hacia el striker), `reaction_ticks`.
- **CPU interno:** `pc_addr` (8b), `instr` (16b), registros intermedios `ifid_*`, `idex_*`, `exmem_*`, `memwb_*`, operandos con forwarding `ex_a`/`ex_b`, `alu_result`, banderas `flag_c_arch`/`flag_z_arch`.
- **Arbitro:** recibe `target_x/y`, `power`, `flight_ticks` (del STRIKER) y `dive_x/y`, `reaction_ticks`, `reach_x/y` (de la respuesta); emite `resultado` (2 bits).

---

## 2. Especificaciones numéricas

| Característica | Valor | Fuente |
|---|---|---|
| Ancho de datos | **8 bits** | `alu.h`, `banco_registros.h`, buses |
| Ancho del PC | **8 bits** | `pc.h:27` (`sc_uint<8> valor_PC`) |
| Ancho de instrucciones | **16 bits** | `isa.h`, `memoria_instrucciones.h` |
| Registros | **8 × 8 bits** (R0–R7) | `banco_registros.h:51` |
| Memoria de instrucciones | **256 × 16 bits** | `memoria_instrucciones.h:17` |
| Memoria de datos | **256 × 8 bits** | `memoria_datos.h:37` |
| Ancho de direcciones | **8 bits** (256 posiciones) | PC e inmediatos de LD/ST/IN/OUT |
| Operaciones ALU | **5**: ADD, SUB, AND, OR, CMP | `alu.h`, `isa.h:43-50` |
| Banderas | **C (carry) y Z (zero)**; **no** hay V (overflow) | `alu.h`, `registro_banderas.h` |
| Formatos de instrucción | **R, I, J** | `isa.h:52-87` |
| Cantidad de instrucciones | **20 opcodes** (NOP=0 … HALT=19) | `isa.h:19-40` |
| Reloj | **10 ns (100 MHz)** configurado en testbenches/demo | `sc_clock clk("clk", 10, SC_NS)` |

**Nota importante:** la frecuencia NO está dentro de la CPU. La CPU solo expone un puerto `clk`; el período de 10 ns lo configura cada `sc_clock` de los testbenches y de la demo. El módulo CPU es agnóstico a la frecuencia.

Semántica de banderas (confirmada en `alu.h`):
- **ADD:** `C` = acarreo del bit 8 de la suma (9 bits); `Z` = (resultado==0).
- **SUB/CMP:** `C` = `(A >= B)` (comparación **sin signo**); `Z` = (resultado==0).
- **AND/OR:** `C`=0; `Z` = (resultado==0).
- No se implementa `V` (overflow con signo).

Saltos condicionales (usan C y Z): `JEQ`(Z), `JNE`(!Z), `JGE`(C), `JLT`(!C), `JGT`(C&&!Z), `JLE`(!C||Z).

---

## 3. Pipeline

### 3.1 Etapas

- **IF (Instruction Fetch):** PC entrega dirección a `MemoriaInstrucciones`; se captura `ifid_instr` y `ifid_pc`.
- **ID (Instruction Decode):** `UnidadControl` decodifica; `BancoRegistros` lee operandos; se calcula el salto.
- **EX (Execute):** `ALU` opera con operandos ya resueltos por forwarding; produce resultado y banderas.
- **MEM (Memory):** `MemoriaDatos` lee (`LD`) o escribe (`ST`); `IN`/`OUT` interactúan con E/S.
- **WB (Write Back):** mux elige ALU / inmediato / memoria / E/S / copia de registro y escribe el banco; actualiza banderas.

### 3.2 Registros intermedios (4)

`ifid_*`, `idex_*`, `exmem_*`, `memwb_*` (señales `sc_signal` actualizadas en un único proceso `reloj()` en `clk.pos()`).

### 3.3 Forwarding, hazards, stalls, branches, flush y HALT

- **Forwarding de datos:** de `EX/MEM` y `MEM/WB` hacia `EX` (`comb_forward`), más un bypass `WB→ID` para dependencia a distancia 3. `ex_a`/`ex_b` seleccionan `FWD_EXMEM`/`FWD_MEMWB`.
- **Forwarding de banderas:** de EX/EX-MEM/MEM-WB hacia la ID (`comb_flags`), para `CMP`→salto inmediato.
- **Hazard load-use:** `detectar_hazard` detiene cuando `LD` en EX alimenta a una instrucción en ID que usa el mismo registro; se inserta una burbuja (`burbuja_idex`) y se congela IF/ID y PC.
- **Stall:** el PC se congela (`pc.stall(pc_freeze)`) y el contador `stalls_totales` lo registra.
- **Branches:** se resuelven en ID. `control_pc` calcula `branch_taken = ifid_valid && ctrl_salto && !freeze`; el PC salta a `ctrl_dir_salto`. Al ciclo siguiente se invalida IF/ID (`flush`). Contadores `branches_tomados`/`flushes_totales`.
- **Flush:** por branch tomado o por HALT detectado en ID.
- **HALT:** `halt_ahora` detiene el fetch; `halt_stop` congela; cuando HALT llega a WB, `halted_reg` se vuelve verdadero y `halted` se mantiene **sticky** hasta reset.

### 3.4 Reset

En `clk.pos()` con `reset==1`:
- PC → 0 (`pc.h`), banco de registros → 0 (`banco_registros.h`), banderas → 0 (`registro_banderas.h`), memoria de datos → 0 (`memoria_datos.h`).
- El pipeline se invalida por completo (`ifid_valid/idex_valid/exmem_valid/memwb_valid = false`).
- `halt_stop=false`, `halted_reg=false`.
- El Mailbox, en su propio reset, limpia los campos, `r_req`, `r_ack`, `r_ticks` y `contando`.

---

## 4. Comunicación

### 4.1 Mapa de E/S (`communication_mailbox.h`)

**STRIKER escribe / GOALKEEPER lee:**
| Dir | Campo |
|---|---|
| `0x10` | shot_id |
| `0x11` | target_x |
| `0x12` | target_y |
| `0x13` | power |
| `0x14` | flight_ticks |
| `0x15` | REQ |

**GOALKEEPER escribe / STRIKER lee:**
| Dir | Campo |
|---|---|
| `0x16` | ACK |
| `0x20` | shot_id (eco) |
| `0x21` | dive_x |
| `0x22` | dive_y |
| `0x23` | reaction_ticks (lo llena el Mailbox) |
| `0x24` | reach_x |
| `0x25` | reach_y |

### 4.2 REQ/ACK y reaction_ticks

- **REQ:** escribir `0x15=1` arma `r_req`, inicia el temporizador y resetea `r_ticks=0`.
- **ACK:** GOALKEEPER escribe en `0x16`; solo se acepta si `r_req && !r_ack`, y detiene el temporizador.
- **`reaction_ticks` lo produce el Mailbox**, no la CPU: un contador que incrementa en cada `clk.pos()` mientras hay solicitud pendiente, se detiene con ACK y satura en 255. Lo que la CPU escriba en `0x23` se ignora.
- **Cierre:** escribir `0x15=0` baja REQ y ACK y libera el canal para la siguiente solicitud.

### 4.3 Orden exacto de la transacción

1. STRIKER escribe `0x10..0x14`.
2. STRIKER escribe `0x15=1` (REQ) → arranca el temporizador.
3. GOALKEEPER lee `0x15=1`, lee los 5 campos, ejecuta su programa.
4. GOALKEEPER escribe la respuesta `0x20..0x25`.
5. GOALKEEPER escribe `0x16` (ACK) → se congela el temporizador.
6. STRIKER lee `0x16=1` y la respuesta `0x20..0x25`.
7. STRIKER escribe `0x15=0`.
8. El Mailbox desactiva ACK y permite otra solicitud.

### 4.4 Buses distintos

- **Bus interno de la CPU:** `io_write/io_read/io_addr/io_wdata/io_rdata` (lado GOALKEEPER del Mailbox).
- **Bus externo del Mailbox (lado STRIKER):** `st_we/st_addr/st_wdata/st_re/st_rdata` (escritura/lectura por protocolo) y `ack` visible.
Ninguna CPU accede a los registros internos de la otra.

---

## 5. Estrategia del portero (`goalkeeper_program.h`)

### 5.1 Uso de los cinco parámetros

- `shot_id`: su bit 0 (paridad) decide el signo del sesgo horizontal de `dive_x`; además se devuelve como eco.
- `target_x`: base para `dive_x`.
- `target_y`: base para `dive_y`.
- `power`: decide si `reach` suma +8 (umbral 128).
- `flight_ticks`: decide el sesgo vertical de `dive_y` (+16 si <128) y resta −8 al `reach` si <128; además el Árbitro lo usa como límite de tiempo.

### 5.2 Fórmulas exactas

```text
paridad = shot_id & 1
dive_x  = target_x + 16  si paridad == 1
        = target_x - 16  si paridad == 0
dive_y  = target_y + 16  si flight_ticks < 128
        = target_y       si flight_ticks >= 128
reach   = 16 + (8 si power >= 128) - (8 si flight_ticks < 128)
reach_x = reach_y = reach
```

### 5.3 Rango válido y limitaciones

- Coordenadas válidas de portería: `target_x/y ∈ [32,223]`; en ese rango `±16` no desborda el byte.
- **Casos no cubiertos:** no hay saturación/clamping para coordenadas fuera de `[32,223]` (el Árbitro da MISS pero el `dive` puede quedar fuera de rango o envolver en 8 bits, p. ej. `target_x<16` con id par). `power` y `flight_ticks` son independientes (no se aplica `flight = 256 - power`). La estrategia es determinista y de umbrales fijos (128).

---

## 6. Árbitro (`arbitro.h`)

- **Portería:** `target_x ∈ [32,223]` y `target_y ∈ [32,223]`.
- **MISS:** si `target_x` o `target_y` fuera de `[32,223]`, o si `power == 0`.
- **SAVE:** `reaction_ticks <= flight_ticks` **y** `|target_x - dive_x| <= reach_x` **y** `|target_y - dive_y| <= reach_y`.
- **GOAL:** cualquier caso dentro de la portería que no cumpla las tres condiciones de SAVE.
- **Unidades/rangos:** todos los campos son enteros de 8 bits (0–255). `reaction_ticks` y `flight_ticks` en ciclos; coordenadas en unidades de 8 bits. Las comparaciones usan diferencias absolutas con `int` para evitar desbordes por signo.

Resultados: `ARB_MISS=0`, `ARB_GOAL=1`, `ARB_SAVE=2` (`resultado`, 2 bits).

---

## 7. Demostración (`demo_goalkeeper`)

- **Componentes reales:** `CPU_Goalkeeper`, `CommunicationMailbox`, `Arbitro` y el programa `goalkeeper_program::construir()`.
- **Componentes auxiliares:** `StrikerDemo` (generador de solicitudes por protocolo, no escribe en la CPU), el decodificador de mnemónicos y el formato/colores ANSI.
- **Información de pipeline mostrada (de `cpu.traza`):** por ciclo, la instrucción en IF, ID, EX, MEM y WB (solo ciclos con IN/OUT en EX); los eventos de E/S (IN/OUT con dirección y valor real de `ex_a`); el banco de registros observable tras ejecutar.
- **Ejecución:** `cd build && ./demo_goalkeeper` (o `NO_COLOR=1 ./demo_goalkeeper`). Menú: 1 disparo de prueba, 2 personalizado (valida 0–255), 3 secuencia automática, 4 estadísticas, 5 información técnica, 6 salir.

---

## 8. Presentación

### 8.1 Ficha técnica resumida

| | GOALKEEPER-8 |
|---|---|
| Tipo | Computador RISC de 8 bits, pipeline 5 etapas |
| Datos / PC / instrucción | 8 / 8 / 16 bits |
| Registros | 8 × 8 bits |
| Memoria | instrucciones 256×16, datos 256×8 |
| ALU | ADD, SUB, AND, OR, CMP |
| Banderas | C, Z (sin V) |
| ISA | 20 instrucciones, formatos R/I/J |
| Reloj de referencia | 10 ns (100 MHz), externo (`sc_clock`) |
| Comunicación | CommunicationMailbox, protocolo REQ/ACK |
| Programa del portero | 50 instrucciones, determinista |

### 8.2 Tabla de módulos

| Módulo | Tipo | Función |
|---|---|---|
| PC | secuencial | contador de programa |
| MemoriaInstrucciones | combinacional | ROM 256×16 |
| UnidadControl | combinacional | decodificación |
| BancoRegistros | secuencial | 8×8, 2L/1E |
| ALU | combinacional | operaciones + C/Z |
| RegistroBanderas | secuencial | C/Z selectivo |
| MemoriaDatos | secuencial | RAM 256×8 |
| CPU_Goalkeeper | contenedor | pipeline 5 etapas |
| CommunicationMailbox | secuencial | REQ/ACK + temporizador |
| Arbitro | combinacional | MISS/GOAL/SAVE |
| StrikerSim/StrikerDemo | auxiliar | emulador STRIKER |

### 8.3 Tabla de puertos y señales

**CPU_Goalkeeper:** `clk`(in), `reset`(in), `io_write`(out), `io_read`(out), `io_addr`(out,8), `io_wdata`(out,8), `io_rdata`(in,8), `halted`(out).

**CommunicationMailbox:** `clk`, `reset`, `st_we`, `st_addr`(8), `st_wdata`(8), `st_re`, `st_rdata`(out,8), `gk_we`, `gk_addr`(8), `gk_wdata`(8), `gk_re`, `gk_rdata`(out,8), `req`(out), `ack`(out), `reaction_ticks`(out,8).

**Arbitro:** `target_x/y`, `power`, `flight_ticks`, `dive_x/y`, `reaction_ticks`, `reach_x/y` (todos in, 8 bits), `resultado` (out, 2 bits).

### 8.4 Diez preguntas técnicas difíciles

**1. ¿Por qué no hay bandera V (overflow)?**
Porque la ALU (`alu.h`) solo produce `carry` y `zero`; no hay salida ni registro para overflow. La ISA opera todo en 8 bits sin signo; `CMP` compara `A >= B` sin signo. No implementada por diseño.

**2. ¿Cómo se resuelve una dependencia `LD R0` → `OUT R0` (load-use)?**
`detectar_hazard` ve `LD` en EX y un consumidor en ID que usa el mismo `rd`; asigna `stall`, congela IF/ID y PC, e inserta una burbuja en ID/EX. Tras el stall, el dato viaja por `WB→ID` o `MEM/WB→EX`. En la estrategia real esto ocurre en `LD R0,0x00` → `OUT R0,0x20`.

**3. ¿Por qué el salto condicional se decide en ID y no en EX?**
`UnidadControl` produce `ctrl_salto`/`ctrl_dir_salto` a partir de `ifid_instr` (ID) y las banderas reenviadas (`comb_flags`). `control_pc` lo usa directamente. El flush solo invalida IF/ID. (El diseño evita esperar hasta EX, como se verifica en `test_branch.cpp`.)

**4. ¿Qué hace `MOV R6,R2` internamente y por qué no usa la ALU?**
`UnidadControl` (`MOV`) pone `write_back_sel = WB_RA` y `reg_a_sel = ra`; en WB el dato es `ex_a` (la copia del registro fuente), sin pasar por ALU y **sin modificar banderas**.

**5. ¿Quién produce `reaction_ticks` y por qué el programa escribe 0 en `0x23`?**
Lo produce el Mailbox (`r_ticks`), incrementado en cada `clk.pos()` mientras hay solicitud, detenido con ACK, saturado en 255. El `OUT R7,0x23` de la CPU es un marcador que el Mailbox **ignora** (rama `a==0x23` vacía).

**6. ¿Puede la CPU emitir una respuesta duplicada?**
No. El programa escribe una vez y luego espera `REQ=0`. Además el Mailbox solo acepta la respuesta si `r_req && !r_ack` y el ACK una sola vez. El monitor de `test_fase6` confirma exactamente 7 OUT por solicitud (70 en 10 solicitudes).

**7. ¿Qué pasa con `power` y `flight_ticks`? ¿Son dependientes?**
Son campos independientes del protocolo; no se valida `flight = 256 - power`. La estrategia usa `power` (umbral 128) para `reach` y `flight_ticks` (umbral 128) para `dive_y` y `reach`; el Árbitro usa `flight_ticks` como límite de tiempo.

**8. ¿Por qué `CMP R4,R0` compara contra `R0` y no contra un inmediato?**
La ISA no tiene CMP con inmediato. El programa carga la constante 128 en un registro (`LDI R0,128`) y luego hace `CMP R4,R0`. El carry de `CMP` (`A>=B`) se usa en `JGE`/`JLT`.

**9. ¿Qué ocurre si el disparo está fuera de `[32,223]`?**
El Árbitro devuelve MISS (`arbitro.h`). La CPU igualmente calcula un `dive` (puede quedar fuera de rango o envolver en 8 bits para `target_x<16` con id par), pero el resultado final es MISS. La estrategia no hace clamping.

**10. ¿Por qué `HALT` no está en el programa principal del portero?**
El portero nunca debe detenerse: tras responder espera `REQ=0` y vuelve a `INICIO`. `HALT` se usa solo en un programa mínimo de prueba (`construir_halt()`). La señal `halted` es sticky hasta reset, pero en la integración queda en 0.

---

**Fin del inventario.** No se realizaron modificaciones ni commits; el repositorio permanece en `8130d1b03cb86d405d730fd4f85b53fb470ef86f`.

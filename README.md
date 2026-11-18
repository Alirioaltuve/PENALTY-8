# STRIKER-8 — Computador lanzador de PENALTY-8

Computador RISC de **8 bits** implementado en **C++ y SystemC** que simula a un
jugador ejecutando un lanzamiento de penalti. Calcula las coordenadas del
disparo, la potencia, el tiempo de vuelo y las distancias al centro del arco, y
se comunica con **GOALKEEPER-8** mediante un buzón con handshake **REQ/ACK**.

Esta versión implementa el contrato **PENALTY-8 v1.0**:

- Instrucciones fijas de **16 bits** y memoria de programa de **256 palabras**.
- **PC de 8 bits** con direccionamiento por palabra (`PC = PC + 1`).
- Exactamente **8 registros** generales de 8 bits (`R0`–`R7`).
- ALU con códigos de operación de **3 bits** y banderas **Z** y **C**.
- ISA con **opcode de 5 bits** y formatos **R, I y J**.
- `CommunicationMailbox` con petición de **5 bytes**, respuesta de **6 bytes**,
  puertos `0x10`–`0x25` y contador `reaction_ticks`.

> Todo el cálculo del lanzamiento se ejecuta dentro del programa de la CPU
> simulada. Ninguna interfaz gráfica participa en los cálculos y el procesador
> no se sustituye por funciones C++ que calculen el tiro directamente.

---

## 1. Estructura del entregable

```
striker/
├── striker.h            ISA, ALU, control, registros, memorias, buzón y Striker8
├── striker.cpp          Implementación del hardware y del secuenciador
├── programa_striker.h   Programa máquina de 99 palabras de 16 bits y constantes
├── pruebas_striker.cpp  Banco de pruebas (561 comprobaciones)
├── Makefile             Compilación y ejecución (WSL/Linux y Windows)
└── README.md            Este documento
```

---

## 2. Arquitectura del computador

```
                         ┌──────────────────────────────────────────┐
   entrada_x ───────────►│                                          │
   entrada_y ───────────►│              STRIKER-8 (CPU)             │
   entrada_potencia ────►│                                          │
   entrada_modo ────────►│   PC (8) ─► Memoria de instrucciones     │
   lanzar ──────────────►│    │              256 x 16 bits           │
                         │    │                   │                  │
                         │    ▼                   ▼                  │
                         │  Secuenciador ─────► IR (16)              │
                         │  Búsqueda/Dec./Ejecución                 │
                         │       │      │        │                  │
                         │       │      │        └──► ALU8 ──┐       │
                         │       │      │                    │       │
                         │       │      └──► Banco de 8 registros ◄─┤
                         │       │                              │   │
                         │       └─────────► Memoria de datos ◄─┘   │
                         │                    256 bytes            │
                         │                        │                  │
                         │                        ▼                  │
                         │        CommunicationMailbox (REQ/ACK)    │
                         └───────────┬──────────────────┬───────────┘
                                     │                  │
                              req_valid / req_byte  resp_valid /
                              solicitud_enviada     resp_indice / resp_dato
                              reaction_ticks        resp_byte / respuesta_lista
```

### Unidades implementadas

| Unidad | Tipo SystemC | Descripción |
|---|---|---|
| `ALU8` | `SC_MODULE` | ADD, SUB, AND, OR y CMP de 8 bits. Código de operación de 3 bits. Genera resultado, Z y C. |
| `UnidadControl` | `SC_MODULE` | Decodifica el opcode de 5 bits y activa las señales de control. Detecta opcodes no válidos. |
| `BancoRegistros` | `SC_MODULE` | 8 registros de 8 bits (`R0`…`R7`) con escritura síncrona. |
| `MemoriaInstrucciones` | `SC_MODULE` | 256 palabras de 16 bits, lectura combinacional. |
| `MemoriaDatos` | `SC_MODULE` | 256 bytes de datos, escritura síncrona. |
| `CommunicationMailbox` | `SC_MODULE` | Buzón REQ/ACK de 5 bytes de petición y 6 de respuesta, con `reaction_ticks`. |
| `PC` | `sc_signal<sc_uint<8>>` | Contador de programa de 8 bits, direccionamiento por palabra. |
| `IR` | `sc_signal<sc_uint<16>>` | Registro de instrucción de 16 bits. |
| `Striker8` | `SC_MODULE` | CPU que integra y conecta todas las unidades. |

### Salidas de diagnóstico

| Señal | Significado |
|---|---|
| `pc_actual` | PC de 8 bits (dirección de palabra). |
| `ir_actual` | Instrucción de 16 bits en el IR. |
| `bandera_cero` | Copia de la bandera Z. |
| `bandera_acarreo` | Copia de la bandera C. |
| `ocupado` | La CPU está ejecutando un lanzamiento. |
| `detenido` | La CPU alcanzó `HALT` (o se detuvo por instrucción inválida). |
| `error_instruccion` | Se encontró un opcode reservado o no válido. |
| `error_comunicacion` | El buzón detectó un error de protocolo. |

---

## 3. ISA de 16 bits (propuesta provisional v1.0)

> **La ISA todavía no está confirmada por el desarrollador de GOALKEEPER-8.**
> Los códigos, formatos y longitudes de esta sección son una propuesta
> pendiente de validación conjunta. No se declara definitiva ni integrada.

Cada instrucción ocupa **exactamente una palabra de 16 bits**. El PC avanza
siempre una palabra (`PC = PC + 1`).

### Formato R (registro a registro)

```
[15:11] opcode   [10:8] Rd   [7:5] Rs1   [4:2] Rs2   [1:0] 00
```

### Formato I (inmediato, memoria o puerto)

```
[15:11] opcode   [10:8] Rd/Rs   [7:0] imm8 / dir8 / puerto8
```

### Formato J (salto)

```
[15:11] opcode   [10:8] 000   [7:0] dir8 (dirección de palabra)
```

### Tabla de opcodes

| Opcode | Mnemónico | Formato | Operación |
|:---:|---|:---:|---|
| `0x00` | `NOP` | — | No hace nada. |
| `0x01` | `MOV Rd, Rs1` | R | `Rd ← Rs1` |
| `0x02` | `ADD Rd, Rs1, Rs2` | R | `Rd ← Rs1 + Rs2` |
| `0x03` | `SUB Rd, Rs1, Rs2` | R | `Rd ← Rs1 − Rs2` |
| `0x04` | `AND Rd, Rs1, Rs2` | R | `Rd ← Rs1 AND Rs2` |
| `0x05` | `OR Rd, Rs1, Rs2` | R | `Rd ← Rs1 OR Rs2` |
| `0x06` | `CMP Rs1, Rs2` | R | Actualiza Z y C con `Rs1 − Rs2`. |
| `0x07` | `LDI Rd, imm8` | I | `Rd ← imm8` |
| `0x08` | `LD Rd, dir8` | I | `Rd ← mem[dir8]` |
| `0x09` | `ST Rs, dir8` | I | `mem[dir8] ← Rs` |
| `0x0A` | `IN Rd, puerto8` | I | `Rd ← puerto[puerto8]` |
| `0x0B` | `OUT Rs, puerto8` | I | `puerto[puerto8] ← Rs` |
| `0x0C` | `JMP dir8` | J | `PC ← dir8` |
| `0x0D` | `JEQ dir8` | J | Si `Z`, `PC ← dir8` |
| `0x0E` | `JNE dir8` | J | Si `!Z`, `PC ← dir8` |
| `0x0F` | `JGE dir8` | J | Si `C`, `PC ← dir8` (es decir, `A ≥ B`) |
| `0x10` | `JLT dir8` | J | Si `!C`, `PC ← dir8` (es decir, `A < B`) |
| `0x11` | `JGT dir8` | J | Si `C && !Z`, `PC ← dir8` (`A > B`) |
| `0x12` | `JLE dir8` | J | Si `!C || Z`, `PC ← dir8` (`A ≤ B`) |
| `0x13` | `HALT` | — | Detiene la CPU. |
| `0x14`–`0x1F` | *reservado* | — | **No son instrucciones válidas.** |

`isa8::es_valida(op)` acepta `0x00`–`0x13`. `0x14`–`0x1F` activan
`error_instruccion` y detienen la CPU de forma segura.

### Codificación y decodificación

```cpp
R = (op << 11) | (Rd << 8) | (Rs1 << 5) | (Rs2 << 2);
I = (op << 11) | (R  << 8) | imm8;
J = (op << 11) | dir8;
```

El header incluye los ayudantes `isa8::codigo()`, `campo_d()`, `campo_s1()`,
`campo_s2()`, `inmediato()`, `codificar_r()`, `codificar_i()`, `codificar_j()`,
`formato_r()`, `formato_i()`, `formato_j()` y `es_valida()`.

---

## 4. ALU de 8 bits y banderas

Códigos de operación de **3 bits**:

| Código | Operación | Resultado | C |
|:---:|---|---|:---:|
| `000` | `ADD` | `(A + B) & 0xFF` | 1 si hay acarreo de salida (`A + B > 255`) |
| `001` | `SUB` | `(A − B) & 0xFF` | 1 si `A ≥ B` (sin préstamo), 0 si `A < B` |
| `010` | `AND` | `A AND B` | 0 |
| `011` | `OR` | `A OR B` | 0 |
| `100` | `CMP` | `(A − B) & 0xFF` (solo banderas) | 1 si `A ≥ B`, 0 si `A < B` |

La bandera **Z** vale 1 cuando el resultado de 8 bits es cero. Se actualiza en
`ADD`, `SUB`, `AND`, `OR` y `CMP`. La bandera **C** se actualiza en esas mismas
operaciones con la semántica de la tabla. `MOV`, `LDI`, `LD`, `IN`, `ST` y
`OUT` no modifican las banderas.

Con esta definición:

- `JGE` salta si `A ≥ B`.
- `JLT` salta si `A < B`.
- `JGT` salta si `A > B`.
- `JLE` salta si `A ≤ B`.
- `JEQ` y `JNE` usan únicamente Z.

---

## 5. Banco de registros y memorias

### Banco de registros

- Exactamente **8 registros** de 8 bits (`R0`…`R7`).
- Escritura síncrona en el flanco de subida con habilitación (`escritura`).
- Lectura mediante `valor(indice)`; el índice se enmascara a 3 bits.
- `reiniciar()` pone los 8 registros a cero.

### Memoria de instrucciones

- **256 palabras de 16 bits**, lectura combinacional.
- `cargar(programa, tamaño)` devuelve `true` si el programa cupo completo.
- **Antes de copiar, `cargar()` limpia las 256 palabras a `0x0000`.** Así un
  programa corto cargado después de uno más largo nunca hereda instrucciones
  residuales: todas las palabras sobrantes quedan como `NOP`.
- Si el programa supera las 256 palabras se trunca de forma segura y devuelve
  `false`; nunca se desborda el arreglo.
- Un puntero nulo se rechaza con `false`.
- `limpiar()` borra las 256 palabras.

### Memoria de datos

- **256 bytes**, escritura síncrona (`ST`).
- Lectura mediante `contenido(dir)`.
- `limpiar()` borra la memoria; `escribir_directo()` se usa para precargar las
  constantes del programa.
- Las constantes del programa (`0xE0`–`0xEA`) se cargan al iniciar y se
  conservan entre lanzamientos.

---

## 6. Ciclo de ejecución de la CPU (pipeline de 5 etapas)

La CPU ya no ejecuta una instrucción completa antes de empezar la siguiente.
Ahora usa un **pipeline de 5 etapas** y cuatro registros de pipeline:

```
IF → ID → EX → MEM → WB
     IF/ID  ID/EX  EX/MEM  MEM/WB
```

| Etapa | Acción |
|---|---|
| **IF** (búsqueda) | Lee `mem_programa[PC]` y avanza `PC ← PC + 1`. |
| **ID** (decodificación) | Decodifica el opcode, lee `Rs1`/`Rs2` y resuelve los saltos con las banderas reenviadas desde EX. |
| **EX** (ejecución) | Calcula la ALU, el destino de un salto y el dato que se guardará o enviará. |
| **MEM** (memoria) | Lee/escribe la memoria de datos, lee puertos de E/S y escribe en el buzón. |
| **WB** (escritura) | Escribe el resultado en el banco de registros. |

Detalles de sincronización:

- El PC es de 8 bits y direcciona **palabras**, no bytes.
- La etapa WB escribe el banco **antes** de que ID lea los registros
  (*write-first*). Así una instrucción puede ver el resultado de la que se
  retira en ese mismo ciclo.
- **Forwarding**: EX toma operandos desde `EX/MEM` y `MEM/WB` cuando detecta
  una dependencia. Esto evita casi todas las burbujas.
- **Peligro load-use**: si un `LD` o `IN` va seguido de una instrucción que usa
  su destino, ID se congela un ciclo y EX recibe una burbuja. El consumidor
  llega a EX cuando el dato ya está en `MEM/WB`.
- **Saltos**: se resuelven en ID. Si el salto se toma, se descarta la
  instrucción que estaba en IF y se pierde **un solo ciclo**. Las banderas se
  reenvían desde EX si el `CMP`/`ADD` que las produjo está en esa etapa.
- **HALT / instrucción inválida**: se dejan fluir hasta WB y se suprime la
  búsqueda. Por eso `PC` queda en la palabra siguiente a `HALT` (99 en el
  programa de 99 palabras) y `IR` conserva la instrucción retirada.
- El buzón conserva el protocolo REQ/ACK y el pulso `req_valid` de un ciclo.
  La escritura de `OUT` se deja en los cables y el buzón la consume en el
  siguiente flanco, igual que en el diseño anterior.

Estados del secuenciador:

```
REPOSO → ACTIVO (IF/ID/EX/MEM/WB) → DETENIDO
```

---

## 7. CommunicationMailbox (REQ/ACK)

### Mapa de puertos

| Puerto | Nombre | Acceso | Significado |
|:---:|---|:---:|---|
| `0x10` | `REQ` | W/R | W: 1 inicia la petición. R: 1 si hay transacción activa. |
| `0x11` | `ACK` | W/R | R: 1 si la respuesta de 6 bytes está completa. W: 1 confirma y limpia ACK. |
| `0x12` | `STATE` | R | bit0 busy, bit1 pendiente, bit2 listo, bit3 error. |
| `0x13` | `REQ_IDX` | W/R | Índice `0..4` para `REQ_DATA`. |
| `0x14` | `REQ_DATA` | W/R | Byte de petición seleccionado. |
| `0x15` | `RESP_IDX` | W/R | Índice `0..5` para `RESP_DATA`. |
| `0x16` | `RESP_DATA` | R | Byte de respuesta seleccionado. |
| `0x17` | `TICKS_L` | R | `reaction_ticks`, byte bajo. |
| `0x18` | `TICKS_H` | R | `reaction_ticks`, byte alto. |
| `0x19`–`0x1D` | `REQ_0..4` | W/R | Bytes 0..4 de la petición. |
| `0x1E`–`0x23` | `RESP_0..5` | R | Bytes 0..5 de la respuesta. |
| `0x24` | `CLEAR` | W | 1 limpia todo el buzón. |
| `0x25` | `IRQ` | R | 1 si hay respuesta lista (sin limpiar ACK). |

### Protocolo

1. La CPU escribe los **5 bytes de petición** en `0x19`–`0x1D` (o mediante
   `REQ_IDX`/`REQ_DATA`).
2. La CPU escribe `REQ = 1` en `0x10`. El buzón comprueba que los 5 bytes estén
   presentes, activa `busy`, pone `pendiente = 1` y genera un **pulso de un
   ciclo** en `req_valid` con los 5 bytes en `req_byte[0..4]`.
3. GOALKEEPER-8 entrega **6 bytes** mediante `resp_valid`, `resp_indice` y
   `resp_dato`, en orden estricto `0,1,2,3,4,5`.
4. Al recibir el sexto byte se activa `resp_ready`/ACK, `busy = 0` y
   `pendiente = 0`.
5. La CPU lee los 6 bytes (`0x1E`–`0x23` o `RESP_IDX`/`RESP_DATA`) y
   `reaction_ticks` (`0x17`/`0x18`), y escribe `ACK = 1` en `0x11` para
   confirmar. ACK limpia `resp_ready` pero **conserva los bytes**.

### Reglas y errores

- `REQ` sin los 5 bytes de petición activa `error` y no inicia la transacción.
- Un índice de petición fuera de `0..4` o de respuesta fuera de `0..5` activa
  `error`.
- Un byte de respuesta con índice distinto del esperado (fuera de orden)
  activa `error` y no se acepta.
- `CLEAR` (`0x24 = 1`) limpia petición, respuesta, estado, error y ticks.
- `RESET` limpia todo el buzón.

### reaction_ticks

`reaction_ticks` es un contador de 16 bits que empieza en 0 al aceptar `REQ`,
se incrementa cada ciclo mientras la transacción está pendiente y se **congela**
al llegar el primer byte de respuesta. La CPU lo lee en `0x17`/`0x18` y lo
guarda en `0x36`/`0x37`.

---

## 8. Programa de lanzamiento

El programa (`programa_striker.h`, **99 palabras de 16 bits**) ejecuta el
algoritmo completo dentro de la CPU:

1. **Lee** `X`, `Y`, potencia y modo por los puertos `IN` `0x00`–`0x03`.
2. **Modo automático**: si el modo es distinto de 0, decide la esquina superior
   derecha `X = 28`, `Y = 12` y una potencia de `90`.
3. **Satura** los parámetros: `X ≤ 31`, `Y ≤ 15`, `P ≤ 100`.
4. **Calcula** `dx = |X − 16|`, `dy = |Y − 8|` con comparaciones sin signo.
5. **Calcula** `Pf = min(P + Y, 100)`.
6. **Calcula** `T = 20 + dx + dy + (100 − Pf)`.
7. **Almacena** `X`, `Y`, `T`, `Pf`, `dx`, `dy` e `ID` en la memoria de datos.
8. **Escribe** la petición de 5 bytes en el buzón y activa `REQ`.
9. **Espera ACK** con un límite de **64 iteraciones** (timeout autónomo).
10. Si llega la respuesta, **lee y guarda** los 6 bytes y `reaction_ticks`,
    marca el estado como 1 y confirma con `ACK = 1`.
11. Termina con `HALT`. Si no hay respuesta, termina igualmente con el estado
    `0x38 = 0`.

### Contrato del modo

- `0` → manual: se usan los valores del usuario.
- `1` → automático: el programa elige `(28, 12)` y potencia `90`.
- Cualquier otro valor → el programa solo compara contra cero, por lo que
  también sigue el camino automático. Este comportamiento está documentado y
  cubierto por la prueba 13.

### Petición enviada al portero

| Byte | Contenido | Valores |
|:---:|---|---|
| 0 | Identificador del mensaje | `0xA1` |
| 1 | Coordenada X del balón | 0–31 |
| 2 | Coordenada Y del balón | 0–15 |
| 3 | Tiempo de vuelo | 1–255 |
| 4 | Potencia | 0–100 |

### Mapa de memoria de datos

| Dirección | Contenido | Dirección | Contenido |
|:---:|---|:---:|---|
| `0x10` | X final | `0x30`–`0x35` | respuesta de 6 bytes |
| `0x11` | Y final | `0x36` | `reaction_ticks` (bajo) |
| `0x12` | Tiempo de vuelo | `0x37` | `reaction_ticks` (alto) |
| `0x13` | Potencia final | `0x38` | 0 = timeout, 1 = respuesta |
| `0x14` | dx | `0xE0` | 31 (`MAX_X`) |
| `0x15` | dy | `0xE1` | 15 (`MAX_Y`) |
| `0x16` | Identificador `0xA1` | `0xE2` | 100 (`MAX_P`) |
| | | `0xE3` | 16 (`CENTRO_X`) |
| | | `0xE4` | 8 (`CENTRO_Y`) |
| | | `0xE5` | 20 (`BASE_T`) |
| | | `0xE6` | `0xA1` (ID) |
| | | `0xE7` | 28 (`AUTO_X`) |
| | | `0xE8` | 12 (`AUTO_Y`) |
| | | `0xE9` | 90 (`AUTO_P`) |
| | | `0xEA` | 0 (`CERO`) |

### Registros usados por el programa

| Registro | Uso |
|---|---|
| `R0` | X |
| `R1` | Y |
| `R2` | Potencia solicitada; después, temporal de entrada y respuesta |
| `R3` | Modo; después, contador de reintentos de ACK |
| `R4` | Potencia final |
| `R5` | Tiempo de vuelo |
| `R6` | dx; después, temporal 0/1 para la espera de ACK |
| `R7` | dy; después, constante auxiliar de comparación |

> `dx` y `dy` quedan almacenados en `0x14` y `0x15`. Los registros `R2`, `R3` y
> `R6` se reutilizan como temporales durante la espera de ACK.

### Listado resumido del programa

El listado completo, palabra a palabra, está en `programa_striker.h`. Las
etiquetas principales son:

| Etiqueta | Palabra | Función |
|---|---:|---|
| `manual` | 10 | Camino manual tras leer el modo. |
| `x_ok` | 14 | X validada. |
| `y_ok` | 18 | Y validada. |
| `p_ok` | 22 | Potencia validada. |
| `pf_ok` | 28 | Potencia final calculada. |
| `dx_pos` | 34 | X ≥ 16. |
| `dx_done` | 36 | dx calculado. |
| `dy_pos` | 42 | Y ≥ 8. |
| `dy_done` | 44 | dy calculado. |
| `wait` | 70 | Espera de ACK. |
| `resp_ok` | 79 | Lectura de la respuesta. |
| `fin` | 98 | `HALT`. |

---

## 9. Compilación y ejecución

### WSL / Linux (entorno principal)

Requisitos: `g++` con C++17, SystemC 3.0.2 y `pkg-config`.

```bash
cd ruta/del/proyecto/striker
make          # compila las pruebas
make run      # compila y ejecuta
make clean    # borra el ejecutable
```

El `Makefile` detecta SystemC con:

```bash
pkg-config --cflags --libs systemc
```

Si el archivo `.pc` no existe, usa `-lsystemc -lpthread`. El estándar por
defecto en Linux/WSL es `-std=c++17`.

### Windows (MSYS2 / MinGW, opcional)

```bash
make SYSTEMC_HOME=<ruta-a-systemc> run
```

El `Makefile` usa `-std=c++20` y `-lws2_32` en Windows, y permite sobrescribir
`SYSTEMC_HOME`, `SYSTEMC_CFLAGS`, `SYSTEMC_LDFLAGS` y `SYSTEMC_LIBS`.

---

## 10. Pruebas

`pruebas_striker.cpp` ejecuta **15 grupos** y **561 comprobaciones**. El
programa devuelve `0` si todas pasan y `1` si hay algún fallo.

1. Codificación de la ISA de 16 bits y formatos R, I y J.
2. ALU de 3 bits, incluida la semántica `C = 1` si `A ≥ B`.
3. Banco de 8 registros.
4. Memoria de 256 palabras y memoria de datos.
5. Lanzamiento manual.
6. Lanzamiento automático.
7. Saturación de parámetros.
8. Caso extremo.
9. Tiro por encima del centro.
10. Timeout autónomo sin GOALKEEPER-8 (64 reintentos).
11. `CommunicationMailbox` aislado: petición incompleta, REQ, ACK, índices
    inválidos, respuesta fuera de orden, `CLEAR`, reset y congelación de
    `reaction_ticks`.
12. Banderas Z/C a nivel de CPU y saltos `JLT`/`JGT` (tomados y no tomados).
13. Modo fuera de rango (`modo = 2`).
14. Instrucción reservada `0x14` y programa `NOP + HALT`.
15. Pipeline: solapamiento de instrucciones (21 instrucciones + `HALT` en
    25 ciclos, frente a 63 del modelo multiciclo).

### Ayudante para la evaluación en vivo

`BancoPruebas::probar_programa_profesor(const uint16_t* prog, size_t n, int maximo = 6000)`
carga cualquier programa de 16 bits, lo ejecuta hasta `HALT` e imprime un
resumen con **ciclos ejecutados, PC final, IR final, banderas Z y C y los ocho
registros `R0`–`R7`**.

```cpp
const uint16_t mi_programa[] = {
    isa8::codificar_i(isa8::OP_LDI, 0, 5),
    isa8::codificar_i(isa8::OP_LDI, 1, 7),
    isa8::codificar_r(isa8::OP_ADD, 2, 0, 1),
    isa8::codificar_r(isa8::OP_SUB, 3, 2, 1),
    isa8::codificar_j(isa8::OP_HALT, 0)
};
banco_pruebas.probar_programa_profesor(mi_programa, 5);
```

Al final de `make run` se ejecuta un programa de ejemplo con este ayudante:

```
  Palabras cargadas : 5
  Ciclos ejecutados : 9
  PC final          : 5 (0x05)
  IR final          : 0x9800
  Banderas          : Z = 0   C = 1
  Registros         :  R0=  5  R1=  7  R2= 12  R3=  5  R4=  0  R5=  0  R6=  0  R7=  0
  Estado            : DETENIDO en HALT
```

### Resultados esperados de los lanzamientos

| Caso | Entrada (X, Y, P, modo) | X | Y | T | Pf | dx | dy | Petición |
|---|---|---:|---:|---:|---:|---:|---:|---|
| Manual | 20, 8, 85, 0 | 20 | 8 | 31 | 93 | 4 | 0 | `A1 14 08 1F 5D` |
| Automático | 5, 3, 50, 1 | 28 | 12 | 36 | 100 | 12 | 4 | `A1 1C 0C 24 64` |
| Saturado | 200, 200, 200, 0 | 31 | 15 | 42 | 100 | 15 | 7 | `A1 1F 0F 2A 64` |
| Extremo | 0, 0, 0, 0 | 0 | 0 | 144 | 0 | 16 | 8 | `A1 00 00 90 00` |
| Centro | 5, 12, 50, 0 | 5 | 12 | 73 | 62 | 11 | 4 | `A1 05 0C 49 3E` |

En las pruebas integradas, el portero simulado devuelve 6 bytes arbitrarios
(configurables) y se comprueba que:

- El pulso `req_valid` se genera exactamente una vez por lanzamiento.
- Los 5 bytes de la petición son correctos.
- Los 6 bytes de respuesta se reciben en orden y se guardan en `0x30`–`0x35`.
- `reaction_ticks` es mayor que 0, se congela y coincide con los bytes
  guardados en `0x36`/`0x37`.
- El programa confirma con ACK y deja `0x38 = 1`.
- Sin portero, el programa termina por timeout con `0x38 = 0`.

Salida final esperada:

```
Comprobaciones: 561   Fallos: 0
RESULTADO: TODAS LAS PRUEBAS PASARON
```

---

## 11. Limitaciones y pendientes para la integración

- **ISA provisional**: los códigos, formatos y longitudes deben confirmarse
  con el desarrollador de GOALKEEPER-8 antes de la integración definitiva.
- **Semántica de los 6 bytes de respuesta**: el programa los transporta y los
  guarda, pero su significado (tipo de respuesta, resultado del portero, etc.)
  debe definirse conjuntamente.
- **Sin conexión real con GOALKEEPER-8**: el banco de pruebas usa un portero
  simulado que respeta el handshake REQ/ACK. No se implementa el modelo interno
  del portero ni se inventan instrucciones para él.
- **Interfaz SFML**: no forma parte de este conjunto de archivos; aquí solo se
  entrega el núcleo del computador. No se ha modificado ningún archivo de la
  interfaz gráfica.
- **Carga de programa en caliente**: el programa se carga antes del reset o del
  lanzamiento. No se define un protocolo de recarga durante la ejecución.

---

## 12. Restricciones cumplidas

- ✅ C++ y SystemC (sin Python).
- ✅ Instrucciones fijas de 16 bits y 256 palabras de programa.
- ✅ PC de 8 bits con direccionamiento por palabra.
- ✅ Exactamente 8 registros de 8 bits (`R0`–`R7`).
- ✅ ALU de 3 bits con ADD, SUB, AND, OR, CMP y banderas Z/C.
- ✅ `C = 1` si `A ≥ B` en SUB y CMP.
- ✅ ISA de opcode de 5 bits con formatos R, I y J.
- ✅ `LDI`, `LD`, `ST`, `IN`, `OUT`, `JMP`, `JEQ`, `JNE`, `JGE`, `JLT`,
  `JGT` y `JLE`.
- ✅ `CommunicationMailbox` con REQ/ACK, petición de 5 bytes, respuesta de
  6 bytes y `reaction_ticks` en `0x10`–`0x25`.
- ✅ Programa de lanzamiento reescrito en 99 palabras de 16 bits.
- ✅ Mismos resultados de lanzamiento que la versión anterior.
- ✅ Cálculos realizados por el programa de la CPU, no por la interfaz.
- ✅ Código y documentación en español.
- ✅ C++17 y SystemC 3.0.2 en WSL.

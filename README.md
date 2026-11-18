# PENALTY-8 — GOALKEEPER-8

Proyecto universitario de Arquitectura del Computador (C++ + SystemC).

PENALTY-8 está formado por dos computadores RISC de 8 bits:

- **STRIKER-8**: calcula y ejecuta los disparos. *Desarrollado por Rafael.*
- **GOALKEEPER-8**: analiza el disparo, decide su reacción y responde.

Este repositorio contiene **GOALKEEPER-8 completo** (un computador RISC
pipeline de cinco etapas, programable), la **comunicación**
(`CommunicationMailbox`), el **árbitro**, un **simulador del lado STRIKER**
(`StrikerSim`) y dos ejecutables:

- **`demo_goalkeeper`**: demostración interactiva del sistema de penaltis.
- **`eval_goalkeeper`**: evaluador que ensambla y ejecuta programas RISC
  independientes (escritos en ensamblador `.asm`) en la CPU real.

> **Aviso importante:** no existe código del STRIKER-8 de Rafael en este
> repositorio. La integración documentada aquí es con un simulador de protocolo,
> no con el STRIKER real. Ver `docs/cpu_striker.md` y
> `docs/guia_integracion_striker.md`.

---

## Arquitectura

| Característica | Especificación |
|---|---|
| Arquitectura | RISC |
| Ancho de datos | 8 bits |
| Registros | 8 (R0–R7) |
| Ancho de PC | 8 bits |
| Ancho de instrucción | 16 bits (20 instrucciones, formatos R/I/J) |
| Memoria de instrucciones | 256 palabras de 16 bits |
| Memoria de datos | 256 bytes |
| ALU | ADD, SUB, AND, OR, CMP (banderas C y Z) |
| Ejecución | Pipeline RISC de 5 etapas (IF-ID-EX-MEM-WB) |
| Extras | forwarding, hazard detection, stalls, branches, flush, HALT |
| Reset | Síncrono, activo en alto |

Detalles: `docs/arquitectura_goalkeeper.md` e `inventario_tecnico.md`.

---

## Estructura

```
.
├── isa.h                      ISA y helpers de codificación
├── banco_registros.h          Banco de 8 × 8 bits
├── alu.h                      Unidad aritmético-lógica
├── pc.h                       Contador de programa
├── memoria_instrucciones.h    Memoria de programa (256 × 16)
├── memoria_datos.h            Memoria de datos (256 × 8)
├── unidad_control.h           Decodificador combinacional
├── registro_banderas.h        Registro de banderas C/Z
├── cpu_goalkeeper.h           CPU completa (pipeline)
├── communication_mailbox.h    Canal REQ/ACK + temporizador
├── arbitro.h                  Reglas MISS/GOAL/SAVE
├── goalkeeper_program.h       Programa del portero
├── striker_sim.h              Simulador del lado STRIKER (protocolo)
├── assembler.h                Ensamblador de programas .asm
├── demo_goalkeeper.cpp        Demostración interactiva de penaltis
├── eval_goalkeeper.cpp        Evaluador de programas independientes
├── testbench*.cpp / test_*.cpp  Pruebas (cada una con su sc_main)
├── examples/                  13 programas .asm de ejemplo
├── CMakeLists.txt
└── docs/                      Documentación
```

---

## Cómo compilar

Requisitos: `cmake`, `g++` (C++17) y `SystemC` 2.3.x (`pkg-config --exists systemc`).

```bash
mkdir -p build
cd build
cmake ..
make -j4
```

Se generan todos los ejecutables, incluidos `demo_goalkeeper` y
`eval_goalkeeper`.

---

## Cómo ejecutar la demostración de penaltis

```bash
./build/demo_goalkeeper
```

Muestra un menú interactivo (disparo de prueba, disparo personalizado, secuencia
automática, estadísticas, información técnica). Ejecuta la CPU, el Mailbox y el
Árbitro reales; el lado STRIKER se emula. Texto plano: `NO_COLOR=1`.

Guía completa: `docs/GUIA_DEMO_PENALTIS.md`.

---

## Cómo ejecutar programas RISC independientes

`eval_goalkeeper` ensambla un archivo `.asm` y lo ejecuta en la CPU real:

```bash
./build/eval_goalkeeper examples/suma.asm
```

Opciones:

```bash
./build/eval_goalkeeper examples/bucle.asm --max-ciclos 1000 --mem 16
```

- `--max-ciclos N`: límite de ciclos (detecta bucles infinitos).
- `--mem N`: cuántas posiciones de memoria de datos mostrar.

Escribe tu propio programa (máximo 256 instrucciones):

```asm
; mi_suma.asm
LDI R1, 7
LDI R2, 8
ADD R3, R1, R2
HALT
```

```bash
./build/eval_goalkeeper mi_suma.asm
```

Resultado esperado: `Estado final: HALT` y `R3 = 15`.

Guías completas: `docs/GUIA_EJECUCION_PROGRAMAS.md` y `docs/evaluacion.md`.

---

## Cómo ejecutar las pruebas

```bash
cd build
ctest --output-on-failure
```

Resultado esperado: `100% tests passed` (21 pruebas). Incluyen módulos,
pipeline, forwarding, hazards, branches, comunicación, E/S, estrategia, Fase 6,
ensamblador, núcleo adicional y aceptación.

---

## Cómo se comunican (protocolo REQ/ACK)

Mediante `CommunicationMailbox`:

- STRIKER escribe el disparo en `0x10–0x14` y activa `REQ` en `0x15`.
- GOALKEEPER lee, ejecuta y escribe la respuesta en `0x20–0x25` y `ACK` en `0x16`.
- STRIKER lee la respuesta, baja `REQ` y espera `ACK=0`.
- El tiempo de reacción (`reaction_ticks`) lo mide el Mailbox.

Contrato completo para conectar el STRIKER-8 real:
`docs/contrato_integracion.md` y `docs/guia_integracion_striker.md`.

---

## Limitaciones conocidas

1. **No hay STRIKER-8 real** en el repositorio; se usa `StrikerSim` (simulador
   de protocolo). El mapa de E/S debe confirmarse con Rafael.
2. **`IN` sin dispositivo externo:** el evaluador fija el bus de entrada a 0,
   por lo que `IN` devuelve 0. `OUT` sí escribe de verdad en el bus (se observa).
   Para entradas reales hay que conectar el `CommunicationMailbox`.
3. La estrategia del portero está definida para `target_x/y ∈ [32,223]`; no
   incluye clamping para todo 0–255.
4. No se implementa la bandera `V` (overflow).

---

## Documentación

- `docs/arquitectura_goalkeeper.md` — arquitectura y pipeline.
- `docs/protocolo_comunicacion.md` — mapa de E/S y REQ/ACK.
- `docs/contrato_integracion.md` — contrato STRIKER↔GOALKEEPER.
- `docs/guia_integracion_striker.md` — cómo conectar STRIKER-8.
- `docs/GUIA_DEMO_PENALTIS.md` — guía de la demostración de penaltis.
- `docs/GUIA_EJECUCION_PROGRAMAS.md` — guía del evaluador de programas.
- `docs/evaluacion.md` — referencia del evaluador.
- `inventario_tecnico.md` — ficha técnica completa.
- `docs/*.md` — documentación por módulo (ALU, PC, banco, memorias, UC, CPU,
  Mailbox, árbitro, ISA, programa, integración, STRIKER).

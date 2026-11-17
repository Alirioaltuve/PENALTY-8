# PENALTY-8 — GOALKEEPER-8

Proyecto universitario de Arquitectura del Computador (C++ + SystemC).

4. Pipeline totalmente implementado y funcional.
4. Pipeline totalmente implementado y funcional.
PENALTY-8 está formado por dos computadores RISC de 8 bits:
4. Pipeline totalmente implementado y funcional.
5. No se implementa la bandera `V`.
6. La interfaz gráfica (SFML) no forma parte de esta etapa.

- **STRIKER-8**: calcula y ejecuta los disparos. *Desarrollado por Rafael.*
- **GOALKEEPER-8**: analiza el disparo, decide su reacción y responde.

Este repositorio contiene **GOALKEEPER-8 completo**, la **comunicación**
(`CommunicationMailbox`) y el **árbitro**, además de un **simulador del lado
STRIKER** (`StrikerSim`) para validar el protocolo mientras el STRIKER real no
está disponible.

> **Aviso importante:** no existe código del STRIKER-8 de Rafael en este
> repositorio. La integración documentada aquí es con un simulador de protocolo,
> no con el STRIKER real. Ver `docs/cpu_striker.md`.

---

## Arquitectura

| Característica      | Especificación              |
|---------------------|-----------------------------|
| Arquitectura        | RISC                        |
| Ancho de datos      | 8 bits                      |
| Registros           | 8 (R0–R7)                   |
| Ancho de PC         | 8 bits                      |
| Ancho de instrucción| 16 bits                     |
| Memoria de instrucciones | 256 palabras de 16 bits |
| Memoria de datos    | 256 bytes                   |
| ALU                 | 8 bits                      |
| Reloj               | 10 ns / 100 MHz             |
| Ejecución           | Pipeline RISC de 5 etapas     |
| Pipeline            | Pipeline RISC de 5 etapas     |
| Reset               | Síncrono, activo en alto    |

ISA y formatos: ver `docs/isa.md`.

---

## Estructura de carpetas

```
.
├── isa.h                      Definición de ISA y helpers
├── banco_registros.h          Registro de 8 x 8 bits
├── alu.h                      Unidad aritmético-lógica
├── pc.h                       Contador de programa
├── memoria_instrucciones.h    Memoria de programa (256 x 16)
├── memoria_datos.h            Memoria de datos (256 x 8)
├── unidad_control.h           Decodificador combinacional
├── registro_banderas.h        Registro de banderas C/Z
├── cpu_goalkeeper.h           CPU completa
├── communication_mailbox.h    Canal REQ/ACK + temporizador
├── arbitro.h                  Reglas MISS/GOAL/SAVE
├── goalkeeper_program.h       Programa del portero
├── striker_sim.h              Simulador del lado STRIKER (protocolo)
├── testbench*.cpp             Pruebas (cada una con sc_main)
├── test_*.cpp                 Pruebas unitarias/de integración
├── CMakeLists.txt
└── docs/                      Documentación por módulo
```

---

## Cómo compilar

Requisitos: `cmake`, `g++`, `SystemC` (`pkg-config --exists systemc`).

```bash
mkdir -p build
cd build
cmake ..
make -j4
```

## Cómo ejecutar las pruebas

```bash
cd build
ctest --output-on-failure
```

Para ejecutar una prueba concreta:

```bash
./build/testbench_integracion
./build/test_cpu_goalkeeper
```

También se puede compilar un archivo suelto:

```bash
g++ -std=c++17 -I/usr/include testbench_integracion.cpp -lsystemc -o sim
./sim
```

---

## Cómo funciona GOALKEEPER-8

1. El **PC** entrega la dirección.
2. La **memoria de instrucciones** entrega la instrucción de 16 bits.
3. La **Unidad de Control** la decodifica y genera las señales.
4. El **banco de registros** entrega los operandos.
5. La **ALU** calcula y produce banderas.
6. El **registro de banderas** las guarda solo cuando corresponde.
7. La **memoria de datos** o la **E/S** se actualizan si la instrucción lo pide.
8. El **PC** avanza o salta.

El **programa del portero** (`goalkeeper_program.h`) implementa la lógica:
espera REQ, lee el disparo, calcula la respuesta (dive/reach), la escribe y
activa ACK.

## Cómo se comunican

Mediante `CommunicationMailbox`, con un protocolo REQ/ACK:

- STRIKER escribe el disparo (0x10–0x14) y activa REQ (0x15).
- GOALKEEPER lee, ejecuta y escribe la respuesta (0x20–0x25) y ACK (0x16).
- STRIKER lee la respuesta y baja REQ.
- El tiempo de reacción se mide en el mailbox.

Detalle completo en `docs/protocolo_comunicacion.md`.

---

## Estado actual

| Elemento                          | Estado                          |
|-----------------------------------|---------------------------------|
| Banco de registros                | Implementado, compilado, probado |
| ALU                               | Implementado, compilado, probado |
| PC                                | Implementado, compilado, probado |
| Memoria de instrucciones          | Implementado, compilado, probado |
| Memoria de datos                  | Implementado, compilado, probado |
| Unidad de Control                 | Implementado, compilado, probado |
| Registro de banderas              | Implementado, compilado, probado |
| CPU GOALKEEPER                    | Implementado, compilado, probado |
| Programa del portero              | Implementado, compilado, probado |
| CommunicationMailbox              | Implementado, compilado, probado (con StrikerSim) |
| Árbitro                           | Implementado, compilado, probado |
| Integración GOALKEEPER+Mailbox    | Probada con StrikerSim          |
| Integración con STRIKER-8 real    | **Pendiente** (no hay código)   |
| Interfaz gráfica                  | **No iniciada** (fuera de alcance) |

---

## Limitaciones conocidas

1. **No hay STRIKER-8 real** en el repositorio: se usa `StrikerSim`. El mapa de
   E/S y el protocolo deben confirmarse con Rafael.
2. El portero usa una estrategia simple: `dive = target`, `reach = 64`. No
   incluye clamping ni estrategia avanzada.
3. El árbitro trata `power = 0` como tiro inválido (MISS), decisión documentada.
5. No se implementa la bandera `V`.

## Documentación por módulo

Ver la carpeta `docs/`:
`banco_registros.md`, `alu.md`, `pc.md`, `memoria_instrucciones.md`,
`memoria_datos.md`, `unidad_control.md`, `registro_banderas.md`,
`cpu_goalkeeper.md`, `communication_mailbox.md`, `arbitro.md`, `integracion.md`,
`programa_goalkeeper.md`, `protocolo_comunicacion.md`, `isa.md`,
`cpu_striker.md`.

## Arquitectura pipeline

GOALKEEPER-8 implementa un pipeline RISC de cinco etapas que permite la superposición de instrucciones:

- **ETAPA IF (Instruction Fetch)**: Obtiene la dirección desde el PC, lee la instrucción en memoria y calcula la dirección secuencial. El pipeline IF/ID captura la instrucción y el PC.

- **ETAPA ID (Instruction Decode)**: Decodifica la instrucción mediante la Unidad de Control. Lee los registros del banco de registros y genera las señales de control necesarias. El pipeline ID/EX captura los operandos, el opcode, el inmediato y las señales de control.

- **ETAPA EX (Execute)**: Ejecuta operaciones aritméticas y lógicas mediante la ALU. Resuelve las condiciones de salto y detecta hazards de datos. El pipeline EX/MEM captura el resultado de ALU, el registro destino y las banderas.

- **ETAPA MEM (Memory Access)**: Accede a la memoria de datos para instrucciones LD y ST. Para las instrucciones que no acceden a memoria, pasa través sin efectos secundarios. El pipeline MEM/WB captura el dato de writeback.

- **ETAPA WB (Write Back)**: Escribe resultados en el banco de registros cuando corresponde y actualiza las banderas C y Z. El pipeline MEM/WB decide si la escritura se realiza según el selector WB.

### Registros intermedios del pipeline

1. **IF/ID**: instrucción (16 bits), PC (8 bits), dirección secuencial siguiente (8 bits), válida.
2. **ID/EX**: operandos A y B (8 bits cada uno), identificador de registro destino (3 bits), opcode (5 bits), inmediato (8 bits), opcode ALU (3 bits), B inmediato (1 bit), selector writeback (3 bits), escritura en memoria (1 bit), dirección memoria (8 bits), escritura E/S (2 bits), dirección E/S (8 bits), actualizar banderas (1 bit), salto (1 bit), dirección salto (8 bits), PC siguiente (8 bits), banderas C/Z, válida.
3. **EX/MEM**: resultado ALU (8 bits), identificador de registro destino (3 bits), escritura en memoria (1 bit), dirección memoria (8 bits), datos para escritura (8 bits), banderas C/Z, válida.
4. **MEM/WB**: selector writeback (3 bits), dato de writeback (8 bits), identificador de registro destino (3 bits), banderas C/Z, válida.

### Forwarding y hazard detection

El pipeline implementa forwarding desde EX/MEM y MEM/WB hacia la etapa EX para manejar dependencias de datos. También detecta hazards de load-use que requieren detener el pipeline temporalmente yinsertar burbujas. Los saltos y saltos condicionales provocan flush de las instrucciones incorrectas en el pipeline.

Pruebas integradas: el pipeline ejecuta correctamente 4 disparos de penalti con resultados SAVE, GOAL (tardío), MISS y SAVE, con tiempos de reacción de 25-27 ciclos.

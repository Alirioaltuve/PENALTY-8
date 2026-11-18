# Guía de ejecución de la demostración de penaltis

Esta guía explica cómo compilar y usar la demostración interactiva de
GOALKEEPER-8 (`demo_goalkeeper`), que simula un penalti completo: el STRIKER
envía un disparo, GOALKEEPER ejecuta su programa RISC y el Árbitro decide el
resultado.

## Qué es la demostración

`demo_goalkeeper` instancia los **componentes reales** del proyecto:

- `CPU_Goalkeeper` (ejecuta el programa RISC del portero).
- `CommunicationMailbox` (protocolo REQ/ACK).
- `Arbitro` (decide ATAJADA / GOL / DISPARO AFUERA).

El lado STRIKER se **emula** mediante un generador de solicitudes que usa solo
las señales externas del Mailbox (`st_we/st_addr/st_wdata/st_re/st_rdata`). No
escribe en la CPU.

## Compilar

```bash
mkdir -p build
cd build
cmake ..
make demo_goalkeeper
```

## Ejecutar

```bash
./demo_goalkeeper
```

Para texto plano sin colores ANSI:

```bash
NO_COLOR=1 ./demo_goalkeeper
```

## Menú

1. **Simular un disparo de prueba:** ejecuta un disparo fijo y muestra todas las
   etapas (recepción, procesamiento, comunicación, respuesta y resultado).
2. **Simular un disparo personalizado:** pide los cinco parámetros
   (`shot_id`, `target_x`, `target_y`, `power`, `flight_ticks`), cada uno 0–255.
3. **Ejecutar secuencia automática:** cuatro disparos predefinidos (atajada,
   gol, fuera del área y límite reacción/vuelo).
4. **Mostrar estadísticas de la sesión:** total, atajadas, goles, afuera y
   porcentaje de atajadas.
5. **Mostrar información técnica:** resumen de la arquitectura.
6. **Salir.**

## Qué se muestra en cada disparo

- **Recepción:** los cinco campos del disparo (protocolo externo).
- **Procesamiento:** el pipeline real (IF/ID/EX/MEM/WB) con los ciclos de IN/OUT.
- **Comunicación:** lecturas `IN` y escrituras `OUT` reales del bus.
- **Registros:** el banco de registros tras ejecutar el programa.
- **Respuesta:** `shot_id`, `dive_x`, `dive_y`, `reaction_ticks`, `reach_x`,
  `reach_y`.
- **Resultado:** ATAJADA, GOL o DISPARO AFUERA (del Árbitro real).

## Resultados de la secuencia automática

| Disparo | Resultado esperado |
|---|---|
| Atajada clara | ATAJADA |
| Gol (portero no alcanza) | GOL |
| Disparo fuera del área | DISPARO AFUERA |
| Límite reacción vs vuelo | GOL |

## Limitaciones

- STRIKER se emula; no hay integración con el STRIKER-8 real.
- La estrategia del portero está definida para `target_x/y ∈ [32,223]`.

# GOALKEEPER-8

Computador RISC de 8 bits con pipeline de cinco etapas (IF-ID-EX-MEM-WB),
diseñado como el portero de PENALTY-8. Este repositorio contiene la
**implementación individual de GOALKEEPER-8** y su **interfaz de integración**
con STRIKER-8.

> **Importante:** STRIKER-8 (el disparador) NO está incluido aquí. La
> integración entre ambos computadores se realiza mediante el
> `CommunicationMailbox` y el protocolo REQ/ACK descritos en `docs/`.
>
> Esta es una versión independiente (núcleo + demostración), sin los
> testbenches de desarrollo; las pruebas completas residen en el repositorio de
> desarrollo.

## Requisitos

- Sistema operativo Linux.
- Compilador C++17.
- CMake >= 3.16.
- SystemC 2.3.x instalado y visible por `pkg-config` (`systemc.pc`).

## Compilar

```bash
mkdir build && cd build
cmake ..
make
```

## Ejecutar la demostración

```bash
./demo_goalkeeper
```

La demostración ejecuta la **CPU real**, el **CommunicationMailbox real** y el
**Árbitro real**; el lado STRIKER se emula mediante un generador de solicitudes
que solo usa las señales externas del Mailbox (no accede a la CPU).

Menú:

1. Simular un disparo de prueba.
2. Simular un disparo personalizado (5 parámetros de 8 bits).
3. Ejecutar una secuencia automática (atajada, gol, fuera, límite reacción).
4. Mostrar estadísticas de la sesión.
5. Mostrar información técnica.
6. Salir.

Para texto plano sin colores: `NO_COLOR=1 ./demo_goalkeeper`.

## Estructura

- `demo_goalkeeper.cpp` — demostración interactiva por terminal.
- `cpu_goalkeeper.h` — CPU RISC pipeline (IF-ID-EX-MEM-WB, forwarding, stalls, branches, HALT).
- `pc.h`, `memoria_instrucciones.h`, `unidad_control.h`, `banco_registros.h`,
  `alu.h`, `registro_banderas.h`, `memoria_datos.h` — unidades de la CPU.
- `isa.h` — ISA compartida de 16 bits (20 instrucciones, formatos R/I/J).
- `communication_mailbox.h` — canal REQ/ACK entre STRIKER y GOALKEEPER.
- `goalkeeper_program.h` — programa RISC del portero.
- `arbitro.h` — decide ATAJADA / GOL / DISPARO AFUERA.
- `striker_sim.h` — emulador de STRIKER a nivel de protocolo (referencia).

## Documentación para integración

- `docs/arquitectura_goalkeeper.md` — arquitectura y funcionamiento de la CPU.
- `docs/protocolo_comunicacion.md` — mapa de E/S, REQ/ACK, formatos.
- `docs/guia_integracion_striker.md` — cómo conectar STRIKER-8.
- `docs/contrato_integracion.md` — contrato compartido entre ambos computadores.
- `inventario_tecnico.md` — ficha técnica completa de referencia.

## Contrato de comunicación (resumen)

Solicitud (STRIKER → GOALKEEPER): `0x10` shot_id, `0x11` target_x, `0x12`
target_y, `0x13` power, `0x14` flight_ticks, `0x15` REQ.

Respuesta (GOALKEEPER → STRIKER): `0x16` ACK, `0x20` shot_id, `0x21` dive_x,
`0x22` dive_y, `0x23` reaction_ticks, `0x24` reach_x, `0x25` reach_y.

Ver `docs/contrato_integracion.md` para el detalle completo.

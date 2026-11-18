# Evaluador de programas — GOALKEEPER-8

Herramienta para ensamblar y ejecutar programas independientes escritos en la
ISA de 20 instrucciones de GOALKEEPER-8, sobre el computador real (sin
simular la CPU).

## Uso

```bash
cd build
make eval_goalkeeper
./eval_goalkeeper ../examples/suma.asm
```

Opciones:

- `--max-ciclos N` — límite de ciclos (por defecto 100000).
- `--mem N` — cuántas posiciones de memoria de datos mostrar (por defecto 32).

```bash
./eval_goalkeeper ../examples/bucle.asm --max-ciclos 1000 --mem 16
```

## Sintaxis admitida

Una instrucción por línea. Comentarios con `;` o `#`. Etiquetas con `:`.

| Instrucción | Operandos | Descripción |
|---|---|---|
| `NOP` | — | No hace nada |
| `LDI Rd, imm` | registro, inmediato 0–255 | Rd = imm |
| `MOV Rd, Ra` | registro, registro | Rd = Ra |
| `ADD Rd, Ra, Rb` | 3 registros | Rd = Ra + Rb |
| `SUB Rd, Ra, Rb` | 3 registros | Rd = Ra − Rb |
| `AND Rd, Ra, Rb` | 3 registros | Rd = Ra & Rb |
| `OR Rd, Ra, Rb` | 3 registros | Rd = Ra \| Rb |
| `CMP Ra, Rb` | 2 registros | fija C y Z (Ra − Rb) |
| `JMP etiqueta` | etiqueta/dirección | salto incondicional |
| `JEQ/JNE/JGE/JLT/JGT/JLE etiqueta` | etiqueta/dirección | salto condicional |
| `LD Rd, addr` | registro, dirección 0–255 | Rd = mem[addr] |
| `ST Rs, addr` | registro, dirección 0–255 | mem[addr] = Rs |
| `IN Rd, addr` | registro, dirección 0–255 | Rd = puerto[addr] |
| `OUT Rs, addr` | registro, dirección 0–255 | puerto[addr] = Rs |
| `HALT` | — | detiene el computador |

Notas:

- `Rs` (fuente) en `ST` y `OUT` se codifica en el campo `rd` (bits 10:8), por la
  definición de la ISA.
- Los inmediatos y direcciones se escriben en decimal o hexadecimal. El prefijo
  `0x` (o `0X`) indica hexadecimal; también se acepta el prefijo `$` (por
  ejemplo `$FF` equivale a `0xFF`).
- Las direcciones de salto son **absolutas** (formato J de 8 bits).

## Etiquetas

```asm
LDI R0, 5
LDI R1, 0
bucle:
ADD R1, R1, R7
SUB R0, R0, R7
JNE bucle
HALT
```

Las etiquetas se resuelven en dos pasadas a la dirección (índice) de la
instrucción. Las etiquetas duplicadas o inexistentes producen error.

## Errores

El ensamblador valida y rechaza (indicando número de línea y motivo):

- Instrucción desconocida.
- Cantidad de operandos incorrecta.
- Registro inválido (no R0–R7).
- Inmediato o dirección fuera de 0–255.
- Etiqueta duplicada o inexistente.
- Programa con más de 256 instrucciones (no se trunca).

## Resultados

Al terminar, `eval_goalkeeper` muestra:

- Palabras de 16 bits ensambladas.
- Ciclos ejecutados.
- Estado final (`HALT` o `LIMITE DE CICLOS`).
- Registros R0–R7.
- Posiciones de memoria de datos no nulas.
- Contadores de stalls, branches tomados y flushes.
- Eventos `OUT` del bus de E/S real.

## Entrada y salida (IN/OUT)

- **`OUT Rs, addr`** genera una escritura real en el bus de salida de la CPU.
  El evaluador la observa y la muestra como `OUT 0xNN = valor`. Esta escritura
  proviene de la CPU real, no de un cálculo simulado.
- **`IN Rd, addr`** lee del bus de entrada. En el evaluador actual **no hay
  ningún dispositivo de entrada conectado**, por lo que el bus de entrada está
  fijado a 0 y `IN` devuelve **siempre 0**. Es un comportamiento real (no
  simulado), pero limita los programas que dependen de entradas externas.

Para permitir entradas reales en una futura integración, basta con conectar el
`CommunicationMailbox` a los puertos `io_*` de la CPU y cargar datos en las
direcciones de solicitud `0x10..0x14` (como hacen `test_cpu_io.cpp` y
`test_communication.cpp`). Entonces `IN Rd, 0x11` leería, por ejemplo, el valor
de `target_x` provisto por un dispositivo.

## Límite de ciclos (`--max-ciclos`)

`--max-ciclos N` limita el número de ciclos de reloj. Si el programa llega a
`HALT` antes del límite, se informa `Estado final: HALT`. Si se alcanza el
límite sin llegar a `HALT` (por ejemplo, un bucle infinito), se informa
`Estado final: LIMITE DE CICLOS`.

En ambos casos el evaluador termina con **código de salida 0**: el límite de
ciclos es un resultado informado, no un error del ensamblador. Los errores
reales (archivo inexistente o error de ensamblado) sí terminan con código 1.

## Límites de la ISA

- Registros: R0–R7 (8 bits).
- Inmediatos y direcciones: 0–255.
- Máximo 256 instrucciones por programa.
- No hay flag de overflow (`V`).
- `MOV`, `ST`, `OUT` no modifican banderas; `ADD/SUB/AND/OR/CMP` sí.

## Programas de ejemplo

En `examples/`: `suma.asm`, `logica.asm`, `mov.asm`, `cmp.asm`, `saltos.asm`,
`memoria.asm`, `forwarding.asm`, `hazard.asm`, `bucle.asm`, `flush.asm`,
`halt.asm`, `io.asm`, `combinado.asm`.

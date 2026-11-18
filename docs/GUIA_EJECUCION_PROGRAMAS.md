# Guía práctica: ejecutar programas propios en GOALKEEPER-8

Esta guía explica, paso a paso, cómo compilar GOALKEEPER-8 y ejecutar programas
escritos en ensamblador, **sin necesidad de conocer C++ ni SystemC**.

El programa se escribe en un archivo `.asm`, se traduce a las instrucciones de
16 bits del computador (ensamblado) y se ejecuta en la CPU RISC real.

---

## A. Requisitos

- **Sistema operativo:** Linux.
- **Compilador:** `g++` con soporte de C++17.
- **CMake** (versión 3.16 o superior).
- **SystemC** 2.3.x instalado y visible por `pkg-config`.

Comprueba que CMake encuentra SystemC:

```bash
pkg-config --modversion systemc
```

Si imprime un número de versión (por ejemplo `2.3.3`), todo está listo. Si no
imprime nada o da error, instala SystemC y asegúrate de que el archivo
`systemc.pc` esté en el `PKG_CONFIG_PATH`.

Compilar el proyecto desde cero:

```bash
cd <directorio-del-proyecto>
mkdir -p build
cd build
cmake ..
make
```

---

## B. Compilar el evaluador

Desde el directorio del proyecto:

```bash
# 1. Entrar al directorio del proyecto
cd GOALKEEPER-8            # (ajusta la ruta a tu carpeta)

# 2. Crear el directorio de compilación
mkdir -p build
cd build

# 3. Configurar con CMake
cmake ..

# 4. Compilar el evaluador
make eval_goalkeeper
```

El ejecutable queda en `build/eval_goalkeeper`.

Para ejecutar todas las pruebas (verificación de regresión):

```bash
ctest
```

El resultado esperado es `100% tests passed` (21 pruebas).

---

## C. Ejecutar un programa existente

En la carpeta `examples/` hay 13 programas de demostración ya validados:

`suma.asm`, `logica.asm`, `mov.asm`, `cmp.asm`, `saltos.asm`, `memoria.asm`,
`forwarding.asm`, `hazard.asm`, `bucle.asm`, `flush.asm`, `halt.asm`, `io.asm`,
`combinado.asm`.

Desde **dentro de `build/`**, ejecútalos así:

```bash
./eval_goalkeeper ../examples/suma.asm
./eval_goalkeeper ../examples/logica.asm
./eval_goalkeeper ../examples/mov.asm
./eval_goalkeeper ../examples/cmp.asm
./eval_goalkeeper ../examples/saltos.asm
./eval_goalkeeper ../examples/memoria.asm
./eval_goalkeeper ../examples/forwarding.asm
./eval_goalkeeper ../examples/hazard.asm
./eval_goalkeeper ../examples/bucle.asm
./eval_goalkeeper ../examples/flush.asm
./eval_goalkeeper ../examples/halt.asm
./eval_goalkeeper ../examples/io.asm
./eval_goalkeeper ../examples/combinado.asm
```

Desde **la raíz del proyecto**, usa `./build/eval_goalkeeper`:

```bash
./build/eval_goalkeeper examples/suma.asm
```

**Qué aparece en pantalla** (y cómo interpretarlo):

- `Instrucciones:` cuántas instrucciones tiene el programa.
- `Palabras de 16 bits:` la codificación binaria de cada instrucción.
- `Ciclos ejecutados:` cuántos ciclos de reloj usó la CPU.
- `Estado final:` `HALT` (terminó correctamente) o `LIMITE DE CICLOS`.
- `Registros:` el valor final de `R0` a `R7`.
- `Memoria de datos:` posiciones de memoria que quedaron con valor distinto de 0.
- `Contadores del pipeline:` stalls, branches tomados y flushes.
- `Eventos OUT:` escrituras reales al bus de salida.

Para `suma.asm`, el resultado esperado es `R3 = 30` y `R4 = 10`.

---

## D. Crear un programa nuevo

1. Crea un archivo de texto, por ejemplo `mi_suma.asm`, con este contenido:

```asm
; Carga dos valores y los suma
LDI R1, 7
LDI R2, 8
ADD R3, R1, R2
HALT
```

2. Ejecútalo (desde `build/`):

```bash
./eval_goalkeeper ../mi_suma.asm
```

**Resultado esperado:** en la sección `Registros`, `R3 = 15` (7 + 8) y
`Estado final: HALT`.

Cómo comprobarlo: busca la línea `R3 = 15` en la salida.

---

## E. Sintaxis del ensamblador

Una instrucción por línea. Los comentarios empiezan con `;` o `#`. Las etiquetas
terminan en `:`.

### Registros y valores

- **Registros:** `R0` a `R7` (8 bits cada uno).
- **Números:** decimal (por ejemplo `10`) o hexadecimal (`0x0A` o `$0A`).
- **Rango:** los inmediatos y direcciones van de 0 a 255.

### Instrucciones (las 20 de la ISA)

**Transferencia de datos:**

| Instrucción | Efecto |
|---|---|
| `LDI Rd, valor` | Rd = valor inmediato |
| `MOV Rd, Ra` | Rd = Ra (copia) |

**Aritméticas:**

| Instrucción | Efecto |
|---|---|
| `ADD Rd, Ra, Rb` | Rd = Ra + Rb |
| `SUB Rd, Ra, Rb` | Rd = Ra − Rb |

**Lógicas:**

| Instrucción | Efecto |
|---|---|
| `AND Rd, Ra, Rb` | Rd = Ra AND Rb (bit a bit) |
| `OR Rd, Ra, Rb` | Rd = Ra OR Rb (bit a bit) |

**Comparación:**

| Instrucción | Efecto |
|---|---|
| `CMP Ra, Rb` | fija las banderas C y Z según Ra − Rb (no escribe registros) |

**Saltos (dirección absoluta 0–255 o etiqueta):**

| Instrucción | Condición |
|---|---|
| `JMP dir` | incondicional |
| `JEQ dir` | si Z = 1 (igual) |
| `JNE dir` | si Z = 0 (distinto) |
| `JGE dir` | si C = 1 (Ra ≥ Rb) |
| `JLT dir` | si C = 0 (Ra < Rb) |
| `JGT dir` | si C = 1 y Z = 0 (mayor) |
| `JLE dir` | si C = 0 o Z = 1 (menor o igual) |

**Memoria:**

| Instrucción | Efecto |
|---|---|
| `LD Rd, dir` | Rd = memoria[dir] |
| `ST Rs, dir` | memoria[dir] = Rs (Rs es el registro fuente) |

**Entrada / salida:**

| Instrucción | Efecto |
|---|---|
| `IN Rd, dir` | Rd = puerto[dir] |
| `OUT Rs, dir` | puerto[dir] = Rs (Rs es el registro fuente) |

**Control:**

| Instrucción | Efecto |
|---|---|
| `NOP` | no hace nada |
| `HALT` | detiene el computador |

### Reglas importantes

- **Etiquetas:** nombre seguido de `:` al inicio de línea; se resuelven a la
  dirección (índice) de la instrucción. Se pueden usar en los saltos.

```asm
LDI R0, 3
bucle:
SUB R0, R0, R7
JNE bucle
HALT
```

- **Direcciones absolutas:** los saltos usan la dirección exacta de la
  instrucción (0 = primera instrucción).
- **Máximo 256 instrucciones** por programa; si se supera, el ensamblador lo
  rechaza con error.
- **IN/OUT:** `OUT` escribe de verdad en el bus de salida (se muestra). `IN`
  devuelve 0 porque el evaluador no tiene ningún dispositivo de entrada
  conectado (ver sección F de `docs/evaluacion.md`).

---

## F. Ejecutar con límite de ciclos

Para detectar bucles infinitos, usa `--max-ciclos`:

```bash
./eval_goalkeeper ../examples/bucle.asm --max-ciclos 1000
```

Si el programa termina antes del límite, verás `Estado final: HALT`.

Un programa con bucle infinito:

```asm
inicio:
JMP inicio
```

```bash
./eval_goalkeeper ../mi_bucle.asm --max-ciclos 20
```

Verás `Estado final: LIMITE DE CICLOS` y `Ciclos ejecutados: 20`. Esto indica
que el programa no llegó a `HALT`. El evaluador termina con código 0 en ambos
casos (el límite es un resultado, no un error).

También puedes limitar cuánta memoria mostrar con `--mem N` (por defecto 32):

```bash
./eval_goalkeeper ../examples/memoria.asm --mem 16
```

---

## G. Errores frecuentes

| Error en el archivo | Mensaje del evaluador | Cómo corregirlo |
|---|---|---|
| `LDI R8, 1` | `linea N: registro invalido 'R8'` | Usa un registro R0–R7 |
| `FOO R0, R1` | `linea N: instruccion desconocida 'FOO'` | Revisa el nombre de la instrucción |
| `JMP noexiste` | `linea N: destino invalido ...` | Define la etiqueta `noexiste:` |
| `LDI R0, 300` | `linea N: valor/direccion invalido ... (0..255)` | Usa un valor entre 0 y 255 |
| 257 instrucciones | `linea N: el programa supera 256 instrucciones` | Reduce el programa |
| Archivo inexistente | `No se pudo abrir el archivo: ...` | Revisa la ruta del archivo |

Los errores de ensamblado y de archivo terminan con código de salida 1.

---

## H. Tabla rápida de comandos

```bash
# Compilar (desde la raíz del proyecto)
mkdir -p build && cd build && cmake .. && make

# Ejecutar las pruebas
ctest

# Ejecutar un programa de ejemplo
./eval_goalkeeper ../examples/suma.asm

# Ejecutar con límite de ciclos y volcado de memoria
./eval_goalkeeper ../examples/bucle.asm --max-ciclos 1000 --mem 16
```

Para crear y ejecutar tu propio programa:

```bash
# 1. Crea mi_programa.asm (ver sección D)
# 2. Ejecútalo
./eval_goalkeeper ../mi_programa.asm
```

---

## Programas de ejemplo disponibles

Los 13 archivos de `examples/` son: `suma.asm`, `logica.asm`, `mov.asm`,
`cmp.asm`, `saltos.asm`, `memoria.asm`, `forwarding.asm`, `hazard.asm`,
`bucle.asm`, `flush.asm`, `halt.asm`, `io.asm` y `combinado.asm`.

Cada uno tiene un comentario al inicio indicando el resultado esperado.

# Arquitectura de GOALKEEPER-8

## 1. Resumen

GOALKEEPER-8 es un computador RISC de **8 bits** con un **pipeline de cinco
etapas** (IF-ID-EX-MEM-WB). Su función es recibir un disparo, ejecutar su
programa y responder con una decisión de movimiento (`dive` y `reach`).

| Característica | Valor |
|---|---|
| Ancho de datos | 8 bits |
| Ancho del PC | 8 bits (256 instrucciones direccionables) |
| Ancho de instrucción | 16 bits (opcode de 5 bits) |
| Registros | 8 × 8 bits (R0–R7) |
| Memoria de instrucciones | 256 × 16 bits |
| Memoria de datos | 256 × 8 bits |
| ALU | ADD, SUB, AND, OR, CMP |
| Banderas | C (carry) y Z (zero); sin V |
| ISA | 20 instrucciones, formatos R/I/J |

## 2. Componentes

- **PC** (`pc.h`): contador de programa. Avanza, salta a `direccion_salto` o se
  congela por `stall`. Reset a 0.
- **MemoriaInstrucciones** (`memoria_instrucciones.h`): ROM 256×16 de lectura
  combinacional. Se carga con `cargar_programa()`.
- **UnidadControl** (`unidad_control.h`): decodifica la instrucción y genera
  todas las señales de control.
- **BancoRegistros** (`banco_registros.h`): 8 registros de 8 bits; dos lecturas
  y una escritura; reset síncrono.
- **ALU** (`alu.h`): ADD, SUB, AND, OR, CMP; produce `carry` y `zero`.
- **RegistroBanderas** (`registro_banderas.h`): almacena C y Z solo cuando la
  instrucción debe actualizarlas.
- **MemoriaDatos** (`memoria_datos.h`): RAM 256×8; escritura en flanco; reset
  síncrono.
- **CPU_Goalkeeper** (`cpu_goalkeeper.h`): contenedor que conecta las siete
  unidades anteriores y el pipeline de cinco etapas.
- **CommunicationMailbox** (`communication_mailbox.h`): canal entre STRIKER y
  GOALKEEPER (ver `protocolo_comunicacion.md`).
- **Arbitro** (`arbitro.h`): decide el resultado (MISS/GOAL/SAVE).

## 3. Pipeline

Etapas:

1. **IF (Instruction Fetch):** PC entrega la dirección; se captura la
   instrucción y el PC en el registro IF/ID.
2. **ID (Instruction Decode):** la Unidad de Control decodifica; el banco de
   registros lee los operandos; se evalúa el salto.
3. **EX (Execute):** la ALU opera sobre los operandos ya resueltos por
   forwarding y produce resultado y banderas.
4. **MEM (Memory):** la memoria de datos lee (`LD`) o escribe (`ST`); `IN`/`OUT`
   interactúan con E/S.
5. **WB (Write Back):** un multiplexor elige el dato (ALU, inmediato, memoria,
   E/S o copia de registro) y lo escribe en el banco; actualiza banderas.

Registros intermedios: `ifid_*`, `idex_*`, `exmem_*`, `memwb_*` (señales
`sc_signal` actualizadas en `clk.pos()`).

### 3.1 Forwarding

- De `EX/MEM` y `MEM/WB` hacia `EX` para los operandos de la ALU.
- Bypass `WB→ID` para dependencias a distancia 3.
- Forwarding de banderas (C/Z) desde EX, EX/MEM y MEM/WB hacia ID.

### 3.2 Hazards y stalls

- **Load-use:** si un `LD` en EX alimenta a una instrucción en ID que usa el
  mismo registro, se inserta una burbuja y se congelan IF/ID y el PC.

### 3.3 Branches, flush y HALT

- Los saltos se resuelven en ID. El PC salta a `ctrl_dir_salto`; al ciclo
  siguiente se invalida IF/ID (flush).
- `HALT` detiene el fetch; cuando llega a WB, la señal `halted` se vuelve
  verdadera y queda fija hasta reset.

### 3.4 Reset

Con `reset=1` en `clk.pos()`: PC→0, registros→0, banderas→0, memoria de
datos→0 y el pipeline queda invalidado.

## 4. Programa del portero

`goalkeeper_program.h` construye un programa de 50 instrucciones que:

1. Espera `REQ`.
2. Lee los cinco campos (`0x10..0x14`).
3. Calcula `dive_x`, `dive_y` y `reach` (ver `contrato_integracion.md`).
4. Escribe la respuesta (`0x20..0x25`) y activa ACK (`0x16`).
5. Espera `REQ=0` y vuelve al inicio.

## 5. Documentación relacionada

- `protocolo_comunicacion.md` — mapa de E/S y protocolo REQ/ACK.
- `guia_integracion_striker.md` — cómo conectar STRIKER-8.
- `contrato_integracion.md` — contrato compartido.
- `inventario_tecnico.md` — referencia técnica completa.

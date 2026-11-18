# ISA de GOALKEEPER-8

Instrucciones de 16 bits. Opcode en `[15:11]` (5 bits).

## Tabla de opcodes

| Opcode | Instrucción |
|--------|-------------|
| 00000  | NOP         |
| 00001  | LDI         |
| 00010  | MOV         |
| 00011  | ADD         |
| 00100  | SUB         |
| 00101  | AND         |
| 00110  | OR          |
| 00111  | CMP         |
| 01000  | JMP         |
| 01001  | JEQ         |
| 01010  | JNE         |
| 01011  | JGE         |
| 01100  | JLT         |
| 01101  | JGT         |
| 01110  | JLE         |
| 01111  | LD          |
| 10000  | ST          |
| 10001  | IN          |
| 10010  | OUT         |
| 10011  | HALT        |

## Formatos

### Formato R
`[15:11]=opcode`, `[10:8]=RD`, `[7:5]=RA`, `[4:2]=RB`, `[1:0]` reservados.
Usado por `ADD`, `SUB`, `AND`, `OR`, `CMP`, `MOV`.

### Formato I
`[15:11]=opcode`, `[10:8]=registro`, `[7:0]=inmediato/dirección`.
Usado por `LDI`, `LD`, `ST`, `IN`, `OUT`.

### Formato J
`[15:11]=opcode`, `[10:3]=dirección`, `[2:0]` reservados.
Usado por `JMP`, `JEQ`, `JNE`, `JGE`, `JLT`, `JGT`, `JLE`.

## Codificación interna de la ALU

| Operación | Código |
|-----------|--------|
| ADD       | 000    |
| SUB       | 001    |
| AND       | 010    |
| OR        | 011    |
| CMP       | 100    |

## Semántica de banderas
- ADD: `C=1` si hay acarreo; `Z=1` si el resultado es 0.
- SUB/CMP: `C=1` si `A >= B`; `Z=1` si el resultado es 0.
- AND/OR: `C=0`; `Z` según el resultado.
- No se implementa `V`.

## Saltos condicionales (usan C y Z)
- `JEQ`: Z=1
- `JNE`: Z=0
- `JGE`: C=1
- `JLT`: C=0
- `JGT`: C=1 y Z=0
- `JLE`: C=0 o Z=1

## Helpers
`isa.h` ofrece `formato_r`, `formato_i`, `formato_j` para construir
instrucciones sin manipular bits a mano.

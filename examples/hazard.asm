; Hazard load-use: LD seguido de un consumidor (requiere stall)
; Resultado esperado: mem[0x20] = 7, R1 = 7, R2 = 14
LDI R0, 7
ST R0, 0x20
LD R1, 0x20
ADD R2, R1, R1
HALT

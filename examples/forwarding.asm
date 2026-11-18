; Dependencias encadenadas: forwarding EX/MEM y MEM/WB
; Resultado esperado: R1 = 2, R2 = 4, R3 = 8
LDI R0, 1
ADD R1, R0, R0
ADD R2, R1, R1
ADD R3, R2, R2
HALT

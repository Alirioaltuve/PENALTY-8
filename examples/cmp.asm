; Comparacion: CMP seguido de JEQ
; Resultado esperado: R5 = 1 (R0 == R1)
LDI R0, 7
LDI R1, 7
CMP R0, R1
JEQ iguales
LDI R5, 0
JMP fin
iguales: LDI R5, 1
fin: HALT

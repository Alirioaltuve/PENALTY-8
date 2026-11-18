; Logica: AND y OR
; Resultado esperado: R4 = 2 (10 & 6), R5 = 14 (10 | 6)
LDI R0, 10
LDI R1, 6
AND R4, R0, R1
OR R5, R0, R1
HALT

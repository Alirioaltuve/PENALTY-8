; Programa combinado con varias instrucciones diferentes
; Resultado esperado:
;   R0=10, R1=6, R2=16, R3=4, R4=2, R5=14, R6=10, R7=16, mem[0x10]=16
LDI R0, 10
LDI R1, 6
ADD R2, R0, R1
SUB R3, R0, R1
AND R4, R0, R1
OR  R5, R0, R1
MOV R6, R0
ST  R2, 0x10
LD  R7, 0x10
HALT

; Saltos: JEQ, JNE, JGE, JLT, JGT, JLE (JMP se cubre en flush.asm)
; Cada prueba fija un registro distinto a 1 si el salto se cumple.
; Resultado esperado: R2=1, R3=1, R4=1, R5=1, R6=1, R7=1
LDI R0, 5

; JEQ (R0 == 5)
LDI R1, 5
CMP R0, R1
JEQ a1
LDI R2, 0
JMP a2
a1: LDI R2, 1
a2:

; JNE (R0 != 4)
LDI R1, 4
CMP R0, R1
JNE b1
LDI R3, 0
JMP b2
b1: LDI R3, 1
b2:

; JGE (R0 >= 5)
LDI R1, 5
CMP R0, R1
JGE c1
LDI R4, 0
JMP c2
c1: LDI R4, 1
c2:

; JLT (R0 < 6)
LDI R1, 6
CMP R0, R1
JLT d1
LDI R5, 0
JMP d2
d1: LDI R5, 1
d2:

; JGT (R0 > 4)
LDI R1, 4
CMP R0, R1
JGT e1
LDI R6, 0
JMP e2
e1: LDI R6, 1
e2:

; JLE (R0 <= 5)
LDI R1, 5
CMP R0, R1
JLE f1
LDI R7, 0
JMP f2
f1: LDI R7, 1
f2:

HALT

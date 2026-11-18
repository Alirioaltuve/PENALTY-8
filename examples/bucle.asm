; Bucle: suma 1 a R1 cinco veces mediante salto hacia atras
; Resultado esperado: R0 = 0, R1 = 5
LDI R0, 5
LDI R1, 0
LDI R7, 1
bucle: ADD R1, R1, R7
SUB R0, R0, R7
JNE bucle
HALT

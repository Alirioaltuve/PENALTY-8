; HALT con instrucciones pendientes en el pipeline
; Resultado esperado: R0=5, R1=10, R2=15, R3=0 (no ejecutada)
LDI R0, 5
LDI R1, 10
ADD R2, R0, R1
HALT
LDI R3, 99

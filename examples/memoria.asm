; Memoria: ST y LD
; Resultado esperado: mem[0x10] = 55, R1 = 55
LDI R0, 55
ST R0, 0x10
LD R1, 0x10
HALT

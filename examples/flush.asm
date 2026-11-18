; Branch flush: la instruccion trampa no debe ejecutarse
; Resultado esperado: R0 = 1 (y no 99)
LDI R0, 1
JMP destino
LDI R0, 99
destino: HALT

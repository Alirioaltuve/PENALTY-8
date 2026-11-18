; Entrada/Salida: OUT escribe al bus de salida (observable), IN lee del bus
; Sin dispositivo externo conectado, IN lee 0.
; Resultado esperado: OUT 0x30 = 42, R1 = 0
LDI R0, 42
OUT R0, 0x30
IN R1, 0x11
HALT

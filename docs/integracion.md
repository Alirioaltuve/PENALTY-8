# Sistema de Integración

## 1. Qué es
El ensamblaje que une `CPU_Goalkeeper`, `CommunicationMailbox`, `Arbitro` y el
lado STRIKER (`StrikerSim`) para simular un penalti completo.

## 2. Responsabilidad
Demostrar que el portero recibe un disparo, ejecuta su programa, responde, y que
el árbitro decide el resultado.

## 3. Por qué es un módulo independiente
La integración prueba el contrato entre computadores sin mezclar la lógica de
cada uno.

## 4. Entradas
El disparo (lo aporta el STRIKER). El sistema tiene `clk` y `reset`.

## 5. Salidas
Los resultados por disparo y el resultado del árbitro (impresos por el
testbench).

## 6. Señales que utiliza
El bus de E/S de la CPU, el bus del striker, el estado del mailbox y las señales
del árbitro.

## 7. Funcionamiento interno
Flujo de un penalti:
1. STRIKER escribe los cinco bytes y REQ.
2. GOALKEEPER detecta REQ, lee, ejecuta su programa y escribe la respuesta.
3. GOALKEEPER activa ACK.
4. STRIKER lee la respuesta y baja REQ.
5. El árbitro evalúa y decide.
6. Se repite para el siguiente disparo.

## 8. Lógica combinacional o secuencial
Combina ambos: datapath combinacional y estado secuencial.

## 9. Momento del reloj
Reloj de 10 ns (100 MHz). Todos los módulos de estado en flanco positivo.

## 10. Conexión con otros módulos
`testbench_integracion.cpp` conecta todos los módulos y carga el programa del
portero.

## 11. Instrucciones que intervienen
El programa del portero (ver `programa_goalkeeper.md`).

## 12. Pruebas realizadas
`testbench_integracion.cpp` ejecuta 4 disparos:
- Disparo 1 → SAVE.
- Disparo 2 → GOAL (reacción tardía).
- Disparo 3 → MISS (fuera de X).
- Disparo 4 → SAVE.

`test_goalkeeper_strategy.cpp` ejecuta 11 solicitudes, incluidas las fronteras
`power = 127/128/129` y `flight_ticks = 127/128/129`, y comprueba los valores
reales de los registros que alimentan los `OUT`. También verifica `shot_id`,
`reaction_ticks` y múltiples solicitudes seguidas.

## 13. Ejemplo sencillo
```
Escenario SAVE:
STRIKER: id=1, target=(128,128), power=100, flight=200
GOALKEEPER: dive=(144,128), reach=(16,16), reaction=43
ARBITRO: reaction<=flight y alcanza → SAVE
```

## 14. Decisiones de diseño
- `power` y `flight_ticks` son campos independientes; no se deriva uno del otro
  ni se valida `flight = 256 - power`.
- **No existe código de STRIKER-8 en el repositorio.** Para no inventar acuerdos
  con Rafael, el lado STRIKER se simula a nivel de protocolo con `StrikerSim`.
  Esto valida el mailbox y la CPU, pero **no** sustituye la integración con el
  STRIKER real.
- El mapa de E/S y el formato de paquetes son la propuesta de referencia y deben
  confirmarse con el STRIKER real.

# CommunicationMailbox

## 1. Qué es
Módulo independiente que implementa el canal de comunicación entre STRIKER-8 y
GOALKEEPER-8: datos del disparo, respuesta del portero, protocolo REQ/ACK y
medición del tiempo de reacción.

## 2. Responsabilidad
Mediar la comunicación sin que una CPU acceda a los registros internos de la
otra, y medir cuántos ciclos tarda el portero en responder.

## 3. Por qué es un módulo independiente
La comunicación es un contrato entre dos computadores. Aislarla permite probarla
y cambiarla sin tocar las CPU.

## 4. Entradas
- `clk`, `reset`.
- Lado STRIKER: `st_we`, `st_addr`, `st_wdata`, `st_re`.
- Lado GOALKEEPER: `gk_we`, `gk_addr`, `gk_wdata`, `gk_re`.

## 5. Salidas
- `st_rdata`, `gk_rdata` (datos leídos por cada lado).
- `req` (visible al portero), `ack` (visible al striker).
- `reaction_ticks` (tiempo medido).

## 6. Señales que utiliza
Registros internos (como `sc_signal`) para los cinco bytes del disparo, los
campos de respuesta, `req`, `ack` y el contador de ciclos.

## 7. Funcionamiento interno
- **Lectura**: combinacional según `*_addr`.
- **Escritura**: en flanco positivo según `*_we`.
- **REQ/ACK**: escribir REQ=1 inicia la solicitud y arranca el temporizador;
  escribir ACK detiene el temporizador. Escribir REQ=0 cierra la solicitud y
  desactiva ACK.
- **reaction_ticks (0x23)**: lo llena el mailbox con el valor medido; se ignora
  lo que escriba el programa.

## 8. Lógica combinacional o secuencial
Lectura combinacional; escritura, protocolo y temporizador secuenciales.

## 9. Momento del reloj
Protocolo y temporizador en `clk.pos()`.

## 10. Conexión con otros módulos
- Lado GOALKEEPER ↔ bus de E/S de `CPU_Goalkeeper`.
- Lado STRIKER ↔ `StrikerSim` (o el STRIKER-8 real).
- `reaction_ticks` se entrega a la respuesta y al árbitro.

## 11. Instrucciones que intervienen
`IN` y `OUT` del lado GOALKEEPER (leer 0x10–0x15, escribir 0x16 y 0x20–0x25).

## 12. Pruebas realizadas
En `testbench_integracion.cpp`:
- 4 disparos consecutivos, con REQ/ACK correctos.
- `reaction_ticks` medido (40–44 ciclos en la integración actual; depende del
  programa del portero).
- `shot_id` preservado.
- Sin pérdida de solicitudes entre disparos.

En `test_communication.cpp`, H verifica un valor exacto de 5 ciclos e I verifica
la saturación en 255.

## 13. Ejemplo sencillo
```
STRIKER: escribe 0x10..0x14, luego 0x15=1
GOALKEEPER: lee 0x15=1, lee datos, escribe respuesta y 0x16 (ACK)
STRIKER: lee 0x16=1 y 0x20..0x25, escribe 0x15=0
```

## 14. Decisiones de diseño
- El temporizador se mide desde la aceptación de REQ hasta ACK, sin superar 255.
- `reaction_ticks` se sobrescribe con el valor medido para no “inventar” el
  tiempo desde el programa, como exige el enunciado.
- Una sola solicitud pendiente a la vez.
- `power` y `flight_ticks` son datos independientes; el Mailbox los transfiere sin validar `flight = 256 - power`.
- El mapa de direcciones es el propuesto; **debe confirmarse con Rafael** si su
  STRIKER-8 usa otro mapa.

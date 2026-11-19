# Protocolo de Comunicación

## 1. Qué es
El contrato de datos y sincronización entre STRIKER-8 y GOALKEEPER-8.

## 2. Responsabilidad
Definir qué bytes se escriben, en qué orden, y cómo se sincronizan con REQ/ACK.

## 3. Por qué está documentado aparte
Es un acuerdo entre dos equipos. Debe poder cambiarse sin tocar el hardware, y
requiere validación de ambos lados.

## 4. Entradas/Salidas
Ver mapa de E/S.

## 5. Mapa de E/S (propuesta de referencia)
STRIKER escribe / GOALKEEPER lee:

| Dirección | Campo        |
|-----------|--------------|
| 0x10      | shot_id      |
| 0x11      | target_x     |
| 0x12      | target_y     |
| 0x13      | power        |
| 0x14      | flight_ticks |
| 0x15      | REQ          |

GOALKEEPER escribe / STRIKER lee:

| Dirección | Campo          |
|-----------|----------------|
| 0x16      | ACK            |
| 0x20      | shot_id        |
| 0x21      | dive_x         |
| 0x22      | dive_y         |
| 0x23      | reaction_ticks |
| 0x24      | reach_x        |
| 0x25      | reach_y        |

## 6. Formato del mensaje
- Petición: 5 bytes (shot_id, target_x, target_y, power, flight_ticks).
- Respuesta: 6 bytes (shot_id, dive_x, dive_y, reaction_ticks, reach_x,
  reach_y).

### 6.1 Independencia de `power` y `flight_ticks`

`power` y `flight_ticks` son dos campos independientes del protocolo. No se
deriva `flight_ticks` de `power` ni se rechazan combinaciones por la fórmula
`flight_ticks = 256 - power`; esa fórmula pertenecía a una propuesta inicial y no
forma parte de este contrato. GOALKEEPER usa `power` para decidir el alcance y
`flight_ticks` para el sesgo vertical, el ajuste de alcance y el tiempo de
reacción del Árbitro, sin imponer una relación entre ambos.

## 7. Secuencia REQ/ACK
1. STRIKER escribe los datos.
2. STRIKER activa REQ (0x15=1).
3. GOALKEEPER detecta REQ, lee y ejecuta.
4. GOALKEEPER escribe la respuesta y activa ACK (escritura a 0x16).
5. STRIKER recibe la respuesta.
6. STRIKER baja REQ (0x15=0).
7. El sistema desactiva ACK.
8. Se permite una nueva solicitud.

## 8. Lógica
La sincronización es secuencial (flanco de reloj). Los datos se mantienen
estables durante el intercambio.

## 9. Tiempo de reacción
Se mide desde la aceptación de REQ hasta ACK. El contador empieza en 0, se
incrementa por flanco, se detiene con ACK y no supera 255.

## 10. Conexión
Implementado por `CommunicationMailbox`.

## 11. Instrucciones usadas
`IN`/`OUT` en el portero; lectura/escritura de bus en el striker.

## 12. Validación
El protocolo fue validado en la rama de desarrollo (`portero-prueba`) mediante
varios intercambios consecutivos: REQ/ACK correctos, identificador preservado y
tiempo de reacción medido. Los archivos de pruebas de esa rama no se incluyen
en esta distribución.

## 13. Ejemplo
```
STRIKER: 0x10=1, 0x11=128, 0x12=128, 0x13=100, 0x14=200, 0x15=1
GK:      0x15 leído 1 → responde 0x20..0x25 y 0x16
STRIKER: lee 0x16=1, 0x20..0x25, luego 0x15=0
```

## 14. Decisiones de diseño
- Se adoptó la propuesta de referencia del enunciado.
- `power` y `flight_ticks` son independientes; no se valida `flight = 256 - power`.
- **Riesgo/conflicto:** no se dispone del código del STRIKER-8 de Rafael, por lo
  que este mapa debe confirmarse antes de la integración real. Si su mapa o
  formato difieren, el cambio se concentra en `communication_mailbox.h` y en el
  driver del striker.

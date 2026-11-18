# Árbitro

## 1. Qué es
Módulo combinacional que decide el resultado de un penalti.

## 2. Responsabilidad
Aplicar las reglas: MISS, GOAL o SAVE, a partir del disparo y de la respuesta del
portero.

## 3. Por qué es un módulo independiente
El árbitro no debe estar dentro del portero ni de la comunicación: es una regla
externa y verificable por separado.

## 4. Entradas
- Disparo: `target_x`, `target_y`, `power`, `flight_ticks`.
- Respuesta: `dive_x`, `dive_y`, `reaction_ticks`, `reach_x`, `reach_y`.

## 5. Salidas
- `resultado`: 0=MISS, 1=GOAL, 2=SAVE.

## 6. Señales que utiliza
Ninguna interna.

## 7. Funcionamiento interno
1. Si el balón está fuera de la portería (X o Y fuera de `[32,223]`): MISS.
2. Si `power == 0` (tiro inválido): MISS.
3. Si `reaction_ticks <= flight_ticks` **y** `|target_x-dive_x| <= reach_x`
   **y** `|target_y-dive_y| <= reach_y`: SAVE.
4. En otro caso: GOAL.

## 8. Lógica combinacional o secuencial
Combinacional pura.

## 9. Momento del reloj
No usa reloj.

## 10. Conexión con otros módulos
Recibe el disparo de STRIKER y la respuesta del portero (vía mailbox).

## 11. Instrucciones que intervienen
Ninguna directamente; opera sobre los datos intercambiados.

## 12. Pruebas realizadas
`test_arbitro.cpp`:
- SAVE, GOAL por alcance, GOAL por tiempo.
- MISS fuera de X, MISS fuera de Y, MISS por potencia 0.
- Límites de portería (32/223 dentro; 224 fuera).
- Alcance exacto (SAVE) y alcance excedido por 1 (GOAL).
- Reacción exacta en el límite (SAVE).

## 13. Ejemplo sencillo
```
target=(128,128), dive=(128,128), reach=(10,10), reaction=50, flight=100
→ SAVE
```

## 14. Decisiones de diseño
- Se trata `power == 0` como disparo inválido (MISS). Es una decisión
  documentada; si se prefiere otro criterio, es un cambio localizado.
- Las comparaciones usan diferencias absolutas para respetar las coordenadas
  `0..255`.
- `power` y `flight_ticks` se reciben como campos independientes; el Árbitro no
  deriva ni valida `flight = 256 - power`.

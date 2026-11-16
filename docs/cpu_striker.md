# CPU STRIKER (no disponible)

## 1. Qué es
STRIKER-8 es el computador que calcula y ejecuta los disparos. Su desarrollo
está a cargo de Rafael.

## 2. Estado en este repositorio
**NO existe código de STRIKER-8 en el repositorio.** No hay módulos de ISA,
memoria, unidad de control ni CPU de STRIKER que se puedan auditar o integrar.

## 3. Qué se hizo en su lugar
Para poder validar la comunicación y la integración sin inventar acuerdos con
Rafael, se implementó `StrikerSim` (`striker_sim.h`): un **simulador del lado
STRIKER a nivel de protocolo**.

## 4. Qué hace `StrikerSim`
- Escribe los cinco bytes del disparo en 0x10–0x14.
- Activa REQ (0x15=1).
- Espera ACK (0x16=1).
- Lee la respuesta en 0x20–0x25.
- Baja REQ (0x15=0) y espera que ACK se desactive.
- Repite con varios disparos.
- Publica disparo y respuesta al árbitro.

## 5. Qué NO hace
- No ejecuta una ISA de STRIKER.
- No implementa la lógica de un computador STRIKER.
- No garantiza compatibilidad con el STRIKER real de Rafael.

## 6. Responsabilidad del simulador
Permitir probar el `CommunicationMailbox` y la `CPU_Goalkeeper` de punta a
punta mientras el STRIKER real no esté disponible.

## 7. Por qué es independiente
Es un banco de pruebas, no hardware del proyecto. Puede eliminarse o sustituirse
por el STRIKER real sin afectar a GOALKEEPER.

## 8. Lógica
Secuencial, mediante un `SC_THREAD` sincronizado con el reloj.

## 9. Momento del reloj
Conduce el bus en el flanco negativo y muestrea en el flanco positivo, para que
los datos estén estables cuando el mailbox los captura.

## 10. Conexión
Se conecta al lado STRIKER del `CommunicationMailbox` y al `Arbitro`.

## 11. Instrucciones
Ninguna; no ejecuta ISA.

## 12. Pruebas realizadas
`testbench_integracion.cpp` usa `StrikerSim` con 4 escenarios: SAVE, GOAL tardío,
MISS y SAVE.

## 13. Ejemplo
```
escribir(0x10,id); escribir(0x11,tx); ... escribir(0x15,1);
while(leer(0x16)==0) wait(posedge);
leer(0x20..0x25);
escribir(0x15,0);
```

## 14. Decisiones y pendientes
- **Pendiente crítico:** integrar el STRIKER-8 real de Rafael.
- Antes de integrar hay que confirmar con Rafael: ISA, opcodes, mapa de E/S,
  formato de paquetes, convención de banderas y protocolo.
- El cambio de integración debería limitarse al driver del striker y, si cambia
  el mapa, a `communication_mailbox.h`.

# Guía de integración para STRIKER-8

Esta guía describe cómo STRIKER-8 debe conectarse a GOALKEEPER-8 a través del
`CommunicationMailbox`. Todo lo aquí descrito está implementado y verificado en
el código (`communication_mailbox.h` y `striker_sim.h` como referencia).

## 1. Señales que STRIKER debe proporcionar (lado externo del Mailbox)

| Señal | Dirección | Ancho | Descripción |
|---|---|---|---|
| `st_we` | salida | 1 bit | Habilita una escritura (se captura en flanco positivo) |
| `st_addr` | salida | 8 bits | Dirección del registro a leer/escribir |
| `st_wdata` | salida | 8 bits | Dato a escribir |
| `st_re` | salida | 1 bit | Habilita una lectura (convención) |
| `st_rdata` | entrada | 8 bits | Dato leído del Mailbox |
| `ack` | entrada | 1 bit | Confirmación de GOALKEEPER (visible) |

STRIKER **no** accede a los registros internos de GOALKEEPER: toda la
comunicación pasa por estas señales.

## 2. Temporización

- **Escritura:** STRIKER coloca `st_addr` y `st_wdata`, sube `st_we`, lo
  mantiene durante un **flanco positivo de reloj** y luego lo baja. El Mailbox
  captura en `clk.pos()` mientras `st_we==1`.

```
st_we = 0; esperar flanco negativo;
st_addr = direccion; st_wdata = valor;
st_we = 1;
esperar flanco positivo;   // el Mailbox captura aquí
esperar flanco negativo;
st_we = 0;
```

- **Lectura:** la salida `st_rdata` es combinacional respecto a `st_addr`. STRIKER
  coloca `st_addr`, sube `st_re` (opcional, por convención), espera un breve
  asentamiento y lee `st_rdata`.

```
st_addr = direccion; st_re = 1;
esperar asentamiento (p. ej. 1 ns);
leer st_rdata;
st_re = 0;
```

## 3. Enviar un disparo (solicitud)

Escribir los cinco campos y luego activar REQ:

| Dirección | Campo |
|---|---|
| `0x10` | shot_id |
| `0x11` | target_x |
| `0x12` | target_y |
| `0x13` | power |
| `0x14` | flight_ticks |
| `0x15` | REQ (escribir 1) |

```
escribir(0x10, shot_id);
escribir(0x11, target_x);
escribir(0x12, target_y);
escribir(0x13, power);
escribir(0x14, flight_ticks);
escribir(0x15, 1);      // activa REQ
```

El Mailbox solo acepta los campos si no hay una solicitud pendiente
(`!r_req`); la escritura de `0x15=1` arma `r_req`, limpia ACK, resetea el
temporizador a 0 y lo arranca.

## 4. Esperar ACK

GOALKEEPER procesa la solicitud y escribe ACK en `0x16`. STRIKER debe sondeos
hasta verlo en alto:

```
mientras (leer(0x16) == 0):
    esperar flanco positivo;
```

## 5. Leer la respuesta

Cuando `0x16==1`, leer la respuesta completa:

| Dirección | Campo |
|---|---|
| `0x20` | shot_id (eco) |
| `0x21` | dive_x |
| `0x22` | dive_y |
| `0x23` | reaction_ticks |
| `0x24` | reach_x |
| `0x25` | reach_y |

```
shot_id  = leer(0x20);
dive_x   = leer(0x21);
dive_y   = leer(0x22);
reaction = leer(0x23);
reach_x  = leer(0x24);
reach_y  = leer(0x25);
```

`reaction_ticks` lo mide el Mailbox (desde REQ hasta ACK), no la CPU.

## 6. Liberar REQ

```
escribir(0x15, 0);   // baja REQ
```

## 7. Esperar que ACK vuelva a cero

```
mientras (leer(0x16) != 0):
    esperar flanco positivo;
```

## 8. Iniciar el siguiente disparo

Solo cuando `ACK==0` y `REQ==0` el Mailbox admite una nueva solicitud. Repetir
desde la sección 3.

## 9. Orden completo del intercambio

1. STRIKER escribe los cinco campos (`0x10..0x14`).
2. STRIKER escribe `0x15=1` (REQ).
3. GOALKEEPER lee `0x15=1`, lee los cinco campos y ejecuta su programa.
4. GOALKEEPER escribe la respuesta (`0x20..0x25`).
5. GOALKEEPER escribe `0x16` (ACK).
6. STRIKER lee la respuesta.
7. STRIKER escribe `0x15=0` (baja REQ).
8. El Mailbox baja ACK.
9. STRIKER espera `0x16==0` antes de la siguiente solicitud.

## 10. Reglas importantes

- **Una sola solicitud pendiente a la vez:** una segunda `REQ=1` mientras hay
  una pendiente se ignora.
- **Respuesta inmutable tras ACK:** el Mailbox solo acepta `0x20..0x25` si
  `r_req && !r_ack`.
- **ACK único:** el Mailbox activa ACK una sola vez por solicitud.
- **reaction_ticks** satura en 255.

## 11. Referencia de implementación

`striker_sim.h` es un emulador de STRIKER a nivel de protocolo que implementa
exactamente esta secuencia. NO es el STRIKER-8 real; úsese como referencia para
el bus de E/S de STRIKER.

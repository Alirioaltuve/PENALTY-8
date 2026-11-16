# Memoria de Datos

## 1. Qué es
Memoria de lectura/escritura de 256 posiciones de 8 bits.

## 2. Responsabilidad
Almacenar los datos que el programa guarda y recupera (`LD`/`ST`).

## 3. Por qué es un módulo independiente
Es un espacio separado de las instrucciones y de la E/S. Mantenerla aparte
evita conflictos y refleja la arquitectura acordada.

## 4. Entradas
- `clk`, `reset`.
- `write_enable`: habilita la escritura (`ST`).
- `direccion`: dirección de 8 bits.
- `write_data`: dato a escribir.

## 5. Salidas
- `read_data`: dato leído de la dirección.

## 6. Señales que utiliza
`sc_signal<sc_uint<8>> memoria[256]`. Se usan señales para que la lectura
combinacional reaccione también cuando el contenido cambia.

## 7. Funcionamiento interno
- Lectura combinacional: `read_data = memoria[direccion]`.
- Escritura en flanco positivo si `write_enable`.
- Reset síncrono: limpia las 256 posiciones.

## 8. Lógica combinacional o secuencial
Lectura combinacional; escritura y reset secuenciales.

## 9. Momento del reloj
Escritura en `clk.pos()`.

## 10. Conexión con otros módulos
- Dirección y dato de escritura provienen de la CPU (Unidad de Control y banco
  de registros).
- La lectura vuelve al writeback.

## 11. Instrucciones que intervienen
`LD` (lectura) y `ST` (escritura).

## 12. Pruebas realizadas
`test_memoria_datos.cpp`:
- Escritura y lectura de 0xAA en la dirección 5.
- Sobrescritura con 0x55.
- Dirección no escrita devuelve 0.
- El reset limpia la memoria.

## 13. Ejemplo sencillo
```
write_enable=1, direccion=5, write_data=0xAA (flanco) → mem[5]=0xAA
direccion=5 → read_data=0xAA
```

## 14. Decisiones de diseño
- Se separa completamente de la E/S.
- Se eligió `sc_signal` interno para garantizar lecturas actualizadas tras una
  escritura en la misma dirección (caso ST seguido de LD).

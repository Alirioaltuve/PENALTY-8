# Memoria de Instrucciones

## 1. Qué es
Memoria de solo lectura que almacena el programa: 256 instrucciones de 16 bits.

## 2. Responsabilidad
Entregar la instrucción correspondiente a la dirección que indica el PC.

## 3. Por qué es un módulo independiente
La memoria de programa es un espacio distinto del de datos y de la E/S.
Separarla refleja la arquitectura Harvard simplificada acordada.

## 4. Entradas
- `direccion`: dirección de 8 bits (viene del PC).

## 5. Salidas
- `instruccion`: instrucción de 16 bits.

## 6. Señales que utiliza
Arreglo interno `memoria[256]` de `sc_uint<16>`.

## 7. Funcionamiento interno
Lectura combinacional: `instruccion = memoria[direccion]`.
La memoria se inicializa en cero y se carga con `cargar_programa()` antes de
iniciar la simulación.

## 8. Lógica combinacional o secuencial
Combinacional.

## 9. Momento del reloj
No usa reloj. La lectura se estabiliza tras un delta.

## 10. Conexión con otros módulos
- Recibe la dirección del PC.
- Entrega la instrucción a la Unidad de Control.

## 11. Instrucciones que intervienen
Todas (es la fuente de las instrucciones).

## 12. Pruebas realizadas
`testbench_memoria_instrucciones.cpp`:
- Se carga un programa de prueba con `cargar_programa()`.
- Lectura de las direcciones 0, 5 y 6 con los valores esperados.

## 13. Ejemplo sencillo
```
programa[5] = 0x1234
direccion = 5 → instruccion = 0x1234
```

## 14. Decisiones de diseño
- Ya no depende de una instrucción escrita a mano en el constructor: ahora se
  carga el programa con `cargar_programa(const std::vector<sc_uint<16>>&)`.
- Se conserva el tamaño de 256 palabras de 16 bits y la lectura combinacional.

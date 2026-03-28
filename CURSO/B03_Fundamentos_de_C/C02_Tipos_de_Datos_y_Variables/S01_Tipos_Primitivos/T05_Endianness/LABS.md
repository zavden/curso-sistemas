# Labs — Endianness

## Descripcion

Laboratorio para observar el orden de bytes en memoria, detectar el endianness
del sistema, usar las funciones de conversion host/network, y escribir/leer
datos binarios de forma portable.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Herramientas: `xxd` (para inspeccionar archivos binarios)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Detectar endianness | Determinar si el sistema es little o big-endian con cast y union |
| 2 | Orden de bytes en memoria | Visualizar como se almacenan uint16_t y uint32_t byte a byte |
| 3 | Funciones de conversion | Usar htonl, ntohl, htons, ntohs para convertir entre host y network byte order |
| 4 | Datos binarios portables | Escribir y leer enteros en big-endian a un archivo, verificar con xxd |

## Archivos

```
labs/
├── README.md          ← Ejercicios paso a paso
├── detect_endian.c    ← Programa para la parte 1 (deteccion de endianness)
├── byte_order.c       ← Programa para la parte 2 (visualizacion de bytes)
├── net_convert.c      ← Programa para la parte 3 (htonl, htons, etc.)
└── portable_file.c    ← Programa para la parte 4 (lectura/escritura portable)
```

## Como ejecutar

```bash
cd labs/
# Seguir las instrucciones del README.md
```

## Tiempo estimado

~20 minutos

## Recursos creados

| Recurso | Tipo | Se elimina al final |
|---------|------|---------------------|
| Ejecutables compilados | Binarios | Si (limpieza final) |
| data.bin | Archivo de datos binarios | Si (limpieza final) |

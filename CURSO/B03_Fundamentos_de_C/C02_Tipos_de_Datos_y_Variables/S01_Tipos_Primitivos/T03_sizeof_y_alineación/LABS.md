# Labs — sizeof y alineacion

## Descripcion

Laboratorio para explorar el operador `sizeof`, los requisitos de alineacion
de tipos primitivos, el padding que el compilador inserta en structs, y el
efecto de `__attribute__((packed))` en el layout de memoria.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | sizeof y alignof | Tamano y alineacion de tipos primitivos y arrays |
| 2 | Padding en structs | offsetof revela el padding insertado por el compilador |
| 3 | Reordenar campos | Minimizar padding ordenando campos de mayor a menor |
| 4 | packed structs | __attribute__((packed)) elimina padding y sus consecuencias |

## Archivos

```
labs/
├── README.md          ← Ejercicios paso a paso
├── sizeof_basics.c    ← sizeof y alignof de tipos primitivos y arrays
├── padding_inspect.c  ← offsetof para visualizar padding en structs
├── reorder_fields.c   ← Comparacion de struct con campos reordenados
└── packed_struct.c    ← Structs packed vs normales, peligros de desalineacion
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

# Labs — Declaracion y acceso a arrays

## Descripcion

Laboratorio para practicar la declaracion, inicializacion y acceso a arrays en
C. Se explora sizeof, el patron ARRAY_SIZE, inicializadores designados, acceso
fuera de limites (UB), VLAs y el decay de array a puntero en funciones.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Declaracion y sizeof | sizeof de arrays, conteo de elementos con sizeof(arr)/sizeof(arr[0]) |
| 2 | Inicializacion | Completa, parcial, con ceros, tamano automatico, designated initializers |
| 3 | Acceso e iteracion | Lectura, escritura, out-of-bounds (UB), iteracion con for, sanitizers |
| 4 | Arrays y funciones | Decay a puntero, sizeof pierde informacion, VLAs |

## Archivos

```
labs/
├── README.md   ← Ejercicios paso a paso
├── sizes.c     ← Programa para explorar sizeof de arrays
├── init.c      ← Modos de inicializacion de arrays
├── access.c    ← Acceso, modificacion e iteracion
├── oob.c       ← Acceso fuera de limites (UB)
├── vla.c       ← Variable-Length Arrays (C99)
└── decay.c     ← Decay de array a puntero en funciones
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

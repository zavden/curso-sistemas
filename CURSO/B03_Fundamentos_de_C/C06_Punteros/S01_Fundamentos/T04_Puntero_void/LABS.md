# Labs — Puntero void*

## Descripcion

Laboratorio para explorar el puntero generico `void*`: asignacion desde
cualquier tipo, restricciones de desreferencia, funciones genericas con
`memcpy`, y uso en funciones de la biblioteca estandar (`malloc`, `qsort`,
`bsearch`).

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | void* generico | Asignar int*, double*, char* a void* — conversion implicita ida y vuelta |
| 2 | No se puede desreferenciar | Necesidad de cast, peligro del cast incorrecto, sizeof de tipos |
| 3 | Funcion generica | swap generico con void* y memcpy para ints, doubles y structs |
| 4 | void* en la practica | malloc retorna void*, qsort y bsearch usan void* y comparadores |

## Archivos

```
labs/
├── README.md        ← Ejercicios paso a paso
├── void_basic.c     ← Programa para la parte 1 (asignacion y conversion)
├── void_deref.c     ← Programa para la parte 2 (desreferencia y cast)
├── swap_generic.c   ← Programa para la parte 3 (swap generico)
└── void_stdlib.c    ← Programa para la parte 4 (malloc, qsort, bsearch)
```

## Como ejecutar

```bash
cd labs/
# Seguir las instrucciones del README.md
```

## Tiempo estimado

~25 minutos

## Recursos creados

| Recurso | Tipo | Se elimina al final |
|---------|------|---------------------|
| Ejecutables compilados | Binarios | Si (limpieza final) |

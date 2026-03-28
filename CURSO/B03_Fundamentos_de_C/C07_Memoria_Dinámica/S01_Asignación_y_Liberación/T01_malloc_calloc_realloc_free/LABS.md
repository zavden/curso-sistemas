# Labs — malloc, calloc, realloc, free

## Descripcion

Laboratorio para practicar la alocacion y liberacion de memoria dinamica en C.
Se usan `malloc`, `calloc`, `realloc` y `free`, verificando resultados en
memoria y observando el mapa del proceso con `/proc/self/maps`.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`
- Acceso a `/proc/self/maps` (disponible en Linux)

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | malloc y free | Alocar int y array, verificar NULL, no castear en C |
| 2 | calloc vs malloc | Memoria zero-initialized vs sin inicializar |
| 3 | realloc | Crecer array dinamico, patron seguro con puntero temporal |
| 4 | Mapa de memoria | Direcciones de heap, stack, globals y /proc/self/maps |

## Archivos

```
labs/
├── README.md          ← Ejercicios paso a paso
├── malloc_basic.c     ← malloc/free con int y array
├── calloc_vs_malloc.c ← Comparacion calloc (ceros) vs malloc (basura)
├── realloc_grow.c     ← Array dinamico con realloc y patron seguro
└── heap_map.c         ← Direcciones de memoria y /proc/self/maps
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
| `malloc_basic` | Ejecutable compilado | Si (limpieza final) |
| `calloc_vs_malloc` | Ejecutable compilado | Si (limpieza final) |
| `realloc_grow` | Ejecutable compilado | Si (limpieza final) |
| `heap_map` | Ejecutable compilado | Si (limpieza final) |

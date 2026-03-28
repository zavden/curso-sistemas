# Labs — Arena allocators

## Descripcion

Laboratorio para implementar y usar un arena allocator (bump allocator).
Se construye la estructura Arena paso a paso, se aloca memoria para distintos
tipos, se practica el patron reset para reusar memoria, y se compara el
rendimiento de arena vs malloc/free individual.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`
- Valgrind instalado (opcional, para verificar memoria)

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Arena basica | Implementar arena_init, arena_alloc (bump pointer), arena_destroy |
| 2 | Tipos mixtos en la arena | Alocar ints, doubles, structs y strings desde la misma arena |
| 3 | Arena con reset | Reusar memoria con arena_reset sin realocar (patron frame allocator) |
| 4 | Arena vs malloc/free | Comparar rendimiento de arena frente a malloc/free individual en loop |

## Archivos

```
labs/
├── README.md          ← Ejercicios paso a paso
├── arena.h            ← Implementacion del arena (header-only)
├── arena_basic.c      ← Programa para la parte 1
├── arena_types.c      ← Programa para la parte 2
├── arena_reset.c      ← Programa para la parte 3
└── arena_vs_malloc.c  ← Programa para la parte 4
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

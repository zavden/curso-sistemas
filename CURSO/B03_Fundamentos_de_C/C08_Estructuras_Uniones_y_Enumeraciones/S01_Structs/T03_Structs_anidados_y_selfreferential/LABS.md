# Labs -- Structs anidados y self-referential

## Descripcion

Laboratorio para practicar structs anidados (struct dentro de struct),
structs self-referential (puntero a si mismo), implementacion de una linked
list basica con operaciones de crear, recorrer, insertar y liberar nodos, y
forward declarations para structs mutuamente referenciados.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Structs anidados | Struct dentro de struct, acceso con doble punto, acceso via puntero |
| 2 | Self-referential structs | Struct Node con puntero a si mismo, por que no se puede contener a si mismo |
| 3 | Linked list basica | Crear nodos, insertar, recorrer, liberar memoria |
| 4 | Forward declaration | Declarar un struct antes de definirlo, structs mutuamente referenciados |

## Archivos

```
labs/
├── README.md          <- Ejercicios paso a paso
├── nested.c           <- Struct anidado con Date y Person
├── selfreferential.c  <- Struct Node self-referential
├── linked_list.c      <- Linked list con operaciones completas
└── forward_decl.c     <- Forward declarations y structs mutuamente referenciados
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

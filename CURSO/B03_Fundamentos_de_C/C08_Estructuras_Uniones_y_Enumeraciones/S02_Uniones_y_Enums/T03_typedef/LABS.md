# Labs — typedef

## Descripcion

Laboratorio para practicar los usos principales de `typedef`: alias de tipos
primitivos, structs y enums; simplificacion de punteros a funcion; y
encapsulacion con opaque types en compilacion multi-archivo.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | typedef basico | Alias para primitivos, structs, enums; typedef vs #define |
| 2 | Punteros a funcion | typedef simplifica declaraciones, dispatch, callbacks, arrays de function pointers |
| 3 | Opaque types | Encapsulacion con typedef + forward declaration, compilacion multi-archivo |

## Archivos

```
labs/
├── README.md           <- Ejercicios paso a paso
├── basic_typedef.c     <- Alias para primitivos, structs, enums (parte 1)
├── define_vs_typedef.c <- Diferencia entre typedef y #define (parte 1)
├── func_pointers.c     <- typedef con punteros a funcion (parte 2)
├── stack.h             <- Interfaz publica del opaque type (parte 3)
├── stack.c             <- Implementacion privada del opaque type (parte 3)
└── stack_main.c        <- Programa que usa el opaque type (parte 3)
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
| Archivos objeto (.o) | Intermedios | Si (limpieza final) |

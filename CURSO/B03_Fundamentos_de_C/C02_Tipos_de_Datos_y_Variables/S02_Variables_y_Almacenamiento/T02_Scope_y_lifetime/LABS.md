# Labs -- Scope y lifetime

## Descripcion

Laboratorio para observar en la practica los conceptos de scope (visibilidad),
lifetime (tiempo de vida) y linkage (enlace) de variables en C. Se demuestra
shadowing, errores de scope, el bug clasico de retornar puntero a variable
local, y compilacion multi-archivo con `nm` para verificar visibilidad de
simbolos.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Herramientas binutils: `nm` (incluida con GCC)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Block scope y shadowing | Variables en bloques internos ocultan a las externas, -Wshadow detecta esto |
| 2 | File scope y linkage | Internal vs external linkage, static a nivel de archivo, verificacion con nm |
| 3 | Lifetime -- dangling pointer | Retornar puntero a variable local (UB), alternativas seguras |
| 4 | Compilacion multi-archivo con linkage | Dos modulos con static int count independientes, nm para verificar |

## Archivos

```
labs/
├── README.md              <- Ejercicios paso a paso
├── block_scope.c          <- Shadowing y block scope
├── scope_error.c          <- Codigo que no compila (variable fuera de scope)
├── dangling_pointer.c     <- Retorno de puntero a variable local (UB)
├── linkage_visible.c      <- Funciones y variables con internal/external linkage
├── linkage_visible.h      <- Header para linkage_visible
├── linkage_visible_main.c <- Main que usa linkage_visible
├── linkage_counter.c      <- Modulo contador con static int count
├── linkage_counter.h      <- Header para el contador
├── linkage_logger.c       <- Modulo logger con static int count (independiente)
├── linkage_logger.h       <- Header para el logger
└── linkage_main.c         <- Main que usa ambos modulos
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
| *.o | Archivos objeto | Si (limpieza por parte y final) |
| Ejecutables compilados | Binarios | Si (limpieza por parte y final) |

# Labs -- Enteros

## Descripcion

Laboratorio para explorar los tipos enteros de C: sus tamanos con `sizeof`,
sus rangos con `limits.h`, los tipos de ancho fijo de `stdint.h`, y los format
specifiers portables de `inttypes.h`.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Tamanos de tipos enteros | `sizeof` de cada tipo, diferencia signed/unsigned, signedness de `char` |
| 2 | Rangos y limites | `limits.h` con los rangos minimo y maximo de cada tipo |
| 3 | Tipos de ancho fijo | `stdint.h`: exact-width, least-width, fast y tipos especiales |
| 4 | Format specifiers portables | Macros `PRId32`, `PRIx64`, etc. de `inttypes.h` |

## Archivos

```
labs/
├── README.md            <- Ejercicios paso a paso
├── sizes.c              <- sizeof de tipos basicos (parte 1)
├── limits.c             <- Rangos con limits.h (parte 2)
├── fixed_width.c        <- Tipos de stdint.h (parte 3)
└── format_specifiers.c  <- Macros de inttypes.h (parte 4)
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

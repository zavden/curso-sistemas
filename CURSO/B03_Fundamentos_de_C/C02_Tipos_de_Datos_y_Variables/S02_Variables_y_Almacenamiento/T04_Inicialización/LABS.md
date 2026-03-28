# Labs -- Inicializacion

## Descripcion

Laboratorio para verificar experimentalmente las reglas de inicializacion en C:
valores por defecto segun storage class, designated initializers (C99), compound
literals, e inicializacion parcial con zero-fill.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Valores por defecto | Globales/static = 0, locales = basura, warnings del compilador |
| 2 | Designated initializers | Inicializar structs por nombre y arrays por indice (C99) |
| 3 | Compound literals | Crear valores temporales sin variable intermedia (C99) |
| 4 | Inicializacion parcial y {0} | Zero-fill implicito, patron {0} para arrays y structs |

## Archivos

```
labs/
├── README.md            <- Ejercicios paso a paso
├── default_values.c     <- Programa para la parte 1
├── designated_init.c    <- Programa para la parte 2
├── compound_literals.c  <- Programa para la parte 3
└── partial_init.c       <- Programa para la parte 4
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

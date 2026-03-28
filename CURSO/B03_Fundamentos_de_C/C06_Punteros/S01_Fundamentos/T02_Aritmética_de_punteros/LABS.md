# Labs -- Aritmetica de punteros

## Descripcion

Laboratorio para observar experimentalmente como la aritmetica de punteros
opera en unidades de elementos (no bytes), como restar punteros produce
distancias en elementos, como comparar punteros permite recorrer arrays con
centinelas, y que operaciones el compilador rechaza por invalidas.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Stride de punteros | `p+n` avanza `sizeof(*p)*n` bytes -- int*, char*, double* |
| 2 | Resta de punteros | `ptrdiff_t`, distancia en elementos vs bytes |
| 3 | Comparacion de punteros | `<`, `>`, `==`, recorrido con puntero centinela begin/end |
| 4 | Operaciones ilegales | Sumar punteros, multiplicar, dividir -- errores del compilador |

## Archivos

```
labs/
├── README.md    <- Ejercicios paso a paso
├── stride.c     <- Programa para demostrar el stride de int*, char*, double*
├── ptrdiff.c    <- Programa para demostrar resta de punteros y ptrdiff_t
├── compare.c    <- Programa para demostrar comparacion y recorrido centinela
└── illegal.c    <- Programa con operaciones ilegales comentadas
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
| stride, ptrdiff, compare, illegal | Ejecutables compilados | Si (limpieza final) |

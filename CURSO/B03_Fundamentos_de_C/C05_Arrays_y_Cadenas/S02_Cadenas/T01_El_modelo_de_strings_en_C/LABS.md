# Labs — El modelo de strings en C

## Descripcion

Laboratorio para explorar el modelo de null-terminated strings en C: como se
almacenan en memoria, la diferencia entre `char[]` y `char *`, operaciones
manuales sin `string.h`, y los errores mas comunes.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Null-terminated strings | El terminador `\0`, `sizeof` vs `strlen`, visualizar bytes en memoria |
| 2 | `char[]` vs `const char *` | Array modificable vs puntero a `.rodata`, `sizeof` de cada uno |
| 3 | Operaciones manuales | Implementar `strlen`, `strcmp`, `strcpy` sin `string.h` |
| 4 | Errores comunes | Olvidar `\0`, `sizeof` de puntero, comparar con `==`, null embebido |

## Archivos

```
labs/
├── README.md        <- Ejercicios paso a paso
├── null_term.c      <- Programa para la parte 1 (sizeof vs strlen, bytes)
├── arr_vs_ptr.c     <- Programa para la parte 2 (array vs puntero)
├── manual_ops.c     <- Programa para la parte 3 (strlen, strcmp, strcpy manuales)
└── common_errors.c  <- Programa para la parte 4 (errores clasicos)
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
| Archivos `.s` (assembly) | Intermedios | Si (limpieza de parte 2) |

# Labs — string.h

## Descripcion

Laboratorio para explorar las funciones de `string.h`: longitud, copia (segura
e insegura), concatenacion, comparacion, busqueda y operaciones de memoria.
Se observan las trampas clasicas de `strncpy` y `strcat`, y se practica el
patron seguro con `snprintf`.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | strlen, strcpy, strncpy | Longitud de strings, copia insegura, trampas de strncpy, patron seguro con snprintf |
| 2 | strcat, strncat | Concatenacion, manejo de buffer, concatenacion segura con snprintf |
| 3 | strcmp, strncmp, strchr, strstr | Comparacion de contenido vs punteros, busqueda de caracteres y substrings |
| 4 | memcpy, memset, memmove, memcmp | Operaciones sobre bloques de bytes, solapamiento, trampa de memset con ints |

## Archivos

```
labs/
├── README.md            <- Ejercicios paso a paso
├── str_length_copy.c    <- Programa para la parte 1 (strlen, strcpy, strncpy, snprintf)
├── str_concat.c         <- Programa para la parte 2 (strcat, strncat, snprintf)
├── str_compare_search.c <- Programa para la parte 3 (strcmp, strncmp, strchr, strstr)
└── mem_operations.c     <- Programa para la parte 4 (memcpy, memset, memmove, memcmp)
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

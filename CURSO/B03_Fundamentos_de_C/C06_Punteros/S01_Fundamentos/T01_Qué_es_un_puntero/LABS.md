# Labs — Que es un puntero

## Descripcion

Laboratorio para experimentar con punteros en C: obtener direcciones con `&`,
desreferenciar con `*`, modificar datos a traves de punteros, verificar tamanos
con `sizeof`, y usar punteros como parametros de funciones.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Direccion y desreferencia | Operadores `&` y `*`, formato `%p`, modelo mental de punteros |
| 2 | Modificar datos a traves de punteros | Escritura con `*p = valor`, swap con punteros |
| 3 | sizeof de punteros y NULL | Todos los punteros tienen el mismo tamano, inicializacion con NULL |
| 4 | Punteros y funciones | Paso por puntero, output parameters, retornar multiples valores |

## Archivos

```
labs/
├── README.md      ← Ejercicios paso a paso
├── address.c      ← Programa para explorar & y *
├── modify.c       ← Modificacion de datos y swap con punteros
├── sizes.c        ← sizeof de punteros y demo de NULL
└── functions.c    ← Paso por puntero y output parameters
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

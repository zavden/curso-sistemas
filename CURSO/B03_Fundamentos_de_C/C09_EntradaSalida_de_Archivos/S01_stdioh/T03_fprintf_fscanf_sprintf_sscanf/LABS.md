# Labs — fprintf, fscanf, snprintf, sscanf

## Descripcion

Laboratorio para practicar las cuatro funciones de I/O formateado mas
importantes de `<stdio.h>`: escritura a archivo con `fprintf`, lectura con
`fscanf`, formateo seguro en buffer con `snprintf`, y parseo de strings con
`sscanf`. Se verifican valores de retorno y se detectan errores de parseo y
truncacion.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | fprintf | Escribir datos formateados a archivo, log con timestamp, fprintf a stderr |
| 2 | fscanf | Parsear datos de archivo, verificar valor de retorno, manejar lineas corruptas |
| 3 | snprintf | Formatear strings en buffer, detectar truncacion, patron string builder |
| 4 | sscanf | Parsear strings, extraer campos con formatos complejos, validacion de input |

## Archivos

```
labs/
├── README.md          ← Ejercicios paso a paso
├── fprintf_log.c      ← Programa que escribe log formateado a archivo
├── fscanf_parse.c     ← Programa que parsea datos de archivo con fscanf
├── snprintf_buf.c     ← Programa que demuestra snprintf y deteccion de truncacion
└── sscanf_extract.c   ← Programa que extrae campos de strings con sscanf
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
| `server.log` | Archivo de datos | Si (limpieza final) |
| `sensors.dat` | Archivo de datos | Si (limpieza final) |
| `users.csv` | Archivo de datos | Si (limpieza final) |

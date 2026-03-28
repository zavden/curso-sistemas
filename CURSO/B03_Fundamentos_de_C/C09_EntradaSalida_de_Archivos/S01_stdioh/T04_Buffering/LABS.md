# Labs — Buffering

## Descripcion

Laboratorio para observar los tres modos de buffering de stdio (full, line,
unbuffered), forzar el flush con fflush, configurar el modo y tamano del buffer
con setvbuf, y demostrar el problema clasico de output perdido por buffering
ante un crash.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Herramientas de terminal: `strace` (para contar syscalls)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Tres modos de buffering | stdout (line), stderr (unbuffered), archivo (full) |
| 2 | fflush | Forzar flush explicito, diferencia terminal vs pipe/archivo |
| 3 | setvbuf | Cambiar modo y tamano del buffer, contar syscalls con strace |
| 4 | Problema del buffering | printf sin '\n' + crash = output perdido |

## Archivos

```
labs/
├── README.md          <- Ejercicios paso a paso
├── buf_modes.c        <- Programa para demostrar los tres modos (parte 1)
├── fflush_demo.c      <- Programa para demostrar fflush y terminal vs pipe (parte 2)
├── setvbuf_demo.c     <- Programa para demostrar setvbuf con distintos modos (parte 3)
└── crash_demo.c       <- Programa que crashea y pierde output (parte 4)
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
| buf_output.txt | Archivo de texto | Si (limpieza final) |
| fflush_output.txt | Archivo de texto | Si (limpieza final) |
| setvbuf_output.txt | Archivo de texto | Si (limpieza final) |
| crash_log.txt | Archivo de texto | Si (limpieza final) |
| crash_log_safe.txt | Archivo de texto | Si (limpieza final) |

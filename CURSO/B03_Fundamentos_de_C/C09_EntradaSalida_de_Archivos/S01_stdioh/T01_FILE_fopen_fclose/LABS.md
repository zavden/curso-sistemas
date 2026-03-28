# Labs -- FILE*, fopen, fclose

## Descripcion

Laboratorio para practicar la apertura y cierre de archivos con `fopen`/`fclose`,
experimentar con los modos de apertura y sus efectos sobre el contenido, trabajar
con los streams estandar, y observar las consecuencias de no cerrar archivos.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | fopen y fclose basico | Abrir, escribir, cerrar, releer, y provocar errores de apertura |
| 2 | Los 3 streams estandar | stdout vs stderr, redireccion independiente con > y 2> |
| 3 | Modos de apertura | Efecto de w (trunca), a (append), r+ (sobrescribe sin truncar), w+ vs r+ |
| 4 | Errores comunes | File descriptor leaks, limite EMFILE, patron correcto de apertura/cierre |

## Archivos

```
labs/
├── README.md        ← Ejercicios paso a paso
├── open_close.c     ← Abrir, escribir, cerrar y releer un archivo
├── std_streams.c    ← Demostrar stdout y stderr como streams separados
├── open_modes.c     ← Comparar modos w, a, r+ y su efecto en el contenido
└── fd_limit.c       ← Mostrar error EMFILE al no cerrar archivos
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
| Archivos temporales (testfile.txt, modes_test.txt, etc.) | Archivos de texto | Si (limpieza por partes y final) |
| Ejecutables compilados | Binarios | Si (limpieza final) |
| Archivos .c creados inline (error_test.c, wplus_test.c, fd_correct.c) | Codigo fuente temporal | Si (limpieza final) |

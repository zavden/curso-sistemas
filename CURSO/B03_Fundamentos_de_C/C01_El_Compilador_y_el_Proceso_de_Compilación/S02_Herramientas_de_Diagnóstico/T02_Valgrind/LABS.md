# Labs -- Valgrind

## Descripcion

Laboratorio para detectar y diagnosticar errores de memoria en programas C
usando Valgrind (Memcheck). Se practican los cuatro tipos de errores mas
comunes: memory leaks, accesos invalidos al heap, uso de memoria despues de
free, y double free. El ejercicio final combina todos los errores para
practicar la lectura sistematica de reportes de Valgrind.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Valgrind instalado (`valgrind --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Memory leaks | Detectar fugas con --leak-check=full, leer HEAP SUMMARY y LEAK SUMMARY |
| 2 | Accesos invalidos | Invalid read/write, memoria no inicializada, --track-origins=yes |
| 3 | Use-after-free y double free | Las tres stack traces de Valgrind, errores invisibles sin herramientas |
| 4 | Encontrar todos los bugs | Analisis manual + Valgrind en un programa con multiples errores combinados |

## Archivos

```
labs/
├── README.md            ← Ejercicios paso a paso
├── leak.c               ← Programa con memory leaks (parte 1)
├── invalid_access.c     ← Programa con accesos invalidos al heap (parte 2)
├── use_after_free.c     ← Programa con use-after-free y double free (parte 3)
└── buggy.c              ← Programa con todos los tipos de errores (parte 4)
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
| leak, invalid_access, use_after_free, buggy | Ejecutables compilados | Si (limpieza final) |

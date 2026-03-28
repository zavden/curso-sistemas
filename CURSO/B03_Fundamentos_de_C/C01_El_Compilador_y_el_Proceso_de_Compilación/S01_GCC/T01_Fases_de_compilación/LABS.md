# Labs — Fases de compilación

## Descripcion

Laboratorio para observar y analizar cada fase del pipeline de GCC:
preprocesamiento, compilación, ensamblado y enlace. Se examinan los archivos
intermedios (.i, .s, .o) y se practica la compilación multi-archivo.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Herramientas binutils: `nm`, `objdump`, `file`, `size` (incluidas con GCC)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Las 4 fases | Detener GCC en cada fase y examinar la salida |
| 2 | Archivos intermedios | Contenido de .i, .s, .o — que contiene cada uno |
| 3 | Compilacion multi-archivo | Compilar y enlazar archivos separados, errores de enlace |
| 4 | Efecto de la optimizacion | Comparar assembly con -O0 vs -O2 |

## Archivos

```
labs/
├── README.md        ← Ejercicios paso a paso
├── phases.c         ← Programa simple para las partes 1 y 2
├── math_utils.h     ← Header para la parte 3
├── math_utils.c     ← Implementacion para la parte 3
├── main_multi.c     ← Programa principal para la parte 3
└── optimize.c       ← Programa para la parte 4
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
| *.i, *.s, *.o | Archivos intermedios | Si (limpieza final) |
| Ejecutables compilados | Binarios | Si (limpieza final) |

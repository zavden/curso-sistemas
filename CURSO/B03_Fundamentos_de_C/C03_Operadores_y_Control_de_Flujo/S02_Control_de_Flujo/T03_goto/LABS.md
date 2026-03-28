# Labs — goto

## Descripcion

Laboratorio para experimentar con `goto` en C: saltos basicos, scope de labels,
el patron de cleanup centralizado para manejo de errores, y comparacion
estructural entre codigo con goto y sin goto.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | goto basico | Salto incondicional, labels, scope de labels |
| 2 | Patron de cleanup | Manejo de errores con goto: fopen, malloc, cleanup centralizado |
| 3 | goto vs alternativas | Comparacion estructural entre la version con goto y sin goto |

## Archivos

```
labs/
├── README.md          ← Ejercicios paso a paso
├── goto_basic.c       ← Programa con goto basico (salto hacia adelante)
├── goto_scope.c       ← Demuestra function scope de labels
├── goto_cleanup.c     ← Patron de cleanup centralizado con goto
└── no_goto_cleanup.c  ← Misma logica sin goto (para comparar)
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
| `/tmp/goto_lab_test.txt` | Archivo temporal | Si (el programa lo elimina automaticamente) |

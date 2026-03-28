# Labs — Operador coma y ternario

## Descripcion

Laboratorio para practicar el operador ternario (`?:`), el operador coma (`,`),
y entender la tabla de precedencia completa de C con ejemplos concretos.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Operador ternario | Uso basico, en printf, const, y tipo del resultado |
| 2 | Operador coma | Evaluacion izquierda a derecha, valor de retorno, uso en for |
| 3 | Precedencia completa | Interaccion entre coma, ternario, asignacion y otros |

## Archivos

```
labs/
├── README.md            ← Ejercicios paso a paso
├── ternary_basic.c      ← Ternario: seleccion, printf, const, plural
├── ternary_types.c      ← Ternario: promocion de tipos, asociatividad
├── comma_basics.c       ← Coma: valor de retorno, precedencia, for
├── comma_vs_separator.c ← Coma operador vs coma separador
└── precedence_demo.c    ← Precedencia: ternario, coma, asignacion
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

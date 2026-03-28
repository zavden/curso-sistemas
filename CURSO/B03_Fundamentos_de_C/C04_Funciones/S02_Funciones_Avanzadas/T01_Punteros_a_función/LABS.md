# Labs — Punteros a funcion

## Descripcion

Laboratorio para practicar la declaracion, asignacion y uso de punteros a
funcion en C. Se cubren callbacks, qsort con comparadores custom, y dispatch
tables como alternativa a switch.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Sintaxis basica | Declarar, asignar, llamar a traves de puntero a funcion; equivalencias con `&` y `(*fp)()`; typedef |
| 2 | Callbacks | Pasar funcion como argumento: apply (transformar array) y filter_count (contar con predicado) |
| 3 | qsort | Usar qsort de stdlib con funciones comparadoras custom para ints y strings |
| 4 | Dispatch table | Array de punteros a funcion como calculadora; seleccion por indice |

## Archivos

```
labs/
├── README.md      ← Ejercicios paso a paso
├── basics.c       ← Programa para la parte 1 (sintaxis basica)
├── callback.c     ← Programa para la parte 2 (callbacks y predicados)
├── sorting.c      ← Programa para la parte 3 (qsort con comparadores)
└── dispatch.c     ← Programa para la parte 4 (dispatch table)
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

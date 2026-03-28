# Labs — Declaracion y uso de structs

## Descripcion

Laboratorio para practicar la declaracion, inicializacion, acceso, copia y uso
de structs en funciones y arrays. Se cubren designated initializers, el operador
punto, el operador flecha, y las limitaciones de comparacion entre structs.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Declaracion e inicializacion | Positional vs designated initializers, campos a cero, acceso con punto |
| 2 | Punteros a struct | Operador flecha (->), paso por puntero, const correctness |
| 3 | Copia y comparacion | Asignacion con =, shallow copy, por que == no funciona, alternativas |
| 4 | Funciones y arrays | Retornar struct por valor, compound literals, arrays de structs |

## Archivos

```
labs/
├── README.md           ← Ejercicios paso a paso
├── basics.c            ← Declaracion, inicializacion, acceso con punto
├── pointers.c          ← Punteros a struct, operador flecha, paso a funciones
├── copy_compare.c      ← Copia con =, comparacion campo a campo, shallow copy
└── functions_arrays.c  ← Struct como retorno, compound literals, arrays de structs
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

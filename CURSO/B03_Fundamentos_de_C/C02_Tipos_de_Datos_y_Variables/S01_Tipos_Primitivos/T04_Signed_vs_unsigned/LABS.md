# Labs — Signed vs unsigned

## Descripcion

Laboratorio para observar en la practica como los mismos bits se interpretan
de forma diferente segun el tipo sea signed o unsigned: representacion en
complemento a dos, comportamiento de overflow, conversiones implicitas
peligrosas, y como GCC puede detectar estos problemas con warnings.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Representacion en bits | Mismos bits, diferente interpretacion (complemento a 2) |
| 2 | Overflow signed vs unsigned | UB en signed, wrapping definido en unsigned |
| 3 | Conversiones implicitas | Trampas al comparar o mezclar signed con unsigned |
| 4 | Warnings de GCC | Detectar problemas con -Wsign-compare, -Wsign-conversion, -Wconversion |

## Archivos

```
labs/
├── README.md              ← Ejercicios paso a paso
├── bit_representation.c   ← Tabla de bits signed vs unsigned (parte 1)
├── overflow_behavior.c    ← Wrapping unsigned y UB signed (parte 2)
├── implicit_conversions.c ← Trampas de comparacion y conversion (parte 3)
└── warnings_demo.c        ← Codigo que dispara warnings de GCC (parte 4)
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

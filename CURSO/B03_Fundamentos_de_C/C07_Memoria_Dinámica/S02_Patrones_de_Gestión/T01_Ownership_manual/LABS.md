# Labs — Ownership manual

## Descripcion

Laboratorio para practicar las convenciones de ownership en C: quien aloca,
quien libera, como transferir responsabilidad, y como detectar errores de
ownership con Valgrind.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Valgrind instalado (`valgrind --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Caller owns | Funcion retorna memoria alocada, el caller hace free |
| 2 | Callee owns (create/destroy) | Struct con constructor/destructor, encapsulacion del recurso |
| 3 | Transfer ownership | Mover responsabilidad de memoria entre funciones, documentar con comentarios |
| 4 | Errores de ownership | Double free, use-after-free, leaks, use-after-transfer con Valgrind |

## Archivos

```
labs/
├── README.md            ← Ejercicios paso a paso
├── caller_owns.c        ← Funciones que retornan memoria alocada (parte 1)
├── callee_owns.c        ← Struct Person con create/destroy (parte 2)
├── transfer.c           ← Stack con transferencia de ownership (parte 3)
└── ownership_errors.c   ← Programa con errores intencionales (parte 4)
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

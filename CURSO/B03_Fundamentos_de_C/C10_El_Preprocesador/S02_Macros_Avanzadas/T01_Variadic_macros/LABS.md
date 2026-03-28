# Labs -- Variadic macros

## Descripcion

Laboratorio para practicar macros variadicos: `__VA_ARGS__` basico, el problema
de cero argumentos (coma extra), las soluciones `##__VA_ARGS__` (extension GCC)
y `__VA_OPT__` (C23), macros LOG/DEBUG con `__FILE__`/`__LINE__`, y uso de
`gcc -E` para inspeccionar la expansion.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`
- GCC 8+ para soporte de `__VA_OPT__` (parte 3)

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | `__VA_ARGS__` basico | Macros con argumentos variables, `...` y `__VA_ARGS__` |
| 2 | Problema de cero argumentos | Coma extra sin args variadicos, `##__VA_ARGS__`, solucion sin parametro fijo |
| 3 | `__VA_OPT__` (C23) | Solucion estandar para cero argumentos, comparacion con `##__VA_ARGS__` |
| 4 | Sistema de logging | Macros LOG con niveles, `__FILE__`, `__LINE__`, `__func__`, compilacion condicional |
| 5 | `gcc -E` y expansion | Usar el preprocesador para ver la expansion real de macros variadicos |

## Archivos

```
labs/
├── README.md           ← Ejercicios paso a paso
├── basic_variadic.c    ← __VA_ARGS__ basico con LOG y PRINT_ALL
├── zero_args_problem.c ← Problema de coma extra, ##__VA_ARGS__
├── va_opt_demo.c       ← __VA_OPT__ de C23
├── log_system.c        ← Sistema de logging con niveles y file/line
└── gcc_expand.c        ← Archivo reducido para inspeccionar expansion con gcc -E
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
| Ejecutables compilados (basic_variadic, zero_args_problem, va_opt_demo, log_system, gcc_expand) | Binarios | Si (limpieza final) |
| expanded.c | Salida del preprocesador | Si (limpieza final) |

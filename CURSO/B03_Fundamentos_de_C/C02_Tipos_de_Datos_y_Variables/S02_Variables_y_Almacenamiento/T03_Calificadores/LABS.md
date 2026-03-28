# Labs — Calificadores

## Descripcion

Laboratorio para experimentar con los tres calificadores de tipo en C: `const`,
`volatile` y `restrict`. Se observan errores de compilacion, se compara el
assembly generado con y sin calificadores, y se verifica el impacto en la
optimizacion del compilador.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | const | Variables const, punteros a const, const puntero, const en funciones |
| 2 | volatile | Efecto en optimizacion, comparar assembly con y sin volatile |
| 3 | restrict | Promesa de no-aliasing, comparar assembly con y sin restrict |

## Archivos

```
labs/
├── README.md          ← Ejercicios paso a paso
├── const_basics.c     ← Programa con las cuatro combinaciones de const y punteros
├── const_errors.c     ← Programa con errores de const para descomentar
├── const_function.c   ← Funcion con parametro const (const correctness)
├── volatile_demo.c    ← Lectura doble con y sin volatile
├── volatile_loop.c    ← Loop con y sin volatile (inspeccion de assembly)
└── restrict_demo.c    ← Suma de arrays con y sin restrict
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
| *.s | Archivos assembly | Si (limpieza final) |

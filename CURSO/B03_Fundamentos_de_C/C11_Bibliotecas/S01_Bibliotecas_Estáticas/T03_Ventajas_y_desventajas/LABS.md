# Labs — Ventajas y desventajas del enlace estatico

## Descripcion

Laboratorio para comparar de forma practica el enlace estatico contra el
dinamico: compilar el mismo programa en ambas modalidades, medir diferencias de
tamano con `ls -lh` y `size`, inspeccionar dependencias con `ldd`, verificar
portabilidad, evaluar uso de disco con multiples programas y observar el impacto
de actualizar una biblioteca.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Paquete de desarrollo de libc estatica (`glibc-static` en Fedora, `libc6-dev`
  en Debian/Ubuntu)
- Herramientas: `ar`, `ldd`, `size`, `file`
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Biblioteca y compilacion | Se construye `libmathutil.a` y `libmathutil.so`, se compila el mismo programa con enlace estatico y dinamico |
| 2 | Tamano del binario | Diferencia de tamano entre binario estatico y dinamico con `ls -lh` y `size` |
| 3 | Dependencias e inspeccion | `ldd` y `file` revelan las dependencias de cada binario |
| 4 | Portabilidad | El binario estatico funciona al copiarse a otro directorio; el dinamico falla sin la `.so` |
| 5 | Uso de disco | Multiples programas con la misma biblioteca: impacto en disco de estatico vs dinamico |
| 6 | Actualizacion de biblioteca | Actualizar la `.so` surte efecto sin recompilar; con `.a` hay que recompilar |

## Archivos

```
labs/
├── README.md        ← Ejercicios paso a paso
├── mathutil.h       ← API publica de utilidades matematicas
├── mathutil.c       ← Implementacion de la biblioteca (v1.0.0)
├── mathutil_v2.c    ← Implementacion actualizada (v2.0.0) para parte 6
├── app_calc.c       ← Programa cliente: factorial, fibonacci, primalidad
└── app_stats.c      ← Segundo programa cliente: listado de primos y fibonacci
```

## Como ejecutar

```bash
cd labs/
# Seguir las instrucciones de README.md
```

## Tiempo estimado

~30 minutos

## Recursos creados

| Recurso | Tipo | Se elimina al final |
|---------|------|---------------------|
| `mathutil.o` | Archivo objeto | Si |
| `libmathutil.a` | Biblioteca estatica | Si |
| `libmathutil.so` | Biblioteca dinamica | Si |
| `app_calc.o`, `app_stats.o` | Archivos objeto | Si |
| `calc_static`, `calc_dynamic` | Ejecutables | Si |
| `stats_static`, `stats_dynamic` | Ejecutables | Si |
| `portability_test/` | Directorio de prueba | Si |

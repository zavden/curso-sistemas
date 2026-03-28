# Labs -- Versionado de soname

## Descripcion

Laboratorio para construir bibliotecas compartidas con soname embebido, crear
la cadena de tres nombres (linker name, soname, real name), demostrar una
actualizacion compatible (minor bump) sin recompilar el programa, y verificar
la coexistencia de versiones incompatibles (major bump).

## Prerequisitos

- GCC instalado (`gcc --version`)
- `readelf`, `objdump`, `ldd` y `ln` disponibles
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Crear biblioteca con soname | Compilar con `-Wl,-soname`, crear cadena de symlinks, verificar con `readelf` y `objdump` |
| 2 | Enlazar un programa | El ejecutable registra la dependencia por soname (NEEDED), no por real name |
| 3 | Actualizacion compatible | Minor bump (1.0.0 a 1.1.0): el programa sigue funcionando sin recompilar |
| 4 | Coexistencia de majors | Major bump (v1 a v2): ambas versiones coexisten, cada programa usa la suya |

## Archivos

```
labs/
├── README.md          ← Ejercicios paso a paso
├── calc.h             ← Header publico para libcalc ABI v1
├── calc_v1.c          ← Implementacion v1.0.0 (add, mul)
├── calc_v1_minor.c    ← Implementacion v1.1.0 (agrega sub, compatible)
├── calc_v2.h          ← Header publico para libcalc ABI v2 (API incompatible)
├── calc_v2.c          ← Implementacion v2.0.0 (add cambia a 3 argumentos)
├── program_v1.c       ← Cliente que usa la API v1
└── program_v2.c       ← Cliente que usa la API v2
```

## Como ejecutar

```bash
cd labs/
# Seguir las instrucciones del README.md
```

## Tiempo estimado

~30 minutos

## Recursos creados

| Recurso | Tipo | Se elimina al final |
|---------|------|---------------------|
| `calc_v1.o`, `calc_v1_minor.o`, `calc_v2.o` | Archivos objeto | Si (limpieza final) |
| `libcalc.so.1.0.0`, `libcalc.so.1.1.0`, `libcalc.so.2.0.0` | Bibliotecas compartidas | Si (limpieza final) |
| `libcalc.so`, `libcalc.so.1`, `libcalc.so.2` | Symlinks | Si (limpieza final) |
| `program_v1`, `program_v2` | Ejecutables | Si (limpieza final) |

# Labs -- Macros predefinidas

## Descripcion

Laboratorio para explorar las macros predefinidas del compilador C: macros
estandar (`__FILE__`, `__LINE__`, `__func__`, `__DATE__`, `__TIME__`,
`__STDC__`, `__STDC_VERSION__`), extensiones GCC/Clang (`__GNUC__`,
`__SIZEOF_*__`, `__COUNTER__`), y herramientas del compilador (`gcc -dM -E`).
Incluye uso practico en logging, assert personalizado y build info embebido.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Macros estandar basicas | __FILE__, __LINE__, __func__, __DATE__, __TIME__, __STDC_VERSION__, efecto de -std en __STDC_VERSION__ |
| 2 | Logging, assert y error tracking | Macros de logging con ubicacion del caller, assert personalizado con #cond, RETURN_ERROR |
| 3 | Build info embebido | Deteccion de compilador, plataforma, arquitectura, NDEBUG, -Wdate-time, strings en binario |
| 4 | __COUNTER__ y gcc -dM -E | Generacion de nombres unicos, traza numerada, listar todas las macros predefinidas del compilador |

## Archivos

```
labs/
├── README.md              ← Ejercicios paso a paso
├── predefined_basics.c    ← Macros estandar: __FILE__, __LINE__, __func__, etc.
├── debug_log.c            ← Logging, assert personalizado, RETURN_ERROR
├── build_info.c           ← Informacion de build: compilador, plataforma, timestamp
└── counter_demo.c         ← __COUNTER__ y generacion de nombres unicos
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
| Ejecutables compilados (predefined_basics, debug_log, build_info, counter_demo) | Binarios | Si (limpieza final) |
| /tmp/std_assert.c, /tmp/std_assert | Archivo temporal y binario | Si (limpieza parte 2) |

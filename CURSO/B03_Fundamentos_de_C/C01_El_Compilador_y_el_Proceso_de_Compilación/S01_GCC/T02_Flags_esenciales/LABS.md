# Labs -- Flags esenciales

## Descripcion

Laboratorio para experimentar con los flags mas importantes de GCC: niveles de
warnings (-Wall, -Wextra, -Wpedantic, -Werror), seleccion de estandar (-std),
flags de debug (-g), niveles de optimizacion (-O), y sets recomendados para
desarrollo y produccion.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Herramientas basicas: `file`, `ls`, `wc` (incluidas en cualquier Linux)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Niveles de warnings | Como -Wall, -Wextra, -Wpedantic y -Werror detectan distintos problemas |
| 2 | Seleccion del estandar | Diferencia entre -std=gnu11 y -std=c11 -Wpedantic con extensiones GNU |
| 3 | Flags de debug y optimizacion | Efecto de -g y -O en tamano de binario y assembly generado |
| 4 | Sets recomendados | Compilacion con sets de desarrollo vs produccion |

## Archivos

```
labs/
├── README.md        <- Ejercicios paso a paso
├── warnings.c       <- Programa con problemas deliberados para warnings
├── standard.c       <- Programa con extensiones GNU (zero-length array, statement expr, binary literal)
└── optimize_cmp.c   <- Loop de suma para comparar -O0 vs -O2
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
| Ejecutables compilados | Binarios | Si (limpieza por parte y final) |
| *.s | Assembly generado | Si (limpieza por parte y final) |

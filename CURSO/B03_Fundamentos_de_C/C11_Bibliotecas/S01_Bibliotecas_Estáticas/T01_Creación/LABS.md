# Labs -- Creacion de bibliotecas estaticas

## Descripcion

Laboratorio para crear una biblioteca estatica desde cero: escribir codigo
fuente con header, compilar a archivos objeto con `gcc -c`, empaquetar con
`ar rcs`, inspeccionar con `ar t`, `nm` y `file`, enlazar con `gcc -L. -l`,
y verificar el enlace selectivo comparando tamanios de ejecutables.

## Prerequisitos

- GCC instalado (`gcc --version`)
- `ar`, `nm`, `file` disponibles (incluidos en `binutils`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Codigo fuente y archivos objeto | Compilar .c a .o con `gcc -c`, inspeccionar con `file` y `nm` |
| 2 | Creacion del .a | Empaquetar .o con `ar rcs`, verificar con `ar t` y `nm` |
| 3 | Enlazar y ejecutar | Usar `-L.` y `-l` para enlazar, ejecutar el programa |
| 4 | Enlace selectivo | Verificar que el linker solo incluye los .o necesarios del .a |
| 5 | Comparacion de tamanios | Medir diferencia de tamanio entre enlace directo con .o y con .a |

## Archivos

```
labs/
├── README.md         <- Ejercicios paso a paso
├── vec3.h            <- Header publico (struct Vec3 + prototipos)
├── vec3.c            <- Operaciones matematicas (add, scale, dot, length)
├── vec3_print.c      <- Funciones de impresion (print, report)
├── main.c            <- Programa cliente que usa todas las funciones
└── main_minimal.c    <- Programa cliente que usa solo vec3_add
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
| `vec3.o`, `vec3_print.o` | Archivos objeto | Si (limpieza final) |
| `libvec3.a` | Biblioteca estatica | Si (limpieza final) |
| `demo_vec3`, `demo_minimal`, `demo_direct` | Ejecutables | Si (limpieza final) |

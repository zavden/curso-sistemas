# Labs -- Creacion de bibliotecas dinamicas

## Descripcion

Laboratorio para crear una biblioteca dinamica (`.so`) desde cero: compilar
con `-fPIC`, enlazar con `gcc -shared`, inspeccionar con `file`, `nm -D`,
`readelf -d` y `objdump -d`, comparar codigo PIC vs no-PIC, y contrastar
el `.so` con un `.a` en el ejecutable final.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Herramientas `file`, `nm`, `readelf`, `objdump` disponibles
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Compilar con -fPIC | Generar objetos PIC, comparar codigo PIC vs no-PIC con objdump y readelf |
| 2 | Crear el .so | `gcc -shared`, verificar con `file`, comparar formatos .o / .a / .so |
| 3 | Inspeccionar simbolos y secciones | `nm -D` para simbolos exportados, `readelf -d` para seccion .dynamic |
| 4 | Que pasa sin -fPIC | Intentar crear .so sin PIC, observar error o TEXTREL |
| 5 | Comparar .so vs .a | Tamano del ejecutable, dependencias (ldd), simbolos (nm) |

## Archivos

```
labs/
├── README.md       ← Ejercicios paso a paso
├── vec3.h          ← Header con API de vectores 3D
├── vec3.c          ← Implementacion (se compila con -fPIC)
├── vec3_nopic.c    ← Misma implementacion, para compilar sin -fPIC
└── main_vec3.c     ← Programa cliente que usa la biblioteca
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
| `vec3_pic.o`, `vec3_nopic.o` | Archivos objeto | Si (limpieza final) |
| `libvec3.so`, `libvec3_nopic.so` | Bibliotecas dinamicas | Si (limpieza final) |
| `libvec3.a` | Biblioteca estatica | Si (limpieza final) |
| `main_static`, `main_dynamic` | Ejecutables | Si (limpieza final) |

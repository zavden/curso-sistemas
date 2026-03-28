# Labs -- dlopen, dlsym, dlclose

## Descripcion

Laboratorio para practicar carga dinamica en runtime con la API de
`dlfcn.h`: abrir bibliotecas con `dlopen`, obtener punteros a funciones con
`dlsym`, diagnosticar errores con `dlerror`, y descargar con `dlclose`.
Culmina con un sistema de plugins donde se agregan `.so` nuevos sin
recompilar el host.

## Prerequisitos

- GCC instalado (`gcc --version`)
- `nm`, `ldd` y `file` disponibles
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | dlopen y dlsym basico | Cargar libm en runtime, obtener cos() con dlsym, compilar con -ldl |
| 2 | Manejo de errores | dlerror para biblioteca no encontrada, simbolo inexistente, RTLD_NOW vs RTLD_LAZY |
| 3 | Sistema de plugins | Interfaz comun (plugin.h), host que carga plugins, dlclose para limpieza |
| 4 | Plugins sin recompilar | Agregar plugin_reverse.so y plugin_count.so sin tocar el host |

## Archivos

```
labs/
├── README.md          ← Ejercicios paso a paso
├── plugin.h           ← Interfaz comun para todos los plugins
├── dlopen_basic.c     ← Cargar libm en runtime, llamar cos() via dlsym
├── dlerror_demo.c     ← Manejo de errores: archivo no encontrado, simbolo inexistente
├── plugin_upper.c     ← Plugin que convierte texto a mayusculas
├── plugin_reverse.c   ← Plugin que invierte el texto
├── plugin_count.c     ← Plugin que cuenta caracteres, palabras y lineas
└── host.c             ← Programa host que descubre y carga plugins
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
| `dlopen_basic` | Ejecutable | Si (limpieza parte 1) |
| `dlerror_demo` | Ejecutable | Si (limpieza parte 2) |
| `host` | Ejecutable | Si (limpieza parte 4) |
| `plugins/*.so` | Bibliotecas compartidas | Si (limpieza parte 4) |

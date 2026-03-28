# Labs — Punteros colgantes y wild pointers

## Descripcion

Laboratorio para provocar, observar y diagnosticar dangling pointers (punteros
colgantes) y wild pointers (punteros salvajes). Se usan las advertencias del
compilador, Valgrind y el patron SAFE_FREE para detectar y prevenir estos bugs.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Valgrind instalado (`valgrind --version`)
- Opcional: libasan instalada para usar `-fsanitize=address`
  - Fedora: `sudo dnf install libasan`
  - Debian/Ubuntu: `sudo apt install libasan8` (el numero de version puede variar)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Dangling pointer: use-after-free | Acceso a memoria liberada, deteccion con warnings y Valgrind |
| 2 | Dangling pointer: retornar local | Puntero a stack frame destruido, UB silencioso |
| 3 | Wild pointer | Puntero no inicializado, deteccion con -Wuninitialized |
| 4 | Prevencion: SAFE_FREE y verificacion | Patron free+NULL, verificar antes de usar, herramientas |

## Archivos

```
labs/
├── README.md        ← Ejercicios paso a paso
├── dangling_uaf.c   ← Use-after-free (parte 1)
├── dangling_local.c ← Retornar direccion de variable local (parte 2)
├── wild_pointer.c   ← Puntero no inicializado (parte 3)
└── safe_free.c      ← Patron SAFE_FREE y prevencion (parte 4)
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

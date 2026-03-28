# Labs -- Buffer overflows

## Descripcion

Laboratorio para observar buffer overflows en accion, entender por que ocurren,
y aprender a prevenirlos. Se usan las protecciones del compilador (stack canary,
FORTIFY_SOURCE, AddressSanitizer) para detectar overflows de forma segura, sin
explotar vulnerabilidades.

## Prerequisitos

- GCC instalado (`gcc --version`)
- libasan instalado para AddressSanitizer (Fedora: `sudo dnf install libasan`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Stack buffer overflow | strcpy sin verificar tamano, observar corrupcion de variables adyacentes |
| 2 | Funciones peligrosas vs seguras | gets vs fgets, strcpy vs strncpy, sprintf vs snprintf |
| 3 | Defensas del compilador | Stack canaries (-fstack-protector), ASan (-fsanitize=address), FORTIFY_SOURCE |
| 4 | Escribir codigo seguro | Patrones para validar tamanos antes de copiar |

## Archivos

```
labs/
├── README.md           <- Ejercicios paso a paso
├── stack_overflow.c    <- Programa con overflow intencional (Parte 1)
├── dangerous_funcs.c   <- Funciones peligrosas vs seguras (Parte 2)
├── fortify_demo.c      <- Programa para demostrar FORTIFY_SOURCE (Parte 3)
└── safe_copy.c         <- Patrones de codigo seguro (Parte 4)
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

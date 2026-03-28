# Labs — open, close, read, write

## Descripcion

Laboratorio para practicar las cuatro syscalls fundamentales de POSIX I/O:
abrir archivos con distintos flags y permisos, escribir datos manejando short
writes, leer archivos con loop hasta EOF, e inspeccionar file descriptors.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`
- Sistema Linux con `/proc/self/fd` disponible (parte 4)

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | open y close | Flags (O_RDONLY, O_WRONLY, O_CREAT, O_TRUNC, O_EXCL), permisos, errno |
| 2 | write | Escribir a stdout y archivos, verificar retorno, write_all para short writes |
| 3 | read | Leer con buffer pequeno, partial reads, loop hasta EOF (retorno 0) |
| 4 | File descriptors | /proc/self/fd, STDIN_FILENO/STDOUT_FILENO/STDERR_FILENO, reutilizacion de fds |

## Archivos

```
labs/
├── README.md        ← Ejercicios paso a paso
├── open_close.c     ← Abrir/cerrar archivos, flags, errno
├── write_demo.c     ← Escribir datos, write_all con loop
├── read_demo.c      ← Leer archivo, partial reads, EOF
└── fd_inspect.c     ← Inspeccionar /proc/self/fd, reutilizacion de fds
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
| Ejecutables compilados (open_close, write_demo, read_demo, fd_inspect) | Binarios | Si (limpieza final) |
| output.txt, written.txt, testdata.txt | Archivos de datos | Si (limpieza final) |
| fd_test_1.txt, fd_test_2.txt, fd_test_3.txt | Archivos de prueba | Si (limpieza final) |

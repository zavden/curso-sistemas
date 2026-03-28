# Labs — lseek

## Descripcion

Laboratorio para practicar el posicionamiento dentro de archivos con `lseek`.
Se cubren los tres modos de seek, la obtencion del tamano de archivo, el
acceso aleatorio a registros de tamano fijo, y la creacion de sparse files.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`
- Herramientas: `hexdump`, `stat`, `du` (incluidas en la mayoria de distros)

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | SEEK_SET, SEEK_CUR, SEEK_END | Posicionamiento absoluto, relativo y desde el final |
| 2 | Obtener tamano de archivo | Patron lseek(fd, 0, SEEK_END) con restauracion de posicion |
| 3 | Acceso aleatorio a registros | Lectura/escritura de structs en posiciones calculadas |
| 4 | Sparse files | Huecos con lseek mas alla del final, tamano logico vs real |

## Archivos

```
labs/
├── README.md        <- Ejercicios paso a paso
├── seek_basics.c    <- Posicionamiento con los tres modos de seek
├── file_size.c      <- Obtener tamano de archivo preservando la posicion
├── records.c        <- Base de datos de registros con acceso aleatorio
└── sparse.c         <- Crear y verificar un sparse file de 100 MB
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
| seek_test.dat | Archivo de prueba (26 bytes) | Si (limpieza parte 1) |
| size_test.dat | Archivo de prueba (29 bytes) | Si (limpieza parte 2) |
| records.dat | Archivo binario de registros (~360 bytes) | Si (limpieza parte 3) |
| sparse.dat | Sparse file (~100 MB logicos, ~8 KB reales) | Si (limpieza parte 4) |
| Ejecutables compilados | Binarios | Si (limpieza final) |

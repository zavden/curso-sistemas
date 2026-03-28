# Labs — fread, fwrite, fgets, fputs

## Descripcion

Laboratorio para practicar I/O de archivos en modo texto (fgets/fputs) y modo
binario (fread/fwrite). Se exploran buffers parciales, serializacion de structs,
rendimiento de copia por bloques vs byte a byte, y el uso correcto de feof/ferror.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | fgets y fputs | Lectura linea a linea, '\n' incluido, buffer parcial, quitar '\n' con strcspn |
| 2 | fread y fwrite | Serializar/deserializar structs en binario, endianness, inspeccion con xxd |
| 3 | Copiar archivos | Byte a byte (fgetc/fputc) vs bloques (fread/fwrite), medir rendimiento |
| 4 | feof y ferror | Por que `while (!feof(f))` es incorrecto, forma correcta de detectar EOF |

## Archivos

```
labs/
├── README.md          ← Ejercicios paso a paso
├── text_lines.c       ← Escritura con fputs y lectura con fgets (buffer pequeno)
├── strip_newline.c    ← Eliminar '\n' de lineas leidas con fgets
├── binary_structs.c   ← Escribir y leer structs con fwrite/fread
├── copy_file.c        ← Copiar archivos: byte a byte vs bloques de 4096
└── eof_error.c        ← Uso correcto e incorrecto de feof y ferror
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
| `sample.txt` | Archivo de texto de prueba | Si (limpieza parte 1) |
| `sensors.bin` | Archivo binario con structs | Si (limpieza parte 2) |
| `testfile.bin`, `bigfile.bin` | Archivos de prueba para copia | Si (limpieza parte 3) |
| `copy_byte.tmp`, `copy_block.tmp` | Copias generadas | Si (limpieza parte 3) |
| `numbers.bin` | Archivo binario con ints | Si (limpieza parte 4) |
| Ejecutables compilados | Binarios | Si (limpieza final) |

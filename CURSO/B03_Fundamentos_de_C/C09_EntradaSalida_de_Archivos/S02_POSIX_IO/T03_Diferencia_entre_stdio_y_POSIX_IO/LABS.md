# Labs -- Diferencia entre stdio y POSIX I/O

## Descripcion

Laboratorio para comparar las dos APIs de I/O de Linux realizando las mismas
operaciones con ambas, convirtiendo entre `FILE*` y fd, midiendo el impacto del
buffering en el rendimiento, y consolidando criterios de decision.

## Prerequisitos

- GCC instalado (`gcc --version`)
- `strace` instalado (`strace --version`)
- Herramientas: `diff`, `md5sum`, `dd` (incluidas en la mayoria de distribuciones)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Misma operacion con ambas APIs | Copiar archivo con stdio vs POSIX, comparar syscalls con strace |
| 2 | fileno y fdopen | Convertir entre FILE* y fd, peligro de mezclar APIs |
| 3 | Benchmark de rendimiento | Escritura con 4 estrategias: impacto del tamano de buffer y buffering |
| 4 | Cuando usar cada API | Tabla de decision, ejercicio de clasificacion de escenarios |

## Archivos

```
labs/
├── README.md        ← Ejercicios paso a paso
├── copy_stdio.c     ← Copia de archivo con stdio (fopen/fread/fwrite)
├── copy_posix.c     ← Copia de archivo con POSIX I/O (open/read/write)
├── bridge.c         ← Conversion entre FILE* y fd (fileno/fdopen)
├── mix_danger.c     ← Peligro de mezclar stdio y POSIX I/O sin fflush
└── bench_write.c    ← Benchmark de escritura con 4 estrategias
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
| Ejecutables compilados (copy_stdio, copy_posix, bridge, mix_danger, bench_write) | Binarios | Si (limpieza final) |
| testfile.bin, copy_stdio.out, copy_posix.out | Archivos de prueba | Si (limpieza final) |
| bridge_test.txt, mix_broken.txt, mix_fixed.txt | Archivos de prueba | Si (limpieza final) |
| bench_tmp.dat | Archivo temporal del benchmark | Si (eliminado por el programa) |

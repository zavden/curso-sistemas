# Labs — cgroups v2

## Descripcion

Laboratorio practico para explorar cgroups v2: verificar la
version, navegar la jerarquia en /sys/fs/cgroup, inspeccionar
controladores (memory, cpu, pids), y ver los limites del
contenedor.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Jerarquia de cgroups | Version, /sys/fs/cgroup, controladores |
| 2 | Controladores | memory, cpu, pids — leer limites y uso |
| 3 | cgroups y Docker | Como Docker usa cgroups para limitar recursos |

## Archivos

```
labs/
└── README.md    ← Guia paso a paso (documento principal del lab)
```

## Como ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte.

## Tiempo estimado

~15 minutos.

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| Ninguno | — | — | — |

Este lab no crea recursos Docker.

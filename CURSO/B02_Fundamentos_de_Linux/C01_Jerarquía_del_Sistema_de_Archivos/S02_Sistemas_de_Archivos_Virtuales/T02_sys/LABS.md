# Labs — /sys

## Descripcion

Laboratorio practico para explorar el filesystem virtual /sys (sysfs):
dispositivos de bloque, interfaces de red, seguir la jerarquia de
symlinks, y comparar con /proc.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Estructura de /sys | Categorias: block, class, bus, devices |
| 2 | Interfaces de red | /sys/class/net/ — estado, MAC, estadisticas |
| 3 | Jerarquia de symlinks | class → devices, vista logica vs fisica |

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

~10 minutos.

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| Ninguno | — | — | — |

Este lab no crea recursos Docker.

# Labs — iostat

## Descripcion

Laboratorio practico para iostat: salida basica (tps, kB_read/s,
kB_wrtn/s), estadisticas extendidas (-x) con columnas clave (r/s,
w/s, r_await, w_await, aqu-sz, %util), interpretacion de metricas
(await como indicador principal, %util en HDD vs SSD, aqu-sz),
tipo de carga (secuencial vs aleatoria), diagnostico de disco
saturado, por particion (-p), y combinacion con vmstat/iotop.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Salida basica y extendida | iostat, -d, -x, -h, primera linea |
| 2 | Metricas clave | await, %util, aqu-sz, tipo de carga |
| 3 | Diagnostico | Disco saturado, por particion, flujo completo |

## Archivos

```
labs/
└── README.md    <- Guia paso a paso (documento principal del lab)
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

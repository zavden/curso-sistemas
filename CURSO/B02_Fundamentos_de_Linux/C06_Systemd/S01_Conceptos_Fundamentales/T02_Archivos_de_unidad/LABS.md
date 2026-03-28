# Labs — Archivos de unidad

## Descripcion

Laboratorio practico para dominar los archivos de unidad de systemd:
las tres ubicaciones (/usr/lib vs /etc vs /run) y su precedencia,
la estructura interna ([Unit]/[Service]/[Install]), drop-ins y
overrides, daemon-reload, y herramientas de inspeccion (systemctl cat,
systemd-delta, systemd-analyze verify).

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Ubicaciones y precedencia | /usr/lib vs /etc vs /run, contar, precedencia |
| 2 | Estructura interna | Secciones [Unit]/[Service]/[Install], directivas |
| 3 | Drop-ins y verificacion | Crear overrides, systemd-analyze verify |

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

~25 minutos.

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| Ninguno | — | — | — |

Este lab no crea recursos Docker.

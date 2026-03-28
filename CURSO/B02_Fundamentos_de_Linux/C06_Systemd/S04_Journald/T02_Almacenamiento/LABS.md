# Labs — Almacenamiento

## Descripcion

Laboratorio practico para configurar el almacenamiento del journal
de systemd: Storage= (volatile, persistent, auto, none), ubicaciones
(/var/log/journal vs /run/log/journal), limites de tamano (SystemMaxUse,
RuntimeMaxUse), rotacion, compresion, rate limiting, forwarding, seal,
y configuracion recomendada para servidores.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Storage y ubicaciones | volatile vs persistent vs auto, directorios |
| 2 | Limites y rotacion | SystemMaxUse, RuntimeMaxUse, vacuum, compress |
| 3 | Rate limiting y config | RateLimitBurst, forwarding, seal, servidores |

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

~20 minutos.

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| Ninguno | — | — | — |

Este lab no crea recursos Docker.

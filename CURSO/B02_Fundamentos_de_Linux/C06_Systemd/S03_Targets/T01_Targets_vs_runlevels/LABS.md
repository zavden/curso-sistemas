# Labs — Targets vs runlevels

## Descripcion

Laboratorio practico para entender la transicion de runlevels
(SysVinit) a targets (systemd): mapeo runlevel→target, targets
principales (poweroff, rescue, emergency, multi-user, graphical),
targets de sincronizacion (network, basic, sysinit), y las ventajas
del modelo de targets (paralelo, multiple, extensible).

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Runlevels y mapeo | Runlevels 0-6, aliases, equivalencias |
| 2 | Targets principales | rescue, emergency, multi-user, graphical |
| 3 | Targets de sincronizacion | Cadena de boot, network vs online |

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

# Labs — Crear una unidad de servicio custom

## Descripcion

Laboratorio practico para crear servicios systemd: anatomia completa
de un .service, Type= (simple, forking, notify, oneshot, exec),
comandos de ejecucion (ExecStart, ExecStartPre, ExecReload, ExecStop),
usuario/grupo, variables de entorno, EnvironmentFile, hardening de
seguridad, y verificacion con systemd-analyze.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Type y ExecStart | simple, oneshot, forking, notify, prefijos |
| 2 | Entorno y directorios | User, Environment, EnvironmentFile, dirs |
| 3 | Hardening y verificacion | ProtectSystem, PrivateTmp, analyze security |

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

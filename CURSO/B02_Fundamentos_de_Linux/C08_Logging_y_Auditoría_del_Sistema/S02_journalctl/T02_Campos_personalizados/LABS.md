# Labs — Campos personalizados

## Descripcion

Laboratorio practico para campos del journal en systemd: campos
del sistema (prefijo _, confiables), campos de usuario (sin prefijo),
campos internos (__CURSOR), SYSLOG_IDENTIFIER vs _COMM, MESSAGE_ID
(UUIDs estandarizados), campos personalizados desde systemd-cat y
la API sd_journal, _TRANSPORT, exploracion de campos, y
SyslogIdentifier/LogExtraFields en unit files.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Tipos de campos | Sistema (_), usuario, internos (__), transports |
| 2 | Identifiers y MESSAGE_ID | SYSLOG_IDENTIFIER, _COMM, UUIDs estandar |
| 3 | Campos custom y exploracion | systemd-cat, LogExtraFields, exploracion |

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

~20 minutos.

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| Ninguno | — | — | — |

Este lab no crea recursos Docker.

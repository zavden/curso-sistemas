# Labs — Integración con rsyslog

## Descripcion

Laboratorio practico para entender la coexistencia de journald y
rsyslog: flujo de logs, ForwardToSyslog, imuxsock vs imjournal,
archivos de log por distribucion (Debian vs RHEL), comparacion
entre journalctl y archivos de texto, escenarios de configuracion
(journal-only, rsyslog-only, ambos), y reenvio remoto.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Coexistencia y flujo | ForwardToSyslog, imuxsock vs imjournal, archivos |
| 2 | journalctl vs texto | Comparacion de capacidades, ventajas de cada uno |
| 3 | Escenarios y reenvio | journal-only, rsyslog-only, ambos, forwarding remoto |

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

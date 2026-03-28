# Labs — Forward a syslog

## Descripcion

Laboratorio practico para el forwarding journald→rsyslog: los dos
mecanismos (ForwardToSyslog con imuxsock vs imjournal), configurar
ForwardToSyslog, que campos se pierden en el forwarding, imjournal
y acceso a campos $!, problemas comunes (duplicados, mensajes
perdidos, rate limiting), verificar el flujo completo, escenarios
de configuracion (solo journal, solo rsyslog, ambos), y migracion.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Mecanismos de forwarding | ForwardToSyslog, imjournal, flujo |
| 2 | Problemas y diagnostico | Duplicados, perdidos, rate limiting |
| 3 | Escenarios y comparacion | Solo journal, solo rsyslog, ambos |

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

# Labs — Configuración de rsyslog

## Descripcion

Laboratorio practico para entender rsyslog: el modelo syslog
(facility, priority, action), la configuracion en /etc/rsyslog.conf,
modulos de input y output, reglas de enrutamiento (facility.priority),
operadores de prioridad (.= .! .none), drop-ins en /etc/rsyslog.d/,
el guion (-) para escritura buffered, logger para enviar mensajes,
y diferencias Debian vs RHEL.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Modelo syslog y config | Facility, priority, action, rsyslog.conf |
| 2 | Reglas y operadores | Enrutamiento, .= .none, drop-ins, guion |
| 3 | logger y diagnostico | Enviar mensajes, verificar, Debian vs RHEL |

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

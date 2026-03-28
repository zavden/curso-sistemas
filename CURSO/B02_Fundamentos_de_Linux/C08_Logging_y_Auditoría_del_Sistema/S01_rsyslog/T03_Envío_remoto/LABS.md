# Labs — Envío remoto

## Descripcion

Laboratorio practico para envio remoto de logs con rsyslog:
por que centralizar logs, TCP vs UDP vs RELP, configuracion del
cliente (UDP, TCP, TCP con buffering/queue), configuracion del
servidor receptor (imtcp, imudp), separar logs por host (dynaFile),
seguridad con TLS (certificados, puerto 6514), RELP para entrega
garantizada, firewall, verificacion del envio, y tabla comparativa
de protocolos.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Protocolos y cliente | UDP, TCP, buffering, multiples servidores |
| 2 | Servidor y separacion | Receptor, dynaFile por host, por IP |
| 3 | Seguridad y verificacion | TLS, RELP, firewall, diagnostico |

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

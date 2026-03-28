# Labs — Sintaxis crontab

## Descripcion

Laboratorio practico para dominar la sintaxis de crontab: los 5 campos
(minuto, hora, dia del mes, mes, dia de la semana), caracteres especiales
(*, coma, guion, /), la regla OR entre dia del mes y dia de la semana,
cadenas especiales (@daily, @reboot), gestion con crontab -e/-l/-r,
salida y redireccion, y errores comunes (%, PATH, permisos).

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Los 5 campos y caracteres | *, coma, guion, /, combinaciones, OR |
| 2 | Cadenas especiales y gestion | @daily/@reboot, crontab -e/-l/-r |
| 3 | Salida y errores comunes | Redireccion, %, PATH, permisos |

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

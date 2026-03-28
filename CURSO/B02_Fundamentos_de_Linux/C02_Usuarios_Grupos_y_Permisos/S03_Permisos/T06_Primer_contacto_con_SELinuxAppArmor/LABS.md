# Labs — Primer contacto con SELinux/AppArmor

## Descripcion

Laboratorio practico para un primer contacto con MAC (Mandatory
Access Control): verificar el estado de SELinux en AlmaLinux y
AppArmor en Debian, inspeccionar contextos y perfiles, y
entender cuando MAC puede bloquear algo.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | DAC vs MAC | Concepto, por que existe, como se complementan |
| 2 | SELinux (AlmaLinux) | getenforce, contextos, -Z |
| 3 | AppArmor (Debian) | aa-status, perfiles, logs |

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

~10 minutos.

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| Ninguno | — | — | — |

Este lab no crea recursos Docker.

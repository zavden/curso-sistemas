# Labs — Usuarios de sistema vs regulares

## Descripcion

Laboratorio practico para clasificar usuarios por tipo, crear
usuarios de sistema con useradd -r, y entender la diferencia entre
nologin y /bin/false. Incluye una auditoria basica de seguridad.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Clasificar usuarios | Rangos de UID, contar por tipo |
| 2 | Crear usuario de sistema | useradd -r, nologin, UID bajo |
| 3 | Seguridad y comparacion | nologin vs false, auditoria, Debian vs RHEL |

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

~15 minutos.

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| Ninguno | — | — | — |

Este lab no crea recursos Docker.

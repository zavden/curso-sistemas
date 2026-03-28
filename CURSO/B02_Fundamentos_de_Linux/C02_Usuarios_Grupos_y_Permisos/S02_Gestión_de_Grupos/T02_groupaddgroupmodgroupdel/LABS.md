# Labs — groupadd, groupmod, groupdel

## Descripcion

Laboratorio practico para el ciclo de vida completo de grupos:
crear, modificar, eliminar. Incluye directorios compartidos con
SGID y la herramienta gpasswd.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | groupadd y groupmod | Crear grupos, renombrar, cambiar GID |
| 2 | Directorio compartido | SGID, permisos de grupo, herencia |
| 3 | groupdel y gpasswd | Eliminar, restricciones, gestion de miembros |

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

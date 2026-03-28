# Labs — Archivos fundamentales

## Descripcion

Laboratorio practico para inspeccionar los cuatro archivos de identidad
de Linux: /etc/passwd, /etc/shadow, /etc/group y /etc/gshadow. Interpretar
campos, comparar entre Debian y AlmaLinux, y usar herramientas de consulta.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | /etc/passwd | Campos, UIDs, shells, campo GECOS |
| 2 | /etc/shadow | Hashes, algoritmos, fechas, estados |
| 3 | /etc/group y herramientas | Grupos, miembros, getent, id, consistencia |

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

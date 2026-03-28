# Labs — rpm

## Descripcion

Laboratorio practico para dominar rpm como herramienta de bajo nivel:
anatomia de nombres RPM, consultas con rpm -q (info, archivos, dueno,
dependencias), verificacion de integridad (rpm -V), scripts del
paquete, y comparacion con dpkg.

## Prerequisitos

- Entorno del curso levantado (alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Consultas rpm -q | -qi, -ql, -qf, -qR, -qc, contar |
| 2 | Verificar integridad | rpm -V, codigos S5T, config vs binario |
| 3 | Scripts y comparacion | --scripts, --triggers, rpm vs dpkg |

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

~20 minutos.

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| Ninguno | — | — | — |

Este lab no crea recursos Docker.

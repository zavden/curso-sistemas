# Labs — Dependencias

## Descripcion

Laboratorio practico para dominar las dependencias de systemd:
los dos ejes independientes (activacion y ordenamiento), Wants
vs Requires vs BindsTo vs Requisite, After/Before, PartOf,
Conflicts, targets como puntos de sincronizacion, dependencias
implicitas (DefaultDependencies), y deteccion de errores.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Activacion y ordenamiento | Wants, Requires, After, Before, PartOf |
| 2 | Targets y sincronizacion | network.target vs online, cadena de boot |
| 3 | Verificacion y errores | verify, dependency loops, errores comunes |

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

~25 minutos.

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| Ninguno | — | — | — |

Este lab no crea recursos Docker.

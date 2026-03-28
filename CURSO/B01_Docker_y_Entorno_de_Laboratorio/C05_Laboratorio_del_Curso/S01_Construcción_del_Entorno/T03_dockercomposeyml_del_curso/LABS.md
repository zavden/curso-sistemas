# Labs — docker-compose.yml del curso

## Descripcion

Laboratorio practico para construir el compose.yml del laboratorio del
curso, entender cada opcion de configuracion, y verificar que los
volumenes compartidos, bind mounts y SYS_PTRACE funcionan correctamente.

## Prerequisitos

- Docker y Docker Compose instalados
- Conexion a internet (para el primer build)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Examinar el compose.yml | Cada opcion y su proposito |
| 2 | Construir y levantar | Build, up, ps, logs |
| 3 | Volumenes y bind mounts | Datos compartidos, persistencia |
| 4 | Flujo de trabajo diario | exec, compilar, comunicacion entre contenedores |

## Archivos

```
labs/
├── README.md                      ← Guia paso a paso
├── compose.yml                    ← Compose file del laboratorio
├── dockerfiles/
│   ├── debian/
│   │   └── Dockerfile             ← Imagen Debian dev
│   └── alma/
│       └── Dockerfile             ← Imagen AlmaLinux dev
└── src/
    └── hello.c                    ← Programa de prueba
```

## Como ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte.

## Tiempo estimado

~25 minutos (sin contar el tiempo de build de imagenes).

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| `debian-dev` | Contenedor | 2 | Si |
| `alma-dev` | Contenedor | 2 | Si |
| Imagenes de build | Imagen | 2 | Si |
| `labs_workspace` | Volumen | 2 | Si |

Ningun recurso del lab persiste despues de completar la limpieza.

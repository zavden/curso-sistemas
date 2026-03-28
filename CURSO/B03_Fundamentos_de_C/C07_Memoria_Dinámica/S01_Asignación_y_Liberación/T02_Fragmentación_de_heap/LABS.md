# Labs -- Fragmentacion de heap

## Descripcion

Laboratorio para observar y analizar la fragmentacion del heap: como se
fragmenta la memoria tras ciclos de malloc/free, la diferencia entre
fragmentacion externa e interna, y estrategias practicas para mitigarla.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Visualizar fragmentacion | Alocar/liberar alternando, imprimir direcciones para ver huecos |
| 2 | Fragmentacion externa | Huecos entre bloques impiden alocar un bloque grande contiguo |
| 3 | Fragmentacion interna | malloc_usable_size revela el overhead del allocator por bloque |
| 4 | Estrategias de mitigacion | Tamanos uniformes, pool allocator, batch allocation, realloc |

## Archivos

```
labs/
├── README.md          ← Ejercicios paso a paso
├── frag_visual.c      ← Alocar/liberar alternando para ver direcciones
├── frag_external.c    ← Demostrar fragmentacion externa con huecos
├── frag_internal.c    ← Medir overhead con malloc_usable_size
└── frag_mitigation.c  ← Estrategias: pool, batch, realloc
```

## Como ejecutar

```bash
cd labs/
# Seguir las instrucciones del README.md
```

## Tiempo estimado

~25 minutos

## Recursos creados

| Recurso | Tipo | Se elimina al final |
|---------|------|---------------------|
| frag_visual | Ejecutable compilado | Si (limpieza final) |
| frag_external | Ejecutable compilado | Si (limpieza final) |
| frag_internal | Ejecutable compilado | Si (limpieza final) |
| frag_mitigation | Ejecutable compilado | Si (limpieza final) |

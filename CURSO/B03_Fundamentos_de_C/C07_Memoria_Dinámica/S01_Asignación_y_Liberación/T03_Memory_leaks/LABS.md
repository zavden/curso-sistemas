# Labs — Memory leaks

## Descripcion

Laboratorio para provocar, detectar y corregir memory leaks en C. Se usan
programas con leaks intencionales, se analizan con Valgrind para entender las
categorias de leaks, y se aplican las correcciones verificando que Valgrind
reporte cero perdidas.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Valgrind instalado (`valgrind --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Tipos de leaks | Leak simple, leak por reasignacion, leak en error path |
| 2 | Categorias de Valgrind | definitely lost, indirectly lost, still reachable |
| 3 | Leak en programa real | Funcion que aloca y retorna, cadena de ownership |
| 4 | Corregir leaks | Version arreglada de cada caso, verificar con Valgrind |

## Archivos

```
labs/
├── README.md              ← Ejercicios paso a paso
├── simple_leaks.c         ← Tres leaks clasicos (simple, reasignacion, error path)
├── valgrind_categories.c  ← Leaks que producen cada categoria de Valgrind
├── ownership.c            ← Leaks por ownership incorrecto
└── fixed_leaks.c          ← Versiones corregidas de todos los leaks
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
| simple_leaks | Ejecutable | Si (limpieza final) |
| valgrind_categories | Ejecutable | Si (limpieza final) |
| ownership | Ejecutable | Si (limpieza final) |
| fixed_leaks | Ejecutable | Si (limpieza final) |

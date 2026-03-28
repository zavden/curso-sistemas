# PLAN — Curso Linux-C-Rust

## Resumen

Curso profesional de Linux usando C, Rust y Docker. Cubre desde fundamentos hasta nivel
intermedio-avanzado equivalente a LPIC-2 y RHCSA, con preparación para WebAssembly.
Todo el contenido práctico se ejecuta en contenedores Docker sobre Debian/Ubuntu y
Alma/Rocky/Fedora, salvo los bloques de GUI y multimedia que requieren acceso al display
del host.

---

## Prerrequisitos

- Bash y CLI básico (navegación, redirección, pipes, permisos básicos)
- Docker básico (montar contenedores simples vía CLI)

---

## Convención de Carpetas

```
BXX_Nombre_Bloque/
  CXX_Nombre_Capitulo/
    SXX_Nombre_Seccion/
      TXX_Nombre_Topico/
        README.md          # Teoría y explicaciones (español)
        *.c / *.rs         # Código fuente (inglés)
        Makefile / Cargo.toml  # Si aplica
        Dockerfile         # Si aplica
        docker-compose.yml # Si aplica
```

Cada tópico es autocontenido: su carpeta tiene todo lo necesario para estudiarse
de forma independiente.

---

## Tipos de Capítulo

- **[A]** — Aislado: una sola tecnología, progresivo.
- **[M]** — Multi-temático: requiere conocimiento previo de capítulos aislados.
- **[P]** — Proyecto: aplicación práctica, puede mezclar tecnologías.

---

## Imágenes Docker

| Imagen | Uso |
|---|---|
| `debian:bookworm` | Familia Debian para prácticas de administración |
| `almalinux:9` | Familia RHEL para prácticas de administración |
| Custom `dev-debian` | Debian + gcc, make, cmake, gdb, valgrind, rustup |
| Custom `dev-alma` | Alma + gcc, make, cmake, gdb, valgrind, rustup |

Se construirán imágenes custom con Dockerfile multi-stage para minimizar espacio
en disco. Los contenedores se reutilizan con volúmenes montados.

---

## Dependencias entre Bloques

```
B01 (Docker) ──────────────────────── se usa transversalmente
 │
B02 (Linux Fundamentos) ──┬── B08 (Almacenamiento)
 │                         ├── B09 (Redes) ──── B10 (Servicios)
 │                         └── B11 (Seguridad/Kernel)
 │
 ├── B03 (C) ── B04 (Build Systems) ── B06 (Sistemas en C) ──┐
 │                                                             │
 └── B05 (Rust) ── B07 (Sistemas en Rust) ──┬── B12 (GUI) ── B13 (Multimedia)
                                            │                 │
                                            └── B14 (Interop/WASM) ◄──┘
```

Lectura: una flecha A → B significa "A es prerrequisito de B".

---

## Bloques

| # | Archivo | Bloque |
|---|---------|--------|
| 01 | [BLOQUE_01.md](BLOQUE_01.md) | Docker y Entorno de Laboratorio |
| 02 | [BLOQUE_02.md](BLOQUE_02.md) | Fundamentos de Linux |
| 03 | [BLOQUE_03.md](BLOQUE_03.md) | Fundamentos de C |
| 04 | [BLOQUE_04.md](BLOQUE_04.md) | Sistemas de Compilación |
| 05 | [BLOQUE_05.md](BLOQUE_05.md) | Fundamentos de Rust |
| 06 | [BLOQUE_06.md](BLOQUE_06.md) | Programación de Sistemas en C |
| 07 | [BLOQUE_07.md](BLOQUE_07.md) | Programación de Sistemas en Rust |
| 08 | [BLOQUE_08.md](BLOQUE_08.md) | Almacenamiento y Sistemas de Archivos |
| 09 | [BLOQUE_09.md](BLOQUE_09.md) | Redes |
| 10 | [BLOQUE_10.md](BLOQUE_10.md) | Servicios de Red |
| 11 | [BLOQUE_11.md](BLOQUE_11.md) | Seguridad, Kernel y Arranque |
| 12 | [BLOQUE_12.md](BLOQUE_12.md) | GUI con Rust (egui) — Fases 1, 2 y 3 |
| 13 | [BLOQUE_13.md](BLOQUE_13.md) | Multimedia con GStreamer — Fases 4 y 5 |
| 14 | [BLOQUE_14.md](BLOQUE_14.md) | Interoperabilidad C/Rust y WebAssembly |

---

## Resumen de Bloques

| # | Bloque | Capítulos | Tipo principal |
|---|--------|-----------|----------------|
| 01 | Docker y Entorno de Laboratorio | 5 | Aislado + Multi |
| 02 | Fundamentos de Linux | 9 | Aislado |
| 03 | Fundamentos de C | 11 | Aislado |
| 04 | Sistemas de Compilación | 3 | Aislado |
| 05 | Fundamentos de Rust | 11 | Aislado |
| 06 | Programación de Sistemas en C | 8 | Aislado + Proyecto |
| 07 | Programación de Sistemas en Rust | 9 | Aislado + Multi + Proyecto |
| 08 | Almacenamiento y Sistemas de Archivos | 5 | Aislado |
| 09 | Redes | 7 | Aislado + Multi |
| 10 | Servicios de Red | 5 | Aislado |
| 11 | Seguridad, Kernel y Arranque | 7 | Aislado + Proyecto |
| 12 | GUI con Rust (egui) | 10 | Aislado + Proyecto |
| 13 | Multimedia con GStreamer | 5 | Aislado + Proyecto |
| 14 | Interoperabilidad C/Rust y WebAssembly | 4 | Multi + Proyecto |
| | **Total** | **99 capítulos** | |

---

## Cobertura de Certificaciones

### RHCSA (EX200)
- Herramientas esenciales → B02
- Shell scripting → B02 (C05)
- Sistemas en ejecución → B02 (C03, C06), B11 (C04)
- Almacenamiento local → B08
- Sistemas de archivos → B08
- Despliegue y mantenimiento → B02 (C04, C06), B10
- Networking básico → B09
- Usuarios y grupos → B02 (C02)
- Seguridad → B09 (C05), B11 (C01, C03)

### LPIC-2 (201 + 202)
- Capacity Planning → B02 (C09)
- Linux Kernel → B11 (C05)
- System Startup → B11 (C04)
- Filesystem and Devices → B02 (C01), B08
- Advanced Storage → B08 (C03, C04)
- Network Configuration → B09
- System Maintenance → B11 (C06)
- DNS → B09 (C02), B10 (C02)
- HTTP Services → B10 (C03)
- File Sharing → B10 (C04)
- Network Client Management → B10 (C05)
- E-Mail Services → B10 (C05)
- System Security → B11 (C01, C02, C03)

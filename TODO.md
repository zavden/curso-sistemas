# Curso Linux-C-Rust

Objetivo: Hacer un curso de Linux usando C, Rust y Docker.
Conocimientos previos: Bash (y CLI) básico, Docker básico (solo montar contenedores sencillos via CLI).
Niveles: De básico hasta intermedio-avanzado equivalente a un LPIC-2 y RHCSA para conseguir trabajo de TI y tener los conocimientos previos para WebAssembly
Nota: No es necesario estudiar Shaders ni Frontend específico, solo tecnologías web previas para WebAssembly y necesarias para LPIC-2 y RHCSA.

Puntos importantes
- El curso debe incluir una introducción de Docker para poder montar los contenedores.
- No abusar del espacio en disco, descargar sólo las imágenes necesarias y reutilizar contenedores.
- Incluir también una sección de Redes.

El curso se definirá por:

    Bloques -> Capítulos -> Secciones -> Tópicos.

Especificaciones:


* Cada Bloque es un tema completo, independiente, que al terminarse debe dominarse tanto para Debian/Ubuntu como para Alma/Rocky/Fedora.

* Existirán 3 tipos de capítulos:
    - Los que contedrán una sola teconología, ya sea Bash, Docker, Make, CMake, Protocolos Web, Redes, C, Rust, etc. es decir, será aislado y progresivo.
    - Los que necesitan del conocimiento previo (que era aislado) para entender un tema multi-temático, por ejemplo, Redes, que necesita saber de CLI, Web, etc.
    - Proyectos complejos: Aquí se verá una aplicación práctica del contendo que puede o no mezclar tópicos independientes.

* Secciones: Temas específicos (punteros, máscaras de recorte, etc), de una sóla tecnología.
* Tópicos: Información específica de la sección ordenada de forma progresiva, puede ser teoría, ejemplos prácticos o mini-proyectos

Ejemplo ilustrativo (no real):

* Bloque: C/Rust básico
    * Capítulo 1: Elementos básicos en C
        * Sección 1: Imprimir en pantalla
            * Tópico 1: Hola mundo
            * Tópico 2: Tipos de datos
            * Tópico 3: Casteo de variables
            ...
        * Sección 2: Condicionales
            ...
    * Capítulo 2: Elementos básicos en Rust
        * Sección 1: Imprimir en pantalla
            * Tópico 1: Hola mundo
            * Tópico 2: Tipos de datos
            * Tópico 3: Casteo de variables
            ...
        * Sección 2: Condicionales
            ...
    * Capítulo N-ésimo : Programas multi-lenguaje (C/Rust) con bibliotecas dinámicas
            ...
    * Capítulo M-ésimo : Proyecto 1: Editor de texto con GUI en C
    ...

La estructura de carpetas del curso debe de ser equivalente al de "Bloques -> Capítulos -> Secciones -> Tópicos", cada tópico debe de ir en una carpeta separada con sus archivos independientes. 


Deberás incluir los siguientes proyectos donde mejor corresponda en el curso:

## Fase 1 — GUI Básica (Formularios y Layout)

El objetivo no es hacer apps útiles, sino aprender a colocar widgets y responder a input.

### Calculadoras

1. App que sume dos números con dos campos de texto y un botón.
   - Mejora 1: Muestra el resultado en tiempo real (sin botón, reacciona al escribir).
   - Mejora 2: Menú desplegable para elegir la operación (+, -, ×, ÷).
   - Mejora 3: Historial de operaciones en una lista debajo.
2. Calculadora regular con teclado numérico en pantalla.
   - Mejora 1: Responde también al teclado físico.
   - Mejora 2: Historial de cálculos (últimas 10 operaciones).
3. ⚠ Calculadora científica (sin, cos, tan, √, log, ^).
   - Nota: los botones extra son fáciles; el reto es el **evaluador de expresiones**
     (`2 + 3 * 4` debe respetar precedencia). Considera usar la crate `meval` para
     una primera versión, y después implementarlo tú mismo.
   - Mejora 1: Historial con expresiones completas (`"2 + 3 * 4 = 14"`).
   - Mejora 2: Conversor a notación científica (`1234567 → 1.234567 × 10⁶`).

### ★ Reloj y temporizadores

*(Enseña el repaint loop continuo — concepto clave para video después)*

1. ★ Reloj digital que muestra la hora actual y se actualiza cada segundo.
   - Mejora 1: Añade fecha debajo.
   - Mejora 2: Reloj analógico con agujas dibujadas con Painter (trigonometría básica).
2. ★ Cronómetro con botones Start / Stop / Reset y display `00:00.000`.
   - Mejora 1: Laps: botón "Lap" guarda el tiempo parcial en una lista.
   - Mejora 2: Cuenta regresiva: el usuario escribe un tiempo y el cronómetro
     cuenta hacia atrás. Alerta visual al llegar a 0.

---

## Fase 2 — Texto y Datos

### Editores de texto

1. Editor de texto que no guarda, solo abre, escribes y cierras.
   - Mejora 1: Seleccionar texto y copiarlo al portapapeles.
   - Mejora 2: Guardar y abrir archivos de texto.
   - Mejora 3: Cambiar fuente y tamaño.
   - Mejora 4: Buscar texto (Ctrl+F resalta todas las ocurrencias).
   - ⚠ Mejora 5 (VI mode): requiere implementar un parser de comandos modal completo.
     No es una "mejora" — es un proyecto independiente de semanas. Omítela aquí
     y vuélvela si terminas el roadmap y quieres un desafío largo.
2. ★ Editor de Markdown con preview en tiempo real.
   - Usa la crate `pulldown-cmark` para parsear Markdown a HTML.
   - Panel izquierdo: texto plano. Panel derecho: preview renderizado con egui.
   - Mejora 1: Botones de formato (negrita, cursiva, código) que insertan la sintaxis.
   - Mejora 2: Exportar a HTML.

### ★ Lista de tareas GUI

*(Puente entre "solo input" de calculadoras y "gestión de estado" de editores)*

1. ★ Lista de tareas con checkbox, campo para añadir tarea y botón delete por ítem.
   - Mejora 1: Filtros: "Todas", "Pendientes", "Completadas".
   - Mejora 2: Reordenar con drag and drop.
   - Mejora 3: Guardar en JSON al cerrar y cargar al abrir.
   - Mejora 4: Múltiples listas (tabs en la parte superior).

### Gestión tabular

1. ★ Tabla editable simple: muestra datos en filas/columnas. Solo texto, sin fórmulas.
   Click en celda para editar. Columnas de ancho fijo.
   - Mejora 1: Añadir y eliminar filas.
   - Mejora 2: Ordenar por columna al hacer click en el header.
   - Mejora 3: Exportar a CSV.
   - Mejora 4: Importar desde CSV.
2. ⚠ Editor tipo Excel con fórmulas (`=SUMA(A1:A3)`).
   - Requiere: parser de expresiones, referencias de celda, evaluador recursivo,
     detección de dependencias circulares. Complejidad comparable al editor de video.
   - Mejor hacerlo **después** del editor de video, cuando ya tienes experiencia con
     state management complejo.
   - Mejora 1: Incrementar/decrementar columnas sin perder info.
   - Mejora 2: Formato: negrita, color de fondo, alineación por celda.
   - Mejora 3: Redimensionar columnas/filas arrastrando el separador.

---

## Fase 3 — Imágenes y Dibujo

### ★ Dibujo libre (antes de cargar imágenes)

*(Enseña Painter sin la complejidad de cargar/guardar archivos)*

1. ★ Lienzo en blanco donde puedes dibujar con el mouse.
   - El mouse dragged pinta píxeles en el canvas.
   - Mejora 1: Selector de color con sliders RGB.
   - Mejora 2: Selector de tamaño del pincel.
   - Mejora 3: Herramienta de borrar.
   - Mejora 4: Guardar el dibujo como PNG.
2. ★ Herramientas geométricas sobre el lienzo:
   - Herramienta línea: click inicio, drag hasta fin.
   - Herramienta rectángulo: click+drag.
   - Herramienta círculo: click+drag define el radio.
   - Mejora 1: Relleno sólido vs solo borde.
   - Mejora 2: Deshacer (Ctrl+Z) con historial de operaciones.

### Apps de imágenes

1. App que carga imágenes y las muestra ajustándose al tamaño de la ventana.
   - Mejora 1: Zoom in/out con scroll del mouse (mantener aspect ratio).
   - Mejora 2: Pan: drag para mover la imagen cuando está más grande que la ventana.
   - Mejora 3: Seleccionar una región rectangular y exportar esa parte como PNG.
   - Mejora 4: Añadir texto posicionable (como el Proyecto 4 del PRE_ROADMAP).
   - Mejora 5: Dibujar rectángulos y flechas sobre la imagen (como el Proyecto 5).
2. ★ Visor de imágenes con navegación: abre una carpeta y puedes navegar
   entre imágenes con flechas ←/→. Muestra nombre y dimensiones en la barra inferior.
   - Mejora 1: Miniaturas de todas las imágenes de la carpeta en un panel lateral.
   - Mejora 2: Buscar imágenes por nombre en la barra de búsqueda.
3. ★ Color picker: haz click en cualquier punto de una imagen cargada y muestra
   los valores RGB, HSL y HEX del color bajo el cursor.
   - Mejora 1: Copia el HEX al portapapeles al hacer click.
   - Mejora 2: Paleta de colores recientes (últimos 16).
   - Mejora 3: Herramienta de cuentagotas que puede muestrear colores fuera de la
     app (captura de pantalla + coordenadas del mouse global). Usa la crate `screenshots`.

---

## Fase 4 — Audio

*(Puente entre imágenes y video. GStreamer sin la complejidad de frames de video)*

### ★ Reproductor de audio

1. ★ Reproductor de audio básico: abre un MP3/WAV/FLAC y reproduce con play/pause.
   - Solo un botón. Sin barra de tiempo, sin nada extra.
   - Enseña: GStreamer con `playbin`, channels de estado, `is_playing`, egui básico.
   - Diferencia clave con video: no hay frames que decodificar ni texturas que subir
     a la GPU. Mucho más simple.
   - Mejora 1: Barra de progreso que muestra posición / duración (no interactiva, solo visual).
   - Mejora 2: Barra interactiva — click para saltar.
   - Mejora 3: Control de volumen con slider.
   - Mejora 4: Muestra metadatos del archivo (título, artista, álbum) usando
     GStreamer Discoverer o la crate `id3`.
2. ★ Reproductor con lista de reproducción:
   - Lista de archivos en el panel izquierdo.
   - Click para reproducir, doble-click para reproducir inmediatamente.
   - Botones: anterior, siguiente, repetir (uno/todo/apagado), mezclar.
   - Mejora 1: Arrastra archivos o carpetas a la lista.
   - Mejora 2: Persiste la lista de reproducción entre sesiones.
   - Mejora 3: Visualizador de forma de onda básico (requiere acceso a muestras de audio
     con AppSink — primer contacto real con el pipeline de audio).

---

## Fase 5 — Video

### Reproductores de video

1. Reproductor básico: play y pause. Sin barra de tiempo.
   - Enseña: AppSink, frames RGBA, texturas GPU, `new_preroll` vs `new_sample`.
   - Consulta: `PRE_ROADMAP.md` — Proyecto 1.
   - Mejora 1: Drag & drop de archivos.
   - Mejora 2: Volumen con slider.
2. Reproductor con timeline interactiva.
   - Consulta: `PRE_ROADMAP.md` — Proyecto 2.
   - Mejora 1: Frame stepping (Shift+← / Shift+→).
   - Mejora 2: Velocidad de reproducción (0.5×, 1×, 2×).
   - Mejora 3: Miniaturas en la timeline al hacer hover.
3. ★ Reproductor con capítulos / marcadores:
   - El usuario puede crear marcadores con nombre en cualquier posición del video.
   - Lista de marcadores a la derecha; click para saltar.
   - Mejora 1: Exportar marcadores como archivo `.txt` con timestamps.
   - Mejora 2: Leer marcadores incrustados en el archivo de video (MKV chapters).

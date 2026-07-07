# Avisos de Terceros (Third-Party Notices)

ProyecThor es software libre bajo licencia MIT (ver `LICENSE`), pero **enlaza
y distribuye SDKs y librerías de terceros que mantienen sus propias
licencias**. Estas licencias son independientes de la de ProyecThor y deben
leerse y respetarse por separado, tanto por el equipo de desarrollo como por
cualquier persona que compile, modifique o redistribuya el proyecto.

> Este documento es informativo y no constituye asesoría legal. Ante
> cualquier duda sobre cómo aplican estas licencias a un caso concreto
> (por ejemplo, distribución comercial, forks, o builds propias), consulta
> con un profesional legal.

---

## Resumen por librería

| Librería | Licencia | Tipo | Enlace | Obligaciones relevantes |
| :--- | :--- | :--- | :--- | :--- |
| **Dear ImGui** | MIT | Permisiva | [github.com/ocornut/imgui](https://github.com/ocornut/imgui) | Mantener aviso de copyright. |
| **GLFW** | zlib/libpng | Permisiva | [glfw.org](https://www.glfw.org) | Mantener aviso de copyright. |
| **GLEW** | MIT / BSD | Permisiva | [glew.sourceforge.net](http://glew.sourceforge.net) | Mantener aviso de copyright. |
| **GLM** | MIT | Permisiva | [github.com/g-truc/glm](https://github.com/g-truc/glm) | Mantener aviso de copyright. |
| **nlohmann/json** | MIT | Permisiva | [github.com/nlohmann/json](https://github.com/nlohmann/json) | Mantener aviso de copyright. |
| **stb_image** | Public Domain / MIT | Permisiva | [github.com/nothings/stb](https://github.com/nothings/stb) | Ninguna obligación práctica. |
| **PDFium** | BSD 3-Clause | Permisiva | [chromium.googlesource.com/.../pdfium](https://chromium.googlesource.com/chromium/src/+/main/third_party/pdfium) | Mantener aviso de copyright, no usar el nombre del proyecto para promoción sin permiso. |
| **LibVLC SDK** | LGPL 2.1 | Copyleft débil | [videolan.org](https://www.videolan.org) | Ver sección especial abajo. |
| **TagLib** | LGPL 2.1 / MPL 1.1 | Copyleft débil | [taglib.github.io](https://taglib.github.io) | Ver sección especial abajo. |

---

## Atención especial: librerías LGPL (LibVLC y TagLib)

A diferencia de las licencias permisivas de la tabla anterior, **LGPL 2.1
impone condiciones adicionales** cuando una librería se enlaza (sobre todo
de forma estática) dentro de un ejecutable con otra licencia:

- Debe quedar disponible el **código fuente** de la librería LGPL usada
  (o al menos un enlace claro a la versión exacta), incluso si el resto
  del proyecto no es LGPL.
- Si el enlace es **estático** (como es el caso de TagLib en ProyecThor),
  la LGPL exige que el usuario final pueda **volver a enlazar** una versión
  modificada de la librería con el ejecutable — típicamente proporcionando
  los archivos objeto (`.o`) o un mecanismo equivalente para relinking.
- Si el enlace es **dinámico** (como LibVLC, distribuido como `.dll`/`.so`
  separado), esta obligación normalmente se cumple con solo mantener la
  librería como archivo aparte, reemplazable por el usuario.

**Antes de tocar, actualizar o volver a empaquetar estas dos dependencias**,
cualquier colaborador debe leer el texto completo de la LGPL 2.1 y confirmar
que el método de build actual sigue cumpliendo estas condiciones. No asumas
que el enlace estático actual de TagLib es válido sin revisar esto primero;
si tienes dudas, consúltalo con el equipo antes de hacer merge de cambios
relacionados con el sistema de build de estas librerías.

Texto completo de la LGPL 2.1: https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html

---

## Regla general para contribuidores

Si tu contribución agrega, actualiza o reemplaza cualquier SDK o librería de
terceros:

1. Identifica la licencia exacta de la nueva versión (las licencias pueden
   cambiar entre versiones de una misma librería).
2. Verifica que sea compatible con la distribución actual de ProyecThor.
3. Actualiza este archivo (`THIRD-PARTY-NOTICES.md`) con la entrada
   correspondiente.
4. Si la licencia es copyleft (LGPL, GPL, MPL, etc.), coméntalo con el
   equipo de desarrollo en Discord antes de integrarla, siguiendo el mismo
   proceso de presentación de cambios descrito en el README.

Mantener este archivo actualizado no es opcional: protege al proyecto y a
quien lo usa o redistribuye.
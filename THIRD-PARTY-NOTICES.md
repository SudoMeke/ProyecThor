# Third-Party Notices

ProyecThor's own source code is released under the MIT License (see `LICENSE`), but the project **links against and distributes third-party SDKs and libraries that carry their own licenses**. These licenses are independent from ProyecThor's own license and must be read and respected separately, both by the development team and by anyone who builds, modifies, or redistributes the project.

> This document is informational and does not constitute legal advice. If you have questions about how these licenses apply to a specific case (for example, commercial distribution, forks, or custom builds), consult a legal professional.

---

## Summary by Library

| Library | License | Type | Link | Relevant Obligations |
| :--- | :--- | :--- | :--- | :--- |
| **Dear ImGui** | MIT | Permissive | [github.com/ocornut/imgui](https://github.com/ocornut/imgui) | Keep copyright notice. |
| **GLFW** | zlib/libpng | Permissive | [glfw.org](https://www.glfw.org) | Keep copyright notice. |
| **GLEW** | MIT / BSD | Permissive | [glew.sourceforge.net](http://glew.sourceforge.net) | Keep copyright notice. |
| **GLM** | MIT | Permissive | [github.com/g-truc/glm](https://github.com/g-truc/glm) | Keep copyright notice. |
| **nlohmann/json** | MIT | Permissive | [github.com/nlohmann/json](https://github.com/nlohmann/json) | Keep copyright notice. |
| **stb_image** | Public Domain / MIT | Permissive | [github.com/nothings/stb](https://github.com/nothings/stb) | No practical obligation. |
| **PDFium** | BSD 3-Clause | Permissive | [chromium.googlesource.com/.../pdfium](https://chromium.googlesource.com/chromium/src/+/main/third_party/pdfium) | Keep copyright notice; do not use the project's name for promotion without permission. |
| **LibVLC SDK** | LGPL 2.1 | Weak copyleft | [videolan.org](https://www.videolan.org) | See special section below. |
| **TagLib** | LGPL 2.1 / MPL 1.1 | Weak copyleft | [taglib.github.io](https://taglib.github.io) | See special section below. |

---

## Special Attention: LGPL Libraries (LibVLC and TagLib)

Unlike the permissive licenses in the table above, **LGPL 2.1 imposes additional conditions** when a library is linked (especially statically) into an executable under a different license:

- The **source code** of the LGPL library used must remain available (or at least a clear link to the exact version used), even if the rest of the project is not LGPL.
- If the linking is **static** (as is the case with TagLib in ProyecThor), the LGPL requires that the end user be able to **relink** a modified version of the library with the executable — typically by providing the object files (`.o`) or an equivalent relinking mechanism.
- If the linking is **dynamic** (as with LibVLC, distributed as a separate `.dll`/`.so`), this obligation is normally satisfied simply by keeping the library as a separate, replaceable file.

**Before touching, updating, or repackaging these two dependencies**, any contributor must read the full text of the LGPL 2.1 and confirm that the current build method still meets these conditions. Do not assume that the current static linking of TagLib is compliant without reviewing this first; if in doubt, check with the team before merging any changes related to the build system for these libraries.

Full text of the LGPL 2.1: https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html

---

## General Rule for Contributors

If your contribution adds, updates, or replaces any third-party SDK or library:

1. Identify the exact license of the new version (licenses can change between versions of the same library).
2. Verify that it is compatible with ProyecThor's current distribution.
3. Update this file (`THIRD-PARTY-NOTICES.md`) with the corresponding entry.
4. If the license is copyleft (LGPL, GPL, MPL, etc.), discuss it with the development team on Discord before integrating it, following the same change-proposal process described in the README.

Keeping this file up to date is not optional: it protects both the project and anyone who uses or redistributes it.

---

## Note on ProyecThor's Own License

This MIT license applies only to ProyecThor's own source code (the code written by the project team within this repository).

ProyecThor uses third-party SDKs and libraries (Dear ImGui, GLFW, GLEW, GLM, LibVLC, TagLib, PDFium, nlohmann/json, stb_image, among others) that keep their own licenses, independent of this one. Some of them — in particular LibVLC and TagLib, under LGPL 2.1 — impose additional conditions on how binaries linking against them can be distributed.

Before distributing, modifying, or redistributing any ProyecThor build, review the sections above.
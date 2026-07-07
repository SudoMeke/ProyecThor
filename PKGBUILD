# Maintainer: Tu Nombre <tu@email.com>
#
# USO:
#   1. Copia este archivo PKGBUILD a la RAIZ de tu proyecto (donde esta el
#      CMakeLists.txt principal), junto a la carpeta packaging/ completa.
#   2. Asegurate de haber agregado el snippet de
#      packaging/CMakeLists-install-snippet.cmake al final de tu
#      CMakeLists.txt.
#   3. Corre:
#         makepkg -si
#      Esto compila TODO desde el codigo fuente que ya tenes en la carpeta
#      (no descarga nada de internet salvo lo que tu propio CMake baja via
#      FetchContent: nlohmann/json y glm), arma el paquete .pkg.tar.zst, y
#      lo instala con pacman pidiendote la clave de sudo.
#   4. Una vez instalado, listo: se ejecuta con "proyecthor" desde cualquier
#      lado, o desde el menu de aplicaciones si agregaste el .desktop/icono.
#
# Si mas adelante subis el proyecto a un repo git y queres que la gente
# instale con "yay -S proyecthor" via AUR, cambia la seccion source()/build()
# para clonar desde tu URL real en vez de usar $startdir directamente.

pkgname=proyecthor
pkgver=0.3.2
pkgrel=1
pkgdesc="Reproductor multimedia con VLC + OpenGL + ImGui"
arch=('x86_64')
url="https://github.com/tuusuario/proyecthor"
license=('custom')
depends=('vlc' 'glfw-x11' 'mesa' 'glibc' 'gcc-libs')
optdepends=('yt-dlp: reproducir enlaces de YouTube directamente')
makedepends=('cmake' 'ninja' 'git' 'pkgconf')
options=('!lto')
source=()
sha256sums=()

build() {
    # Se compila directamente desde la carpeta del proyecto (donde vive
    # este PKGBUILD), sin descargar tarball: $startdir es esa carpeta.
    cmake -S "$startdir" -B "$startdir/build" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build "$startdir/build"
}

package() {
    DESTDIR="$pkgdir" cmake --install "$startdir/build"
}

# Maintainer: Marcus Ramse <marcus@ramse.se>
pkgname=alines-git
pkgdesc='Scriptable remote dynamic menu'
pkgver=master
pkgrel=1
arch=(x86_64)
source=('git+https://github.com/JerwuQu/alines')
sha256sums=('SKIP')
_extracted="${pkgname}-${pkgver}"

pkgver() {
	cd "${srcdir}/${pkgname%-git}"
	git rev-parse --short HEAD
}

build () {
	cd "${srcdir}/${pkgname%-git}"
	make
}

package () {
	cd "${srcdir}/${pkgname%-git}"
	mkdir -p "${pkgdir}/usr/local/bin"
	mv alines-server "${pkgdir}/usr/local/bin/alines-server"
	mv alines-menu "${pkgdir}/usr/local/bin/alines-menu"
}

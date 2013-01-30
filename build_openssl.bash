#! /bin/bash
platform="unknown"
os_str=`uname`

openssl_install="$(pwd)/ssl-install"
openssl_configure="./config"
openssl_flags=""

case $os_str in
Linux)
	platform="linux"
	openssl_flags="--with-zlib-include=/srv/zlib/include --with-zlib-lib=/srv/zlib/lib"
	;;
FreeBSD)
	platform="freebsd"
	;;
Darwin)
	platform="darwin"
	openssl_configure="./Configure darwin64-x86_64-cc"
	;;
esac

openssl_dir="SSL/$platform"
openssl_configure="$openssl_configure enable-crypto no-mdc2 no-rc5 enable-tlsext --prefix='$openssl_install' enable-shared enable-zlib $openssl_flags -fPIC"

rm -rf $openssl_install $openssl_dir

pushd OpenSSL
make clean -j2
$openssl_configure
make depend -j2
make -j2
make install -j2

popd

mkdir -p $openssl_dir
cp -R ssl-install/lib/* $openssl_dir
cp -R ssl-install/include SSL/

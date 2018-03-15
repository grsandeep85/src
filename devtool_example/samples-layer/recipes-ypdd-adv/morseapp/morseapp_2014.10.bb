DESCRIPTION = "This package contains the simple morse code program."
LICENSE = "BSD-2-Clause"
LIC_FILES_CHKSUM = "file://license.txt;md5=47bc60f9825756218080dbd5e11c78a7"

SECTION = "sample"

PR = "r0"

SRC_URI = "file://license.txt \
           file://morse_app.c \
           file://morse_app.h \
           file://morse_codec.c \
           file://morse_codec.h \
           file://morse_gpio.c \
           file://morse_gpio.h \
           file://morse_client.c \
           file://morse_client.h \
           file://morse_server.c \
           file://morse_server.h \
	      "

S = "${WORKDIR}"

do_compile() {
	${CC} ${CFLAGS} ${CINCLUDES} -c -o morse_codec.o  morse_codec.c
	${CC} ${CFLAGS} ${CINCLUDES} -c -o morse_gpio.o   morse_gpio.c
	${CC} ${CFLAGS} ${CINCLUDES} -c -o morse_client.o morse_client.c
	${CC} ${CFLAGS} ${CINCLUDES} -c -o morse_server.o morse_server.c
	${CC} ${CFLAGS} ${CINCLUDES} -c -o morse_app.o    morse_app.c
	${CC} ${LFLAGS} ${LINCLUDES} -o morseapp morse_app.o morse_codec.o morse_gpio.o morse_client.o morse_server.o
}

# tell bitbake to include all files, even outside of default dirs
FILES_${PN} = "/*"

do_install() {
  install -d ${D}${bindir}
  install -m 0755 morseapp ${D}${bindir}

  # shameless hacking: add helper upload scripts
  mkdir -p ${D}/opt
  export ipaddr=`ifconfig eth0 | grep "inet addr" | sed -e "s/.*inet addr://" -e "s/ .*//"`
  echo "scp $USER@$ipaddr:${D}/usr/bin/morseapp ." > ${D}/opt/upload_morseapp.sh
  echo "scp $USER@$ipaddr:`ls ${TOPDIR}/tmp/work/*/morsemod/0.1-r0/morsemod.ko` ." > ${D}/opt/upload_morsemod.sh
  echo "insmod morsemod.ko; echo 8 > /sys/kernel/morsemod/simkey" > ${D}/opt/broadcast_morsemod.sh
  chmod +x ${D}/opt/upload_morseapp.sh
  chmod +x ${D}/opt/upload_morsemod.sh
  chmod +x ${D}/opt/broadcast_morsemod.sh
  # ... and make a tarball for full bootstrapping to new target
  mkdir -p ${TOPDIR}/morseapp
  if [ -f "`ls ${TOPDIR}/tmp/work/*/morsemod/0.1-r0/morsemod.ko`" ] ; then
      cp   `ls ${TOPDIR}/tmp/work/*/morsemod/0.1-r0/morsemod.ko` ${TOPDIR}/morseapp
  fi
  cp ${D}/usr/bin/morseapp                                 ${TOPDIR}/morseapp
  cp -r ${D}/opt                                           ${TOPDIR}/morseapp
  cd ${TOPDIR}/morseapp  
  tar -cf ${TOPDIR}/morseapp.tar *
}

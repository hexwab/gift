# $Id: zlib.mak,v 1.2 2002/06/02 23:37:51 rossta Exp $
# See http://www.gzip.org/zlib/
# http://prdownloads.sourceforge.net/libpng/zlib-1.1.4.tar.gz
# http://telia.dl.sourceforge.net/sourceforge/libpng/zlib-1.1.4.tar.gz

VER=1.1.4
DIR=zlib-${VER}
TAR=${DIR}.tar.gz

all:	get untar configure make

get:	${TAR}

${TAR}:
	wget http://telia.dl.sourceforge.net/sourceforge/libpng/${TAR}
	
untar:	${DIR}/os2/zlib.def

${DIR}/os2/zlib.def:
	tar xvzf ${TAR}

configure: ${DIR}/config.h

${DIR}/config.h:
	( cd ${DIR} && ./configure )

make:	${DIR}/minigzip

${DIR}/minigzip:
	( cd ${DIR} && ${MAKE} )
	@echo To install, type: su -c \"make -f zlib.mak install\"

install:
	( cd ${DIR} && ${MAKE} install )
	@echo To remove ${TAR} and ${DIR}/, type: make -f zlib.mak remove

clean:
	rm -f ${TAR}
	( cd ${DIR} && ${MAKE} clean )

remove:
	rm -fr ${DIR}
	rm -fr ${TAR}

uninstall:
	( cd ${DIR} && ${MAKE} uninstall )
	rm -fr ${DIR}
	rm -fr ${TAR}

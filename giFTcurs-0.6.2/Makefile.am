SUBDIRS = po intl src

man_MANS = giFTcurs.1 giFTcurs.conf.5

EXTRA_DIST = \
	README.ms INSTALL.ms TODO.ms giftcurs.tmac TODO \
	$(man_MANS) \
	giFTcurs.png giFTcurs.xpm

# Regenerate the documentation from source.
docs:
	groff -Tlatin1 -P-cub -ms README.ms | perl ./strip-grotty > README
	groff -Tlatin1 -P-cub -ms INSTALL.ms | perl ./strip-grotty > INSTALL
	groff -Tlatin1 -P-cub -ms TODO.ms | perl ./strip-grotty > TODO

.PHONY: docs

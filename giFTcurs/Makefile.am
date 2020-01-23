SUBDIRS = po intl src

man_MANS = giFTcurs.1 giFTcurs.conf.5

EXTRA_DIST = giFTcurs.png giFTcurs.xpm TODO $(man_MANS)

# Regenerate the documentation from source.
docs:
	groff -Tlatin1 -P-cub -ms README.ms | perl ./strip-grotty > README

.PHONY: docs

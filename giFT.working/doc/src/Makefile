# -*- make -*-
###############################################################################
## $Id: Makefile,v 1.17 2003/08/16 11:11:13 jasta Exp $
###############################################################################

SOURCES = \
	faq.tex \
	install.dbk \
	interface.dbk \
	whatis.tex

SOURCES_GRAPHVIZ = \
	bridge.dot \
	gnutella.dot \
	napster.dot \
	openft.dot

###############################################################################

OUTPUT = $(addsuffix .html,$(basename $(SOURCES)))

OUTPUT_GRAPHVIZ = \
	$(SOURCES_GRAPHVIZ:%.dot=%.png) \
	$(SOURCES_GRAPHVIZ:%.dot=%.ps)

###############################################################################

LATEX = /usr/bin/latex
TTH = /usr/bin/tth
NEATO = /usr/bin/neato
CONVERT = /usr/bin/convert
XSLTPROC = /usr/bin/xsltproc
TIDY = /usr/bin/tidy
LINKS = /usr/bin/links

###############################################################################

define process
	echo "*** PROCESSING $<..."
endef

doc:	doc-html
	@echo "Done."

doc-html:	$(OUTPUT)

###############################################################################

%.png:	%.dot $(NEATO) $(CONVERT)
	neato -Tpng -o $@ $<
	convert -scale "50%" $@ $@

%.ps:	%.dot $(NEATO)
	neato -Tps -o $@ $<

###############################################################################

define tex_gen_html
	@ ${process}
	-yes "" | $(LATEX) $(DOC)
	$(TTH) -w2 -e2 $(DOC).tex
	@cat $(DOC).descr > $(DOC).tmp
	@cat $(DOC).html | \
	  sed "s/<tt>    /<tt>\&nbsp;\&nbsp;\&nbsp;\&nbsp;/g" | \
	  sed "s/ width=\"[^\"]*\"//g" | \
	  egrep . | \
	  $(TIDY) -config tidyrc | \
	  sed "s/&\(.\)uml;/\&quot;\1/g" | \
	  sed "s/<\/tt><\/td>/\&nbsp;<\/tt><\/td>/g" | \
	  sed "s/<h2>/<hr \/><h2>/g" | \
	  sed "s/<title><\/title>//" | \
	  sed "s/<font face=\"helvetica\"><b><font size=\"+4\"><\/font><\/b><\/font> /<b><font face=\"helvetica\" size=\"+3\">/" | \
	  sed "s/<font size=\"+4\">//" | \
	  sed "s/<font size=\"+0\">/<\/font>/" | \
	  sed "s/<\/font><\/font>/<\/b>/" | \
	  sed "s/<tt>\\$$/<tt>\&nbsp;\&nbsp;\\$$/g" | \
	  tac | \
	  sed "1,/<\/div>/d" | \
	  tac >> $(DOC).tmp

	@mv $(DOC).tmp $@

	@echo "</div></body></html>" >> $@
	rm -f $(DOC).toc $(DOC).log $(DOC).aux $(DOC).dvi
endef

define dbk_gen_html
	@ ${process}
	$(XSLTPROC) -o $@ gift-html.xsl $(DOC).dbk
	@cat $(DOC).descr $@ > $(DOC).tmp
	@cat $(DOC).tmp | \
	  sed "s/ xmlns=\"[^\"]*\"//g" | \
	  sed "s/<br><\/br>/<br \/>/g" > $@
	@rm -f $(DOC).tmp
endef

%.html:	DOC = $(basename $(basename $@))

whatis.html:	whatis.tex $(OUTPUT_GRAPHVIZ)
	${tex_gen_html}

%.html:	%.tex %.descr $(LATEX) $(TTH) $(TIDY)
	${tex_gen_html}

%.html:	%.dbk %.descr $(XSLTPROC)
	${dbk_gen_html}

###############################################################################

clean:	clean-html clean-graphviz
	rm -f $(foreach b,$(basename $(SOURCES)),$(b).aux $(b).dvi $(b).log $(b).toc $(b).out)

clean-html:
	rm -f ${OUTPUT}

clean-graphviz:
	rm -f ${OUTPUT_GRAPHVIZ}

distclean:	clean
	rm -f *.aux *.dvi *.log *.toc *.out
	rm -f *.html

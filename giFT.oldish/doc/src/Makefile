###############################################################################
# -*- sh -*-

SOURCES = \
	faq.tex \
	install.dbk \
	interface.dbk \
 	whatis.tex
OUTPUT_HTML = $(addsuffix .html,$(basename $(SOURCES)))
OUTPUT_TEXT = $(OUTPUT_HTML:%.html=%.txt)
OUTPUT_DOC = ../README ../FAQ ../INTERFACE ../unix/INSTALL

LATEX = /usr/bin/latex
TTH = /usr/bin/tth
NEATO = /usr/bin/neato
CONVERT = /usr/bin/convert
XSLTPROC = /usr/bin/xsltproc
TIDY = /usr/bin/tidy
LINKS = /usr/bin/links

define process
	echo "*** PROCESSING $<..."
endef

doc:	doc-html doc-text doc-gen
	@echo "Done."

doc-html:	$(OUTPUT_HTML)

doc-text:	$(OUTPUT_TEXT)

doc-gen:	$(OUTPUT_DOC)

###############################################################################

define tex_gen_html
	@ ${process}
	-yes "" | $(LATEX) $(DOC)
	$(TTH) -w2 -e2 $(DOC).tex
	cat $(DOC).descr > $(DOC).tmp
	cat $(DOC).html | \
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

	mv $(DOC).tmp $@
	# rm -f $(DOC).html

	echo "</div></body></html>" >> $@
	rm -f $(DOC).toc $(DOC).log $(DOC).aux $(DOC).dvi
endef

define dbk_gen_html
	@ ${process}
	$(XSLTPROC) -o $(DOC).tmp gift-html.xsl $(DOC).dbk
	cat $(DOC).descr $(DOC).tmp > $@
	rm -f $(DOC).tmp
endef

%.html:	DOC = $(basename $(basename $@))

whatis.tex.html: whatis.tex $(NEATO) $(CONVERT)
	@echo "...TODO..."
	${tex_gen_html}

%.html: %.tex %.descr $(LATEX) $(TTH) $(TIDY)
	${tex_gen_html}

%.html: %.dbk %.descr $(XSLTPROC)
	${dbk_gen_html}

%.txt:  HTML = $(basename $@).html
%.txt:	%.html $(HTML) $(LINKS)
	@ ${process}
	$(LINKS) -dump $< > $@

###############################################################################

define move_doc
	cp -f $< $@
endef

../README: whatis.txt
	${move_doc}

../FAQ: faq.txt
	${move_doc}

../INTERFACE: interface.txt
	${move_doc}

../unix/INSTALL: install.txt
	${move_doc}

###############################################################################

clean: clean-html clean-txt
	rm -f $(foreach b,$(basename $(SOURCES)),$(b).aux $(b).dvi $(b).log $(b).toc)

clean-html:
	rm -f ${OUTPUT_HTML}

clean-txt:
	rm -f ${OUTPUT_TEXT}

distclean: clean
	rm -f *.aux *.dvi *.log *.toc
	rm -f *.html
	rm -f *.txt
	rm -f ${OUTPUT_DOC}

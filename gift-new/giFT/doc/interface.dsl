<!-- This file defines the DocBook-utils Style Sheet for DocBook
     Eric Bischoff <eric@caldera.de>
-->

<!DOCTYPE style-sheet PUBLIC "-//James Clark//DTD DSSSL Style Sheet//EN" [
  <!ENTITY % html "IGNORE">
  <![%html; [
	<!ENTITY % print "IGNORE">
	<!ENTITY docbook.dsl PUBLIC "-//Norman Walsh//DOCUMENT DocBook HTML Stylesheet//EN" CDATA dsssl>
  ]]>
  <!ENTITY % print "INCLUDE">
  <![%print; [
	<!ENTITY docbook.dsl PUBLIC "-//Norman Walsh//DOCUMENT DocBook Print Stylesheet//EN" CDATA dsssl>
  ]]>
]>

<STYLE-SHEET>

  <STYLE-SPECIFICATION ID="UTILS" USE="DOCBOOK">
    <STYLE-SPECIFICATION-BODY>
;; ===================================================================
;; Generic Parameters
;; (Generic currently means: both print and html)

(define %chapter-autolabel% #t)
(define %section-autolabel% #t)
(define (toc-depth nd) 3)

    </STYLE-SPECIFICATION-BODY>
  </STYLE-SPECIFICATION>

  <STYLE-SPECIFICATION ID="PRINT" USE="UTILS">
    <STYLE-SPECIFICATION-BODY>
;; ===================================================================
;; Print Parameters
;; Call: jade -d docbook-utils.dsl#print

; === Page layout ===
;; (define %paper-type% "A4")		;; use A4 paper - comment this out if needed

; === Media objects ===
(define preferred-mediaobject-extensions  ;; this magic allows to use different graphical
   (list "eps"))			;;   formats for printing and putting online
(define acceptable-mediaobject-extensions
   '())
(define preferred-mediaobject-notations
   (list "EPS"))
(define acceptable-mediaobject-notations
   (list "linespecific"))

; === Rendering ===
(define %head-after-factor% 0.2)	;; not much whitespace after orderedlist head
(define ($paragraph$)			;; more whitespace after paragraph than before
  (make paragraph
    first-line-start-indent: (if (is-first-para)
                                 %para-indent-firstpara%
                                 %para-indent%)
    space-before: (* %para-sep% 4)
    space-after: (/ %para-sep% 4)
    quadding: %default-quadding%
    hyphenate?: %hyphenation%
    language: (dsssl-language-code)
    (process-children)))

</STYLE-SPECIFICATION-BODY>
  </STYLE-SPECIFICATION>

  <STYLE-SPECIFICATION ID="HTML" USE="UTILS">
    <STYLE-SPECIFICATION-BODY>
;; ===================================================================
;; HTML Parameters
;; Call: jade -d docbook-utils.dsl#html

; === File names ===
(define %root-filename% "index")	;; name for the root html file
(define %html-ext% ".html")		;; default extension for html output files
(define %html-prefix% "")               ;; prefix for all filenames generated (except root)
(define %use-id-as-filename% #t)        ;; if #t uses ID value, if present, as filename
                                        ;;   otherwise a code is used to indicate level
                                        ;;   of chunk, and general element number
                                        ;;   (nth element in the document)
(define use-output-dir #f)              ;; output in separate directory?
(define %output-dir% "HTML")            ;; if output in directory, it's called HTML

; === HTML settings ===
(define %html-pubid% "-//W3C//DTD HTML 4.01 Transitional//EN") ;; Nearly true :-(
(define %html40% #t)

; === Media objects ===
(define preferred-mediaobject-extensions  ;; this magic allows to use different graphical
  (list "png" "jpg" "jpeg"))		;;   formats for printing and putting online
(define acceptable-mediaobject-extensions
  (list "bmp" "gif" "eps" "epsf" "avi" "mpg" "mpeg" "qt"))
(define preferred-mediaobject-notations
  (list "PNG" "JPG" "JPEG"))
(define acceptable-mediaobject-notations
  (list "EPS" "BMP" "GIF" "linespecific"))                                                                                                    
; === Rendering ===
(define %admon-graphics% #f)		;; use symbols for Caution|Important|Note|Tip|Warning

; === Books only ===
(define %generate-book-titlepage% #t)
(define %generate-book-toc% #t)
(define ($generate-chapter-toc$) #f)	;; never generate a chapter TOC in books

; === Articles only ===
(define %generate-article-titlepage% #t)
(define %generate-article-toc% #t)      ;; make TOC


;; Following was added by the giFT project, some stuff is borrowed from ldp.dsl
;; (DSSSL of the Linux Documentation Project).

(element replaceable (make element gi: "font"
    attributes: '(("color" "#006600"))
    (process-children)))

(element optional (make element gi: "i"
    (process-children)))

(define %shade-verbatim% #t)

(define (chunk-skip-first-element-list)
  ;; forces the Table of Contents on separate page
  '())

;; Redefinition of $verbatim-display$
;; Origin: dbverb.dsl
;; Different foreground and background colors for verbatim elements
;; Author: Philippe Martin (feloy@free.fr) 2001-04-07

(define ($verbatim-display$ indent line-numbers?)
  (let ((verbatim-element (gi))
        (content (make element gi: "PRE"
                       attributes: (list
                                   (list "CLASS" (gi)))
                       (if (or indent line-numbers?)
                           ($verbatim-line-by-line$ indent line-numbers?)
                           (process-children)))))
    (if %shade-verbatim%
        (make element gi: "TABLE"
              attributes: (shade-verbatim-attr-element verbatim-element)
              (make element gi: "TR"
                    (make element gi: "TD"
                          (make element gi: "FONT"
                                attributes: (list
                                             (list "COLOR" (car (shade-verbatim-element-colors
                                                                 verbatim-element))))
                                content))))
        content)))

;;
;; Customize this function
;; to change the foreground and background colors
;; of the different verbatim elements
;; Return (list "foreground color" "background color")
;;
(define (shade-verbatim-element-colors element)
  (case element
    (("SYNOPSIS") 
        ;; "label" hack by the giFT project
        (case (attribute-string (normalize "label"))
            (("client") (list "#000000" "#efefff"))
            (("server") (list "#000000" "#ffe0e0"))
            (else (list "#000000" "#e0e0e0"))))
    (else (list "#000000" "#E0E0E0"))))

(define (shade-verbatim-attr-element element)
  (list
   (list "BORDER"
     (cond
      ((equal? element (normalize "SCREEN")) "1")
        (else "0")))
   (list "BGCOLOR" (car (cdr (shade-verbatim-element-colors element))))
   (list "WIDTH" ($table-width$))))

;; End of $verbatim-display$ redefinition

    </STYLE-SPECIFICATION-BODY>
  </STYLE-SPECIFICATION>

  <EXTERNAL-SPECIFICATION ID="DOCBOOK" DOCUMENT="docbook.dsl">

</STYLE-SHEET>

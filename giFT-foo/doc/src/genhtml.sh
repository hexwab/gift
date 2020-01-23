#! /bin/sh
# yes, this is a big fucking mess... i don't care, it works
# god, i suck at this... i welcome anyone who want's to clean this up :)

imagepath='..\/resources\/docs\/'
if [ $1 ]; then
    doc=$1
else
    echo "Usage: $0 [docname] (without .tex or .dbk)"
    echo "You need 'latex', 'tth' (for .tex) and 'xsltproc' (for .dbk) and 'tidy' for "
    echo "this script to work."
    echo "The whatis document requires 'neato' (graphviz) and 'convert' (imagemagick)"
    exit
fi
if [ $doc == whatis ]; then
    for file in $(ls *.dot | cut -d. -f1)
        do neato -Tps -o $file.ps $file.dot; neato -Tpng -o $file.png $file.dot; convert -scale 50% $file.png $file.png
    done
fi
if [ -e $doc.tex ]; then
    latex $doc
    tth -w2 -e2 $doc.tex
    cat $doc.descr > $doc.tmp
    cat $doc.html |
      sed "s/<tt>    /<tt>\&nbsp;\&nbsp;\&nbsp;\&nbsp;/g" |
      sed "s/ width=\"[^\"]*\"//g" |
      egrep . |
      tidy -config tidyrc |
      sed "s/&\(.\)uml;/\&quot;\1/g" |
      sed "s/<\/tt><\/td>/\&nbsp;<\/tt><\/td>/g" |
      sed "s/<h2>/<hr \/><h2>/g" |
      sed "s/<title><\/title>//" |
      sed "s/<font face=\"helvetica\"><b><font size=\"+4\"><\/font><\/b><\/font> /<b><font face=\"helvetica\" size=\"+3\">/" |
      sed "s/<font size=\"+4\">//" |
      sed "s/<font size=\"+0\">/<\/font>/" |
      sed "s/<\/font><\/font>/<\/b>/" |
      sed "s/<tt>\\$/<tt>\&nbsp;\&nbsp;\$/g" |
      tac |
      sed "1,/<\/div>/d" |
      tac >> $doc.tmp

    if [ $doc == whatis ]; then
        cat $doc.tmp | sed "s/<img src=\"/&$imagepath/g" > $doc.html
        rm -f *.ps $doc.tmp
    else
        mv $doc.tmp $doc.html
    fi
    echo "</div></body></html>" >> $doc.html
    rm -f $doc.toc $doc.log $doc.aux $doc.dvi
elif [ -e $doc.dbk ]; then
    xsltproc -o $doc.html gift-html.xsl $doc.dbk
    cat $doc.descr $doc.html > $doc.tmp
    cat $doc.tmp |
      sed "s/ xmlns=\"[^\"]*\"//g" |
      sed "s/<br><\/br>/<br \/>/g" > $doc.html
    rm $doc.tmp
fi

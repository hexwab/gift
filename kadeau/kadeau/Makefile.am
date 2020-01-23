####### kdevelop will overwrite this part!!! (begin)##########
bin_PROGRAMS = kadeau

## INCLUDES were found outside kdevelop specific part

kadeau_SOURCES = kadeautransferviewsource.cpp inifile.cpp inisection.cpp kadeauqueryview.cpp kadeaubrowseview.cpp kadeausearchview.cpp giftsharerequest.cpp kadeautab.cpp kadeauinfotab.cpp gifttransferdownloadevent.cpp gifttransferuploadevent.cpp giftcancelrequest.cpp kadeauutil.cpp kadeautransferviewitem.cpp gifttransfersource.cpp gifttransfereventsourcerequest.cpp gifteventrequest.cpp kadeausearchviewsource.cpp kadeausearchviewitem.cpp kadeausearchtab.cpp kadeautransfertab.cpp giftsearchitem.cpp giftsearchevent.cpp giftsearchrequest.cpp giftstatsevent.cpp giftstatsrequest.cpp giftattachrequest.cpp xmltag.cpp gifttransferrequest.cpp gifttransferevent.cpp giftrequest.cpp giftevent.cpp giftconnection.cpp kadeau.cpp main.cpp 
kadeau_LDADD   =  $(LIB_KDEUI) $(LIB_KDECORE) $(LIB_QT) $(LIBSOCKET)


EXTRA_DIST = main.cpp kadeau.cpp kadeau.h kadeau.desktop lo32-app-kadeau.png lo16-app-kadeau.png giftconnection.cpp giftconnection.h giftevent.cpp giftevent.h giftrequest.cpp giftrequest.h gifttransferevent.cpp gifttransferevent.h gifttransferrequest.cpp gifttransferrequest.h xmltag.cpp xmltag.h giftattachrequest.cpp giftattachrequest.h giftstatsrequest.cpp giftstatsrequest.h giftstatsevent.cpp giftstatsevent.h giftsearchrequest.cpp giftsearchrequest.h giftsearchevent.cpp giftsearchevent.h giftsearchitem.cpp giftsearchitem.h kadeautransfertab.cpp kadeautransfertab.h kadeausearchtab.cpp kadeausearchtab.h kadeauui.rc kadeausearchviewitem.h kadeausearchviewitem.cpp kadeausearchviewsource.cpp kadeausearchviewsource.h gifteventrequest.cpp gifteventrequest.h gifttransfereventsourcerequest.cpp gifttransfereventsourcerequest.h gifttransfersource.cpp gifttransfersource.h kadeautransferviewitem.cpp kadeautransferviewitem.h kadeauutil.cpp kadeauutil.h giftcancelrequest.cpp giftcancelrequest.h gifttransferuploadevent.cpp gifttransferuploadevent.h gifttransferdownloadevent.cpp gifttransferdownloadevent.h kadeauinfotab.cpp kadeauinfotab.h kadeautab.cpp kadeautab.h giftsharerequest.cpp giftsharerequest.h kadeausearchview.cpp kadeausearchview.h kadeaubrowseview.cpp kadeaubrowseview.h kadeauqueryview.cpp kadeauqueryview.h kadeauquery.h inisection.cpp inisection.h inifile.cpp inifile.h kadeautransferviewsource.cpp kadeautransferviewsource.h 

install-data-local:
	$(mkinstalldirs) $(kde_appsdir)/Applications/
	$(INSTALL_DATA) $(srcdir)/kadeau.desktop $(kde_appsdir)/Applications/kadeau.desktop
	$(mkinstalldirs) $(kde_icondir)/locolor/32x32/apps/
	$(INSTALL_DATA) $(srcdir)/lo32-app-kadeau.png $(kde_icondir)/locolor/32x32/apps/kadeau.png
	$(mkinstalldirs) $(kde_icondir)/locolor/16x16/apps/
	$(INSTALL_DATA) $(srcdir)/lo16-app-kadeau.png $(kde_icondir)/locolor/16x16/apps/kadeau.png

uninstall-local:
	-rm -f $(kde_appsdir)/Applications/kadeau.desktop
	-rm -f $(kde_icondir)/locolor/32x32/apps/kadeau.png
	-rm -f $(kde_icondir)/locolor/16x16/apps/kadeau.png

####### kdevelop will overwrite this part!!! (end)############
# this 10 paths are KDE specific. Use them:
# kde_htmldir       Where your docs should go to. (contains lang subdirs)
# kde_appsdir       Where your application file (.kdelnk) should go to. 
# kde_icondir       Where your icon should go to.
# kde_minidir       Where your mini icon should go to.
# kde_datadir       Where you install application data. (Use a subdir)
# kde_locale        Where translation files should go to.(contains lang subdirs)
# kde_cgidir        Where cgi-bin executables should go to.
# kde_confdir       Where config files should go to.
# kde_mimedir       Where mimetypes should go to.
# kde_toolbardir    Where general toolbar icons should go to.
# kde_wallpaperdir  Where general wallpapers should go to.

# set the include path for X, qt and KDE
INCLUDES= $(all_includes)

METASOURCES = AUTO

# the library search path. 
kadeau_LDFLAGS = $(all_libraries) $(KDE_RPATH)

# Uncomment the following two lines if you add a ui.rc file for your application to make use of
# KDE´s XML GUI builing
#rcdir = $(kde_datadir)/kadeau
#rc_DATA = kadeauui.rc

#WARNING: if you use a ui.rc file above, use:

# messages: rc.cpp

# instead of 

# messages:

messages: 
	LIST=`find . -name \*.h -o -name \*.hh -o -name \*.H -o -name \*.hxx -o -name \*.hpp -o -name \*.cpp -o -name \*.cc -o -name \*.cxx -o -name \*.ecpp -o -name \*.C`; \
	if test -n "$$LIST"; then \
	 $(XGETTEXT) $$LIST -o $(podir)/kadeau.pot; \
	fi


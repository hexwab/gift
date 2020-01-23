/****************************************************************************
** KadeauSearchViewItem meta object code from reading C++ file 'kadeausearchviewitem.h'
**
** Created: Wed Apr 3 14:33:22 2002
**      by: The Qt MOC ($Id: kadeausearchviewitem.moc.cpp,v 1.1.1.1 2002/04/12 17:00:53 rjwittams Exp $)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#if !defined(Q_MOC_OUTPUT_REVISION)
#define Q_MOC_OUTPUT_REVISION 9
#elif Q_MOC_OUTPUT_REVISION != 9
#error "Moc format conflict - please regenerate all moc files"
#endif

#include "kadeausearchviewitem.h"
#include <qmetaobject.h>
#include <qapplication.h>



const char *KadeauSearchViewItem::className() const
{
    return "KadeauSearchViewItem";
}

QMetaObject *KadeauSearchViewItem::metaObj = 0;

void KadeauSearchViewItem::initMetaObject()
{
    if ( metaObj )
	return;
    if ( qstrcmp(QListViewItem::className(), "QListViewItem") != 0 )
	badSuperclassWarning("KadeauSearchViewItem","QListViewItem");
    (void) staticMetaObject();
}

#ifndef QT_NO_TRANSLATION

QString KadeauSearchViewItem::tr(const char* s)
{
    return qApp->translate( "KadeauSearchViewItem", s, 0 );
}

QString KadeauSearchViewItem::tr(const char* s, const char * c)
{
    return qApp->translate( "KadeauSearchViewItem", s, c );
}

#endif // QT_NO_TRANSLATION

QMetaObject* KadeauSearchViewItem::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    (void) QListViewItem::staticMetaObject();
#ifndef QT_NO_PROPERTIES
#endif // QT_NO_PROPERTIES
    typedef void (KadeauSearchViewItem::*m1_t0)(giFTSearchItem*);
    typedef void (QObject::*om1_t0)(giFTSearchItem*);
    m1_t0 v1_0 = &KadeauSearchViewItem::addSource;
    om1_t0 ov1_0 = (om1_t0)v1_0;
    QMetaData *slot_tbl = QMetaObject::new_metadata(1);
    QMetaData::Access *slot_tbl_access = QMetaObject::new_metaaccess(1);
    slot_tbl[0].name = "addSource(giFTSearchItem*)";
    slot_tbl[0].ptr = (QMember)ov1_0;
    slot_tbl_access[0] = QMetaData::Public;
    metaObj = QMetaObject::new_metaobject(
	"KadeauSearchViewItem", "QListViewItem",
	slot_tbl, 1,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    metaObj->set_slot_access( slot_tbl_access );
#ifndef QT_NO_PROPERTIES
#endif // QT_NO_PROPERTIES
    return metaObj;
}

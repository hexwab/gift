/****************************************************************************
** KadeauTransferViewItem meta object code from reading C++ file 'kadeautransferviewitem.h'
**
** Created: Thu Apr 4 23:58:15 2002
**      by: The Qt MOC ($Id: kadeautransferviewitem.moc.cpp,v 1.1.1.1 2002/04/12 17:00:53 rjwittams Exp $)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#if !defined(Q_MOC_OUTPUT_REVISION)
#define Q_MOC_OUTPUT_REVISION 9
#elif Q_MOC_OUTPUT_REVISION != 9
#error "Moc format conflict - please regenerate all moc files"
#endif

#include "kadeautransferviewitem.h"
#include <qmetaobject.h>
#include <qapplication.h>



const char *KadeauTransferViewItem::className() const
{
    return "KadeauTransferViewItem";
}

QMetaObject *KadeauTransferViewItem::metaObj = 0;

void KadeauTransferViewItem::initMetaObject()
{
    if ( metaObj )
	return;
    if ( qstrcmp(QListViewItem::className(), "QListViewItem") != 0 )
	badSuperclassWarning("KadeauTransferViewItem","QListViewItem");
    (void) staticMetaObject();
}

#ifndef QT_NO_TRANSLATION

QString KadeauTransferViewItem::tr(const char* s)
{
    return qApp->translate( "KadeauTransferViewItem", s, 0 );
}

QString KadeauTransferViewItem::tr(const char* s, const char * c)
{
    return qApp->translate( "KadeauTransferViewItem", s, c );
}

#endif // QT_NO_TRANSLATION

QMetaObject* KadeauTransferViewItem::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    (void) QListViewItem::staticMetaObject();
#ifndef QT_NO_PROPERTIES
#endif // QT_NO_PROPERTIES
    QMetaData::Access *slot_tbl_access = 0;
    metaObj = QMetaObject::new_metaobject(
	"KadeauTransferViewItem", "QListViewItem",
	0, 0,
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

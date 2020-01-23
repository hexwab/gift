/* giFToxic, a GTK2 based GUI for giFT
 * Copyright (C) 2002, 2003 giFToxic team (see AUTHORS)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,  USA.
 *
 */

#include "gui.h"

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(String) gettext (String)
#else
#define _(String) (String)
#endif

#define N_(String) (String)

typedef enum
{
    EVENT_DOWNLOAD,
    EVENT_UPLOAD,
    EVENT_SEARCH,
    EVENT_SRC_SEARCH
} EventType;

typedef struct
{
    gint	    num;
    EventType	type;
} EventID;

typedef enum
{
    STATUS_ACTIVE,
    STATUS_PAUSED,
    STATUS_COMPLETE,
	STATUS_CANCELLED,
    STATUS_NUM
} TransferStatus;

typedef struct
{
    EventID	    *id;
    gchar	    *status;
    gchar	    *user;
    gchar	    *url;
    gulong	    start;
    gulong	    transmitted;
    gulong	    size;
} Chunk;
    
typedef struct
{
    EventID	    *id;
    TransferStatus  status;
    gchar	    *filename;
    gchar	    *user;
    gchar	    *hash;
    gulong	    size;
    gulong	    transmitted;
    gulong	    throughput;
    gulong	    elapsed;
} Transfer;

typedef struct
{
    gchar	    *user;
    gchar	    *node;
    gchar	    *url;
    gchar	    *filename;
    gchar	    *dir;
    gchar	    *hash;
    gchar	    *mime;
    gchar	    *bitrate;
    gulong	    duration;
    gulong	    filesize;
    gboolean	availability;
    gint	    id;
} SearchResult;

typedef struct
{
	gdouble		size_local;
	gdouble		size_remote;
    gulong		files_local;
    gulong		files_remote;
    gulong		users;
	gint		downloads;
	gint		uploads;
	GSList		*networks; /* list of networks we're connected to */
	gchar		*connected_networks;
} Stats;

typedef enum
{
	SROOT,
    SSOURCES,
    SNAME,
    SSIZEHUMAN,
    SUSER,
    SAVAIL,
	SUSER_UNIQUE,
    SSIZE,
    SHASH,
    SURL,
    SMIME,
    SBITRATE,
    SN_COLS
} SearchColumn;

typedef enum
{
    TNAME,
    TUSER,
    TSTATUS,
    TPROGRESS,
    TSIZEHUMAN,
    TSPEED,
	TETA,
    TTRANS,
    THASH,
    TID,
    TSIZE,
    TTRACKED_SIZE,
    TTRACKED_TIME,
    TSTATUS_NUM,
    TUPDATED,
	TUSER_UNIQUE,
    TN_COLS
} TransferColumn;

typedef enum
{
    TRANSFER_ACTION_PAUSE,
    TRANSFER_ACTION_RESUME,
    TRANSFER_ACTION_CANCEL
} TransferAction;

typedef struct
{
    TreeStore		store;
    GtkTreePath		*path;
    gint			id;
} Moo; /* OMG, what a bad name :/ */

typedef enum
{
    SEARCH_MODE_ALL,
    SEARCH_MODE_AUDIO,
    SEARCH_MODE_VIDEO,
	SEARCH_MODE_IMG,
	SEARCH_MODE_DOCS,
	SEARCH_MODE_SOFTWARE,
	SEARCH_MODE_USER,
	SEARCH_MODE_HASH,
} SearchMode;

typedef enum
{
	AUTO_CLEAN_CANCELLED = 1 << 0,
	AUTO_CLEAN_COMPLETED = 1 << 1
} AutoClean;

typedef struct
{
    gchar		*target_host; /* host and port to connect to */
    gint		target_port;
	AutoClean	autoclean; /* sets which transfers should be removed automatically */
	gboolean	sharing;
} Options;

typedef enum
{
	STATUSBAR_TRANSFERS,
	STATUSBAR_MISC,
	STATUSBAR_NUM
} Statusbar;


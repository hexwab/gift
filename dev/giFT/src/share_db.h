/*
 * share_db.h
 *
 * Copyright (C) 2001-2002 giFT project (gift.sourceforge.net)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef __SHARE_DB_H
#define __SHARE_DB_H

/*****************************************************************************/

#define SDBRECORD_HEADER      (sizeof (ft_uint32) * 2)
#define SDBRECORD_TYPE_HEADER (sizeof (ft_uint32) * 2)

typedef struct _share_db
{
	FILE *f;
	char *path;
	char *mode;
	int   nrec;
} SDB;

typedef struct
{
	SDB  *db;
	off_t start;                       /* start of record */
	off_t written;                     /* total written to this record */
	int   nrecs;                       /* number of fields in record */
	int   fatal;                       /* used internally */
} SDBRecord;

typedef enum
{
	SDBRECORD_UINT32 = 1,
	SDBRECORD_STR    = 2
} SDBRecordType;

/*****************************************************************************/

SDB       *sdb_new        (char *path, char *mode);
int        sdb_open       (SDB *db);
int        sdb_create     (SDB *db);
void       sdb_unlink     (SDB *db);
void       sdb_close      (SDB *db);

int        sdb_write_file (SDB *db, FileShare *file);
FileShare *sdb_read_file  (SDB *db, FileShare *file);

/*****************************************************************************/

#endif /* __SHARE_DB_H */

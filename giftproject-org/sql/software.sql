-------------------------------------------------------------------------------
--
-- $Id: software.sql,v 1.2 2004/08/24 16:06:43 jasta Exp $
--
-- Copyright (C) 2004 giFT project (gift.sourceforge.net)
--
-- This program is free software; you can redistribute it and/or modify it
-- under the terms of the GNU General Public License as published by the
-- Free Software Foundation; either version 2, or (at your option) any
-- later version.
--
-- This program is distributed in the hope that it will be useful, but
-- WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
-- General Public License for more details.
--
-------------------------------------------------------------------------------

DROP TABLE softwareType;
DROP TABLE project;
DROP TABLE projectScreenshot;

-------------------------------------------------------------------------------

CREATE TABLE softwareType
(
	id               int           PRIMARY KEY AUTO_INCREMENT,
	parentId         int,

	name             varchar(32)   UNIQUE KEY
);

CREATE TABLE project
(
	id               int           PRIMARY KEY AUTO_INCREMENT,
	softwareTypeId   int,

	name             varchar(32)   UNIQUE KEY,
	url              varchar(255),
	description      text
);

CREATE TABLE projectScreenshot
(
	id               int           PRIMARY KEY AUTO_INCREMENT,
	projectId        int,

	url              varchar(255),
	urlThumb         varchar(255),

	description      text
);

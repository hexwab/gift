-------------------------------------------------------------------------------
--
-- $Id: software-data.sql,v 1.6 2005/01/04 19:09:20 hexwab Exp $
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

DELETE FROM softwareType      WHERE 1;
DELETE FROM project           WHERE 1;
DELETE FROM projectScreenshot WHERE 1;

-------------------------------------------------------------------------------

INSERT INTO softwareType (id, name) VALUES (1, 'Network Plugins');
INSERT INTO softwareType (id, name) VALUES (2, 'User Interfaces');
INSERT INTO softwareType (id, name) VALUES (3, 'External Utilities');

INSERT INTO softwareType (id, parentId, name) VALUES (4, 2, 'UNIX');
INSERT INTO softwareType (id, parentId, name) VALUES (5, 2, 'Mac OS X');
INSERT INTO softwareType (id, parentId, name) VALUES (6, 2, 'Windows');
INSERT INTO softwareType (id, parentId, name) VALUES (7, 2, 'Any Platform');

-------------------------------------------------------------------------------

INSERT INTO project VALUES
	(1, 1, 'gift-openft', NULL,
	 'A network designed exclusively around the giFT framework, loosely based '
	 'on the main design principles of the FastTrack network.  OpenFT offers '
	 'pretty much all of the major features you\'d expect to see from the '
	 'larger networks (Gnutella, FastTrack, eDonkey, etc), despite maintaining '
	 'a relatively small and manageable user base.  This network plugin was '
	 'built and maintained by the core giFT developers, and is considered an '
	 'official component of the giFT project.');

INSERT INTO project VALUES
	(2, 1, 'gift-gnutella', NULL,
	 'Gnutella1 plugin officially distributed by the giFT project.  More '
	 'should be written about this plugin later.');

INSERT INTO project VALUES
	(3, 1, 'gift-fasttrack', 'http://gift-fasttrack.berlios.de/',
	 'Implements the hugely popular FastTrack network used by KaZaA (and '
	 'formerly Morpheus  and Grokster).  Please note that this plugin was '
	 'produced independent of the KaZaA source code and does not necessarily '
	 'behave the same.  In other words, you\'re accessing the same network '
	 'that KaZaA is, but in a slightly different way.');

INSERT INTO project VALUES
	(4, 1, 'gift-ares', 'http://gift-ares.berlios.de/',
	 'A plugin that connects to the <a href="http://www.aresgalaxy.org">Ares</a> '
	'network, another FastTrack-like network.');

INSERT INTO project VALUES
	(5, 1, 'gift-opennap', 'http://gift-opennap.berlios.de/',
	 'Mostly defunct plugin to access opennap servers (via the Napster '
	 'protocol).');

INSERT INTO project VALUES
	(6, 1, 'gift-soulseek', 'http://gift-soulseek.sourceforge.net/',
	 'Totally broken mess that needs to be rewritten entirely to be of any '
	 'use.');

INSERT INTO project VALUES
	(7, 1, 'gift-donkey', 'http://code-monkey.de/?gift-donkey',
	 'A work-in-progress plugin being developed as the first '
	 '<a href="/dev.mhtml">giFT 0.12.x</a> plugin.  The plugin shows great '
	 'promise but there is much work to be done to see it through to '
	 'completion.  Interested developers are welcomed and encouraged.');

-------------------------------------------------------------------------------

INSERT INTO project VALUES
	(10, 4, 'giFTcurs', 'http://www.nongnu.org/giftcurs/',
	 'Console-based GUI interface using the curses C library.  This is by '
	 'far the coolest, most useful, and "best" giFT client available.  It is '
	 'highly recommended, and officially supported, despite being written '
	 'by a third party.');

INSERT INTO projectScreenshot VALUES
	(1, 10,
	 '/images/screenshots/giftcurs.png',
	 '/images/screenshots/giftcurs.thumb.png',
	 'TODO');

INSERT INTO project VALUES
	(14, 5, 'XFactor', 'http://www.xfactor.cc/',
	 'Designed with the beauty of iTunes and distributed with pre-compiled '
	 'binaries (giFT and plugins).  Features include: internal previewing, '
	 'iTunes integration, Nodes and Ban List updating, download corruption '
	 'notification; uses very minimal system resources and designed to be a '
	 'simple no hassle p2p client.');

INSERT INTO projectScreenshot VALUES
	(5, 14,
	 '/images/screenshots/xfactor.png',
	 '/images/screenshots/xfactor.thumb.png',
	 'TODO');

INSERT INTO project VALUES
	(13, 5, 'Poisoned', 'http://gottsilla.net/poisoned.php',
	 'Robust Mac OS X client which was featured on TechTV\'s The Screen '
	 'Savers as a "Download of the Day" some time around June 2003.');

INSERT INTO projectScreenshot VALUES
	(4, 13,
	 '/images/screenshots/poisoned.png',
	 '/images/screenshots/poisoned.thumb.png',
	 'TODO');

INSERT INTO project VALUES
	(11, 6, 'KCeasy', 'http://www.kceasy.com/',
	 'Superb Windows client which bundles precompiled binaries of giftd, '
	 'gift-gnutella, and gift-openft.  The interface attempts to mimic '
	 'KaZaA so that Windows users migrating to giFT will feel immediately '
	 'comfortable.');

INSERT INTO projectScreenshot VALUES
	(2, 11,
	 '/images/screenshots/kceasy.jpg',
	 '/images/screenshots/kceasy.thumb.jpg',
	 'TODO');

INSERT INTO project VALUES
	(12, 4, 'Apollon', 'http://apollon.sourceforge.net/',
	 'Feature-rich, mature KDE client.  It is very intuitive, easy to use, '
	 'and available in several languages.');

INSERT INTO projectScreenshot VALUES
	(3, 12,
	 '/images/screenshots/apollon.png',
	 '/images/screenshots/apollon.thumb.png',
	 'TODO');

INSERT INTO project VALUES
	(15, 4, 'giFTui', 'http://giftui.sourceforge.net/',
	 'GTK+ 2.4 tab-based interface striving for HIG 2.0 compliance.');

INSERT INTO projectScreenshot VALUES
	(6, 15,
	 '/images/screenshots/giftui.png',
	 '/images/screenshots/giftui.thumb.png',
	 'TODO');

INSERT INTO project VALUES
	(16, 6, 'giFTwin32', 'http://www.runestig.com/giFTwin32.html',
	 'Light-weight alternative to KCeasy with a similar look-and-feel.');

INSERT INTO projectScreenshot VALUES
	(7, 16,
	 '/images/screenshots/giftwin32.jpg',
	 '/images/screenshots/giftwin32.thumb.jpg',
	 'TODO');

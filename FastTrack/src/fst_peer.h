/*
 * $Id: fst_peer.h,v 1.2 2004/11/10 20:00:57 mkern Exp $
 *
 * Copyright (C) 2003 giFT-FastTrack project
 * http://developer.berlios.de/projects/gift-fasttrack
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

#ifndef __FST_PEER_H
#define __FST_PEER_H

/*****************************************************************************/

/* Register node as peer of other node. */
void fst_peer_insert (Dataset *gpeers, FSTNode *node, Dataset **peers,
                      FSTNode *peer);

/* Remove entire peer set of session from global peer set. */
void fst_peer_remove (Dataset *gpeers, FSTNode *node, Dataset *peers);

/*****************************************************************************/

#endif /* __FST_PEER_H */

/*
 * tree.h
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

#ifndef __TREE_H
#define __TREE_H

/*****************************************************************************/

typedef struct _tree_node
{
	/* parent/child */
	struct _tree_node *parent;
	struct _tree_node *child;

	/* siblings */
	struct _tree_node *prev;
	struct _tree_node *next;

	/* user data */
	void *data;
} TreeNode;

typedef struct
{
	TreeNode *root;
} Tree;

/*****************************************************************************/

typedef int  (*TreeNodeCompare) (void *a, void *b);
typedef void (*TreeForeach)     (TreeNode *node, void *udata,
                                 int depth);

/*****************************************************************************/

TreeNode *tree_insert       (Tree **tree, TreeNode *parent, TreeNode *sibling,
                             void *data);
void      tree_remove       (Tree **tree, TreeNode *node);
void      tree_destroy      (Tree **tree);
void      tree_destroy_free (Tree **tree);

TreeNode *tree_find         (Tree **tree, TreeNode *start,
                             int recurse, TreeNodeCompare func, void *data);
void      tree_foreach      (Tree **tree, TreeNode *start, int depth,
                             int recurse, TreeForeach func, void *data);

/*****************************************************************************/

#endif /* __TREE_H */

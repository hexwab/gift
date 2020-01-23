#!/usr/bin/env python

#
# ParseLib Library, this lib takes care of all the parsing
# Written by Bart Verwilst <verwilst@gentoo.org>
#
# gift_interface:
# Copyright (C) 2002 giFT project (gift.sourceforge.net)
# Author: Eelco Lempsink
# Version 0.1.2
#
import string, re, time, os 
from urllib import unquote
from TreeViewItem import *
import declare
from ThreadLib import *

"""Parse and tree functions for giFT's interface protocol.

This module is meant for giFT client developers.  It provides a set
of basic function that they'll definately need.  The purpose of this
module is the provide functions to modify data, without having to
interpreting it.  There are two sets of functions and a couple of
variables:

Parsing functions:
parse_server_object() -- Parse data as recieved from giFT to an object.
parse_client_object() -- Parse an object to a string, to send to giFT.

Tree functions:
tree_lookup() -- Look key arguments up in a tree object.
tree_lookup_mod() -- Look key modifiers up in a tree object.
tree_insert() -- Insert keys into a tree object.

Variables:
You'll only need these if you want to work with tree objects
directly.  Using tree_lookup() is recommended.
nodename -- contains the dictionary key used for the name of the node
argument -- contains the dictionary key used for arguments
modifier -- contains the dictionary key used for modifiers
children -- contains the dictionary key used for child nodes

See the interface protocol documentation for more information about
giFT's interface protocol.
"""

# Basicly C's enum
(TOKEN_TEXT,
 TOKEN_SPACE,
 TOKEN_PAREN_OPEN,
 TOKEN_PAREN_CLOSE,
 TOKEN_BRACK_OPEN,
 TOKEN_BRACK_CLOSE,
 TOKEN_BRACE_OPEN,
 TOKEN_BRACE_CLOSE,
 TOKEN_SEMICOLON) = range(9)

# These will be used for the names of the dictonary keys, use these, not their
# values!
argument = 'a'
modifier = 'm'
children = 'c'
nodename = 'n'

# Link tokens and the element they precede
abrevs = { TOKEN_PAREN_OPEN : argument,
           TOKEN_BRACK_OPEN : modifier,
           TOKEN_BRACE_OPEN : children,
           TOKEN_TEXT       : nodename }

contextlist = [ [ '(', TOKEN_PAREN_OPEN ],
                [ ')', TOKEN_PAREN_CLOSE ],
                [ '[', TOKEN_BRACK_OPEN ],
                [ ']', TOKEN_BRACK_CLOSE ],
                [ '{', TOKEN_BRACE_OPEN ],
                [ '}', TOKEN_BRACE_CLOSE ],
                [ ';', TOKEN_SEMICOLON] ]

token_chars = zip(*contextlist)[0]

escape_chrs = '()[]{};\\'

def parse_server_object(tree_string):
    """parse_server_object(tree_string) -> tree object    
    
    A function to parse a string that has been sent by the giFT server

    The returned object will look like this (think 'tree'):
    { nodename: 'COMMAND', children:
        [ { nodename: 'key', ... },
          { nodename: 'key', ... },
          { nodename: 'SUBCOMMAND', children:
                [ { nodename: 'key', ... },
                   ...
                ]
          }
        ]
    }
    
    Besides the nodename and children there can be two other keys
    (argument and modifier).  Please see the Interface Protocol
    documentation for more information, it'll make you understand the
    tree structure better.
    """

    # Why one big messy function? Because it's waaay faster than a few small
    # and clean ones.

    context = TOKEN_TEXT    # Default context, in this context a token of
                            # the type TOKEN_TEXT will become a key
    token_type = TOKEN_TEXT
    parsed = []
    keyindex = -1           # Dirty

    while len(tree_string):
        token = ''          # This is waaay faster than a special function
                            # to get the token

        if context != TOKEN_PAREN_OPEN and context != TOKEN_BRACK_OPEN:
            tree_string = string.lstrip(tree_string)

        # Find out if we're dealing with a special token, cut it of the total
        # string and return the type if we do.
        for contexttest in contextlist:
            if tree_string[0] == contexttest[0]:
                token = tree_string[0]
                token_type = contexttest[1]
                tree_string = tree_string[1:]
                break

        # No special tokens? Then get a text token. 
        if not token:
            token_type = TOKEN_TEXT
            while len(tree_string):
                c = tree_string[0]
                # Unescaping, the right way (not just replacing '\\')
                if c == '\\':
                    shift = 1
                elif c in token_chars or (context == TOKEN_TEXT and c in string.whitespace):
                    break
                else:
                    shift = 0
                token = token + tree_string[shift]
                tree_string = tree_string[shift+1:]

        abrev = abrevs[context]

        if token_type == TOKEN_TEXT:
            if context == TOKEN_TEXT:
                # Make a new key
                keyindex == keyindex + 1
                parsed.append({ abrev : token })
            elif context == TOKEN_PAREN_OPEN or context == TOKEN_BRACK_OPEN:
                # Append or create with a token the argument or the modifier
                if parsed[keyindex].has_key(abrev):
                    parsed[keyindex][abrev] = parsed[keyindex][abrev] + ' ' + token
                else:
                    parsed[keyindex][abrev] = token
        elif token_type == TOKEN_PAREN_OPEN or token_type == TOKEN_BRACK_OPEN:
            # Next token will be info in the argument or modifier
            context = token_type
        elif token_type == TOKEN_PAREN_CLOSE or token_type == TOKEN_BRACK_CLOSE:
            context = TOKEN_TEXT
        elif token_type == TOKEN_BRACE_OPEN:
            # Ooh, recursion. What's left of the tree_string will be returned too
            parsed[keyindex][abrevs[token_type]], tree_string = parse_server_object(tree_string)
        elif token_type == TOKEN_BRACE_CLOSE:
            return parsed, tree_string
        elif token_type == TOKEN_SEMICOLON:
            # Hackity-hack
            tree = parsed.pop(0)
            tree[abrevs[TOKEN_BRACE_OPEN]] = parsed
            return tree

def parse_client_object(tree, depth = 0):
    """parse_client_object(tree) -> tree_string
    
    A function to parse a special 'tree' object into a string that
    can be sent to the giFT server.  See parse_server_object.__doc__
    for a definition of the tree object.

    The generated tree_string attribute will look like this:
    
    command [modifier] (argument)
        key [modifier] (argument)
        subcommand [modifier] (argument) {
            key [modifier] (argument)
        }
    ;

    Tabs are used for indentation.  See the documentation on the giFT
    Interface Protocol for more information about this format.
    """
    
    parsed = ''
    indent = depth * '\t'

    if type(tree) == dict:
        # If it's a list, it can be parsed recursivly
        tree = [tree]
        # The COMMAND doesn't use 'child-delimiters'
        child_start = '\n'
        child_end = ';'
    else:
        child_start = ' {\n'
        child_end = depth * '\t' + '}\n'

    for node in tree:
        # Escaping
        for element in node.keys():
            if element == argument or element == modifier:
                token_list = list(node[element])
                for i in range(len(token_list)):
                    if token_list[i] in escape_chrs:
                        token_list[i] = '\\' + token_list[i]
                node[element] = ''.join(token_list)

        name = node[abrevs[TOKEN_TEXT]]
        arg, mod, children = ('',) * 3  # Will create 3 empty variables

        # Give it up for readability! :/
        if node.has_key(abrevs[TOKEN_PAREN_OPEN]):
            arg = ' (' + node[abrevs[TOKEN_PAREN_OPEN]] + ')'
        if node.has_key(abrevs[TOKEN_BRACK_OPEN]):
            mod = ' [' + node[abrevs[TOKEN_BRACK_OPEN]] + ']'
        if node.has_key(abrevs[TOKEN_BRACE_OPEN]):
            # And another lovely case of recursion.
            children = child_start + parse_client_object(node[abrevs[TOKEN_BRACE_OPEN]], depth+1) + child_end
        else:
            # I confess, this is an ugly solution. 
            children = '\n'

        # Construct the block
        parsed = parsed + indent + name + mod + arg + children

    return parsed

def _tree_lookup(tree, lookup, key=argument):
    """[internal]"""
    
    lookup = list(lookup.split('/'))
    result = list()
    # We need a list for iteration purposes
    if type(tree) != list: tree = [tree]
    
    for node in tree:
        if node[nodename] == lookup[0]:
            # Are we there yet?
            if len(lookup) > 1:
                for a in _tree_lookup(node[children], "/".join(lookup[1:])):
                    result.append(a)
            elif node.has_key(key):
                # Yes, we are...
                result.append(node[key])
                
    return result

def tree_lookup(tree, *lookups):
    """tree_lookup(tree, [lookup[, ...]]) -> result list

    Returns a list of arguments looked up in tree.  The values are
    looked up in the children of the main node, unless you didn't
    specify a lookup.  If needed, specify levels, seperated by '/'.
    If more than one lookup is specified a list of tuples will be
    returned.

    Examples: 
    tree_lookup(tree, 'ITEM/availabilty') ->
        ['-1']    
    tree_lookup(tree, 'DOWNLOADS/DOWNLOAD', 'DOWNLOADS/DOWNLOAD/transmit') ->
        [('1', '1048576'), ('2', '349525')]
    """
    
    result = []
    for lookup in lookups:
        result.append(_tree_lookup(tree[children], lookup))
    
    if len(result) == 1:
        return result[0]
    else:
        if lookups:
            # Elegant and hackish at the same time ;)
            return apply(zip, result)
        else:
            # Only the latter applies to this... :/
            result = _tree_lookup(tree, tree[nodename])
            if result: return result

def tree_lookup_mod(tree, *lookups):
    """tree_lookup(tree, [lookup[, ...]]) -> result list

    Returns a list of modifiers looked up in tree.  See
    tree_lookup.__doc__ for further documentation.
    """
    # Aargh! So... much... double... code...! :(
    
    result = []
    for lookup in lookups:
        result.append(_tree_lookup(tree[children], lookup, modifier))
    
    if len(result) == 1:
        return result[0]
    else:
        if lookups:
            # Elegant and hackish at the same time ;)
            return apply(zip, result)
        else:
            # Only the latter applies to this... :/
            result = _tree_lookup(tree, tree[nodename], modifier)
            if result: return result

def _tree_insert(tree, insert, *args):
    """[internal]"""
    
    if type(args[0]) == tuple: args = args[0]
    if len(tree) == 0: tree = list()
    # We need a list for iteration purposes
    if type(tree) != list: tree = [tree]
    
    insert = list(insert.split('/'))

    # Every node must have a name, so don't worry about the index being shifted
    nodelist = [node[nodename] for node in tree if node.has_key(nodename)]
    if insert[0] in nodelist: 
        tree_index = nodelist.index(insert[0])
    else:
        tree.append({nodename:insert[0]})
        tree_index = -1
    
    if len(insert) > 1:
        # Make sure there are children ;)
        tree[tree_index][children] = ((tree[tree_index].has_key(children) and tree[tree_index][children]) or list())
        tree[tree_index][children] = _tree_insert(tree[tree_index][children], "/".join(insert[1:]), args)
    else:
        for input in zip((argument, modifier),args): 
            if input[1]: tree[tree_index][input[0]] = input[1]

    return tree

def tree_insert(insert_dict, tree={}):
    """tree_insert(insert_dict, tree={}) -> tree object
    
    Insert nodes, with arguments and modifiers, into a tree object,
    or create a new one.  See parse_server_object.__doc__ for a
    definition of the tree object.
    
    A few examples of insert_dict elements and the command they'll
    create:
    insert_dict = { 
        'COMMAND' : argument,                   # COMMAND (argument);
        'COMMAND/key' : argument,               # COMMAND key (argument);
        'COMMAND/key' : (argument,),            # COMMAND key (argument);
        'COMMAND/key' : (argument, modifier),   # COMMAND key [modifier] (argument);
        'COMMAND/key' : '',                     # COMMAND key;
        'COMMAND/SUBCOMMAND/key : argument      # COMMAND SUBCOMMAND { key (argument) };
    }
    
    Note that adding multiple keys is not possible.  They'll be
    overwritten.  Also note that you can make an object without a
    main command, if you don't specify a 'root'.  Use this in your
    advantage :) 
    """
    
    for insert in insert_dict.keys():
        tree = _tree_insert(tree, insert, insert_dict[insert])

    if len(tree) > 1:
        return tree
    else:
        return tree[0]

######
######
## MAIN PARSELIB STARTS HERE! ##
######
######

class ParseLib:
	def __init__(self):
		self.win = declare.win
		self.ListFull = 0 ## Max nr of items displayed?
		self.SearchItemURL = declare.SearchItemURL ## Contains the OpenFT:// url
		self.SearchItemHash = declare.SearchItemHash ## Contains the hashes of the items
		self.SearchItemUser = declare.SearchItemUser ## Contains username (ip) of user
		self.SearchItemSize = declare.SearchItemSize ## Filesize of item
		self.SearchItemSaveName = declare.SearchItemSaveName ## Name that it will be saved as
		self.CurrentSearchID = declare.CurrentSearchID ## ID of current search
		self.SItemCount = 1 ## count of Search items
		self.SItemList = [] ## Contains the actual search items
		self.DItemCount = 1 ## count of Downloading items
		self.DItemList = {} ## Contains the actual downloading items
		self.DItemList[0] = {} ##  { 0 = {}, 1 = {'url' : "OpenFT://...,...}, ...}
		
	def SortTags(self, sock, args): ## This will sort the arguments we get, and adjust the GUI and such
		## Make the item's SearchItem vars..
		self.SearchItemURL[str(self.SItemCount)] = ""
		self.SearchItemHash[str(self.SItemCount)] = ""
		self.SearchItemUser[str(self.SItemCount)] = ""
		self.SearchItemSize[str(self.SItemCount)] = ""
		self.SearchItemSaveName[str(self.SItemCount)] = ""
		
		## - ATTACH function - ##
		if args[nodename] == "ATTACH":
			self.win.lblgiFTVersion.setText(tree_lookup(args, 'version')[0])

		## - STATS function - ##
		elif args[nodename] == "STATS":
			if self.win.lblOpenFTStatus.text != "Connected":
				self.win.lblOpenFTStatus.setText("Connected") ## Put us as online when we get the STATS
				
			self.win.lblOpenFTUsers.setText(tree_lookup(args, 'OpenFT/users')[0]) ## Display nr of OpenFT users
			self.win.lblOpenFTFiles.setText(tree_lookup(args, 'OpenFT/files')[0]) ## Display nr of OpenFT shared files
			self.win.lblOpenFTSize.setText(tree_lookup(args, 'OpenFT/size')[0]) ## Display total OpenFT share size
		
		## - ITEM function - ##
		elif args[nodename] == "ITEM":
			self.CurrentSearchID = tree_lookup(args)[0]
			
			if len(args[children]) == 0: ## No results found
				if self.SItemCount == 0:
					qApp.lock()
					TreeViewItem(self.win.lstSearch, (" ", "No results found", " ", " ", " "))
					qApp.unlock()
				else:
					if self.win.lstSearch.childCount() == 0: ## List is still empty
						qApp.lock()
						for x in self.SItemList:
							TreeViewItem(self.win.lstSearch, x)
						qApp.unlock()
					
			else: ## Results found!
				if self.SItemCount < 101: ## List not full yet
					#children = args["children"]
					Item = {}
					
					## Shop off the URL so it looks better in the results listview
					Item["url"] = os.path.basename(tree_lookup(args, "url")[0])
					self.SearchItemURL[str(self.SItemCount)] = Item["url"]
					## Let's filter some URL encoding
					Item["url"] = unquote(Item["url"])
					
					##Make sure it can display the used protocol
					if tree_lookup(args, "url")[0][:6] == "OpenFT":
						Item["protocol"] = " OpenFT "
					else:
						Item["protocol"] = " Unknown "
								
					Item["node"] = tree_lookup(args, "node")[0]
							
					## Fill up our per-item list
					self.SearchItemURL[str(self.SItemCount)] = tree_lookup(args, "url")[0]
					self.SearchItemHash[str(self.SItemCount)] = tree_lookup(args, "hash")[0]
					self.SearchItemUser[str(self.SItemCount)] = tree_lookup(args, "user")[0]
					self.SearchItemSize[str(self.SItemCount)] = tree_lookup(args, "size")[0]
					self.SearchItemSaveName[str(self.SItemCount)] = Item["url"]
					
					Item["number"] = str(self.SItemCount) 
					
					Item["type"] = "--"
					if string.lower(Item["url"][-3:]) == "mp3":
						Item["type"] = "mp3"
					elif string.lower(Item["url"][-3:]) == "ogg":
						Item["type"] = "ogg"
					elif string.lower(Item["url"][-3:]) == "mpg":
						Item["type"] = "mpg"
					elif string.lower(Item["url"][-3:]) == "jpg":
						Item["type"] = "jpg"
										
					Item["size"] = " " + str("%.2f" % (float(tree_lookup(args, "size")[0]) / 1024.0 / 1024.0)) + " MB "
					self.SItemCount = self.SItemCount + 1
					self.SItemList.append((Item["number"], Item["url"], 
						Item["type"], Item["size"], Item["node"], Item["protocol"]))
					
					if int(self.SItemCount) == 100 / 10: ## send cancel after 10%
						sock.send("SEARCH(" + str(declare.CurrentSearchID) + ") action(cancel) ;")
						
				else: ## List is full
					if self.ListFull == 1:
						pass
					else:
						qApp.lock()
						for x in self.SItemList:
							TreeViewItem(self.win.lstSearch, x)
						qApp.unlock()
						self.ListFull = 1

		## - ITEM function - ##
		elif args[nodename] == "DOWNLOADS":
			for id in tree_lookup(args, "DOWNLOAD"):
				if id not in declare.ID_List:
					declare.ID_List.append(id)
			#print str(len(tree_lookup(args, "DOWNLOAD")))
			#print tree_lookup(args, "DOWNLOAD/SOURCE")
			
			#if self.win.lstDownloads.childCount() < len(tree_lookup(args, "DOWNLOAD")) or self.win.lstDownloads.childCount() > len(tree_lookup(args, "DOWNLOAD")): ## List is still not totally populated
			
			self.win.lstDownloads.clear()
			for x in range(len(tree_lookup(args, "DOWNLOAD"))):
				#print "SOURCES: ", str(len(tree_lookup(args, "DOWNLOAD/SOURCE")))
				id = tree_lookup(args, "DOWNLOAD")[x]
				url = os.path.basename(unquote(tree_lookup(args, "DOWNLOAD/SOURCE/url")[x]))
				## Get % complete
				completed = int(tree_lookup(args, "DOWNLOAD/size")[x]) / 100
				## Now we have the value of 1%, calculate full % now
				completed = str(int(tree_lookup(args, "DOWNLOAD/transmit")[x]) / completed)

				self.DItemList[id] = {"url": tree_lookup(args, "DOWNLOAD/SOURCE/url")[x],
					"user": tree_lookup(args, "DOWNLOAD/SOURCE/user")[x],
					"hash": tree_lookup(args, "DOWNLOAD/hash")[x]}

				qApp.lock()
				TreeViewItem(self.win.lstDownloads, (id, url, " ", " ", (completed + " %"), " ", " "))
				qApp.unlock()
			#else: ## List is just being updated
				#pass
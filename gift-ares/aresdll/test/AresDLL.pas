{*
 * $Id: AresDLL.pas,v 1.2 2005/12/18 17:41:39 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 *}
unit ARESDLL;

{*****************************************************************************}
{*
 * This file specifies the complete interface of our Ares dll.
 *
 * The internal code runs in a separate thread created on initialization. The
 * interface is thread safe meaning all functions can be called at any time.
 * All callbacks from the library are executed in the context of the thread
 * which initialized the dll. This is achieved by creating an internal window
 * at initialization and using it as a message target for SendMessage. When
 * the internal thread wants to raise a callback it uses SendMessage to send
 * a message to the internal window. The thread which created this message
 * window (the one which initialized the dll) is blocked and the message is
 * delievered immediately to the windowproc which then raises the callback.
 * Since this will block the internal thread callbacks should return in a
 * timely manner.
 *}
{*****************************************************************************}

interface
uses
{$IFDEF WIN32}
  Windows;
{$ELSE}
  Wintypes, WinProcs;
{$ENDIF}

{*****************************************************************************}

type
    as_int8 = Char;
    as_uint8 = Byte;
    as_int16 = SmallInt;
    as_uint16 = Word;
    as_int32 = Longint;
    as_uint32 = Longword;
    as_bool = Longint;

    in_addr_t = DWORD;
    in_port_t = Word;

const
    ASTRUE = 1;
    ASFALSE = 0;

{*****************************************************************************}

{$Align Off}

{*****************************************************************************}

const AR_INVALID_HANDLE = nil;

type
    ARSearchHandle = Pointer;
    ARDownloadHandle = Pointer;
    ARUploadHandle = Pointer;

const AR_MAX_META_TAGS = 20; { max number of meta data tags per file }
const AR_HASH_SIZE = 20; { size of sha1 file hash in bytes }

type ARHash = ^AS_UINT8; { points to AR_HASH_SIZE bytes }

type ARRealm = (
	AR_REALM_ANY      = 0,
	AR_REALM_AUDIO    = 1,
	AR_REALM_VIDEO    = 2,
	AR_REALM_DOCUMENT = 3,
	AR_REALM_SOFTWARE = 4,
	AR_REALM_IMAGE    = 5,
	AR_REALM_ARCHIVE  = 6,  { You cannot search for this realm }

    AR_REALM_MAX = $7FFFFFFF { force 32 bit enums }
);

type ARMetaTag = record
    name: PChar;
    value: PChar;
end; {ARStatusData}
PARMetaTag = ^ARMetaTag;

type ARShare = record
    path: PChar;     { path of file }
    size: AS_UINT32; { file size }
    hash: ARHash;    { SHA1 of file }
    { Meta data strings. Set unused entries to nil. }
    meta: array[0..AR_MAX_META_TAGS-1] of ARMetaTag;
end; { ARShare }
PARShare = ^ARShare;

type ARDownloadState = (
	AR_DOWNLOAD_INVALID   = 0,
	AR_DOWNLOAD_NEW       = 1, { Initial state before download is started. }
	AR_DOWNLOAD_ACTIVE    = 2, { Download is transfering/looking for sources. }
	AR_DOWNLOAD_QUEUED    = 3, { Download is locally queued }
	AR_DOWNLOAD_PAUSED    = 4, { Download is paused by user (or disk full). }
	AR_DOWNLOAD_COMPLETE  = 5, { Download completed successfully. }
	AR_DOWNLOAD_FAILED    = 6, { Download was fully transfered but hash
	                             check failed. }
	AR_DOWNLOAD_CANCELLED = 7, { Download was cancelled. }
	AR_DOWNLOAD_VERIFYING = 8, { Download is being verified after
	                             downloading. }

    AR_DOWNLOAD_MAX = $7FFFFFFF { force 32 bit enums }
);

type ARUploadState = (
	AR_UPLOAD_INVALID   = 0,
	AR_UPLOAD_ACTIVE    = 1, { Upload is transfering. }
	AR_UPLOAD_COMPLETE  = 2, { Upload completed successfully. }
	AR_UPLOAD_CANCELLED = 3, { Upload was cancelled (locally or remotely). }

    AR_UPLOAD_MAX = $7FFFFFFF { force 32 bit enums }
);

type ARConfigKey = (
	AR_CONF_PORT         = 0, { Listen port (integer). }
	AR_CONF_USERNAME     = 1, { User name (string). }
	AR_CONF_MAX_DOWNLODS = 2, { Max concurrent downloads (integer). }
	AR_CONF_MAX_SOURCES  = 3, { Max active sources per download (integer). }
	AR_CONF_MAX_UPLOADS  = 4  { Max concurrent uploads (integer). }
);

{*****************************************************************************}

type ARCallbackCode = (
    AR_CB_STATUS   = 0, { connect and stats update }
    AR_CB_RESULT   = 1, { search result update }
	AR_CB_DOWNLOAD = 2, { download state change }
	AR_CB_UPLOAD   = 3, { upload state change }
	AR_CB_PROGRESS = 4, { periodic download and upload progress notify }

    AR_CB_MAX = $7FFFFFFF { force 32 bit enums }
);

{*
 * The callback is raised whenever something interesting happens. The meaning
 * of the parameters depends on the code.
 *}
type PARCALLBACK = procedure (code: ARCallbackCode; param1: Pointer; param2: Pointer) cdecl;

{*
 * AR_CB_STATUS
 * param1: PASStatusData
 * param2: NULL
 *}
type ARStatusData = record
    connected: AS_BOOL;
    connecting: AS_BOOL;
    users: AS_UINT32;
    files: AS_UINT32;
    size: AS_UINT32;
end; {ARStatusData}
PARStatusData = ^ARStatusData;

{*
 * AR_CB_RESULT
 * param1: ASSearchHandle
 * param2: PASSearchResult (NULL when search is complete)
 *}
type ARSearchResult = record
    duplicate: AS_BOOL; { ASTRUE if result with same hash was reported before }
    filename: PChar;
    filesize: AS_UINT32;
    filehash: ARHash;
	realm: ARRealm; { media type of file }
    meta: array[0..AR_MAX_META_TAGS-1] of ARMetaTag;
end; {ARSearchResult}
PARSearchResult = ^ARSearchResult;

{*
 * AR_CB_DOWNLOAD
 *   param1: PARDownload
 *   param2: NULL
 *}
type ARDownload = record
	handle: ARDownloadHandle; { handle to access this download }
	state: ARDownloadState;   { current download state }

    path: PChar;      { path of file we are writing to }
    filename: PChar;  { final file name without path }
    filehash: ARHash; { SHA1 of file }

    filesize: AS_UINT32;
    received: AS_UINT32; { bytes already downloaded }

	{* Meta data strings. Read until name is nil or end of array is
	 * reached *}
    meta: array[0..AR_MAX_META_TAGS-1] of ARMetaTag;
end; {ARDownload}
PARDownload = ^ARDownload;

{*
 * AR_CB_UPLOAD
 *   param1: PARUpload
 *   param2: NULL
 *}
type ARUpload = record
	handle: ARUploadHandle; { handle to access this upload }
	state: ARUploadState;   { current upload state }

    path: PChar;      { path of file we are reading from }
    filename: PChar;  { filename without path }
    filehash: ARHash; { SHA1 of file }

    filesize: AS_UINT32;
    start: AS_UINT32;    { start of requested range }
    stop: AS_UINT32;     { end of requested range (exclusive) }
    sent: AS_UINT32;     { bytes already uploaded }

	ip: IN_ADDR_T;       { ip of user we are uploading to }
	username: PChar;     { name of user we are uploading to }

	{* Meta data strings. Read until name is nil or end of array is
	 * reached *}
    meta: array[0..AR_MAX_META_TAGS-1] of ARMetaTag;
end; {ARUpload}
PARUpload = ^ARUpload;

{*
 * AR_CB_PROGRESS
 *   param1: PARDownloadProgress
 *   param2: PARUploadProgress
 * Note: One of the params may be nil.
 *}
type ARDownloadProgress = record
	download_count: AS_UINT32;  { number of following downloads }

	{ the following record is repeated download_count times }
    downloads: array[0..1] of record
		handle: ARDownloadHandle;
		filesize: AS_UINT32;
		received: AS_UINT32;
	end;
end; { ARDownloadProgress }
PARDownloadProgress = ^ARDownloadProgress;

type ARUploadProgress = record
	upload_count: AS_UINT32;  { number of following uploads }

	{ the following record is repeated upload_count times }
    uploads: array[0..1] of record
		handle: ARUploadHandle;
		start: AS_UINT32;
		stop: AS_UINT32;
		sent: AS_UINT32;
	end;
end; { ARUploadProgress }
PARUploadProgress = ^ARUploadProgress;

{*****************************************************************************}

{$Align On}

{*****************************************************************************}

{*
 * Startup library and set callback. If logfile is not nil it specifies the
 * path the logfile will be written to.
 *}
var ar_startup: function(callback: PARCALLBACK; logfile: PChar): AS_BOOL cdecl;

{*
 * Shutdown everything. Downloads will be stopped and saved to to disk,
 * uploads will be stopped, etc.
 *}
var ar_shutdown: function: AS_BOOL cdecl;

{*****************************************************************************}

{*
 * Connect to network, share files and allow searches.
 *}
var ar_connect: function: AS_BOOL cdecl;

{*
 * Disconnect from network. Searches are no longer possible but downloads and
 * already started uploads continue.
 *}
var ar_disconnect: function: AS_BOOL cdecl;

{*****************************************************************************}

{*
 * Start a new search. Returns opaque handle to search or AR_INVALID_HANDLE
 * on failure.
 *}
var ar_search_start: function(query: PChar; realm: ARRealm): ARSearchHandle cdecl;

{*
 * Remove search. You cannot start any downloads using the results of a
 * download after removing it.
 *}
var ar_search_remove: function(search: ARSearchHandle): AS_BOOL cdecl;

{*****************************************************************************}

{*
 * Start new download from search result. The complete file will be saved as
 * save_path but during download the filename will have the ___ARESTRA___
 * prefix. If a download for the same hash already exists the handle of that
 * download is returned.
 *}
var ar_download_start: function(search: ARSearchHandle; hash: ARHash;
                                save_path: PChar): ARDownloadHandle cdecl;

{*
 * Restart all incomplete downloads in specified directory. Returns number
 * of restarted downloads or -1 on failure.
 *}
var ar_download_restart_dir: function(dir: PChar): LongInt cdecl;

{*
 * Get current state of download.
 *}
var ar_download_state: function(download: ARDownloadHandle): ARDownloadState cdecl;

{*
 * Pause download.
 *}
var ar_download_pause: function(download: ARDownloadHandle): AS_BOOL cdecl;

{*
 * Resume paused download.
 *}
var ar_download_resume: function(download: ARDownloadHandle): AS_BOOL cdecl;

{*
 * Cancel download but don't remove it. It will just be there to look at until
 * you remove it.
 *}
var ar_download_cancel: function(download: ARDownloadHandle): AS_BOOL cdecl;

{*
 * Remove cancelled/complete/failed download and free all associated data.
 *}
var ar_download_remove: function(download: ARDownloadHandle): AS_BOOL cdecl;

{*****************************************************************************}

{*
 * Cancel upload and free all internal resources.
 *}
var ar_upload_cancel: function(upload: ARUploadHandle): AS_BOOL cdecl;

{*****************************************************************************}

{*
 * Begin addition of shares. For efficiency reasons multiple calls to
 * ar_share_add should be sandwiched using ar_share_begin and ar_share_end.
 * Will fail if ar_share_begin has been called before without matching
 * ar_share_end.
 *}
var ar_share_begin: function(): AS_BOOL cdecl;

{*
 * End addition of shares.
 *}
var ar_share_end: function(): AS_BOOL cdecl;

{*
 * Add share. If share->hash is NULL the hash will be calculated before the
 * function returns (very inefficient to do all the time). If the file does
 * not fall into one of Ares' realms or it is an incomplete download it will
 * not be shared and ARFALSE will be returned.
 *}
var ar_share_add: function(share: PARShare): AS_BOOL cdecl;

{*****************************************************************************}

{*
 * Set integer config key to new value.
 *}
var ar_config_set_int: function(key: ARConfigKey; value: AS_INT32): AS_BOOL cdecl;

{*
 * Set string config key to new value.
 *}
var ar_config_set_str: function(key: ARConfigKey; value: PChar): AS_BOOL cdecl;

{*
 * Get integer config key.
 *}
var ar_config_get_int: function(key: ARConfigKey): AS_INT32 cdecl;

{*
 * Get string config key.
 *}
var ar_config_get_str: function(key: ARConfigKey): PChar cdecl;

{*****************************************************************************}

var
  DLLLoaded: Boolean = False;

implementation

var
  SaveExit: pointer;
  DLLHandle: THandle;
  ErrorMode: Integer;

  procedure NewExit; far;
  begin
    ExitProc := SaveExit;
    FreeLibrary(DLLHandle)
  end {NewExit};

procedure LoadDLL;
begin
  if DLLLoaded then Exit;
  ErrorMode := SetErrorMode($8000{SEM_NoOpenFileErrorBox});
  DLLHandle := LoadLibrary('ARESDLL.DLL');
  if DLLHandle >= 32 then
  begin
    DLLLoaded := True;
    SaveExit := ExitProc;
    ExitProc := @NewExit;
    { init }
    @ar_startup := GetProcAddress(DLLHandle,'ar_startup');
    Assert(@ar_startup <> nil);
    @ar_shutdown := GetProcAddress(DLLHandle,'ar_shutdown');
    Assert(@ar_shutdown <> nil);
    { connect }
    @ar_connect := GetProcAddress(DLLHandle,'ar_connect');
    Assert(@ar_connect <> nil);
    @ar_disconnect := GetProcAddress(DLLHandle,'ar_disconnect');
    Assert(@ar_disconnect <> nil);
    { search }
    @ar_search_start := GetProcAddress(DLLHandle,'ar_search_start');
    Assert(@ar_search_start <> nil);
    @ar_search_remove := GetProcAddress(DLLHandle,'ar_search_remove');
    Assert(@ar_search_remove <> nil);
    { download }
    @ar_download_start := GetProcAddress(DLLHandle,'ar_download_start');
    Assert(@ar_download_start <> nil);
    @ar_download_restart_dir := GetProcAddress(DLLHandle,'ar_download_restart_dir');
    Assert(@ar_download_restart_dir <> nil);
    @ar_download_state := GetProcAddress(DLLHandle,'ar_download_state');
    Assert(@ar_download_state <> nil);
    @ar_download_pause := GetProcAddress(DLLHandle,'ar_download_pause');
    Assert(@ar_download_pause <> nil);
    @ar_download_resume := GetProcAddress(DLLHandle,'ar_download_resume');
    Assert(@ar_download_resume <> nil);
    @ar_download_cancel := GetProcAddress(DLLHandle,'ar_download_cancel');
    Assert(@ar_download_cancel <> nil);
    @ar_download_remove := GetProcAddress(DLLHandle,'ar_download_remove');
    Assert(@ar_download_remove <> nil);
    { upload }
    @ar_upload_cancel := GetProcAddress(DLLHandle,'ar_upload_cancel');
    Assert(@ar_upload_cancel <> nil);
    { sharing }
    @ar_share_begin := GetProcAddress(DLLHandle,'ar_share_begin');
    Assert(@ar_share_begin <> nil);
    @ar_share_end := GetProcAddress(DLLHandle,'ar_share_end');
    Assert(@ar_share_end <> nil);
    @ar_share_add := GetProcAddress(DLLHandle,'ar_share_add');
    Assert(@ar_share_add <> nil);
    { config }
    @ar_config_set_int := GetProcAddress(DLLHandle,'ar_config_set_int');
    Assert(@ar_config_set_int <> nil);
    @ar_config_set_str := GetProcAddress(DLLHandle,'ar_config_set_str');
    Assert(@ar_config_set_str <> nil);
    @ar_config_get_int := GetProcAddress(DLLHandle,'ar_config_get_int');
    Assert(@ar_config_get_int <> nil);
    @ar_config_get_str := GetProcAddress(DLLHandle,'ar_config_get_str');
    Assert(@ar_config_get_str <> nil);
  end
  else
  begin
    DLLLoaded := False;
    { Error: ARESDLL.DLL could not be loaded !! }
  end;
  SetErrorMode(ErrorMode)
end {LoadDLL};

begin
  LoadDLL;
end.

unit arestest;

interface

uses
  Windows, Messages, SysUtils, Variants, Classes, Graphics, Controls, Forms,
  Dialogs, ARESDLL, StdCtrls, ComCtrls, Grids, WinSock, Math;

type
  TMainForm = class(TForm)
    DebugGrp: TGroupBox;
    LogMemo: TMemo;
    CtrlGrp: TGroupBox;
    ConnectBtn: TButton;
    DisconnectBtn: TButton;
    StatsLbl: TLabel;
    SearchGrp: TGroupBox;
    SearchBtn: TButton;
    QueryEdit: TEdit;
    ResultListView: TListView;
    DownloadBtn: TButton;
    ResultCountLbl: TLabel;
    MetaGrid: TStringGrid;
    DownloadGrp: TGroupBox;
    DownloadListView: TListView;
    CancelDlBtn: TButton;
    PauseDlBtn: TButton;
    ResumeDlBtn: TButton;
    RemoveDlBtn: TButton;
    RealmCombo: TComboBox;
    UploadGrp: TGroupBox;
    UploadListView: TListView;
    CancelUlBtn: TButton;
    ClearUlBtn: TButton;
    procedure FormCreate(Sender: TObject);
    procedure FormClose(Sender: TObject; var Action: TCloseAction);
    procedure ConnectBtnClick(Sender: TObject);
    procedure DisconnectBtnClick(Sender: TObject);
    procedure SearchBtnClick(Sender: TObject);
    procedure ResultListViewSelectItem(Sender: TObject; Item: TListItem;
      Selected: Boolean);
    procedure DownloadBtnClick(Sender: TObject);
    procedure PauseDlBtnClick(Sender: TObject);
    procedure ResumeDlBtnClick(Sender: TObject);
    procedure CancelDlBtnClick(Sender: TObject);
    procedure RemoveDlBtnClick(Sender: TObject);
    procedure CancelUlBtnClick(Sender: TObject);
    procedure ClearUlBtnClick(Sender: TObject);
  private
    { Private declarations }
    MediaDir: string; { directory for downloads and shares }
    Search: ARSearchHandle;
    ResultCount: Integer;

    procedure cb_status(StatusData: PARStatusData);
    procedure cb_result(Search: ARSearchHandle; Result: PARSearchResult);
    procedure cb_download(Download: PARDownload);
    procedure cb_download_progress(Down: PARDownloadProgress);
    procedure cb_upload(Upload: PARUpload);
    procedure cb_upload_progress(Up: PARUploadProgress);
    procedure Log(str: string);
    procedure ShareDir(dir: string);
  public
    { Public declarations }
  end;

var
  MainForm: TMainForm;

implementation

{$R *.dfm}

{*****************************************************************************}

procedure AresCallbackFunc(code: ARCallbackCode; param1: Pointer; param2: Pointer) cdecl;
begin
    { dispatch callback codes }
    case code of
    AR_CB_STATUS:   MainForm.cb_status(param1);
    AR_CB_RESULT:   MainForm.cb_result(param1, param2);
    AR_CB_DOWNLOAD: MainForm.cb_download(param1);
    AR_CB_UPLOAD:   MainForm.cb_upload(param1);
    AR_CB_PROGRESS:
    begin
        if param1 <> nil then MainForm.cb_download_progress(param1);
        if param2 <> nil then MainForm.cb_upload_progress(param2);
    end;
    else
        MainForm.Log('Unhandled Callback code ' + IntToStr(Ord(code)));
    end;
end;

procedure TMainForm.cb_status(StatusData: PARStatusData);
var
    str: string;
begin
   if StatusData.connected = ASTRUE then str := 'Online, '
   else str := 'Offline, ';
   if StatusData.connecting = ASTRUE then str := str + 'Connecting, ';
   str := str + IntToStr(StatusData.users) + ' users, ';
   str := str + IntToStr(StatusData.files) + ' files, ';
   str := str + IntToStr(StatusData.size) + ' GB';

   StatsLbl.Caption := str;
   Log('Stats: ' + str);
end;

procedure TMainForm.cb_result(Search: ARSearchHandle; Result: PARSearchResult);
var
    Item: TListItem;
    Hex: array[0..41] of Char;
    i: Integer;
    MetaStrings: TStringList;
    RealmStr: string;
begin
    { search complete? }
    if Result = nil then
        Exit;

    BinToHex(PChar(Result.filehash), Hex, AR_HASH_SIZE);
    Hex[40] := Char(0);

    if Result.duplicate = ASTRUE then
    begin
        { Got same result before, find it and increase source count }
        for i := ResultListView.Items.Count - 1 downto 0 do
        begin
            if ResultListView.Items.Item[i].SubItems[3] = Hex then
            begin
                { found it }
                ResultListView.Items.Item[i].SubItems[2] := IntToStr(StrToInt(ResultListView.Items.Item[i].SubItems[2]) + 1);
                Break;
            end;
        end;
    end
    else
    begin
        { New result, just add it }
        case Result.realm of
    	AR_REALM_AUDIO:    RealmStr := 'Audio';
    	AR_REALM_VIDEO:    RealmStr := 'Video';
    	AR_REALM_IMAGE:    RealmStr := 'Image';
    	AR_REALM_SOFTWARE: RealmStr := 'Software';
    	AR_REALM_DOCUMENT: RealmStr := 'Document';
    	AR_REALM_ARCHIVE:  RealmStr := 'Archive';
        else
            RealmStr := 'UNKNOWN'; { should never happen }
        end;

        Item := ResultListView.Items.Add();
        Item.Caption := Result.filename;
        Item.SubItems.Add(RealmStr);
        Item.SubItems.Add(IntToStr(Result.filesize));
        Item.SubItems.Add(IntToStr(1));
        Item.SubItems.Add(Hex);
        { move meta data to string list and attach it to item }
        MetaStrings := TStringList.Create();
        for i := 0 to AR_MAX_META_TAGS - 1 do
        begin
            if Result.meta[i].name = nil then
                Break;
            MetaStrings.Add(Result.meta[i].name + '=' + Result.meta[i].value);
        end;
        Item.Data := MetaStrings;
        { update result count }
        ResultCount := ResultCount + 1;
        ResultCountLbl.Caption := IntToStr(ResultCount);
    end;
end;

procedure TMainForm.cb_download(Download: PARDownload);
var
    StateStr: string;
    Item: TListItem;
    Hex: array[0..41] of Char;
    Share: ARShare;
    i: Integer;
begin
    case Download.state of
	AR_DOWNLOAD_INVALID:   StateStr := 'Invalid';
	AR_DOWNLOAD_NEW:       StateStr := 'New';
	AR_DOWNLOAD_ACTIVE:    StateStr := 'Active';
	AR_DOWNLOAD_QUEUED:    StateStr := 'Queued';
	AR_DOWNLOAD_PAUSED:    StateStr := 'Paused';
	AR_DOWNLOAD_COMPLETE:  StateStr := 'Complete';
	AR_DOWNLOAD_FAILED:    StateStr := 'Failed';
	AR_DOWNLOAD_CANCELLED: StateStr := 'Cancelled';
	AR_DOWNLOAD_VERIFYING: StateStr := 'Verifying';
    end;

    Log('Download ' + StateStr + ': ' + Download.filename);

    BinToHex(PChar(Download.filehash), Hex, AR_HASH_SIZE);
    Hex[40] := Char(0);

    { create new download item in list view if there is none }
    Item := DownloadListView.FindData(0, Download.handle, True, True);
    if Item = nil then
    begin
        Item := DownloadListView.Items.Add();
        Item.Data := Download.handle;
        Item.Caption := Download.filename;
        Item.SubItems.Add('');    { state }
        Item.SubItems.Add('');    { size }
        Item.SubItems.Add('');    { uploaded }
        Item.SubItems.Add('');    { hash }
    end;

    Item.Caption := Download.filename;
    Item.SubItems[0] := StateStr;
    Item.SubItems[1] := IntToStr(Download.filesize);
    Item.SubItems[2] := IntToStr(Download.received);
    Item.SubItems[3] := Hex;

    { share file if download is complete }
    if Download.state = AR_DOWNLOAD_COMPLETE then
    begin
        Log ('Sharing ' + Download.filename);
        Share.path := Download.path;
        Share.size := Download.filesize;
        Share.hash := nil; { make lib calculate hash. not recomended for real world use }
        for i := 0 to AR_MAX_META_TAGS - 1 do
        begin
            Share.meta[i].name := nil;
            Share.meta[i].value := nil;
        end;
        { add share }
        ar_share_add(@Share);
    end;
end;

procedure TMainForm.cb_download_progress(Down: PARDownloadProgress);
var
    Item: TListItem;
    i: Integer;
begin
    for i := 0 to Down.download_count - 1 do
    begin
        { find item }
        Item := DownloadListView.FindData(0, Down.downloads[i].handle, True, True);
        if Item = nil then
        begin
            Log('Download item not found for progress update');
            Continue;
        end;
        { update item }
        Item.SubItems[1] := IntToStr(Down.downloads[i].filesize);
        Item.SubItems[2] := IntToStr(Down.downloads[i].received);
    end;
end;

procedure TMainForm.cb_upload(Upload: PARUpload);
var
    StateStr: string;
    Item: TListItem;
    Hex: array[0..41] of Char;
    Addr : TInAddr;
begin
    case Upload.state of
	AR_UPLOAD_INVALID:   StateStr := 'Invalid';
	AR_UPLOAD_ACTIVE:    StateStr := 'Active';
	AR_UPLOAD_COMPLETE:  StateStr := 'Complete';
	AR_UPLOAD_CANCELLED: StateStr := 'Cancelled';
    end;

    Log('Upload ' + StateStr + ': ' + Upload.filename);

    BinToHex(PChar(Upload.filehash), Hex, AR_HASH_SIZE);
    Hex[40] := Char(0);

    { create new upload item in list view if there is none }
    Item := UploadListView.FindData(0, Upload.handle, True, True);
    if Item = nil then
    begin
        Item := UploadListView.Items.Add();
        Item.Data := Upload.handle;
        Item.Caption := Upload.filename;
        Item.SubItems.Add('');    { state }
        Item.SubItems.Add('');    { user }
        Item.SubItems.Add('');    { size }
        Item.SubItems.Add('');    { sent }
        Item.SubItems.Add('');    { hash }
    end;

    Item.Caption := Upload.filename;
    Item.SubItems[0] := StateStr;
    Addr.S_addr := Upload.ip;
    Item.SubItems[1] := string(Upload.username) + '@' + inet_ntoa (addr);
    Item.SubItems[2] := IntToStr(Upload.stop - Upload.start);
    Item.SubItems[3] := IntToStr(Upload.sent);
    Item.SubItems[4] := Hex;
end;

procedure TMainForm.cb_upload_progress(Up: PARUploadProgress);
var
    Item: TListItem;
    i: Integer;
begin
    for i := 0 to Up.upload_count - 1 do
    begin
        { find item }
        Item := UploadListView.FindData(0, Up.uploads[i].handle, True, True);
        if Item = nil then
        begin
            Log('Upload item not found for progress update');
            Continue;
        end;
        { update item }
        Item.SubItems[2] := IntToStr(Up.uploads[i].stop - Up.uploads[i].start);
        Item.SubItems[3] := IntToStr(Up.uploads[i].sent);
    end;
end;

{*****************************************************************************}

procedure TMainForm.Log(str: string);
begin
        LogMemo.Lines.Add(str);
end;

procedure TMainForm.ShareDir(dir: string);
var
    f: TSearchRec;
    Share: ARShare;
    i: Integer;
begin

    if FindFirst(dir + '\*.*', 0, f) <> 0 then
        Exit;
    ar_share_begin();

    repeat
        Log ('Sharing ' + f.Name);
        Share.path := PChar(dir + '\' + f.Name);
        Share.size := f.Size;
        Share.hash := nil; { make lib calculate hash. not recomended for real world use }
        for i := 0 to AR_MAX_META_TAGS - 1 do
        begin
            Share.meta[i].name := nil;
            Share.meta[i].value := nil;
        end;
        { add share }
        ar_share_add(@Share);
    until FindNext(f) <> 0;

    ar_share_end();
    FindClose(f);
end;
{*****************************************************************************}

procedure TMainForm.FormCreate(Sender: TObject);
var
    files: Integer;
    Port: Integer;
begin
        Search := AR_INVALID_HANDLE;
        LogMemo.Clear();

        if AresDll.DLLLoaded <> True then
        begin
                MessageDlg('Couldn''t load AresDll', mtError, [mbOK], 0);
                Exit;
        end;

        Log('Starting up AresDll');
        if ar_startup (AresCallbackFunc, 'aresdll.log') <> ASTRUE then
        begin
                MessageDlg('ar_startup() failed', mtError, [mbOK], 0);
                Exit;
        end;
        { randomize port until we find one that is available }
        Randomize();
        repeat
            Port := RandomRange(20000, 40000);
        until (ar_config_set_int(AR_CONF_PORT, Port) = ASTRUE);

        { set user name }
        ar_config_set_str(AR_CONF_USERNAME, 'johndoe');

        { create media dir we can use }
        MediaDir := GetCurrentDir() + '\MediaFiles\';
        ForceDirectories(MediaDir);

        { resume downloads in media dir }
        files := ar_download_restart_dir(PChar(MediaDir));
        Log('Resumed files: ' + IntToStr(files));

        { Share files in media dir }
        ShareDir(MediaDir);
end;

procedure TMainForm.FormClose(Sender: TObject; var Action: TCloseAction);
begin
        if AresDll.DLLLoaded then
        begin
                Log('Shutting down AresDll');
                ar_shutdown ();
        end;
end;

procedure TMainForm.ConnectBtnClick(Sender: TObject);
begin
        Log('ar_connect()');
        ar_connect();
end;

procedure TMainForm.DisconnectBtnClick(Sender: TObject);
begin
        Log('ar_disconnect()');
        ar_disconnect ();
end;

procedure TMainForm.SearchBtnClick(Sender: TObject);
var
    query: string;
    i: Integer;
    Realm: ARRealm;
begin
    { If there already is a search, remove it }
    if Search <> AR_INVALID_HANDLE then
    begin
        { clear list view first so we don't accidently access any result data
          after removing the search }
        for i := ResultListView.Items.Count - 1 downto 0 do
            TStringList(ResultListView.Items.Item[i].Data).Free();
        ResultListView.Clear();
        MetaGrid.Cols[0].Clear();
        MetaGrid.Cols[1].Clear();
        ar_search_remove(Search);
        Search := AR_INVALID_HANDLE;
    end;
    ResultCount := 0;
    ResultCountLbl.Caption := IntToStr(ResultCount);

    { Prepare new query }
    query := QueryEdit.Text;
    if Length(query) = 0 then
    begin
        MessageDlg('Query string empty.', mtError, [mbOK], 0);
        Exit;
    end;

    if RealmCombo.Text = 'Audio'          then Realm := AR_REALM_AUDIO
    else if RealmCombo.Text = 'Video'     then Realm := AR_REALM_VIDEO
    else if RealmCombo.Text = 'Images'    then Realm := AR_REALM_IMAGE
    else if RealmCombo.Text = 'Documents' then Realm := AR_REALM_DOCUMENT
    else if RealmCombo.Text = 'Software'  then Realm := AR_REALM_SOFTWARE
    else                                       Realm := AR_REALM_ANY;

    { start search }
    Search := ar_search_start(PChar(query), Realm);
    if Search = AR_INVALID_HANDLE then
    begin
        MessageDlg('ar_search_start() failed.', mtError, [mbOK], 0);
        Exit;
    end;

    Log ('New search for ''' + query + ''' in realm ''' + RealmCombo.Text + '''');
end;

procedure TMainForm.ResultListViewSelectItem(Sender: TObject;
                          Item: TListItem; Selected: Boolean);
var
    MetaStrings: TStringList;
    i: Integer;
begin
    if not Selected then
        Exit;

    { clear old meta data }
    MetaGrid.Cols[0].Clear();
    MetaGrid.Cols[1].Clear();

    { Show new meta data }
    MetaStrings := Item.Data;
    for i := 0 to MetaStrings.Count - 1 do
    begin
        MetaGrid.Cells[0,i] := MetaStrings.Names[i];
        MetaGrid.Cells[1,i] := MetaStrings.Values[MetaStrings.Names[i]];
    end;
end;

procedure TMainForm.DownloadBtnClick(Sender: TObject);
var
    Item: TListItem;
    Filename: string;
    Hash: array[0..AR_HASH_SIZE-1] of AS_UINT8;
    Download: ARDownloadHandle;
begin
    { get necessary data for download }
    Item := ResultListView.Selected;
    if (Item = nil) or (Search = AR_INVALID_HANDLE) then
        Exit;
    HexToBin(PChar(Item.SubItems[3]), @Hash, AR_HASH_SIZE);
    Filename := Item.Caption;

    { start download }
    Download := ar_download_start(Search, @Hash, PChar(MediaDir + Filename));
    if Download = AR_INVALID_HANDLE then
    begin
        MessageDlg('ar_download_start() failed.', mtError, [mbOK], 0);
        Exit;
    end;

    Log('Download started with handle: ' + IntToHex(Integer(Download),8));
end;

procedure TMainForm.PauseDlBtnClick(Sender: TObject);
var
    Item: TListItem;
begin
    Item := DownloadListView.Selected;
    if (Item <> nil) then
        ar_download_pause(Item.Data);
end;

procedure TMainForm.ResumeDlBtnClick(Sender: TObject);
var
    Item: TListItem;
begin
    Item := DownloadListView.Selected;
    if (Item <> nil) then
        ar_download_resume(Item.Data);
end;

procedure TMainForm.CancelDlBtnClick(Sender: TObject);
var
    Item: TListItem;
begin
    Item := DownloadListView.Selected;
    if (Item <> nil) then
        ar_download_cancel(Item.Data);
end;

procedure TMainForm.RemoveDlBtnClick(Sender: TObject);
var
    Item: TListItem;
begin
    Item := DownloadListView.Selected;
    { only remove finished/cancelled downloads }
    if Item <> nil then
    begin
        if ar_download_state(Item.Data) in [AR_DOWNLOAD_COMPLETE, AR_DOWNLOAD_FAILED, AR_DOWNLOAD_CANCELLED] then
        begin
            ar_download_remove(Item.Data);
            { also remove list view item }
            Item.Delete();
        end;
    end;
end;

procedure TMainForm.CancelUlBtnClick(Sender: TObject);
var
    Item: TListItem;
begin
    Item := UploadListView.Selected;
    if (Item <> nil) then
        ar_upload_cancel(Item.Data);
end;

procedure TMainForm.ClearUlBtnClick(Sender: TObject);
var
    i: Integer;
    Item: TListItem;
begin
    { remove all complete and cancelled items }
    for i := UploadListView.Items.Count - 1 downto 0 do
    begin
        Item := UploadListView.Items.Item[i];
        if (Item.SubItems[0] = 'Complete') or (Item.SubItems[0] = 'Cancelled') then
            Item.Delete();
    end;
end;

end.



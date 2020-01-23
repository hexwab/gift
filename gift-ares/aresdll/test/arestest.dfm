object MainForm: TMainForm
  Left = 228
  Top = 308
  Width = 930
  Height = 640
  Caption = 'Ares DLL test application'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  OnClose = FormClose
  OnCreate = FormCreate
  DesignSize = (
    922
    613)
  PixelsPerInch = 96
  TextHeight = 13
  object DebugGrp: TGroupBox
    Left = 440
    Top = 8
    Width = 476
    Height = 169
    Anchors = [akLeft, akTop, akRight]
    Caption = 'Debug'
    TabOrder = 0
    DesignSize = (
      476
      169)
    object StatsLbl: TLabel
      Left = 8
      Top = 144
      Width = 460
      Height = 13
      Anchors = [akLeft, akRight, akBottom]
      AutoSize = False
      Caption = 'Unknown status'
    end
    object LogMemo: TMemo
      Left = 8
      Top = 16
      Width = 460
      Height = 121
      Anchors = [akLeft, akTop, akRight, akBottom]
      Lines.Strings = (
        'LogMemo')
      ScrollBars = ssBoth
      TabOrder = 0
    end
  end
  object CtrlGrp: TGroupBox
    Left = 8
    Top = 8
    Width = 425
    Height = 49
    Caption = 'Controls'
    TabOrder = 1
    object ConnectBtn: TButton
      Left = 16
      Top = 16
      Width = 75
      Height = 25
      Caption = 'Connect'
      TabOrder = 0
      OnClick = ConnectBtnClick
    end
    object DisconnectBtn: TButton
      Left = 96
      Top = 16
      Width = 75
      Height = 25
      Caption = 'Disconnect'
      TabOrder = 1
      OnClick = DisconnectBtnClick
    end
  end
  object SearchGrp: TGroupBox
    Left = 8
    Top = 64
    Width = 425
    Height = 542
    Anchors = [akLeft, akTop, akBottom]
    Caption = 'Search'
    TabOrder = 2
    DesignSize = (
      425
      542)
    object ResultCountLbl: TLabel
      Left = 296
      Top = 22
      Width = 41
      Height = 13
      Alignment = taRightJustify
      Anchors = [akTop, akRight]
      AutoSize = False
      Caption = '0'
    end
    object SearchBtn: TButton
      Left = 344
      Top = 16
      Width = 75
      Height = 25
      Anchors = [akTop, akRight]
      Caption = 'Search'
      TabOrder = 0
      OnClick = SearchBtnClick
    end
    object QueryEdit: TEdit
      Left = 8
      Top = 18
      Width = 187
      Height = 21
      Anchors = [akLeft, akTop, akRight]
      TabOrder = 1
    end
    object ResultListView: TListView
      Left = 8
      Top = 48
      Width = 409
      Height = 404
      Anchors = [akLeft, akTop, akRight, akBottom]
      Columns = <
        item
          Caption = 'Filename'
          Width = 200
        end
        item
          Caption = 'Realm'
          Width = 65
        end
        item
          Caption = 'Size'
          Width = 80
        end
        item
          Caption = 'Sources'
          Width = 60
        end
        item
          Caption = 'Hash'
          Width = 250
        end>
      HideSelection = False
      ReadOnly = True
      RowSelect = True
      TabOrder = 2
      ViewStyle = vsReport
      OnSelectItem = ResultListViewSelectItem
    end
    object DownloadBtn: TButton
      Left = 14
      Top = 461
      Width = 75
      Height = 25
      Anchors = [akLeft, akBottom]
      Caption = 'Download'
      TabOrder = 3
      OnClick = DownloadBtnClick
    end
    object MetaGrid: TStringGrid
      Left = 104
      Top = 459
      Width = 313
      Height = 72
      Anchors = [akLeft, akRight, akBottom]
      ColCount = 2
      DefaultColWidth = 145
      DefaultRowHeight = 14
      FixedCols = 0
      RowCount = 20
      FixedRows = 0
      Options = [goFixedVertLine, goFixedHorzLine, goVertLine, goHorzLine, goColSizing, goRowSelect]
      TabOrder = 4
    end
    object RealmCombo: TComboBox
      Left = 200
      Top = 18
      Width = 91
      Height = 21
      Style = csDropDownList
      Anchors = [akTop, akRight]
      ItemHeight = 13
      ItemIndex = 0
      TabOrder = 5
      Text = 'Any'
      Items.Strings = (
        'Any'
        'Audio'
        'Video'
        'Images'
        'Documents'
        'Software')
    end
  end
  object DownloadGrp: TGroupBox
    Left = 440
    Top = 384
    Width = 476
    Height = 222
    Anchors = [akLeft, akTop, akRight, akBottom]
    Caption = 'Downloads'
    TabOrder = 3
    DesignSize = (
      476
      222)
    object DownloadListView: TListView
      Left = 8
      Top = 16
      Width = 459
      Height = 166
      Anchors = [akLeft, akTop, akRight, akBottom]
      Columns = <
        item
          Caption = 'Filename'
          Width = 200
        end
        item
          Caption = 'State'
          Width = 80
        end
        item
          Caption = 'Size'
          Width = 80
        end
        item
          Caption = 'Downloaded'
          Width = 80
        end
        item
          Caption = 'Hash'
          Width = 250
        end>
      HideSelection = False
      ReadOnly = True
      RowSelect = True
      TabOrder = 0
      ViewStyle = vsReport
    end
    object CancelDlBtn: TButton
      Left = 222
      Top = 189
      Width = 75
      Height = 25
      Anchors = [akLeft, akBottom]
      Caption = 'Cancel'
      TabOrder = 1
      OnClick = CancelDlBtnClick
    end
    object PauseDlBtn: TButton
      Left = 14
      Top = 189
      Width = 75
      Height = 25
      Anchors = [akLeft, akBottom]
      Caption = 'Pause'
      TabOrder = 2
      OnClick = PauseDlBtnClick
    end
    object ResumeDlBtn: TButton
      Left = 94
      Top = 189
      Width = 75
      Height = 25
      Anchors = [akLeft, akBottom]
      Caption = 'Resume'
      TabOrder = 3
      OnClick = ResumeDlBtnClick
    end
    object RemoveDlBtn: TButton
      Left = 302
      Top = 189
      Width = 75
      Height = 25
      Anchors = [akLeft, akBottom]
      Caption = 'Remove'
      TabOrder = 4
      OnClick = RemoveDlBtnClick
    end
  end
  object UploadGrp: TGroupBox
    Left = 440
    Top = 183
    Width = 476
    Height = 194
    Anchors = [akLeft, akTop, akRight]
    Caption = 'Uploads'
    TabOrder = 4
    DesignSize = (
      476
      194)
    object UploadListView: TListView
      Left = 8
      Top = 16
      Width = 459
      Height = 138
      Anchors = [akLeft, akTop, akRight, akBottom]
      Columns = <
        item
          Caption = 'Filename'
          Width = 200
        end
        item
          Caption = 'State'
          Width = 80
        end
        item
          Caption = 'User'
          Width = 80
        end
        item
          Caption = 'Chunk Size'
          Width = 80
        end
        item
          Caption = 'Sent'
          Width = 80
        end
        item
          Caption = 'Hash'
          Width = 250
        end>
      HideSelection = False
      ReadOnly = True
      RowSelect = True
      TabOrder = 0
      ViewStyle = vsReport
    end
    object CancelUlBtn: TButton
      Left = 14
      Top = 161
      Width = 75
      Height = 25
      Anchors = [akLeft, akBottom]
      Caption = 'Cancel'
      TabOrder = 1
      OnClick = CancelUlBtnClick
    end
    object ClearUlBtn: TButton
      Left = 94
      Top = 161
      Width = 91
      Height = 25
      Anchors = [akLeft, akBottom]
      Caption = 'Clear Complete'
      TabOrder = 2
      OnClick = ClearUlBtnClick
    end
  end
end

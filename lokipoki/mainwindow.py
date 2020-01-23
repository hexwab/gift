# Form implementation generated from reading ui file 'ui/mainwindow.ui'
#
# Created: Sun Aug 11 17:51:39 2002
#      by: The PyQt User Interface Compiler (pyuic)
#
# WARNING! All changes made in this file will be lost!


from qt import *


class MainForm(QMainWindow):
    def __init__(self,parent = None,name = None,fl = 0):
        QMainWindow.__init__(self,parent,name,fl)
        self.statusBar()

        if name == None:
            self.setName("MainWindow")

        self.resize(736,570)
        self.setCaption(self.tr(". : LokiPoki : ."))

        self.setCentralWidget(QWidget(self,"qt_central_widget"))
        MainWindowLayout = QGridLayout(self.centralWidget(),1,1,5,5,"MainWindowLayout")

        self.MainTab = QTabWidget(self.centralWidget(),"MainTab")
        self.MainTab.setAcceptDrops(0)
        self.MainTab.setTabShape(QTabWidget.Rounded)

        self.tab = QWidget(self.MainTab,"tab")

        self.Frame3 = QFrame(self.tab,"Frame3")
        self.Frame3.setGeometry(QRect(10,10,325,140))
        self.Frame3.setFrameShape(QFrame.TabWidgetPanel)
        self.Frame3.setFrameShadow(QFrame.Plain)

        self.lblOpenFT = QLabel(self.Frame3,"lblOpenFT")
        self.lblOpenFT.setGeometry(QRect(5,5,85,16))
        lblOpenFT_font = QFont(self.lblOpenFT.font())
        lblOpenFT_font.setBold(1)
        self.lblOpenFT.setFont(lblOpenFT_font)
        self.lblOpenFT.setText(self.tr("OpenFT:"))

        LayoutWidget = QWidget(self.Frame3,"Layout8")
        LayoutWidget.setGeometry(QRect(120,25,195,97))
        Layout8 = QVBoxLayout(LayoutWidget,0,5,"Layout8")

        self.lblgiFTVersion = QLabel(LayoutWidget,"lblgiFTVersion")
        self.lblgiFTVersion.setText(self.tr(""))
        Layout8.addWidget(self.lblgiFTVersion)

        self.lblOpenFTStatus = QLabel(LayoutWidget,"lblOpenFTStatus")
        self.lblOpenFTStatus.setText(self.tr(""))
        Layout8.addWidget(self.lblOpenFTStatus)

        self.lblOpenFTUsers = QLabel(LayoutWidget,"lblOpenFTUsers")
        self.lblOpenFTUsers.setText(self.tr(""))
        Layout8.addWidget(self.lblOpenFTUsers)

        self.lblOpenFTFiles = QLabel(LayoutWidget,"lblOpenFTFiles")
        self.lblOpenFTFiles.setText(self.tr(""))
        Layout8.addWidget(self.lblOpenFTFiles)

        self.lblOpenFTSize = QLabel(LayoutWidget,"lblOpenFTSize")
        self.lblOpenFTSize.setText(self.tr(""))
        Layout8.addWidget(self.lblOpenFTSize)

        LayoutWidget_2 = QWidget(self.Frame3,"Layout7")
        LayoutWidget_2.setGeometry(QRect(25,25,88,97))
        Layout7 = QVBoxLayout(LayoutWidget_2,0,5,"Layout7")

        self.TextLabel1_2 = QLabel(LayoutWidget_2,"TextLabel1_2")
        self.TextLabel1_2.setText(self.tr("Version:"))
        Layout7.addWidget(self.TextLabel1_2)

        self.TextLabel1 = QLabel(LayoutWidget_2,"TextLabel1")
        self.TextLabel1.setText(self.tr("Status:"))
        Layout7.addWidget(self.TextLabel1)

        self.TextLabel2 = QLabel(LayoutWidget_2,"TextLabel2")
        self.TextLabel2.setText(self.tr("Active Hosts:"))
        Layout7.addWidget(self.TextLabel2)

        self.TextLabel2_2 = QLabel(LayoutWidget_2,"TextLabel2_2")
        self.TextLabel2_2.setText(self.tr("Shared Files:"))
        Layout7.addWidget(self.TextLabel2_2)

        self.TextLabel2_3 = QLabel(LayoutWidget_2,"TextLabel2_3")
        self.TextLabel2_3.setText(self.tr("Total Size:"))
        Layout7.addWidget(self.TextLabel2_3)
        self.MainTab.insertTab(self.tab,self.tr("Connection"))

        self.tab_2 = QWidget(self.MainTab,"tab_2")
        tabLayout = QGridLayout(self.tab_2,1,1,5,5,"tabLayout")

        self.Frame8 = QFrame(self.tab_2,"Frame8")
        self.Frame8.setSizePolicy(QSizePolicy(0,5,0,0,self.Frame8.sizePolicy().hasHeightForWidth()))
        self.Frame8.setFrameShape(QFrame.TabWidgetPanel)
        self.Frame8.setFrameShadow(QFrame.Plain)
        Frame8Layout = QGridLayout(self.Frame8,1,1,5,5,"Frame8Layout")

        self.ButtonGroup7 = QButtonGroup(self.Frame8,"ButtonGroup7")
        self.ButtonGroup7.setTitle(self.tr(""))
        self.ButtonGroup7.setColumnLayout(0,Qt.Vertical)
        self.ButtonGroup7.layout().setSpacing(5)
        self.ButtonGroup7.layout().setMargin(10)
        ButtonGroup7Layout = QGridLayout(self.ButtonGroup7.layout())
        ButtonGroup7Layout.setAlignment(Qt.AlignTop)

        Layout12 = QVBoxLayout(None,0,5,"Layout12")

        self.btnEverything = QRadioButton(self.ButtonGroup7,"btnEverything")
        self.btnEverything.setText(self.tr("Everything"))
        self.btnEverything.setChecked(1)
        Layout12.addWidget(self.btnEverything)

        self.btnAudio = QRadioButton(self.ButtonGroup7,"btnAudio")
        self.btnAudio.setText(self.tr("Audio"))
        Layout12.addWidget(self.btnAudio)

        self.btnVideo = QRadioButton(self.ButtonGroup7,"btnVideo")
        self.btnVideo.setText(self.tr("Video"))
        Layout12.addWidget(self.btnVideo)

        ButtonGroup7Layout.addLayout(Layout12,0,0)

        Frame8Layout.addWidget(self.ButtonGroup7,1,0)

        self.ButtonGroup5 = QButtonGroup(self.Frame8,"ButtonGroup5")
        self.ButtonGroup5.setMaximumSize(QSize(32767,70))
        self.ButtonGroup5.setTitle(self.tr(""))
        self.ButtonGroup5.setColumnLayout(0,Qt.Vertical)
        self.ButtonGroup5.layout().setSpacing(5)
        self.ButtonGroup5.layout().setMargin(10)
        ButtonGroup5Layout = QGridLayout(self.ButtonGroup5.layout())
        ButtonGroup5Layout.setAlignment(Qt.AlignTop)

        self.txtSearch = QLineEdit(self.ButtonGroup5,"txtSearch")
        self.txtSearch.setSizePolicy(QSizePolicy(5,5,0,0,self.txtSearch.sizePolicy().hasHeightForWidth()))
        self.txtSearch.setMinimumSize(QSize(125,0))
        self.txtSearch.setMaximumSize(QSize(200,32767))

        ButtonGroup5Layout.addMultiCellWidget(self.txtSearch,0,0,0,2)
        spacer = QSpacerItem(0,0,QSizePolicy.Expanding,QSizePolicy.Minimum)
        ButtonGroup5Layout.addItem(spacer,1,2)
        spacer_2 = QSpacerItem(0,0,QSizePolicy.Expanding,QSizePolicy.Minimum)
        ButtonGroup5Layout.addItem(spacer_2,1,0)

        self.cmdSearch = QPushButton(self.ButtonGroup5,"cmdSearch")
        self.cmdSearch.setSizePolicy(QSizePolicy(0,0,0,0,self.cmdSearch.sizePolicy().hasHeightForWidth()))
        self.cmdSearch.setMinimumSize(QSize(80,0))
        self.cmdSearch.setText(self.tr("Search"))
        self.cmdSearch.setDefault(0)
        self.cmdSearch.setFlat(0)

        ButtonGroup5Layout.addWidget(self.cmdSearch,1,1)

        Frame8Layout.addWidget(self.ButtonGroup5,0,0)

        tabLayout.addWidget(self.Frame8,0,0)

        self.Frame6 = QFrame(self.tab_2,"Frame6")
        self.Frame6.setFrameShape(QFrame.TabWidgetPanel)
        self.Frame6.setFrameShadow(QFrame.Plain)
        Frame6Layout = QGridLayout(self.Frame6,1,1,5,5,"Frame6Layout")

        self.lstSearch = QListView(self.Frame6,"lstSearch")
        self.lstSearch.addColumn(self.tr("#"))
        self.lstSearch.addColumn(self.tr("File"))
        self.lstSearch.addColumn(self.tr("Type"))
        self.lstSearch.addColumn(self.tr("Size"))
        self.lstSearch.addColumn(self.tr("Host"))
        self.lstSearch.addColumn(self.tr("Protocol"))
        self.lstSearch.setFrameShape(QListView.StyledPanel)
        self.lstSearch.setFrameShadow(QListView.Sunken)
        self.lstSearch.setMargin(1)
        self.lstSearch.setSelectionMode(QListView.Extended)
        self.lstSearch.setAllColumnsShowFocus(1)
        self.lstSearch.setShowSortIndicator(0)
        self.lstSearch.setItemMargin(1)
        self.lstSearch.setRootIsDecorated(0)

        Frame6Layout.addWidget(self.lstSearch,0,0)

        tabLayout.addWidget(self.Frame6,0,1)
        self.MainTab.insertTab(self.tab_2,self.tr("Search"))

        self.tab_3 = QWidget(self.MainTab,"tab_3")
        tabLayout_2 = QGridLayout(self.tab_3,1,1,5,5,"tabLayout_2")

        self.GroupBox1 = QGroupBox(self.tab_3,"GroupBox1")
        self.GroupBox1.setTitle(self.tr(". : Uploads : ."))
        self.GroupBox1.setColumnLayout(0,Qt.Vertical)
        self.GroupBox1.layout().setSpacing(5)
        self.GroupBox1.layout().setMargin(8)
        GroupBox1Layout = QGridLayout(self.GroupBox1.layout())
        GroupBox1Layout.setAlignment(Qt.AlignTop)

        self.lstUploads = QListView(self.GroupBox1,"lstUploads")
        self.lstUploads.addColumn(self.tr("#"))
        self.lstUploads.addColumn(self.tr("File"))
        self.lstUploads.addColumn(self.tr("Type"))
        self.lstUploads.addColumn(self.tr("Host"))
        self.lstUploads.addColumn(self.tr("Progress"))
        self.lstUploads.addColumn(self.tr("Bandwidth"))
        self.lstUploads.addColumn(self.tr("Protocol"))
        self.lstUploads.setAllColumnsShowFocus(1)

        GroupBox1Layout.addWidget(self.lstUploads,0,0)

        tabLayout_2.addWidget(self.GroupBox1,1,0)

        self.GroupBox1_2 = QGroupBox(self.tab_3,"GroupBox1_2")
        self.GroupBox1_2.setTitle(self.tr(". : Downloads : ."))
        self.GroupBox1_2.setAlignment(QGroupBox.AlignAuto)
        self.GroupBox1_2.setColumnLayout(0,Qt.Vertical)
        self.GroupBox1_2.layout().setSpacing(5)
        self.GroupBox1_2.layout().setMargin(8)
        GroupBox1_2Layout = QGridLayout(self.GroupBox1_2.layout())
        GroupBox1_2Layout.setAlignment(Qt.AlignTop)

        self.lstDownloads = QListView(self.GroupBox1_2,"lstDownloads")
        self.lstDownloads.addColumn(self.tr("#"))
        self.lstDownloads.addColumn(self.tr("File"))
        self.lstDownloads.addColumn(self.tr("Type"))
        self.lstDownloads.addColumn(self.tr("Host"))
        self.lstDownloads.addColumn(self.tr("Progress"))
        self.lstDownloads.addColumn(self.tr("Bandwidth"))
        self.lstDownloads.addColumn(self.tr("Protocol"))
        self.lstDownloads.setAllColumnsShowFocus(1)

        GroupBox1_2Layout.addWidget(self.lstDownloads,0,0)

        tabLayout_2.addWidget(self.GroupBox1_2,0,0)
        self.MainTab.insertTab(self.tab_3,self.tr("Transfers"))

        self.tab_4 = QWidget(self.MainTab,"tab_4")
        tabLayout_3 = QGridLayout(self.tab_4,1,1,5,5,"tabLayout_3")

        self.Frame5 = QFrame(self.tab_4,"Frame5")
        self.Frame5.setFrameShape(QFrame.TabWidgetPanel)
        self.Frame5.setFrameShadow(QFrame.Plain)
        Frame5Layout = QGridLayout(self.Frame5,1,1,5,5,"Frame5Layout")

        self.ConfigTab = QTabWidget(self.Frame5,"ConfigTab")
        self.ConfigTab.setEnabled(1)
        self.ConfigTab.setBackgroundOrigin(QTabWidget.WidgetOrigin)

        self.tab_5 = QWidget(self.ConfigTab,"tab_5")

        self.ButtonGroup1_2_2 = QButtonGroup(self.tab_5,"ButtonGroup1_2_2")
        self.ButtonGroup1_2_2.setGeometry(QRect(5,5,325,80))
        self.ButtonGroup1_2_2.setTitle(self.tr("Auto Update"))
        self.ButtonGroup1_2_2.setAlignment(QButtonGroup.WordBreak | QButtonGroup.AlignTop)
        self.ButtonGroup1_2_2.setColumnLayout(0,Qt.Vertical)
        self.ButtonGroup1_2_2.layout().setSpacing(5)
        self.ButtonGroup1_2_2.layout().setMargin(10)
        ButtonGroup1_2_2Layout = QGridLayout(self.ButtonGroup1_2_2.layout())
        ButtonGroup1_2_2Layout.setAlignment(Qt.AlignTop)

        self.CheckBox3_2 = QCheckBox(self.ButtonGroup1_2_2,"CheckBox3_2")
        self.CheckBox3_2.setText(self.tr("Enable Automatic Updating"))

        ButtonGroup1_2_2Layout.addWidget(self.CheckBox3_2,0,0)

        self.CheckBox4_2 = QCheckBox(self.ButtonGroup1_2_2,"CheckBox4_2")
        self.CheckBox4_2.setText(self.tr("Ask before doing the actual update"))

        ButtonGroup1_2_2Layout.addWidget(self.CheckBox4_2,1,0)

        self.TextLabel2_5 = QLabel(self.tab_5,"TextLabel2_5")
        self.TextLabel2_5.setGeometry(QRect(45,135,526,271))
        self.TextLabel2_5.setText(self.tr("On popular demand, i'll get rid of the config tabs..\n"
"I think i'm gonna use something like \"Auto-Update [ Enabled ]\" and such, divided into categories, with an \"edit\" button that leads to a new window for each"))
        self.TextLabel2_5.setTextFormat(QLabel.RichText)
        self.TextLabel2_5.setAlignment(QLabel.WordBreak | QLabel.AlignTop)
        self.ConfigTab.insertTab(self.tab_5,self.tr("General"))

        self.tab_6 = QWidget(self.ConfigTab,"tab_6")

        self.TextLabel2_4_2 = QLabel(self.tab_6,"TextLabel2_4_2")
        self.TextLabel2_4_2.setGeometry(QRect(5,5,630,405))
        self.TextLabel2_4_2.setText(self.tr("Possibility to leave out results which has specific words in their content.\n"
"\n"
"Also, predefined wordlists can be set\n"
"\n"
"This tab should be password protected (encrypted pwd, how?)"))
        self.ConfigTab.insertTab(self.tab_6,self.tr("Content Filter"))

        self.tab_7 = QWidget(self.ConfigTab,"tab_7")

        self.TextLabel3_2 = QLabel(self.tab_7,"TextLabel3_2")
        self.TextLabel3_2.setGeometry(QRect(6,13,621,406))
        self.TextLabel3_2.setText(self.tr("Here, you will be able to set the dirs you share.\n"
"\n"
"Also only share certain types of files (if that's possible)"))
        self.ConfigTab.insertTab(self.tab_7,self.tr("My Shared Files"))

        self.tab_8 = QWidget(self.ConfigTab,"tab_8")

        self.GroupBox4_2 = QGroupBox(self.tab_8,"GroupBox4_2")
        self.GroupBox4_2.setGeometry(QRect(5,5,339,160))
        self.GroupBox4_2.setTitle(self.tr("Network"))
        self.GroupBox4_2.setColumnLayout(0,Qt.Vertical)
        self.GroupBox4_2.layout().setSpacing(5)
        self.GroupBox4_2.layout().setMargin(10)
        GroupBox4_2Layout = QGridLayout(self.GroupBox4_2.layout())
        GroupBox4_2Layout.setAlignment(Qt.AlignTop)

        Layout4_3_2_2 = QVBoxLayout(None,0,5,"Layout4_3_2_2")

        self.chkStartgiFT = QCheckBox(self.GroupBox4_2,"chkStartgiFT")
        self.chkStartgiFT.setText(self.tr("Start giFT server on startup"))
        Layout4_3_2_2.addWidget(self.chkStartgiFT)

        self.chkShutdowngiFT = QCheckBox(self.GroupBox4_2,"chkShutdowngiFT")
        self.chkShutdowngiFT.setText(self.tr("Shutdown giFT server on exit"))
        Layout4_3_2_2.addWidget(self.chkShutdowngiFT)

        GroupBox4_2Layout.addLayout(Layout4_3_2_2,0,0)
        self.ConfigTab.insertTab(self.tab_8,self.tr("Advanced"))

        Frame5Layout.addWidget(self.ConfigTab,0,0)

        tabLayout_3.addWidget(self.Frame5,0,0)
        self.MainTab.insertTab(self.tab_4,self.tr("Configuration"))

        MainWindowLayout.addWidget(self.MainTab,0,0)



        self.menubar = QMenuBar(self,"menubar")



#include "MainWindow.h"

#include <stdlib.h>
#include <string>
#include <iostream>

#include <QPushButton>
#include <QSize>
#include <QModelIndex>
#include <QItemSelectionModel>
#include <QtDebug>
#include <QTextCodec>
#include <QMessageBox>
#include <QKeyEvent>
#include <QFileDialog>

#include "HubFrame.h"
#include "HubManager.h"
#include "HashProgress.h"
#include "TransferView.h"
#include "ShareBrowser.h"
#include "QuickConnect.h"
#include "SearchFrame.h"
#include "Settings.h"
#include "FavoriteHubs.h"
#include "FavoriteUsers.h"
#include "DownloadQueue.h"
#include "FinishedTransfers.h"
#include "AntiSpamFrame.h"
#include "IPFilterFrame.h"
#include "ToolBar.h"
#include "Magnet.h"

#include "UPnPMapper.h"
#include "WulforSettings.h"
#include "WulforUtil.h"

#include "Version.h"

using namespace std;

MainWindow::MainWindow (QWidget *parent):
        QMainWindow(parent),
        statusLabel(NULL)
{
    arenaMap.clear();
    arenaWidgets.clear();

    init();

    retranslateUi();

    LogManager::getInstance()->addListener(this);
    TimerManager::getInstance()->addListener(this);
    QueueManager::getInstance()->addListener(this);

    startSocket();

    setStatusMessage(tr("Ready"));

    TransferView::newInstance();

    transfer_dock->setWidget(TransferView::getInstance());

    blockSignals(true);
    fileTransfers->setChecked(transfer_dock->isVisible());
    blockSignals(false);

    if (WBGET(WB_ANTISPAM_ENABLED))
        AntiSpam::newInstance();

    if (WBGET(WB_IPFILTER_ENABLED))
        IPFilter::newInstance();
}

MainWindow::~MainWindow(){
    LogManager::getInstance()->removeListener(this);
    TimerManager::getInstance()->removeListener(this);
    QueueManager::getInstance()->removeListener(this);

    delete arena;

    delete fBar;
    delete tBar;

    qDeleteAll(fileMenuActions);
}

void MainWindow::closeEvent(QCloseEvent *c_e){
    if (!isUnload){
        hide();
        c_e->ignore();

        return;
    }

    blockSignals(true);

    saveSettings();

    TransferView::getInstance()->close();
    TransferView::deleteInstance();

    if (FavoriteHubs::getInstance()){
        FavoriteHubs::getInstance()->setUnload(true);
        FavoriteHubs::getInstance()->close();
    }

    FavoriteHubs::deleteInstance();

    if (FinishedDownloads::getInstance()){
        FinishedDownloads::getInstance()->setUnload(true);
        FinishedDownloads::getInstance()->close();
    }

    if (FinishedUploads::getInstance()){
        FinishedUploads::getInstance()->setUnload(true);
        FinishedUploads::getInstance()->close();
    }

    if (FavoriteUsers::getInstance()){
        FavoriteUsers::getInstance()->setUnload(true);
        FavoriteUsers::getInstance()->close();
    }

    if (DownloadQueue::getInstance()){
        DownloadQueue::getInstance()->setUnload(true);
        DownloadQueue::getInstance()->close();
    }

    QMap< ArenaWidget*, QWidget* > map = arenaMap;
    QMap< ArenaWidget*, QWidget* >::iterator it = map.begin();

    for(; it != map.end(); ++it){
        if (arenaMap.contains(it.key()))//some widgets can autodelete itself from arena widgets
            it.value()->close();
    }

    c_e->accept();
}

void MainWindow::customEvent(QEvent *e){
    if (e->type() == MainWindowCustomEvent::Event){
        MainWindowCustomEvent *c_e = reinterpret_cast<MainWindowCustomEvent*>(e);

        c_e->func()->call();
    }

    e->accept();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *e){
    if (e->type() == QEvent::KeyRelease){
        QKeyEvent *k_e = reinterpret_cast<QKeyEvent*>(e);

        if ((k_e->key() == Qt::Key_Escape) && !isUnload &&
            (k_e->modifiers() == Qt::NoModifier) && isActiveWindow()
           )
           {
                hide();
           }
    }

    return QMainWindow::eventFilter(obj, e);
}

void MainWindow::init(){
    installEventFilter(this);

    arena = new QDockWidget();
    arena->setFloating(false);
    arena->setAllowedAreas(Qt::RightDockWidgetArea);
    arena->setFeatures(QDockWidget::NoDockWidgetFeatures);
    arena->setTitleBarWidget(new QWidget(arena));

    transfer_dock = new QDockWidget(this);
    transfer_dock->setObjectName("transfer_dock");
    transfer_dock->setFloating(false);
    transfer_dock->setAllowedAreas(Qt::BottomDockWidgetArea);
    transfer_dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    transfer_dock->setTitleBarWidget(new QWidget(transfer_dock));

    setCentralWidget(arena);
    //addDockWidget(Qt::RightDockWidgetArea, arena);
    addDockWidget(Qt::BottomDockWidgetArea, transfer_dock);

    transfer_dock->hide();

    history.setSize(30);

    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF-8"));

    setWindowIcon(WulforUtil::getInstance()->getPixmap(WulforUtil::eiICON_APPL));

    setWindowTitle(QString("%1").arg(EISKALTDCPP_WND_TITLE));

    initActions();

    initMenuBar();

    initStatusBar();

    initToolbar();

    loadSettings();
}

void MainWindow::loadSettings(){
    WulforSettings *WS = WulforSettings::getInstance();

    bool showMax = WS->getBool(WB_MAINWINDOW_MAXIMIZED);
    int w = WS->getInt(WI_MAINWINDOW_WIDTH);
    int h = WS->getInt(WI_MAINWINDOW_HEIGHT);
    int xPos = WS->getInt(WI_MAINWINDOW_X);
    int yPos = WS->getInt(WI_MAINWINDOW_Y);

    QPoint p(xPos, yPos);
    QSize  sz(w, h);

    if (p.x() >= 0 || p.y() >= 0)
        this->move(p);

    if (sz.width() > 0 || sz.height() > 0)
        this->resize(sz);

    if (showMax)
        this->showMaximized();

    QString wstate = WSGET(WS_MAINWINDOW_STATE);

    if (!wstate.isEmpty())
        this->restoreState(QByteArray::fromBase64(wstate.toAscii()));
}

void MainWindow::saveSettings(){
    WISET(WI_MAINWINDOW_HEIGHT, height());
    WISET(WI_MAINWINDOW_WIDTH, width());
    WISET(WI_MAINWINDOW_X, x());
    WISET(WI_MAINWINDOW_Y, y());

    WBSET(WB_MAINWINDOW_MAXIMIZED, isMaximized());
    WBSET(WB_MAINWINDOW_HIDE, !isVisible());

    WSSET(WS_MAINWINDOW_STATE, saveState().toBase64());
}

void MainWindow::initActions(){
    {
        WulforUtil *WU = WulforUtil::getInstance();

        fileOptions = new QAction("", this);
        fileOptions->setShortcut(tr("Ctrl+O"));
        fileOptions->setIcon(WU->getPixmap(WulforUtil::eiCONFIGURE));
        connect(fileOptions, SIGNAL(triggered()), this, SLOT(slotFileSettings()));

        fileFileListBrowserLocal = new QAction("", this);
        fileFileListBrowserLocal->setShortcut(tr("Ctrl+L"));
        fileFileListBrowserLocal->setIcon(WU->getPixmap(WulforUtil::eiOWN_FILELIST));
        connect(fileFileListBrowserLocal, SIGNAL(triggered()), this, SLOT(slotFileBrowseOwnFilelist()));

        fileFileListRefresh = new QAction("", this);
        fileFileListRefresh->setShortcut(tr("Ctrl+R"));
        fileFileListRefresh->setIcon(WU->getPixmap(WulforUtil::eiRELOAD));
        connect(fileFileListRefresh, SIGNAL(triggered()), this, SLOT(slotFileRefreshShare()));

        fileHubReconnect = new QAction("", this);
        fileHubReconnect->setIcon(WU->getPixmap(WulforUtil::eiRECONNECT));
        connect(fileHubReconnect, SIGNAL(triggered()), this, SLOT(slotFileReconnect()));

        fileHashProgress = new QAction("", this);
        fileHashProgress->setIcon(WU->getPixmap(WulforUtil::eiHASHING));
        connect(fileHashProgress, SIGNAL(triggered()), this, SLOT(slotFileHashProgress()));

        fileQuickConnect = new QAction("", this);
        fileQuickConnect->setShortcut(tr("Ctrl+H"));
        fileQuickConnect->setIcon(WU->getPixmap(WulforUtil::eiCONNECT));
        connect(fileQuickConnect, SIGNAL(triggered()), this, SLOT(slotQC()));

        fileTransfers = new QAction("", this);
        fileTransfers->setIcon(WU->getPixmap(WulforUtil::eiTRANSFER));
        fileTransfers->setCheckable(true);
        connect(fileTransfers, SIGNAL(toggled(bool)), this, SLOT(slotFileTransfer(bool)));
        //transfer_dock->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);

        fileDownloadQueue = new QAction("", this);
        fileDownloadQueue->setIcon(WU->getPixmap(WulforUtil::eiDOWNLOAD));
        connect(fileDownloadQueue, SIGNAL(triggered()), this, SLOT(slotFileDownloadQueue()));

        fileFinishedDownloads = new QAction("", this);
        fileFinishedDownloads->setIcon(WU->getPixmap(WulforUtil::eiDOWN));
        connect(fileFinishedDownloads, SIGNAL(triggered()), this, SLOT(slotFileFinishedDownloads()));

        fileFinishedUploads = new QAction("", this);
        fileFinishedUploads->setIcon(WU->getPixmap(WulforUtil::eiUP));
        connect(fileFinishedUploads, SIGNAL(triggered()), this, SLOT(slotFileFinishedUploads()));

        fileFavoriteHubs = new QAction("", this);
        fileFavoriteHubs->setIcon(WU->getPixmap(WulforUtil::eiSERVER));
        connect(fileFavoriteHubs, SIGNAL(triggered()), this, SLOT(slotFileFavoriteHubs()));

        fileFavoriteUsers = new QAction("", this);
        fileFavoriteUsers->setIcon(WU->getPixmap(WulforUtil::eiUSERS));
        connect(fileFavoriteUsers, SIGNAL(triggered()), this, SLOT(slotFileFavoriteUsers()));

        fileAntiSpam = new QAction("", this);
        fileAntiSpam->setIcon(WU->getPixmap(WulforUtil::eiSPAM));
        connect(fileAntiSpam, SIGNAL(triggered()), this, SLOT(slotFileAntiSpam()));

        fileIPFilter = new QAction("", this);
        fileIPFilter->setIcon(WU->getPixmap(WulforUtil::eiFILTER));
        connect(fileIPFilter, SIGNAL(triggered()), this, SLOT(slotFileIPFilter()));

        fileSearch = new QAction("", this);
        fileSearch->setShortcut(tr("Ctrl+S"));
        fileSearch->setIcon(WU->getPixmap(WulforUtil::eiFILEFIND));
        connect(fileSearch, SIGNAL(triggered()), this, SLOT(slotFileSearch()));

        fileQuit = new QAction("", this);
        fileQuit->setIcon(WU->getPixmap(WulforUtil::eiEXIT));
        connect(fileQuit, SIGNAL(triggered()), this, SLOT(slotExit()));

        QAction *separator0 = new QAction("", this);
        separator0->setSeparator(true);
        QAction *separator1 = new QAction("", this);
        separator1->setSeparator(true);
        QAction *separator2 = new QAction("", this);
        separator2->setSeparator(true);
        QAction *separator3 = new QAction("", this);
        separator3->setSeparator(true);
        QAction *separator4 = new QAction("", this);
        separator4->setSeparator(true);
        QAction *separator5 = new QAction("", this);
        separator5->setSeparator(true);

        fileMenuActions << fileOptions
                << separator1
                << fileFileListBrowserLocal
                << fileFileListRefresh
                << fileHashProgress
                << separator2
                << fileHubReconnect
                << fileQuickConnect
                << separator0
                << fileTransfers
                << fileDownloadQueue
                << fileFinishedDownloads
                << fileFinishedUploads
                << separator4
                << fileFavoriteHubs
                << fileFavoriteUsers
                << fileSearch
                << separator3
                << fileAntiSpam
                << fileIPFilter
                << separator5
                << fileQuit;
    }
    {
        aboutClient = new QAction("", this);
        connect(aboutClient, SIGNAL(triggered()), this, SLOT(slotAboutClient()));

        aboutQt = new QAction("", this);
        connect(aboutQt, SIGNAL(triggered()), this, SLOT(slotAboutQt()));
    }
}

void MainWindow::initMenuBar(){
    {
        menuFile = new QMenu("", this);

        menuFile->addActions(fileMenuActions);
    }
    {
        menuAbout = new QMenu("", this);

        menuAbout->addAction(aboutClient);
        menuAbout->addAction(aboutQt);
    }

    menuBar()->addMenu(menuFile);
    menuBar()->addMenu(menuAbout);
}

void MainWindow::initStatusBar(){
    statusLabel = new QLabel(statusBar());
    statusLabel->setFrameShadow(QFrame::Plain);
    statusLabel->setFrameShape(QFrame::NoFrame);

    msgLabel = new QLabel(statusBar());
    msgLabel->setFrameShadow(QFrame::Plain);
    msgLabel->setFrameShape(QFrame::NoFrame);
    msgLabel->setAlignment(Qt::AlignRight);

    statusBar()->addPermanentWidget(msgLabel);
    statusBar()->addPermanentWidget(statusLabel);
}

void MainWindow::retranslateUi(){
    //Retranslate menu actions
    {
        menuFile->setTitle(tr("&File"));

        fileOptions->setText(tr("Options"));

        fileFileListBrowserLocal->setText(tr("Open own filelist"));

        fileFileListRefresh->setText(tr("Refresh share"));

        fileHashProgress->setText(tr("Hash progress"));

        fileHubReconnect->setText(tr("Reconnect to hub"));

        fileTransfers->setText(tr("Transfers"));

        fileDownloadQueue->setText(tr("Download queue"));

        fileFinishedDownloads->setText(tr("Finished downloads"));

        fileFinishedUploads->setText(tr("Finished uploads"));

        fileAntiSpam->setText(tr("AntiSpam module"));

        fileIPFilter->setText(tr("IPFilter module"));

        fileFavoriteHubs->setText(tr("Favourite hubs"));

        fileFavoriteUsers->setText(tr("Favourite users"));

        fileSearch->setText(tr("Search"));

        fileQuickConnect->setText(tr("Quick connect"));

        fileQuit->setText(tr("Quit"));

        menuAbout->setTitle(tr("&Help"));

        aboutClient->setText(tr("About EiskaltDC++"));

        aboutQt->setText(tr("About Qt"));
    }
    {
        arena->setWindowTitle(tr("Main layout"));
    }
}

void MainWindow::initToolbar(){
    fBar = new ToolBar(NULL);
    fBar->setObjectName("fBar");
    fBar->addActions(fileMenuActions);
    fBar->setContextMenuPolicy(Qt::CustomContextMenu);
    fBar->setMovable(true);
    fBar->setFloatable(true);
    fBar->setAllowedAreas(Qt::AllToolBarAreas);

    tBar = new ToolBar(NULL);
    tBar->setObjectName("tBar");
    tBar->initTabs();
    tBar->setContextMenuPolicy(Qt::CustomContextMenu);
    tBar->setMovable(true);
    tBar->setFloatable(true);
    tBar->setAllowedAreas(Qt::AllToolBarAreas);

    addToolBar(fBar);
    addToolBar(tBar);
}

void MainWindow::newHubFrame(QString address, QString enc){
    if (address.isEmpty())
        return;

    HubFrame *fr = NULL;

    if (fr = HubManager::getInstance()->getHub(address)){
        mapWidgetOnArena(fr);

        return;
    }

    fr = new HubFrame(NULL, address, enc);
    fr->setAttribute(Qt::WA_DeleteOnClose);

    addArenaWidget(fr);
    mapWidgetOnArena(fr);

    addArenaWidgetOnToolbar(fr);
}

void MainWindow::updateStatus(QMap<QString, QString> map){
    if (!statusLabel)
        return;

    QString stat = QString(tr("<b>%1 : %2  %4 : %5  %3</b>")).arg(map["DOWN"])
                                                        .arg(map["UP"])
                                                        .arg(map["STATS"])
                                                        .arg(map["DSPEED"])
                                                        .arg(map["USPEED"]);
    statusLabel->setText(stat);
}

void MainWindow::setStatusMessage(QString msg){
    msgLabel->setText(msg);
}

void MainWindow::autoconnect(){
    const FavoriteHubEntryList& fl = FavoriteManager::getInstance()->getFavoriteHubs();

    for(FavoriteHubEntryList::const_iterator i = fl.begin(); i != fl.end(); ++i) {
        FavoriteHubEntry* entry = *i;

        if (entry->getConnect()) {
            if (entry->getNick().empty() && SETTING(NICK).empty())
                continue;

            QString encoding = WulforUtil::getInstance()->dcEnc2QtEnc(QString::fromStdString(entry->getEncoding()));

            newHubFrame(QString::fromStdString(entry->getServer()), encoding);
        }
    }
}

void MainWindow::parseCmdLine(){
    QStringList args = qApp->arguments();

    foreach (QString arg, args){
        if (arg.startsWith("magnet:?xt=urn:tree:tiger:")){
            Magnet m(this);
            m.setLink(arg);

            m.exec();
        }
        else if (arg.startsWith("dchub://")){
            newHubFrame(arg, "");
        }
        else if (arg.startsWith("adc://") || arg.startsWith("adcs://")){
            newHubFrame(arg, "UTF-8");
        }
    }
}

void MainWindow::parseInstanceLine(QString data){
    QStringList args = data.split("\n", QString::SkipEmptyParts);

    foreach (QString arg, args){
        if (arg.startsWith("magnet:?xt=urn:tree:tiger:")){
            Magnet m(this);
            m.setLink(arg);

            m.exec();
        }
        else if (arg.startsWith("dchub://")){
            newHubFrame(arg, "");
        }
        else if (arg.startsWith("adc://") || arg.startsWith("adcs://")){
            newHubFrame(arg, "UTF-8");
        }
    }
}

void MainWindow::browseOwnFiles(){
    slotFileBrowseOwnFilelist();
}

void MainWindow::redrawToolPanel(){
    tBar->redraw();
}

void MainWindow::addArenaWidget(ArenaWidget *wgt){
    if (!arenaWidgets.contains(wgt) && wgt && wgt->getWidget()){
        arenaWidgets.push_back(wgt);
        arenaMap[wgt] = wgt->getWidget();
    }
}

void MainWindow::remArenaWidget(ArenaWidget *awgt){
    if (arenaWidgets.contains(awgt)){
        arenaWidgets.removeAt(arenaWidgets.indexOf(awgt));
        arenaMap.erase(arenaMap.find(awgt));

        if (arena->widget() == awgt->getWidget())
            arena->setWidget(NULL);
    }
}

void MainWindow::mapWidgetOnArena(ArenaWidget *awgt){
    if (!arenaWidgets.contains(awgt))
        return;

    if (arena->widget())
        arena->widget()->hide();

    arena->setWidget(arenaMap[awgt]);

    setWindowTitle(awgt->getArenaTitle() + " :: " + QString("%1").arg(EISKALTDCPP_WND_TITLE));

    tBar->mapped(awgt);
}

void MainWindow::remWidgetFromArena(ArenaWidget *awgt){
    if (!arenaWidgets.contains(awgt))
        return;

    if (arena->widget() == awgt->getWidget())
        arena->widget()->hide();
}

void MainWindow::addArenaWidgetOnToolbar(ArenaWidget *awgt){
    if (!arenaWidgets.contains(awgt))
        return;

    tBar->insertWidget(awgt);
}

void MainWindow::remArenaWidgetFromToolbar(ArenaWidget *awgt){
    tBar->removeWidget(awgt);
}

void MainWindow::toggleSingletonWidget(ArenaWidget *a){
    if (!a)
        return;

    if (tBar->hasWidget(a)){
        tBar->removeWidget(a);
        remWidgetFromArena(a);
    }
    else {
        tBar->insertWidget(a);
        mapWidgetOnArena(a);
    }
}

void MainWindow::startSocket(){
    SearchManager::getInstance()->disconnect();
    ConnectionManager::getInstance()->disconnect();

    if (ClientManager::getInstance()->isActive()) {
        QString msg = "";
        try {
            ConnectionManager::getInstance()->listen();
        } catch(const Exception &e) {
            msg = tr("Cannot listen socket because: \n") + QString::fromStdString(e.getError()) + tr("\n\nPlease check your connection settings");

            QMessageBox::warning(this, tr("Connection Manager: Warning"), msg, QMessageBox::Ok);
        }
        try {
            SearchManager::getInstance()->listen();
        } catch(const Exception &e) {
            msg = tr("Cannot listen socket because: \n") + QString::fromStdString(e.getError()) + tr("\n\nPlease check your connection settings");

            QMessageBox::warning(this, tr("Search Manager: Warning"), msg, QMessageBox::Ok);
        }
    }

    UPnPMapper::getInstance()->forward();
}

void MainWindow::showShareBrowser(dcpp::UserPtr usr, QString file, QString jump_to){
    ShareBrowser *sb = new ShareBrowser(usr, file, jump_to);
}

void MainWindow::slotFileBrowseOwnFilelist(){
    static ShareBrowser *local_share = NULL;

    if (arenaWidgets.contains(local_share)){
        mapWidgetOnArena(local_share);

        return;
    }

    UserPtr user = ClientManager::getInstance()->getMe();
    QString file = QString::fromStdString(ShareManager::getInstance()->getOwnListFile());

    local_share = new ShareBrowser(user, file, "");
}

void MainWindow::slotFileRefreshShare(){
    ShareManager *SM = ShareManager::getInstance();

    SM->setDirty();
    SM->refresh(true);

    HashProgress progress(this);
    progress.slotAutoClose(true);

    progress.exec();
}

void MainWindow::slotFileHashProgress(){
    HashProgress progress(this);

    progress.exec();
}

void MainWindow::slotFileReconnect(){
    HubFrame *fr = HubManager::getInstance()->activeHub();

    if (fr)
        fr->reconnect();
}

void MainWindow::slotFileSearch(){
    SearchFrame *sf = new SearchFrame();

    sf->setAttribute(Qt::WA_DeleteOnClose);
}

void MainWindow::slotFileDownloadQueue(){
    if (!DownloadQueue::getInstance())
        DownloadQueue::newInstance();

    toggleSingletonWidget(DownloadQueue::getInstance());
}

void MainWindow::slotFileFinishedDownloads(){
    if (!FinishedDownloads::getInstance())
        FinishedDownloads::newInstance();

    toggleSingletonWidget(FinishedDownloads::getInstance());
}

void MainWindow::slotFileFinishedUploads(){
    if (!FinishedUploads::getInstance())
        FinishedUploads::newInstance();

    toggleSingletonWidget(FinishedUploads::getInstance());
}

void MainWindow::slotFileAntiSpam(){
    AntiSpamFrame fr(this);

    fr.exec();
}

void MainWindow::slotFileIPFilter(){
    IPFilterFrame fr(this);

    fr.exec();
}

void MainWindow::slotFileFavoriteHubs(){
    if (!FavoriteHubs::getInstance())
        FavoriteHubs::newInstance();

    toggleSingletonWidget(FavoriteHubs::getInstance());
}

void MainWindow::slotFileFavoriteUsers(){
    if (!FavoriteUsers::getInstance())
        FavoriteUsers::newInstance();

    toggleSingletonWidget(FavoriteUsers::getInstance());
}

void MainWindow::slotFileSettings(){
    Settings s;

    s.exec();
}

void MainWindow::slotFileTransfer(bool toggled){
    if (toggled){
        transfer_dock->setVisible(true);
        transfer_dock->setWidget(TransferView::getInstance());
    }
    else {
        transfer_dock->setWidget(NULL);
        transfer_dock->setVisible(false);
    }
}

void MainWindow::slotQC(){
    QuickConnect qc;

    qc.exec();
}

void MainWindow::slotExit(){
    setUnload(true);

    close();
}

void MainWindow::slotAboutClient(){
    About a(this);

    a.exec();
}

void MainWindow::slotAboutQt(){
    QMessageBox::aboutQt(this);
}

void MainWindow::on(dcpp::LogManagerListener::Message, time_t t, const std::string& m) throw(){
    QTextCodec *codec = QTextCodec::codecForLocale();

    typedef Func1<MainWindow, QString> FUNC;
    FUNC *func = new FUNC(this, &MainWindow::setStatusMessage, codec->toUnicode(m.c_str()));

    QApplication::postEvent(this, new MainWindowCustomEvent(func));
}

void MainWindow::on(dcpp::QueueManagerListener::Finished, QueueItem *item, const std::string &dir, int64_t) throw(){
    if (item->isSet(QueueItem::FLAG_CLIENT_VIEW | QueueItem::FLAG_USER_LIST)){
        UserPtr user = item->getDownloads()[0]->getUser();
        QString listName = QString::fromStdString(item->getListName());

        typedef Func3<MainWindow, UserPtr, QString, QString> FUNC;
        FUNC *func = new FUNC(this, &MainWindow::showShareBrowser, user, listName, QString::fromStdString(dir));

        QApplication::postEvent(this, new MainWindowCustomEvent(func));
    }
}

void MainWindow::on(dcpp::TimerManagerListener::Second, uint32_t ticks) throw(){
    static quint32 lastUpdate = 0;
    static quint64 lastUp = 0, lastDown = 0;

    quint64 now = GET_TICK();

    quint64 diff = now - lastUpdate;
    quint64 downBytes = 0;
    quint64 upBytes = 0;

    if (diff < 100U)
        diff = 1U;

    quint64 downDiff = Socket::getTotalDown() - lastDown;
    quint64 upDiff = Socket::getTotalUp() - lastUp;

    downBytes = (downDiff * 1000) / diff;
    upBytes = (upDiff * 1000) / diff;

    QMap<QString, QString> map;

    map["STATS"]    = _q(Client::getCounts());
    map["DSPEED"]   = _q(Util::formatBytes(downBytes)) + tr("/s");
    map["DOWN"]     = _q(Util::formatBytes(Socket::getTotalDown()));
    map["USPEED"]   = _q(Util::formatBytes(upBytes)) + tr("/s");
    map["UP"]       = _q(Util::formatBytes(Socket::getTotalUp()));

    lastUpdate = ticks;
    lastUp = Socket::getTotalUp();
    lastDown = Socket::getTotalDown();

    typedef Func1<MainWindow, QMap<QString, QString> > FUNC;
    FUNC *func = new FUNC(this, &MainWindow::updateStatus, map);

    QApplication::postEvent(this, new MainWindowCustomEvent(func));
}

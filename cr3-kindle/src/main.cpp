// CoolReader3 / Qt
// main.cpp - entry point
#include "mainwindow.h"

#undef i386 // uncomment when building for desktop, it's for Qt Creator

#ifndef i386
#include "../../drivers/QKindleFb/qkindlefb.h"

#include "touchscreen.h"
TouchScreen *pTouch;

#endif


MyApplication *pMyApp;

int main(int argc, char *argv[])
{
    int res = 0;
    {
        Device::instance(); // initialize device
#ifndef i386
        pTouch = new TouchScreen();
#endif
        lString16 exedir = LVExtractPath(LocalToUnicode(lString8(argv[0])));
        LVAppendPathDelimiter(exedir);
        lString16 datadir = exedir + L"data/";
        lString16 exefontpath = exedir + L"fonts";

        lString16Collection fontDirs;
        fontDirs.add(exefontpath);
#ifndef i386
        fontDirs.add(lString16(L"/usr/java/lib/fonts"));
        fontDirs.add(lString16(L"/mnt/us/fonts"));
#endif
        CRPropRef props = LVCreatePropsContainer();
        {
            LVStreamRef cfg = LVOpenFileStream(UnicodeToUtf8(datadir + L"cr3.ini").data(), LVOM_READ);
            if(!cfg.isNull()) props->loadFromStream(cfg.get());
        }

        lString16 lang = props->getStringDef(PROP_WINDOW_LANG, "");
        InitCREngineLog(props);
        CRLog::info("main()");

        if(!InitCREngine(argv[0], fontDirs)) {
            printf("Cannot init CREngine - exiting\n");
            return 2;
        }
#ifndef i386
        if (!Device::isTouch()) {
            PrintString(1, 1, "crengine version: " + QString(CR_ENGINE_VERSION), "-c");
            PrintString(1, 2, QString("buid date: %1 %2").arg(__DATE__).arg(__TIME__));
            QString message = "Please wait while application is loading...";
            int xpos = ((Device::getWidth()/12-1)-message.length())/2;
            int ypos = (Device::getHeight()/20-2)/2;
            PrintString(xpos, ypos, message);
        }
#endif

        // set row count
        int rc = props->getIntDef(PROP_WINDOW_ROW_COUNT, 0);
        if(!rc) {
#ifndef i386
            props->setInt(PROP_WINDOW_ROW_COUNT, Device::getModel() != Device::KDX ? 10 : 16);
#else
            props->setInt(PROP_WINDOW_ROW_COUNT, 10);
#endif
            LVStreamRef cfg = LVOpenFileStream(UnicodeToUtf8(datadir + L"cr3.ini").data(), LVOM_WRITE);
            props->saveToStream(cfg.get());
        }

        QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
        MyApplication a(argc, argv);
        pMyApp = &a;
        // set app stylesheet
#ifndef i386
        QString style = (Device::getModel() != Device::KDX ? "stylesheet_k3.qss" : "stylesheet_dx.qss");
        if (Device::getModel() == Device::KPW) style = "stylesheet_pw.qss";
        QFile qss(QDir::toNativeSeparators(cr2qt(datadir)) + style);
        // set up full update interval for the graphics driver
        QKindleFb *pscreen = static_cast<QKindleFb*>(QScreen::instance());
        pscreen->setFullUpdateEvery(props->getIntDef(PROP_DISPLAY_FULL_UPDATE_INTERVAL, 1));
#else
        QFile qss(QDir::toNativeSeparators(cr2qt(datadir)) + "stylesheet_k3.qss");
#endif
        qss.open(QFile::ReadOnly);
        if(qss.error() == QFile::NoError) {
            a.setStyleSheet(qss.readAll());
            qss.close();
        }
        QString translations = cr2qt(datadir) + "i18n";
        QTranslator myappTranslator;
        if(!lang.empty() && lang.compare(L"English")) {
            if(myappTranslator.load(cr2qt(lang), translations))
                QApplication::installTranslator(&myappTranslator);
            else
                qDebug("Can`t load translation file %s from dir %s", UnicodeToUtf8(lang).c_str(), UnicodeToUtf8(qt2cr(translations)).c_str());
        }

        (void) signal(SIGUSR1, sigCatcher);

        MainWindow mainWin;
        a.setMainWindow(&mainWin);
        mainWin.showFullScreen();
        mainWin.doStartupActions();
        res = a.exec();
    }
    ShutdownCREngine();
    return res;
}

void ShutdownCREngine()
{
    HyphMan::uninit();
    ShutdownFontManager();
    CRLog::setLogger(NULL);
}

bool getDirectoryFonts( lString16Collection & pathList, lString16 ext, lString16Collection & fonts, bool absPath )
{
    int foundCount = 0;
    lString16 path;
    for (int di=0; di<pathList.length();di++ ) {
        path = pathList[di];
        LVContainerRef dir = LVOpenDirectory(path.c_str());
        if(!dir.isNull()) {
            CRLog::trace("Checking directory %s", UnicodeToUtf8(path).c_str() );
            for(int i=0; i < dir->GetObjectCount(); i++ ) {
                const LVContainerItemInfo * item = dir->GetObjectInfo(i);
                lString16 fileName = item->GetName();
                if ( !item->IsContainer() && fileName.length()>4 && lString16(fileName, fileName.length()-4, 4)==ext ) {
                    lString16 fn;
                    if ( absPath ) {
                        fn = path;
                        if (!fn.empty() && fn[fn.length()-1]!=PATH_SEPARATOR_CHAR)
                            fn << PATH_SEPARATOR_CHAR;
                    }
                    fn << fileName;
                    foundCount++;
                    fonts.add(fn);
                }
            }
        }
    }
    return foundCount > 0;
}

bool InitCREngine( const char * exename, lString16Collection & fontDirs)
{
    CRLog::trace("InitCREngine(%s)", exename);

    InitFontManager(lString8());

    // Load font definitions into font manager
    // fonts are in files font1.lbf, font2.lbf, ... font32.lbf
    // use fontconfig

    lString16 fontExt = L".ttf";
    lString16Collection fonts;

    getDirectoryFonts( fontDirs, fontExt, fonts, true );
    // load fonts from file
    CRLog::debug("%d font files found", fonts.length());
    if (!fontMan->GetFontCount()) {
        for (int fi=0; fi<fonts.length(); fi++ ) {
            lString8 fn = UnicodeToLocal(fonts[fi]);
            CRLog::trace("loading font: %s", fn.c_str());
            if ( !fontMan->RegisterFont(fn) )
                CRLog::trace("    failed\n");
        }
    }

    if (!fontMan->GetFontCount()) {
        printf("Fatal Error: Cannot open font file(s) .ttf \nCannot work without font\n" );
        return false;
    }
    printf("%d fonts loaded.\n", fontMan->GetFontCount());
    return true;
}

void InitCREngineLog(CRPropRef props)
{
    if(props.isNull())
    {
        CRLog::setStdoutLogger();
        CRLog::setLogLevel( CRLog::LL_FATAL);
        return;
    }
    lString16 logfname = props->getStringDef(PROP_LOG_FILENAME, "stdout");
#ifdef _DEBUG
    //	lString16 loglevelstr = props->getStringDef(PROP_LOG_LEVEL, "DEBUG");
    lString16 loglevelstr = props->getStringDef(PROP_LOG_LEVEL, "OFF");
#else
    lString16 loglevelstr = props->getStringDef(PROP_LOG_LEVEL, "OFF");
#endif
    bool autoFlush = props->getBoolDef(PROP_LOG_AUTOFLUSH, false);

    CRLog::log_level level = CRLog::LL_INFO;
    if ( loglevelstr==L"OFF" ) {
        level = CRLog::LL_FATAL;
        logfname.clear();
    } else if ( loglevelstr==L"FATAL" ) {
        level = CRLog::LL_FATAL;
    } else if ( loglevelstr==L"ERROR" ) {
        level = CRLog::LL_ERROR;
    } else if ( loglevelstr==L"WARN" ) {
        level = CRLog::LL_WARN;
    } else if ( loglevelstr==L"INFO" ) {
        level = CRLog::LL_INFO;
    } else if ( loglevelstr==L"DEBUG" ) {
        level = CRLog::LL_DEBUG;
    } else if ( loglevelstr==L"TRACE" ) {
        level = CRLog::LL_TRACE;
    }
    if ( !logfname.empty() ) {
        if ( logfname==L"stdout" )
            CRLog::setStdoutLogger();
        else if ( logfname==L"stderr" )
            CRLog::setStderrLogger();
        else
            CRLog::setFileLogger(UnicodeToUtf8( logfname ).c_str(), autoFlush);
    }
    CRLog::setLogLevel(level);
    CRLog::trace("Log initialization done.");
}

void wakeUp()
{
    QWidget *active;

    active = qApp->activeWindow();

    Device::suspendFramework();
    if(!active->isFullScreen() && !active->isMaximized()) {
        QWidget *mainwnd = qApp->widgetAt(0,0);
        mainwnd->repaint();
    }

    if (Device::isTouch()) QWSServer::instance()->enablePainting(true);

    active->repaint();
    pMyApp->connectSystemBus();
}

void gotoSleep() {
    pMyApp->disconnectSystemBus();
}

int buttonState = 0;
int newButtonState = 0;
int lastEvent = 0;
bool focusInReader = true;
int oldX = 0;
int oldY = 0;

#define LONG_TAP 500
#define MIN_SWIPE_PIXELS 200

bool myEventFilter(void *message, long *)
{
    QWSEvent* pev = static_cast<QWSEvent*>(message);
    QWSKeyEvent* pke = static_cast<QWSKeyEvent*>(message);
    QWSMouseEvent* pme = pev->asMouse();

    if(pev->type == QWSEvent::Key)
    {
#ifdef i386
        if(pke->simpleData.keycode == Qt::Key_Return) {
            pke->simpleData.keycode = Qt::Key_Select;
            return false;
        }
#endif
        if(pke->simpleData.keycode == Qt::Key_Sleep) {
            gotoSleep();
            return true;
        }
        if(pke->simpleData.keycode == Qt::Key_WakeUp) {
            wakeUp();
            return true;
        }
        // qDebug("QWS key: key=%x, press=%x, uni=%x", pke->simpleData.keycode, pke->simpleData.is_press, pke->simpleData.unicode);
        return false;
    }
#ifndef i386
    else if (pev->type == QWSEvent::Mouse) {
        newButtonState = pme->simpleData.state;

        // save focus state when button was pressed
        if (newButtonState > 0) {
            focusInReader = pMyApp->getMainWindow() && pMyApp->getMainWindow()->isFocusInReader();
            oldX = pme->simpleData.x_root;
            oldY = pme->simpleData.y_root;
        }

        qDebug("mouse: x: %d, y: %d, state: %d, time: %d, %d", pme->simpleData.x_root, pme->simpleData.y_root, pme->simpleData.state, pme->simpleData.time - lastEvent, focusInReader);

        // touch released
        if (newButtonState == 0 && buttonState != 0) {
            int x = pme->simpleData.x_root;
            int y = pme->simpleData.y_root;

            bool isGesture = false;
            if (abs(oldX - x) > MIN_SWIPE_PIXELS || abs(oldY - y) > MIN_SWIPE_PIXELS) isGesture = true;

            TouchScreen::TouchType t = TouchScreen::SINGLE_TAP;
            if (buttonState == Qt::RightButton) t = TouchScreen::DOUBLE_TAP;

            if (t == TouchScreen::SINGLE_TAP && lastEvent != 0 && pme->simpleData.time - lastEvent > LONG_TAP && !isGesture) {
                qDebug("long tap");
                if (focusInReader) {
                    QWSServer::sendKeyEvent(-1, Qt::Key_Menu, Qt::NoModifier, true, false);
                    QWSServer::sendKeyEvent(-1, Qt::Key_Menu, Qt::NoModifier, false, false);
                } else {
                    QWSServer::sendKeyEvent(-1, Qt::Key_Select, Qt::NoModifier, true, false);
                    QWSServer::sendKeyEvent(-1, Qt::Key_Select, Qt::NoModifier, false, false);
                }
                return true;
            }

            // handle custom single tap actions only when in reader
            if (t == TouchScreen::SINGLE_TAP && !focusInReader && !isGesture) {
                return false;
            }

            if (t == TouchScreen::SINGLE_TAP && isGesture) {
                // up - down
                if (y - oldY > MIN_SWIPE_PIXELS) {
                    qDebug("* down swipe");
                    QWSServer::sendKeyEvent(-1, Qt::Key_PageDown, Qt::NoModifier, true, false);
                    QWSServer::sendKeyEvent(-1, Qt::Key_PageDown, Qt::NoModifier, false, false);
                    return true;
                }
                // down - up
                else if (oldY - y > MIN_SWIPE_PIXELS) {
                    qDebug("* up swipe");
                    QWSServer::sendKeyEvent(-1, Qt::Key_PageUp, Qt::NoModifier, true, false);
                    QWSServer::sendKeyEvent(-1, Qt::Key_PageUp, Qt::NoModifier, false, false);
                    return true;
                }
                // right - left
                else if (oldX - x > MIN_SWIPE_PIXELS) {
                    qDebug("* left swipe");
                    QWSServer::sendKeyEvent(-1, Qt::Key_Escape, Qt::NoModifier, true, false);
                    QWSServer::sendKeyEvent(-1, Qt::Key_Escape, Qt::NoModifier, false, false);
                    return true;
                }
                return false;
            }

            Qt::Key key = pTouch->getAreaAction(pme->simpleData.x_root, pme->simpleData.y_root, t);

            if (key != Qt::Key_unknown) {
                QWSServer::sendKeyEvent(-1, key, Qt::NoModifier, true, false);
                QWSServer::sendKeyEvent(-1, key, Qt::NoModifier, false, false);
                buttonState = newButtonState;
                return true;
            } else {
                qDebug("-- area action not defined");
            }
        }
        lastEvent = pme->simpleData.time;
        buttonState = newButtonState;
    }
#endif
    return false;
}

void PrintString(int x, int y, const QString message, const QString opt) {
    QString cmd = QString("/usr/sbin/eips %1 %2 \"%3\" %4").arg(QString().number(x)).arg(QString().number(y)).arg(message).arg(opt);
    system(cmd.toAscii());
}

void sigCatcher(int sig) {
  if(sig == SIGUSR1) {
      Device::isTouch() ? QWSServer::instance()->openMouse() : QWSServer::instance()->openKeyboard();
      wakeUp();
  }
}

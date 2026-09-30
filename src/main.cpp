/***************************************************************************
 *   Copyright (C) 2012 by Santiago González                               *
 *                                                                         *
 ***( see copyright.txt file at root folder )*******************************/

#include <QApplication>
#include <QTranslator>
#include <QStandardPaths>
#include <QtGui>

#include "mainwindow.h"
#include "circuitwidget.h"
#include "editorwindow.h"
#include "batchtest.h"
#include "simuserver.h"
#include "scenariorunner.h"

void myMessageOutput( QtMsgType type, const QMessageLogContext &context, const QString &msg )
{
    QByteArray localMsg = msg.toLocal8Bit();
    const char* file     = context.file ? context.file : "";
    const char* function = context.function ? context.function : "";
    switch (type) {
    case QtDebugMsg:
        if( CircuitWidget::self() ) CircuitWidget::self()->simDebugMessage( msg );
        fprintf( stderr, "%s \n", localMsg.constData() );
        break;
    case QtInfoMsg:
        fprintf(stderr, "Info: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtWarningMsg:
        fprintf(stderr, "Warning: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtCriticalMsg:
        fprintf(stderr, "Critical: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    case QtFatalMsg:
        fprintf(stderr, "Fatal: %s (%s:%u, %s)\n", localMsg.constData(), file, context.line, function);
        break;
    }
}

QString langFile( QString locale )
{
    QString langF = ":/simulide_"+locale+".qm";

    if( !QFile::exists( langF ) ) langF = "";

    return langF;
}

int main( int argc, char *argv[] )
{
    qInstallMessageHandler( myMessageOutput );

#ifdef _WIN32
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
#endif

    // Check for -nogui-ci / -test-ci before QApplication is created so we can
    // force Qt onto a non-windowed platform plugin. These are CI-safe siblings
    // of -nogui/-test: -nogui/-test are left untouched for backward
    // compatibility (still create a real, if hidden/shown, window and need a
    // display); -nogui-ci and -test-ci never create a window at all.
    bool ciOffscreen = false;
    bool testCiRequested = false;
    QString testCiPath;
    for( int i = 1; i < argc; ++i )
    {
        QString a = QString::fromLocal8Bit( argv[i] );
        if( a == "-nogui-ci" ) ciOffscreen = true;
        else if( a == "-test-ci" )
        {
            ciOffscreen = true;
            testCiRequested = true;
            if( i+1 < argc ) testCiPath = QString::fromLocal8Bit( argv[++i] );
        }
    }
    if( ciOffscreen ) qputenv( "QT_QPA_PLATFORM", QByteArray("offscreen") );

    QApplication app( argc, argv );

    QSettings settings( QStandardPaths::standardLocations( QStandardPaths::AppDataLocation).first()+"/simulide.ini",  QSettings::IniFormat, 0l );

    QString locale = QLocale::system().name();
    if( settings.contains( "language" ) ) locale = settings.value( "language" ).toString();

    QString langF = langFile( locale );
    if( langF == "" )
    {
        locale = QLocale::system().name().split("_").first();
        langF = langFile( locale );
    }
    if( langF == "" ) langF = ":/simulide_en.qm";

    QTranslator translator;
    if( translator.load( langF ) )
        app.installTranslator( &translator );

    app.setApplicationVersion( APP_VERSION );

    MainWindow window;
    window.setLoc( locale );
    if( !ciOffscreen ) window.show();

    bool noGui = false;

    for( int i=1; i<argc; ++i )
    {
        QString arg = QString::fromStdString( argv[i] );

        if( arg == "-nogui-ci" ) {} // already handled above
        else if( arg == "-test-ci" ) { ++i; } // already handled above, skip its path arg
        else if( arg == "-nogui")
        {
            window.hideGui();
            noGui = true;
        }
        else if( arg == "-test" )
        {
            i++;
            if( i >= argc ){
                qDebug() <<"ERROR: missing argument for"<< arg;
                break;
            }
            arg = QString::fromStdString( argv[i] );
            QTimer::singleShot( 500, [arg](){ BatchTest::doBatchTest( arg ); } );
            break;
        }
        else{
            QString file = "file://";
            if( arg.startsWith( file ) ) arg.replace( file, "" ).replace("\r\n", "" ).replace("%20", " ");
#ifdef _WIN32
            if( arg.startsWith( "/" )) arg.remove( 0, 1 );
#endif
            if( !QFile::exists( arg ) ){
                qDebug() <<"ERROR: unrecognized argument"<< arg;
                break;
            }
            if( arg.endsWith(".sim2") || arg.endsWith(".sim1"))
            {
                QTimer::singleShot( 500, CircuitWidget::self()
                                  , [arg,noGui]()->void{ CircuitWidget::self()->loadCirc( arg );
                                                   if( noGui ) MainWindow::self()->hideGui(); } );
            }
            else{
                QTimer::singleShot( 500, CircuitWidget::self()
                                   , [arg]()->void{ EditorWindow::self()->loadFile( arg ); } );
            }
            break;
        }
    }

    // -nogui-ci: start the socket control API so an external driver (or our
    // own tests/simctl.py) can run/inspect the simulation. Not started for
    // plain -nogui or a normal GUI launch — only opt-in CI mode listens.
    // Delayed past the generic per-argv file loader's own 500ms
    // QTimer::singleShot (above) — otherwise a fast client can connect and
    // send "run"/etc. before an auto-loaded circuit has actually loaded.
    if( ciOffscreen && !testCiRequested )
    {
        QTimer::singleShot( 600, [&window](){
            SimuServer* server = new SimuServer( &window );
            server->listen();
        });
    }
    // -test-ci: self-contained scenario runner, no socket/external driver
    // needed. Exits the process itself with a pass/fail code.
    else if( testCiRequested )
    {
        QTimer::singleShot( 500, [testCiPath](){
            int code = ScenarioRunner::runAll( testCiPath );
            QCoreApplication::exit( code );
        });
    }

    return app.exec();
}


/***************************************************************************
 *   Copyright (C) 2012 by Santiago González                               *
 *                                                                         *
 ***( see copyright.txt file at root folder )*******************************/

#include <QApplication>
#include <QTranslator>
#include <QStandardPaths>
#include <QTimer>
#include <QtGui>
#include <cstring>

#include "mainwindow.h"
#include "circuitwidget.h"
#include "simuserver.h"

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

    QFile file( langF );
    if( !file.exists() ) langF = "";

    return langF;
}

int main( int argc, char *argv[] )
{
    qInstallMessageHandler( myMessageOutput );

    // Check for --headless / -H before QApplication is created so we can
    // force Qt onto a non-windowed platform plugin.
    bool headless = false;
    for( int i = 1; i < argc; ++i )
    {
        if( strcmp(argv[i], "--headless") == 0 || strcmp(argv[i], "-H") == 0 )
        {
            headless = true;
            break;
        }
    }

    if( headless )
    {
        qputenv( "QT_QPA_PLATFORM", QByteArray("offscreen") );
    }

    QApplication app( argc, argv );

    QSettings settings( QStandardPaths::standardLocations( QStandardPaths::DataLocation).first()+"/simulide.ini",  QSettings::IniFormat, 0l );

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
    translator.load( langF );
    app.installTranslator( &translator );
    app.setApplicationVersion( APP_VERSION );

    MainWindow window;
    window.setLoc( locale );

    // Parse arguments: [circuit_file] [--run] [--headless|-H]
    QString circFile;
    bool autoRun = false;
    for( int i=1; i<argc; ++i )
    {
        QString arg = QString::fromLocal8Bit( argv[i] );
        if( arg == "--run" ) autoRun = true;
        else if( arg == "--headless" || arg == "-H" ) {} // already handled
        else if( arg == "-platform" ) { ++i; } // skip platform value
        else if( arg.endsWith(".simu") || arg.endsWith(".sim1") ) circFile = arg;
    }
    if( !circFile.isEmpty() ) CircuitWidget::self()->loadCirc( circFile );
    if( autoRun ) QTimer::singleShot( 500, CircuitWidget::self(), &CircuitWidget::powerCircOn );

    // Start Unix socket control server
    SimuServer* server = new SimuServer( &window );
    server->listen();

    if( headless )
        fprintf( stderr, "SimulIDE: running headless (offscreen)\n" );
    else
        window.show();

    return app.exec();
}

/***************************************************************************
 *   SimuServer — Unix socket control interface for SimulIDE               *
 ***************************************************************************/

#include <QDebug>
#include <QFile>
#include <QTimer>
#include <QCoreApplication>

#include "simuserver.h"
#include "circuitwidget.h"
#include "circuit.h"
#include "simulator.h"
#include "subcircuit.h"
#include "mcu.h"
#include "usartmodule.h"
#include "potentiometer.h"
#include "push_base.h"
#include "pin.h"

#define SOCKET_PATH "/tmp/simulide.sock"
#define WAIT_TICK_MS 50  // poll interval for wait_serial

SimuServer::SimuServer( QObject* parent )
    : QObject( parent )
    , m_server( new QLocalServer( this ) )
    , m_client( nullptr )
    , m_waitTimer( new QTimer( this ) )
    , m_waitMode( WaitNone )
    , m_waiting( false )
{
    m_waitTimer->setInterval( WAIT_TICK_MS );
    connect( m_waitTimer, &QTimer::timeout, this, &SimuServer::onWaitTick );
}

SimuServer::~SimuServer()
{
    QLocalServer::removeServer( SOCKET_PATH );
}

bool SimuServer::listen()
{
    QLocalServer::removeServer( SOCKET_PATH ); // remove stale socket
    if( !m_server->listen( SOCKET_PATH ) )
    {
        qWarning() << "SimuServer: failed to listen on" << SOCKET_PATH;
        return false;
    }
    connect( m_server, &QLocalServer::newConnection, this, &SimuServer::onNewConnection );
    qDebug() << "SimuServer: listening on" << SOCKET_PATH;
    return true;
}

void SimuServer::onNewConnection()
{
    QLocalSocket* sock = m_server->nextPendingConnection();
    if( !sock ) return;

    // Keep only one client; disconnect previous
    if( m_client )
    {
        if( m_waiting )
        {
            m_waitTimer->stop();
            m_waitMode = WaitNone;
            m_waiting = false;
        }
        m_client->disconnectFromServer();
        m_client->deleteLater();
    }
    m_client = sock;
    connect( m_client, &QLocalSocket::disconnected, this, [this, sock](){
        if( m_client != sock ) return;
        if( m_waiting )
        {
            m_waitTimer->stop();
            m_waitMode = WaitNone;
            m_waiting = false;
        }
        m_client->deleteLater();
        m_client = nullptr;
    });
    connect( m_client, &QLocalSocket::readyRead, this, &SimuServer::onReadyRead );
}

void SimuServer::onReadyRead()
{
    QLocalSocket* client = m_client;
    if( !client ) return;
    if( m_waiting ) return; // busy with wait_serial — drain later

    while( client == m_client && client->canReadLine() )
    {
        QString line = QString::fromUtf8( client->readLine() ).trimmed();
        if( line.isEmpty() ) continue;
        handleCommand( line );
        if( client != m_client ) break; // command closed/replaced the socket
        if( m_waiting ) break; // wait_serial took over; rest will be read after
    }
}



void SimuServer::sendResponse( const QString& resp )
{
    if( !m_client ) return;
    if( m_client->state() != QLocalSocket::ConnectedState ) return;
    m_client->write( (resp + "\n").toUtf8() );
    m_client->flush();
    // Close connection after each response so clients know when to stop reading.
    // wait_serial responses are also sent here, then connection closes.
    if( !m_waiting )
    {
        m_client->disconnectFromServer();
        // Don't delete here — let QLocalSocket handle lifecycle
    }
}

UsartModule* SimuServer::findUsart( const QString& circId, int uartIndex )
{
    Circuit* circuit = Circuit::self();
    if( !circuit ) return nullptr;

    Component* comp = circuit->getCompById( circId );
    if( !comp ) return nullptr;

    Mcu* mcu = dynamic_cast<Mcu*>( comp );
    if( !mcu )
    {
        SubCircuit* subc = dynamic_cast<SubCircuit*>( comp );
        if( subc ) mcu = dynamic_cast<Mcu*>( subc->getMainComp( circId ) );
    }
    if( !mcu ) return nullptr;

    // uartIndex is 1-based; default to 1
    int n = (uartIndex >= 1) ? uartIndex : 1;
    return mcu->getUsart( n );
}

void SimuServer::handleCommand( const QString& cmd )
{
    CircuitWidget* cw = CircuitWidget::self();
    Simulator*     sim = Simulator::self();

    // ── load <path> ──────────────────────────────────────────────
    if( cmd.startsWith("load ") )
    {
        QString path = cmd.mid(5).trimmed();
        if( cw ) QTimer::singleShot( 0, [cw, path](){ cw->loadCirc( path ); });
        sendResponse("ok");
    }
    // ── run ──────────────────────────────────────────────────────
    else if( cmd == "run" )
    {
        if( cw ) QTimer::singleShot( 0, [cw](){ cw->powerCircOn(); });
        sendResponse("ok");
    }
    // ── stop ─────────────────────────────────────────────────────
    else if( cmd == "stop" )
    {
        if( cw ) QTimer::singleShot( 0, [cw](){ cw->powerCircOff(); });
        sendResponse("ok");
    }
    // ── pause ────────────────────────────────────────────────────
    else if( cmd == "pause" )
    {
        if( cw ) QTimer::singleShot( 0, [cw](){ cw->pauseCirc(); });
        sendResponse("ok");
    }
    // ── reset ────────────────────────────────────────────────────
    else if( cmd == "reset" )
    {
        if( cw ) QTimer::singleShot( 0, [cw](){
            cw->powerCircOff();
            QTimer::singleShot( 200, [cw](){ cw->powerCircOn(); });
        });
        sendResponse("ok");
    }
    // ── quit ─────────────────────────────────────────────────────
    else if( cmd == "quit" )
    {
        if( cw )
        {
            QTimer::singleShot( 0, [cw](){
                cw->powerCircOff();
                QTimer::singleShot( 100, [](){ QCoreApplication::quit(); });
            });
        }
        else QTimer::singleShot( 0, [](){ QCoreApplication::quit(); });
        sendResponse("ok");
    }
    // ── status ───────────────────────────────────────────────────
    else if( cmd == "status" )
    {
        QString st = "stopped";
        if( sim )
        {
            if     ( sim->isPaused()  ) st = "paused";
            else if( sim->isRunning() ) st = "running";
        }
        sendResponse( st );
    }
    // ── wait <ms> ─────────────────────────────────────────────────
    else if( cmd.startsWith("wait ") )
    {
        bool ok = false;
        int waitMs = cmd.mid(5).trimmed().toInt( &ok );
        if( !ok || waitMs < 0 )
        {
            sendResponse("error: usage: wait <ms>");
            return;
        }
        if( waitMs == 0 )
        {
            sendResponse("ok");
            return;
        }
        if( !sim || !sim->isRunning() )
        {
            sendResponse("error: simulation not running");
            return;
        }

        m_waitSimTime.targetUs = (sim->circTime() / 1000000ULL) + (uint64_t)waitMs * 1000ULL;
        m_waitMode = WaitSimTimeMode;
        m_waiting = true;
        m_waitTimer->start();
        // Response will be sent from onWaitTick()
    }
    // ── simtime ──────────────────────────────────────────────────
    else if( cmd == "simtime" )
    {
        uint64_t us = sim ? (sim->circTime() / 1000000ULL) : 0;
        sendResponse( QString::number( us ) );
    }
    // ── serial <circId> [<uartN>] ────────────────────────────────
    else if( cmd.startsWith("serial ") )
    {
        QStringList parts = cmd.split(' ', Qt::SkipEmptyParts);
        // parts: ["serial", circId, optional_uartN]
        if( parts.size() < 2 )
        {
            sendResponse("error: usage: serial <circId> [uartN]");
            return;
        }
        QString circId = parts.at(1);
        int uartN = (parts.size() >= 3) ? parts.at(2).toInt() : 1;

        UsartModule* usart = findUsart( circId, uartN );
        if( !usart )
        {
            sendResponse("error: component not found or no USART: " + circId);
            return;
        }
        QByteArray buf = usart->serialCapture();
        if( m_client )
        {
            m_client->write( buf );
            m_client->flush();
            m_client->disconnectFromServer();
        }
    }
    // ── serial_clear <circId> [<uartN>] ──────────────────────────
    else if( cmd.startsWith("serial_clear ") )
    {
        QStringList parts = cmd.split(' ', Qt::SkipEmptyParts);
        if( parts.size() < 2 )
        {
            sendResponse("error: usage: serial_clear <circId> [uartN]");
            return;
        }
        QString circId = parts.at(1);
        int uartN = (parts.size() >= 3) ? parts.at(2).toInt() : 1;

        UsartModule* usart = findUsart( circId, uartN );
        if( !usart )
        {
            sendResponse("error: component not found: " + circId);
            return;
        }
        usart->serialCaptureClear();
        sendResponse("ok");
    }
    // ── send_serial <circId> [<uartN>] <text> ────────────────────
    // Feeds `text` into the target's USART RX, byte by byte, as if it had
    // arrived over the wire. Wokwi-parity equivalent of write-serial.
    else if( cmd.startsWith("send_serial ") )
    {
        QStringList parts = cmd.split(' ', Qt::SkipEmptyParts);
        if( parts.size() < 3 )
        {
            sendResponse("error: usage: send_serial <circId> [uartN] <text>");
            return;
        }
        QString circId = parts.at(1);
        int uartN = 1;
        int textStart = 2;
        bool isUartNum = false;
        int candidate = parts.at(2).toInt(&isUartNum);
        if( isUartNum && candidate >= 1 && candidate <= 8 && parts.size() >= 4 )
        {
            uartN = candidate;
            textStart = 3;
        }
        QString text = parts.mid( textStart ).join(' ');

        UsartModule* usart = findUsart( circId, uartN );
        if( !usart )
        {
            sendResponse("error: component not found: " + circId);
            return;
        }
        QByteArray bytes = text.toUtf8();
        for( char c : bytes ) usart->receiveByte( (uint8_t)c );
        sendResponse("ok");
    }
    // ── wait_serial <circId> [<uartN>] <pattern> <timeout_ms> ───
    else if( cmd.startsWith("wait_serial ") )
    {
        // Syntax variants:
        //   wait_serial <circId> <pattern> <timeout_ms>
        //   wait_serial <circId> <uartN>   <pattern> <timeout_ms>
        QStringList parts = cmd.split(' ', Qt::SkipEmptyParts);
        // parts[0]="wait_serial" parts[1]=circId
        // Then either: [pattern] [timeout]   (3 more)
        //          or: [uartN] [pattern] [timeout] (4 more)
        if( parts.size() < 4 )
        {
            sendResponse("error: usage: wait_serial <circId> [uartN] <pattern> <timeout_ms>");
            return;
        }
        QString circId = parts.at(1);
        int uartN = 1;
        QString pattern;
        int timeoutMs;

        // Last token is always timeoutMs; optional second token is uartN (1-8).
        // Everything between circId (and optional uartN) and timeout is the pattern.
        timeoutMs = parts.last().toInt();
        int patternStart = 2; // default: pattern starts at parts[2]
        bool isUartNum = false;
        int candidate = parts.at(2).toInt(&isUartNum);
        if( isUartNum && candidate >= 1 && candidate <= 8 && parts.size() >= 5 )
        {
            uartN = candidate;
            patternStart = 3;
        }
        // pattern = all tokens from patternStart up to (but not including) last
        QStringList patternParts = parts.mid( patternStart, parts.size() - patternStart - 1 );
        pattern = patternParts.join(' ');

        UsartModule* usart = findUsart( circId, uartN );
        if( !usart )
        {
            sendResponse("error: component not found: " + circId);
            return;
        }

        m_waitSerial.circId    = circId;
        m_waitSerial.uartIndex = uartN;
        m_waitSerial.pattern   = pattern;
        m_waitSerial.timeoutMs = timeoutMs;
        m_waitSerial.elapsedMs = 0;
        m_waitMode = WaitSerialMode;
        m_waiting = true;
        m_waitTimer->start();
        // Response will be sent from onWaitTick()
    }
    // ── set_control <circId> <value> ─────────────────────────────
    // For Potentiometer: value is ohms (double)
    // For Push: value is "press" or "release"
    else if( cmd.startsWith("set_control ") )
    {
        QStringList parts = cmd.split(' ', Qt::SkipEmptyParts);
        if( parts.size() < 3 )
        {
            sendResponse("error: usage: set_control <circId> <value>");
            return;
        }
        QString circId = parts.at(1);
        QString value  = parts.mid(2).join(' ');

        Circuit* circuit = Circuit::self();
        if( !circuit ) { sendResponse("error: no circuit"); return; }

        Component* comp = circuit->getCompById( circId );
        if( !comp ) { sendResponse("error: component not found: " + circId); return; }

        Potentiometer* pot = dynamic_cast<Potentiometer*>( comp );
        if( pot )
        {
            bool ok = false;
            double v = value.toDouble( &ok );
            if( !ok ) { sendResponse("error: value must be a number for Potentiometer"); return; }
            pot->setVal( v );
            sendResponse("ok");
            return;
        }

        PushBase* push = dynamic_cast<PushBase*>( comp );
        if( push )
        {
            if( value == "press" )
                push->onbuttonPressed();
            else if( value == "release" )
                push->onbuttonReleased();
            else { sendResponse("error: value must be 'press' or 'release' for Push"); return; }
            sendResponse("ok");
            return;
        }

        sendResponse("error: component is not a Potentiometer or Push: " + circId);
    }
    // ── pin_voltage <circId> <pinSuffix> ─────────────────────────
    // Returns the voltage at the named pin as a decimal number.
    // pinSuffix is the part after circId- in the full pin id (e.g. PORTC0).
    else if( cmd.startsWith("pin_voltage ") )
    {
        QStringList parts = cmd.split(' ', Qt::SkipEmptyParts);
        if( parts.size() < 3 )
        {
            sendResponse("error: usage: pin_voltage <circId> <pinSuffix>");
            return;
        }
        QString circId    = parts.at(1);
        QString pinSuffix = parts.at(2);

        Circuit* circuit = Circuit::self();
        if( !circuit ) { sendResponse("error: no circuit"); return; }

        Component* comp = circuit->getCompById( circId );
        if( !comp ) { sendResponse("error: component not found: " + circId); return; }

        QString fullPinId = circId + "-" + pinSuffix;
        Pin* found = nullptr;
        for( Pin* p : comp->getPins() )
        {
            if( p && p->pinId() == fullPinId ) { found = p; break; }
        }
        if( !found ) { sendResponse("error: pin not found: " + fullPinId); return; }

        double v = found->getVoltage();
        sendResponse( QString::number( v, 'f', 4 ) );
    }
    // ── pin_state <circId> <pinSuffix> ───────────────────────────
    // Wokwi-parity equivalent of expect-pin: returns "high" or "low",
    // thresholded at 2.5V (assumes 5V logic; all current scenarios are AVR).
    else if( cmd.startsWith("pin_state ") )
    {
        QStringList parts = cmd.split(' ', Qt::SkipEmptyParts);
        if( parts.size() < 3 )
        {
            sendResponse("error: usage: pin_state <circId> <pinSuffix>");
            return;
        }
        QString circId    = parts.at(1);
        QString pinSuffix = parts.at(2);

        Circuit* circuit = Circuit::self();
        if( !circuit ) { sendResponse("error: no circuit"); return; }

        Component* comp = circuit->getCompById( circId );
        if( !comp ) { sendResponse("error: component not found: " + circId); return; }

        QString fullPinId = circId + "-" + pinSuffix;
        Pin* found = nullptr;
        for( Pin* p : comp->getPins() )
        {
            if( p && p->pinId() == fullPinId ) { found = p; break; }
        }
        if( !found ) { sendResponse("error: pin not found: " + fullPinId); return; }

        sendResponse( found->getVoltage() >= 2.5 ? "high" : "low" );
    }
    else
    {
        qWarning() << "SimuServer: unknown command:" << cmd;
        sendResponse("error: unknown command: " + cmd);
    }
}

void SimuServer::onWaitTick()
{
    if( m_waitMode == WaitSimTimeMode )
    {
        Simulator* sim = Simulator::self();
        if( !sim )
        {
            m_waitTimer->stop();
            m_waitMode = WaitNone;
            m_waiting = false;
            sendResponse("error: simulator gone");
            return;
        }

        uint64_t simUs = sim->circTime() / 1000000ULL;
        if( simUs >= m_waitSimTime.targetUs )
        {
            m_waitTimer->stop();
            m_waitMode = WaitNone;
            m_waiting = false;
            sendResponse("ok");
        }
        return;
    }

    if( m_waitMode != WaitSerialMode )
    {
        m_waitTimer->stop();
        m_waitMode = WaitNone;
        m_waiting = false;
        sendResponse("error: invalid wait state");
        return;
    }

    UsartModule* usart = findUsart( m_waitSerial.circId, m_waitSerial.uartIndex );
    if( !usart )
    {
        m_waitTimer->stop();
        m_waitMode = WaitNone;
        m_waiting = false;
        sendResponse("error: component gone");
        return;
    }

    QByteArray buf = usart->serialCapture();
    if( buf.contains( m_waitSerial.pattern.toUtf8() ) )
    {
        m_waitTimer->stop();
        m_waitMode = WaitNone;
        m_waiting = false;
        sendResponse("found");
        return;
    }

    m_waitSerial.elapsedMs += WAIT_TICK_MS;
    if( m_waitSerial.elapsedMs >= m_waitSerial.timeoutMs )
    {
        m_waitTimer->stop();
        m_waitMode = WaitNone;
        m_waiting = false;
        sendResponse("timeout");
    }
}

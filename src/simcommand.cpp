/***************************************************************************
 *   SimCommand — shared command dispatch for SimuServer and ScenarioRunner *
 ***************************************************************************/

#include <QDebug>
#include <QCoreApplication>

#include "simcommand.h"
#include "circuitwidget.h"
#include "circuit.h"
#include "simulator.h"
#include "subcircuit.h"
#include "mcu.h"
#include "usartmodule.h"
#include "potentiometer.h"
#include "push_base.h"
#include "pin.h"

#define WAIT_TICK_MS 50  // poll interval for wait_serial

SimCommand::SimCommand( QObject* parent )
    : QObject( parent )
    , m_waitTimer( new QTimer( this ) )
    , m_waitMode( WaitNone )
    , m_waiting( false )
{
    m_waitTimer->setInterval( WAIT_TICK_MS );
    connect( m_waitTimer, &QTimer::timeout, this, &SimCommand::onWaitTick );
}

SimCommand::~SimCommand() {}

void SimCommand::cancelWait()
{
    if( !m_waiting ) return;
    m_waitTimer->stop();
    m_waitMode = WaitNone;
    m_waiting = false;
}

UsartModule* SimCommand::findUsart( const QString& circId, int uartIndex )
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

void SimCommand::execute( const QString& cmd, std::function<void(const QString&)> respond )
{
    CircuitWidget* cw  = CircuitWidget::self();
    Simulator*     sim = Simulator::self();

    // ── load <path> ──────────────────────────────────────────────
    if( cmd.startsWith("load ") )
    {
        QString path = cmd.mid(5).trimmed();
        if( cw ) QTimer::singleShot( 0, [cw, path](){ cw->loadCirc( path ); });
        respond("ok");
    }
    // ── run ──────────────────────────────────────────────────────
    else if( cmd == "run" )
    {
        if( cw ) QTimer::singleShot( 0, [cw](){ cw->powerCircOn(); });
        respond("ok");
    }
    // ── stop ─────────────────────────────────────────────────────
    else if( cmd == "stop" )
    {
        if( cw ) QTimer::singleShot( 0, [cw](){ cw->powerCircOff(); });
        respond("ok");
    }
    // ── pause ────────────────────────────────────────────────────
    else if( cmd == "pause" )
    {
        if( cw ) QTimer::singleShot( 0, [cw](){ cw->pauseCirc(); });
        respond("ok");
    }
    // ── reset ────────────────────────────────────────────────────
    else if( cmd == "reset" )
    {
        if( cw ) QTimer::singleShot( 0, [cw](){
            cw->powerCircOff();
            QTimer::singleShot( 200, [cw](){ cw->powerCircOn(); });
        });
        respond("ok");
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
        respond("ok");
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
        respond( st );
    }
    // ── wait <ms> ─────────────────────────────────────────────────
    else if( cmd.startsWith("wait ") )
    {
        bool ok = false;
        int waitMs = cmd.mid(5).trimmed().toInt( &ok );
        if( !ok || waitMs < 0 )
        {
            respond("error: usage: wait <ms>");
            return;
        }
        if( waitMs == 0 )
        {
            respond("ok");
            return;
        }
        if( !sim || !sim->isRunning() )
        {
            respond("error: simulation not running");
            return;
        }

        m_waitSimTime.targetUs = (sim->circTime() / 1000000ULL) + (uint64_t)waitMs * 1000ULL;
        m_waitMode = WaitSimTimeMode;
        m_waiting = true;
        m_waitRespond = respond;
        m_waitTimer->start();
        // Response will be sent from onWaitTick()
    }
    // ── simtime ──────────────────────────────────────────────────
    else if( cmd == "simtime" )
    {
        uint64_t us = sim ? (sim->circTime() / 1000000ULL) : 0;
        respond( QString::number( us ) );
    }
    // ── serial <circId> [<uartN>] ────────────────────────────────
    else if( cmd.startsWith("serial ") )
    {
        QStringList parts = cmd.split(' ', Qt::SkipEmptyParts);
        if( parts.size() < 2 )
        {
            respond("error: usage: serial <circId> [uartN]");
            return;
        }
        QString circId = parts.at(1);
        int uartN = (parts.size() >= 3) ? parts.at(2).toInt() : 1;

        UsartModule* usart = findUsart( circId, uartN );
        if( !usart )
        {
            respond("error: component not found or no USART: " + circId);
            return;
        }
        respond( QString::fromUtf8( usart->serialCapture() ) );
    }
    // ── serial_clear <circId> [<uartN>] ──────────────────────────
    else if( cmd.startsWith("serial_clear ") )
    {
        QStringList parts = cmd.split(' ', Qt::SkipEmptyParts);
        if( parts.size() < 2 )
        {
            respond("error: usage: serial_clear <circId> [uartN]");
            return;
        }
        QString circId = parts.at(1);
        int uartN = (parts.size() >= 3) ? parts.at(2).toInt() : 1;

        UsartModule* usart = findUsart( circId, uartN );
        if( !usart )
        {
            respond("error: component not found: " + circId);
            return;
        }
        usart->serialCaptureClear();
        respond("ok");
    }
    // ── send_serial <circId> [<uartN>] <text> ────────────────────
    // Feeds `text` into the target's USART RX, byte by byte, as if it had
    // arrived over the wire. Wokwi-parity equivalent of write-serial.
    else if( cmd.startsWith("send_serial ") )
    {
        QStringList parts = cmd.split(' ', Qt::SkipEmptyParts);
        if( parts.size() < 3 )
        {
            respond("error: usage: send_serial <circId> [uartN] <text>");
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
            respond("error: component not found: " + circId);
            return;
        }
        QByteArray bytes = text.toUtf8();
        for( char c : bytes ) usart->receiveByte( (uint8_t)c );
        respond("ok");
    }
    // ── wait_serial <circId> [<uartN>] <pattern> <timeout_ms> ───
    else if( cmd.startsWith("wait_serial ") )
    {
        // Syntax variants:
        //   wait_serial <circId> <pattern> <timeout_ms>
        //   wait_serial <circId> <uartN>   <pattern> <timeout_ms>
        QStringList parts = cmd.split(' ', Qt::SkipEmptyParts);
        if( parts.size() < 4 )
        {
            respond("error: usage: wait_serial <circId> [uartN] <pattern> <timeout_ms>");
            return;
        }
        QString circId = parts.at(1);
        int uartN = 1;
        QString pattern;
        int timeoutMs;

        // Last token is always timeoutMs; optional second token is uartN (1-8).
        timeoutMs = parts.last().toInt();
        int patternStart = 2;
        bool isUartNum = false;
        int candidate = parts.at(2).toInt(&isUartNum);
        if( isUartNum && candidate >= 1 && candidate <= 8 && parts.size() >= 5 )
        {
            uartN = candidate;
            patternStart = 3;
        }
        QStringList patternParts = parts.mid( patternStart, parts.size() - patternStart - 1 );
        pattern = patternParts.join(' ');

        UsartModule* usart = findUsart( circId, uartN );
        if( !usart )
        {
            respond("error: component not found: " + circId);
            return;
        }

        m_waitSerial.circId    = circId;
        m_waitSerial.uartIndex = uartN;
        m_waitSerial.pattern   = pattern;
        m_waitSerial.timeoutMs = timeoutMs;
        m_waitSerial.elapsedMs = 0;
        m_waitMode = WaitSerialMode;
        m_waiting = true;
        m_waitRespond = respond;
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
            respond("error: usage: set_control <circId> <value>");
            return;
        }
        QString circId = parts.at(1);
        QString value  = parts.mid(2).join(' ');

        Circuit* circuit = Circuit::self();
        if( !circuit ) { respond("error: no circuit"); return; }

        Component* comp = circuit->getCompById( circId );
        if( !comp ) { respond("error: component not found: " + circId); return; }

        Potentiometer* pot = dynamic_cast<Potentiometer*>( comp );
        if( pot )
        {
            bool ok = false;
            double v = value.toDouble( &ok );
            if( !ok ) { respond("error: value must be a number for Potentiometer"); return; }
            pot->setValue( v );
            respond("ok");
            return;
        }

        PushBase* push = dynamic_cast<PushBase*>( comp );
        if( push )
        {
            if( value == "press" )
                push->onbuttonPressed();
            else if( value == "release" )
                push->onbuttonReleased();
            else { respond("error: value must be 'press' or 'release' for Push"); return; }
            respond("ok");
            return;
        }

        respond("error: component is not a Potentiometer or Push: " + circId);
    }
    // ── pin_voltage <circId> <pinSuffix> ─────────────────────────
    // Returns the voltage at the named pin as a decimal number.
    // pinSuffix is the part after circId- in the full pin id (e.g. PORTC0).
    else if( cmd.startsWith("pin_voltage ") )
    {
        QStringList parts = cmd.split(' ', Qt::SkipEmptyParts);
        if( parts.size() < 3 )
        {
            respond("error: usage: pin_voltage <circId> <pinSuffix>");
            return;
        }
        QString circId    = parts.at(1);
        QString pinSuffix = parts.at(2);

        Circuit* circuit = Circuit::self();
        if( !circuit ) { respond("error: no circuit"); return; }

        Component* comp = circuit->getCompById( circId );
        if( !comp ) { respond("error: component not found: " + circId); return; }

        QString fullPinId = circId + "-" + pinSuffix;
        Pin* found = nullptr;
        for( Pin* p : comp->getPins() )
        {
            if( p && p->pinId() == fullPinId ) { found = p; break; }
        }
        if( !found ) { respond("error: pin not found: " + fullPinId); return; }

        double v = found->getVoltage();
        respond( QString::number( v, 'f', 4 ) );
    }
    // ── pin_state <circId> <pinSuffix> ───────────────────────────
    // Wokwi-parity equivalent of expect-pin: returns "high" or "low",
    // thresholded at 2.5V (assumes 5V logic; all current scenarios are AVR).
    else if( cmd.startsWith("pin_state ") )
    {
        QStringList parts = cmd.split(' ', Qt::SkipEmptyParts);
        if( parts.size() < 3 )
        {
            respond("error: usage: pin_state <circId> <pinSuffix>");
            return;
        }
        QString circId    = parts.at(1);
        QString pinSuffix = parts.at(2);

        Circuit* circuit = Circuit::self();
        if( !circuit ) { respond("error: no circuit"); return; }

        Component* comp = circuit->getCompById( circId );
        if( !comp ) { respond("error: component not found: " + circId); return; }

        QString fullPinId = circId + "-" + pinSuffix;
        Pin* found = nullptr;
        for( Pin* p : comp->getPins() )
        {
            if( p && p->pinId() == fullPinId ) { found = p; break; }
        }
        if( !found ) { respond("error: pin not found: " + fullPinId); return; }

        respond( found->getVoltage() >= 2.5 ? "high" : "low" );
    }
    else
    {
        qWarning() << "SimCommand: unknown command:" << cmd;
        respond("error: unknown command: " + cmd);
    }
}

void SimCommand::onWaitTick()
{
    if( m_waitMode == WaitSimTimeMode )
    {
        Simulator* sim = Simulator::self();
        if( !sim )
        {
            m_waitTimer->stop();
            m_waitMode = WaitNone;
            m_waiting = false;
            m_waitRespond("error: simulator gone");
            return;
        }

        uint64_t simUs = sim->circTime() / 1000000ULL;
        if( simUs >= m_waitSimTime.targetUs )
        {
            m_waitTimer->stop();
            m_waitMode = WaitNone;
            m_waiting = false;
            m_waitRespond("ok");
        }
        return;
    }

    if( m_waitMode != WaitSerialMode )
    {
        m_waitTimer->stop();
        m_waitMode = WaitNone;
        m_waiting = false;
        m_waitRespond("error: invalid wait state");
        return;
    }

    UsartModule* usart = findUsart( m_waitSerial.circId, m_waitSerial.uartIndex );
    if( !usart )
    {
        m_waitTimer->stop();
        m_waitMode = WaitNone;
        m_waiting = false;
        m_waitRespond("error: component gone");
        return;
    }

    QByteArray buf = usart->serialCapture();
    if( buf.contains( m_waitSerial.pattern.toUtf8() ) )
    {
        m_waitTimer->stop();
        m_waitMode = WaitNone;
        m_waiting = false;
        m_waitRespond("found");
        return;
    }

    m_waitSerial.elapsedMs += WAIT_TICK_MS;
    if( m_waitSerial.elapsedMs >= m_waitSerial.timeoutMs )
    {
        m_waitTimer->stop();
        m_waitMode = WaitNone;
        m_waiting = false;
        m_waitRespond("timeout");
    }
}

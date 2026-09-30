/***************************************************************************
 *   SimuServer — Unix socket control interface for SimulIDE               *
 *                                                                         *
 *   Commands (newline-terminated):                                        *
 *     load <path>                  — load a .simu/.sim1 circuit file      *
 *     run                          — start simulation (power on)          *
 *     stop                         — stop simulation (power off)          *
 *     pause                        — pause / resume simulation            *
 *     reset                        — stop + run                           *
 *     quit                         — exit SimulIDE                        *
 *     status                       — "running" | "paused" | "stopped"     *
 *     wait <ms>                    — wait for N ms of simulation time      *
 *     simtime                      — current sim time in µs               *
 *     serial <circId> [uartN]      — dump capture buffer (ASCII)          *
 *     serial_clear <circId> [uartN]— clear capture buffer                 *
 *     wait_serial <circId> [uartN] <pattern> <timeout_ms>                 *
 *                                  — block until pattern found or timeout  *
 *                                    replies "found" or "timeout"         *
 *     set_control <circId> <value> — Potentiometer: ohms (double)         *
 *                                    Push: "press" or "release"           *
 *     pin_voltage <circId> <pin>   — read pin voltage in volts            *
 *                                                                         *
 *   Socket path: /tmp/simulide.sock                                       *
 ***************************************************************************/

#ifndef SIMUSERVER_H
#define SIMUSERVER_H

#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>

class UsartModule;

class SimuServer : public QObject
{
    Q_OBJECT
public:
    explicit SimuServer( QObject* parent = nullptr );
    ~SimuServer();

    bool listen();

private slots:
    void onNewConnection();
    void onReadyRead();
    void onWaitTick();

private:
    void handleCommand( const QString& cmd );
    void sendResponse( const QString& resp );

    // Locate a USART from "circId" or "circId uart<N>" tokens
    UsartModule* findUsart( const QString& circId, int uartIndex );

    QLocalServer* m_server;
    QLocalSocket* m_client; // single client at a time

    enum WaitMode {
        WaitNone = 0,
        WaitSerialMode,
        WaitSimTimeMode
    };

    // wait_serial state
    struct WaitSerial {
        QString    circId;
        int        uartIndex;
        QString    pattern;
        int        timeoutMs;
        int        elapsedMs;
    };

    struct WaitSimTime {
        uint64_t targetUs;
    };

    QTimer*      m_waitTimer;
    WaitMode     m_waitMode;
    WaitSerial   m_waitSerial;
    WaitSimTime  m_waitSimTime;
    bool         m_waiting;
};

#endif

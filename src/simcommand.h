/***************************************************************************
 *   SimCommand — shared command dispatch for the headless socket API      *
 *   (SimuServer) and the self-contained -test-ci scenario runner          *
 *   (ScenarioRunner). One place implements run/stop/wait_serial/etc. so   *
 *   both callers stay behaviorally identical.                             *
 *                                                                         *
 *   Commands (newline-terminated):                                        *
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
 *     send_serial <circId> [uartN] <text> — inject text into RX           *
 *     wait_serial <circId> [uartN] <pattern> <timeout_ms>                 *
 *                                  — block until pattern found or timeout  *
 *                                    replies "found" or "timeout"         *
 *     set_control <circId> <value> — Potentiometer: ohms (double)         *
 *                                    Push: "press" or "release"           *
 *     pin_voltage <circId> <pin>   — read pin voltage in volts            *
 *     pin_state <circId> <pin>     — "high"/"low", thresholded at 2.5V    *
 ***************************************************************************/

#pragma once

#include <QObject>
#include <QTimer>
#include <functional>

class UsartModule;

class SimCommand : public QObject
{
        Q_OBJECT
    public:
        explicit SimCommand( QObject* parent = nullptr );
        ~SimCommand();

        // Runs `cmd`, calling `respond` with the reply. Synchronous commands
        // call back immediately (before execute() returns); wait/wait_serial
        // call back later, once satisfied or timed out.
        void execute( const QString& cmd, std::function<void(const QString&)> respond );

        bool isWaiting() { return m_waiting; }
        void cancelWait();

    private slots:
        void onWaitTick();

    private:
        UsartModule* findUsart( const QString& circId, int uartIndex );

        enum WaitMode {
            WaitNone = 0,
            WaitSerialMode,
            WaitSimTimeMode
        };

        struct WaitSerial {
            QString circId;
            int     uartIndex;
            QString pattern;
            int     timeoutMs;
            int     elapsedMs;
        };

        struct WaitSimTime {
            uint64_t targetUs;
        };

        QTimer*     m_waitTimer;
        WaitMode    m_waitMode;
        WaitSerial  m_waitSerial;
        WaitSimTime m_waitSimTime;
        bool        m_waiting;
        std::function<void(const QString&)> m_waitRespond;
};

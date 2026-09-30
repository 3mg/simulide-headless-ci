/***************************************************************************
 *   SimuServer — Unix socket control interface for SimulIDE.              *
 *   Thin QLocalServer wrapper; all command logic lives in SimCommand so   *
 *   it's shared with the self-contained -test-ci ScenarioRunner.          *
 *   Socket path: /tmp/simulide.sock. See simcommand.h for the protocol.   *
 ***************************************************************************/

#pragma once

#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>

class SimCommand;

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

    private:
        void sendResponse( const QString& resp );

        QLocalServer* m_server;
        QLocalSocket* m_client; // single client at a time
        SimCommand*   m_cmd;
};

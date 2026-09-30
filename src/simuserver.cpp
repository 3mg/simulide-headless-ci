/***************************************************************************
 *   SimuServer — Unix socket control interface for SimulIDE               *
 ***************************************************************************/

#include <QDebug>

#include "simuserver.h"
#include "simcommand.h"

#define SOCKET_PATH "/tmp/simulide.sock"

SimuServer::SimuServer( QObject* parent )
    : QObject( parent )
    , m_server( new QLocalServer( this ) )
    , m_client( nullptr )
    , m_cmd( new SimCommand( this ) )
{}

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
        m_cmd->cancelWait();
        m_client->disconnectFromServer();
        m_client->deleteLater();
    }
    m_client = sock;
    connect( m_client, &QLocalSocket::disconnected, this, [this, sock](){
        if( m_client != sock ) return;
        m_cmd->cancelWait();
        m_client->deleteLater();
        m_client = nullptr;
    });
    connect( m_client, &QLocalSocket::readyRead, this, &SimuServer::onReadyRead );
}

void SimuServer::onReadyRead()
{
    QLocalSocket* client = m_client;
    if( !client ) return;
    if( m_cmd->isWaiting() ) return; // busy with wait_serial — drain later

    while( client == m_client && client->canReadLine() )
    {
        QString line = QString::fromUtf8( client->readLine() ).trimmed();
        if( line.isEmpty() ) continue;
        m_cmd->execute( line, [this](const QString& r){ sendResponse(r); } );
        if( client != m_client ) break; // command closed/replaced the socket
        if( m_cmd->isWaiting() ) break; // wait_serial took over; rest will be read after
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
    if( !m_cmd->isWaiting() )
    {
        m_client->disconnectFromServer();
        // Don't delete here — let QLocalSocket handle lifecycle
    }
}

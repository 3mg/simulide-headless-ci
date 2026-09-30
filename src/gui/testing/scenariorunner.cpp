/***************************************************************************
 *   ScenarioRunner — self-contained JSON scenario runner for -test-ci     *
 ***************************************************************************/

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QEventLoop>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>

#include "scenariorunner.h"
#include "simcommand.h"
#include "circuitwidget.h"

static void collectScenarios( const QDir& dir, QStringList& out )
{
    for( const QString& f : dir.entryList( {"scenario.json"}, QDir::Files ) )
        out << dir.absoluteFilePath( f );

    for( const QString& sub : dir.entryList( QDir::Dirs | QDir::NoDotAndDotDot ) )
    {
        QDir child( dir );
        child.cd( sub );
        collectScenarios( child, out );
    }
}

// Runs `cmdStr` through SimCommand and blocks (via a nested event loop, not a
// real sleep) until its response arrives — synchronous commands respond
// immediately, async ones (wait/wait_serial) respond later via QTimer.
static QString runCommand( SimCommand* cmd, const QString& cmdStr )
{
    QString response;
    bool done = false;
    QEventLoop loop;
    cmd->execute( cmdStr, [&]( const QString& r ){
        response = r;
        done = true;
        loop.quit();
    });
    if( !done ) loop.exec();
    return response;
}

static void sleepMs( int ms )
{
    QEventLoop loop;
    QTimer::singleShot( ms, &loop, &QEventLoop::quit );
    loop.exec();
}

static bool runStep( SimCommand* cmd, const QJsonObject& step, QMap<QString,double>& vars, QString& errorOut )
{
    if( step.contains("sleep_ms") )
    {
        sleepMs( step.value("sleep_ms").toInt() );
        return true;
    }

    QString cmdStr = step.value("cmd").toString();
    QString response = runCommand( cmd, cmdStr );

    QString parseAs = step.value("parse").toString();
    bool isNumeric = (parseAs == "int" || parseAs == "float");
    double numeric = 0;
    if( isNumeric )
    {
        QString trimmed = response.trimmed();
        numeric = trimmed.isEmpty() ? 0 : trimmed.toDouble();
    }

    if( step.contains("equals") )
    {
        if( isNumeric )
        {
            QJsonValue expected = step.value("equals");
            double exp = expected.isString() ? expected.toString().toDouble() : expected.toDouble();
            if( numeric != exp )
            {
                errorOut = QString("expected %1, got %2").arg(exp).arg(numeric);
                return false;
            }
        }
        else
        {
            QString exp = step.value("equals").toString();
            if( response != exp )
            {
                errorOut = QString("expected '%1', got '%2'").arg(exp, response);
                return false;
            }
        }
    }
    if( step.contains("contains") )
    {
        QString needle = step.value("contains").toString();
        if( !response.contains(needle) )
        {
            errorOut = QString("expected response to contain '%1', got '%2'").arg(needle, response);
            return false;
        }
    }
    if( step.contains("not_contains") )
    {
        QString needle = step.value("not_contains").toString();
        if( response.contains(needle) )
        {
            errorOut = QString("expected response not to contain '%1', got '%2'").arg(needle, response);
            return false;
        }
    }
    if( step.contains("min") )
    {
        double m = step.value("min").toDouble();
        if( numeric < m )
        {
            errorOut = QString("expected value >= %1, got %2").arg(m).arg(numeric);
            return false;
        }
    }
    if( step.contains("max") )
    {
        double m = step.value("max").toDouble();
        if( numeric > m )
        {
            errorOut = QString("expected value <= %1, got %2").arg(m).arg(numeric);
            return false;
        }
    }
    if( step.contains("min_from") )
    {
        QJsonObject mf = step.value("min_from").toObject();
        QString var = mf.value("var").toString();
        if( !vars.contains(var) )
        {
            errorOut = "unknown captured variable: " + var;
            return false;
        }
        double threshold = vars.value(var) + mf.value("delta").toDouble();
        if( numeric < threshold )
        {
            errorOut = QString("expected value >= %1, got %2").arg(threshold).arg(numeric);
            return false;
        }
    }
    if( step.contains("capture") )
        vars[ step.value("capture").toString() ] = numeric;

    return true;
}

static bool runScenario( SimCommand* cmd, const QString& scenarioFile, QString& nameOut, QString& errorOut )
{
    QFile f( scenarioFile );
    if( !f.open( QIODevice::ReadOnly ) )
    {
        errorOut = "cannot open " + scenarioFile;
        return false;
    }
    QJsonParseError perr;
    QJsonDocument doc = QJsonDocument::fromJson( f.readAll(), &perr );
    if( perr.error != QJsonParseError::NoError )
    {
        errorOut = "invalid JSON: " + perr.errorString();
        return false;
    }
    QJsonObject spec = doc.object();
    QFileInfo fileInfo( scenarioFile );
    nameOut = spec.value("name").toString( fileInfo.dir().dirName() );

    QString circuitRel = spec.value("circuit").toString();
    QString circuitPath = fileInfo.dir().absoluteFilePath( circuitRel );

    CircuitWidget::self()->powerCircOff();
    CircuitWidget::self()->loadCirc( circuitPath );

    QMap<QString,double> vars;
    QJsonArray steps = spec.value("steps").toArray();
    int index = 0;
    for( const QJsonValue& v : steps )
    {
        ++index;
        QString stepError;
        if( !runStep( cmd, v.toObject(), vars, stepError ) )
        {
            errorOut = QString("step %1: %2").arg(index).arg(stepError);
            return false;
        }
    }
    return true;
}

int ScenarioRunner::runAll( const QString& path )
{
    QFileInfo info( path );
    QStringList files;
    if( info.isDir() )
    {
        QDir dir( path );
        collectScenarios( dir, files );
        files.sort();
    }
    else files << path;

    if( files.isEmpty() )
    {
        qWarning() << "ScenarioRunner: no scenario.json found under" << path;
        return 1;
    }

    SimCommand cmd;
    QStringList failed;

    for( const QString& file : files )
    {
        QString name, error;
        bool ok = runScenario( &cmd, file, name, error );
        if( ok ) qDebug().noquote() << "PASS" << name;
        else
        {
            qDebug().noquote() << "FAIL" << name << ":" << error;
            failed << name;
        }
    }

    if( failed.isEmpty() )
    {
        qDebug().noquote() << QString("%1/%2 scenarios passed").arg(files.size()).arg(files.size());
        return 0;
    }
    qDebug().noquote() << QString("%1 of %2 scenarios failed:").arg(failed.size()).arg(files.size());
    for( const QString& f : failed ) qDebug().noquote() << " " << f;
    return 1;
}

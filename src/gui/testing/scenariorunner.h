/***************************************************************************
 *   ScenarioRunner — self-contained JSON scenario runner for -test-ci.    *
 *   Runs the same wire-protocol commands as SimuServer's socket API       *
 *   (via the shared SimCommand), but entirely in-process: no socket, no   *
 *   external driver. Prints a pass/fail summary and returns an exit code. *
 ***************************************************************************/

#pragma once

#include <QString>

class ScenarioRunner
{
    public:
        // path: a single scenario.json file, or a folder to search recursively.
        // Returns 0 if every scenario passed, 1 if any failed.
        static int runAll( const QString& path );
};

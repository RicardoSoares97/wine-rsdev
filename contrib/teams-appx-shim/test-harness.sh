#!/bin/bash
# Clean, repeatable Teams test harness.
#
# The previous ad-hoc `kill -9` cleanup between test runs left orphaned
# named mutexes/pipes in the wineserver, causing ms-teams.exe's own
# SingleInstanceService to believe another instance was already running
# and exit before ever reaching the composition code we're debugging.
# This harness always shuts wineserver down GRACEFULLY and waits for it
# to fully exit before starting a new run, so every run starts from a
# truly clean slate.
#
# Usage:
#   ./test-harness.sh run [seconds]                 - plain run, capture log
#   ./test-harness.sh winedbg <cmdfile> [seconds]   - run under winedbg with
#                                                      the given command script
set -u

export WINEPREFIX="$HOME/.local/share/lutris/teams"
export WINEDLLOVERRIDES="appxdeploymentclient=n,appxpackaging=n"
TEAMS_DIR="$WINEPREFIX/drive_c/MSTeams"
LOGDIR="$HOME/wine-appx-src/harness-logs"
mkdir -p "$LOGDIR"
STAMP=$(date +%Y%m%d-%H%M%S)

clean_shutdown() {
    echo "[harness] shutting down any existing wineserver for this prefix..."
    wineserver -k 2>/dev/null
    # -w blocks until the server this prefix was using has fully exited;
    # if none was running it returns immediately.
    timeout 15 wineserver -w 2>/dev/null
    sleep 1

    # Belt and braces: if anything from THIS prefix's tree is somehow
    # still alive after a graceful -k/-w cycle, only then fall back to
    # killing it, but give it a normal TERM first, not KILL.
    local leftover
    leftover=$(pgrep -f "MSTeams|lutris/teams" 2>/dev/null)
    if [ -n "$leftover" ]; then
        echo "[harness] residual processes after graceful shutdown, sending TERM: $leftover"
        kill $leftover 2>/dev/null
        sleep 2
        leftover=$(pgrep -f "MSTeams|lutris/teams" 2>/dev/null)
        if [ -n "$leftover" ]; then
            echo "[harness] still alive, sending KILL: $leftover"
            kill -9 $leftover 2>/dev/null
            sleep 1
        fi
    fi
    echo "[harness] clean."
}

verify_clean() {
    local remaining
    remaining=$(pgrep -f "MSTeams|lutris/teams" 2>/dev/null | wc -l)
    echo "[harness] processes still matching MSTeams/lutris-teams: $remaining"
    if [ "$remaining" -gt 0 ]; then
        pgrep -af "MSTeams|lutris/teams" 2>/dev/null
    fi
}

mode="${1:-run}"

case "$mode" in
  run)
    seconds="${2:-60}"
    clean_shutdown
    verify_clean
    log="$LOGDIR/run-$STAMP.log"
    echo "[harness] launching ms-teams.exe for up to ${seconds}s, log: $log"
    export WINEDEBUG=fixme,err
    ( cd "$TEAMS_DIR" && timeout "$seconds" wine ms-teams.exe > "$log" 2>&1 )
    echo "[harness] wine exit code: $?"
    echo "[harness] log saved to $log"
    ;;

  winedbg)
    cmdfile="${2:?need a winedbg command file}"
    seconds="${3:-90}"
    clean_shutdown
    verify_clean
    log="$LOGDIR/winedbg-$STAMP.log"
    echo "[harness] launching under winedbg for up to ${seconds}s, log: $log"
    export WINEDEBUG=-all
    ( cd "$TEAMS_DIR" && timeout "$seconds" winedbg "C:\\MSTeams\\ms-teams.exe" < "$cmdfile" > "$log" 2>&1 )
    echo "[harness] winedbg exit code: $?"
    echo "[harness] log saved to $log"
    ;;

  clean)
    clean_shutdown
    verify_clean
    ;;

  *)
    echo "usage: $0 {run [seconds] | winedbg <cmdfile> [seconds] | clean}"
    exit 1
    ;;
esac

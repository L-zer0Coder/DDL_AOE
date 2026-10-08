@echo off
REM ============================================================
REM   16-GAME TEST  -  PARALLEL  -  4 maps x 4 rotations
REM
REM   Runs 8 games at once (your CPU has 8 cores), in 2 batches:
REM       batch 1 : map1 + map2   (8 games)
REM       batch 2 : map3 + map    (8 games)
REM   Each game needs ~65 MB of RAM, so 8 at once is light.
REM
REM   headless : -offscreen
REM   max speed: -freq MAX
REM   -exam    : REQUIRED. Without it, game over pops a modal
REM              dialog and the result file is never written.
REM              (exam mode also writes one JSON line per frame,
REM               so the LAST line is the real verdict.)
REM
REM   Output: .pt\result_MAP_ROT.txt  (verdict line only)
REM           .pt\SUMMARY.txt         (final table)
REM
REM   Stop with Ctrl+C. To kill leftovers:
REM       taskkill /f /im fakegame.exe
REM
REM   Prereq: REBUILD after editing UsrAI.cpp (script warns if stale).
REM   Keep this file ASCII-only; avoid angle brackets and ! marks.
REM ============================================================
setlocal
cd /d "%~dp0"

set QTDIR=D:/Qt/5.9.2/mingw53_32/bin
set MINGW=D:/Qt/Tools/mingw530_32/bin
set PATH=%QTDIR%;%MINGW%;%PATH%
set QT_QPA_PLATFORM=offscreen
set QT_OPENGL=software

REM wait helper: full path so a polluted PATH cannot hijack it
set SLP=%SystemRoot%\System32\ping.exe -n 3 127.0.0.1

REM absolute path on purpose. "start" cannot resolve a relative program
REM path (it searches PATH and fails with "syntax of file name is
REM incorrect"), and a forward slash inside it would be read as a switch.
set EXE=%~dp0.pt\game.bat

if not exist "%EXE%" (
    echo [ERROR] newAOE.exe not found. Build it in Qt Creator first.
    rem pause
    exit /b 1
)

REM ---------- warn if the exe is older than the source ----------
for %%f in ("%EXE%") do set EXET=%%~tf
for %%f in (UsrAI.cpp) do set SRCT=%%~tf
echo.
echo   exe        : %CD%\%EXE%
echo   exe built  : %EXET%
echo   UsrAI.cpp  : %SRCT%
if "%SRCT%" GTR "%EXET%" (
    echo.
    echo   ==========================================================
    echo    WARNING: UsrAI.cpp is NEWER than the exe.
    echo    You would be testing the OLD code.
    echo    Rebuild in Qt Creator first.
    echo   ==========================================================
    echo.
    echo   Press any key to continue anyway, or close to abort.
    rem pause >nul
)

REM ---------- leftover games from a previous run? ----------
tasklist /fi "IMAGENAME eq fakegame.exe" /nh 2>nul | findstr /i "fakegame.exe" >nul
if not errorlevel 1 (
    echo.
    echo   WARNING: fake game is already running. Close it first,
    echo            or run:  taskkill /f /im fakegame.exe
    echo.
    rem pause
)

if not exist output mkdir output
if not exist .pt mkdir .pt
del /q .pt\result_*.txt >nul 2>&1

set SUM=.pt\SUMMARY.txt
echo === 16 games : result === > "%SUM%"

echo.
echo ==========================================================
echo   BATCH 1 / 2 :  map1 + map2  (8 games in parallel)
echo ==========================================================
call :launch map1
call :launch map2
call :waitall

echo.
echo ==========================================================
echo   BATCH 2 / 2 :  map3 + map   (8 games in parallel)
echo ==========================================================
call :launch map3
call :launch map
call :waitall

echo.
echo collecting results ...
call :collect
goto :done


REM ---------------- launch one map (4 rotations) ----------------
:launch
echo   starting %1 : rotations 0 90 180 270
for %%r in (0 90 180 270) do (
    start /b "" "%EXE%" -map "%CD%\%1.njust" -rotate %%r -exam -offscreen -freq MAX -ResultLogFile "%CD%\.pt\result_%1_%%r.txt"
    %SLP% >nul
)
exit /b


REM ---------------- wait until every newAOE.exe is gone ----------------
:waitall
:waitloop
tasklist /fi "IMAGENAME eq fakegame.exe" /nh 2>nul | findstr /i "fakegame.exe" >nul
if not errorlevel 1 (
    %SLP% >nul
    goto waitloop
)
echo   batch finished.
exit /b


REM ---------------- collect verdicts ----------------
:collect
set W=0
set L=0
for %%m in (map1 map2 map3 map) do (
    for %%r in (0 90 180 270) do call :verdict %%m %%r
)
echo. >> "%SUM%"
echo ------------------------------------ >> "%SUM%"
echo  WIN %W%    LOSE %L%    16 games >> "%SUM%"
exit /b


REM ---------------- verdict of one game ----------------
REM The file holds one JSON line per frame; the LAST line is the
REM real result from HandleGameOver. Keep assignments OUTSIDE any
REM parenthesised block, or %LAST% would expand too early.
:verdict
set M=%~1
set R=%~2
set OUT=.pt\result_%M%_%R%.txt
set RES=NO-RESULT
set LAST=
if not exist "%OUT%" goto :vdone
for /f "usebackq delims=" %%a in ("%OUT%") do set LAST=%%a
if "%LAST%"=="" goto :vdone
set RES=LOSE
echo %LAST% | findstr /c:":true" >nul 2>&1 && set RES=WIN
REM shrink the log to just the verdict line
echo %LAST%> "%OUT%.tmp"
move /y "%OUT%.tmp" "%OUT%" >nul 2>&1
:vdone
if "%RES%"=="WIN" set /a W=%W%+1
if "%RES%"=="LOSE" set /a L=%L%+1
echo   %M%   rotate %R%   :  %RES%
echo %M%   rotate %R%   :  %RES% >> "%SUM%"
exit /b


REM ---------------- summary ----------------
:done
echo.
echo ==========================================================
echo   ALL DONE
echo ----------------------------------------------------------
type "%SUM%"
echo ==========================================================
echo   per-game JSON : .pt\result_*.txt
echo   summary       : %SUM%
echo.
rem pause

@echo off
REM ============================================================
REM   16-GAME TEST  -  PARALLEL  -  4 maps x 4 rotations
REM
REM   usage :  run_all.bat            all 4 maps, 16 games
REM            run_all.bat map1       only map1, 4 games (quick check)
REM            run_all.bat map1 map3  those two maps, 8 games
REM
REM   Runs 8 games at a time (this box has 8 cores) in rounds:
REM       round 1 : map1 + map2
REM       round 2 : map3 + map
REM   One game needs only ~55 MB of RAM, so 8 at once is light.
REM   Parallel speedup is real: 16 games take about as long as
REM   2 games, not 16.
REM
REM   flags used per game:
REM     -exam       REQUIRED. Without it, game over pops a modal
REM                 dialog and NO result file is ever written.
REM     -offscreen  no window
REM     -freq MAX   max speed
REM   Note: in exam mode the engine also appends one JSON line per
REM   frame, so a result file grows to a few MB. The LAST line is
REM   the real verdict, written when the game actually ends.
REM
REM   output: output\batch_runs\result_MAP_ROT.txt   (verdict only)
REM           output\batch_runs\SUMMARY.txt
REM
REM   stop  : Ctrl+C in this window
REM   kill  : taskkill /f /im newAOE.exe
REM   prereq: rebuild in Qt Creator after editing UsrAI.cpp
REM           (this script warns when the exe is stale)
REM
REM   ASCII only. No angle brackets anywhere, not even in comments:
REM   cmd parses them as redirection before treating the line as REM.
REM ============================================================
setlocal
cd /d "%~dp0"

set QTDIR=D:/Qt/5.9.2/mingw53_32/bin
set MINGW=D:/Qt/Tools/mingw530_32/bin
set PATH=%QTDIR%;%MINGW%;%PATH%
set QT_QPA_PLATFORM=offscreen
set QT_OPENGL=software

REM delay helper: full path, so a polluted PATH cannot hijack it
set SLP=%SystemRoot%\System32\ping.exe -n 3 127.0.0.1
set GAP=%SystemRoot%\System32\ping.exe -n 2 127.0.0.1

REM Absolute path is mandatory here. "start" cannot resolve a
REM relative program path, and a forward slash inside one is read
REM as a switch. Using a relative path makes every launch fail with
REM "the syntax of the file name is incorrect".
set EXE=%~dp0release\newAOE.exe
if not exist "%EXE%" set EXE=%~dp0debug\newAOE.exe
if not exist "%EXE%" (
    echo [ERROR] newAOE.exe not found.
    echo         looked in %~dp0release and %~dp0debug
    echo         build the project in Qt Creator first.
    pause
    exit /b 1
)

set MAPS=%~1
if "%MAPS%"=="" set MAPS=map1 map2 map3 map
if not "%~2"=="" set MAPS=%*

REM ---------- leftover games from a previous run ----------
tasklist /fi "IMAGENAME eq newAOE.exe" /nh 2>nul | findstr /i "newAOE.exe" >nul
if not errorlevel 1 (
    echo.
    echo   WARNING: newAOE.exe is still running from a previous run.
    echo            Close it first, or run: taskkill /f /im newAOE.exe
    echo.
    pause
    exit /b 1
)

REM ---------- warn when the exe is older than the source ----------
for %%f in ("%EXE%") do set EXET=%%~tf
for %%f in (UsrAI.cpp) do set SRCT=%%~tf
echo.
echo   exe   : %EXE%
echo   built : %EXET%      UsrAI.cpp : %SRCT%
if "%SRCT%" GTR "%EXET%" (
    echo.
    echo   ==========================================================
    echo    WARNING: UsrAI.cpp is NEWER than the exe.
    echo    You would be testing the OLD code.
    echo    Rebuild in Qt Creator first.
    echo   ==========================================================
    echo.
    echo   Press any key to continue anyway, or close to abort.
    pause >nul
)

echo   maps  : %MAPS%
echo.

if not exist output mkdir output
if not exist output\batch_runs mkdir output\batch_runs
del /q output\batch_runs\result_*.txt >nul 2>&1

set SUM=output\batch_runs\SUMMARY.txt
echo === result === > "%SUM%"

REM ---------- rounds: at most 8 games in flight ----------
if "%MAPS%"=="map1 map2 map3 map" (
    call :round map1 map2
    call :round map3 map
) else (
    call :round %MAPS%
)

echo.
echo collecting results ...
set W=0
set L=0
for %%m in (%MAPS%) do (
    for %%r in (0 90 180 270) do call :verdict %%m %%r
)

echo. >> "%SUM%"
echo ------------------------------------ >> "%SUM%"
echo  WIN %W%    LOSE %L% >> "%SUM%"

echo.
echo ==========================================================
echo   ALL DONE
echo ----------------------------------------------------------
type "%SUM%"
echo ==========================================================
echo   verdict files : output\batch_runs\result_*.txt
echo   summary       : %SUM%
echo.
pause
exit /b 0


REM ---------------- launch all rotations of the given maps ----------------
:round
for %%m in (%*) do (
    echo   launching %%m : rotations 0 90 180 270
    for %%r in (0 90 180 270) do (
        start /b "" "%EXE%" -map "%~dp0%%m.njust" -rotate %%r -exam -offscreen -freq MAX -ResultLogFile "%~dp0output\batch_runs\result_%%m_%%r.txt"
        %GAP% >nul
    )
)
call :waitall
exit /b 0


REM ---------------- wait until no newAOE.exe is left ----------------
:waitall
:waitloop
tasklist /fi "IMAGENAME eq newAOE.exe" /nh 2>nul | findstr /i "newAOE.exe" >nul
if not errorlevel 1 (
    %SLP% >nul
    goto waitloop
)
echo   round finished.
exit /b 0


REM ---------------- verdict of one game ----------------
REM The result file holds one JSON line per frame; the LAST line is
REM the real verdict. Keep these assignments OUTSIDE any block or
REM %LAST% would be expanded before the loop assigns it.
:verdict
set M=%~1
set R=%~2
set OUT=output\batch_runs\result_%M%_%R%.txt
set RES=NO-RESULT
set LAST=
if not exist "%OUT%" goto :vdone
for /f "usebackq delims=" %%a in ("%OUT%") do set LAST=%%a
if "%LAST%"=="" goto :vdone
set RES=LOSE
echo %LAST% | findstr /c:":true" >nul 2>&1 && set RES=WIN
REM keep only the verdict line, drop the per-frame lines
echo %LAST%> "%OUT%.tmp"
move /y "%OUT%.tmp" "%OUT%" >nul 2>&1
:vdone
if "%RES%"=="WIN" set /a W=%W%+1
if "%RES%"=="LOSE" set /a L=%L%+1
echo   %M%  rotate %R%  :  %RES%
echo %M%  rotate %R%  :  %RES% >> "%SUM%"
exit /b 0

@echo off
REM Regenerate platform_pcm.c from platform.wav with offline EQ + AGC.
REM Runs as a VS Code task so it does not depend on any interactive shell.
set PY=C:\Users\n11ak\.eide\bin\python36\python3.exe
set TOOLS=H:\git\PCB_PY32E407START_baseboard\sample\piezo\tools
set ROOT=H:\git\PCB_PY32E407START_baseboard\sample\piezo

"%PY%" "%TOOLS%\wav2pcm.py" "%ROOT%\platform.wav" "%ROOT%\Src\platform_pcm.c" --eq --agc --rate 16000
echo REGEN_EXIT=%ERRORLEVEL%
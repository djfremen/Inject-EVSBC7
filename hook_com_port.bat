@echo off
setlocal enabledelayedexpansion
title Tech2 COM Port Monitor
echo Tech2 COM Port Monitor
echo ====================
echo.

:: Check for handle.exe
where handle.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo Warning: handle.exe from Sysinternals not found in PATH.
    echo You can download it from: 
    echo https://docs.microsoft.com/en-us/sysinternals/downloads/handle
    echo.
    echo You can still proceed, but automatic process detection will not work.
    echo.
    set HANDLE_AVAILABLE=0
) else (
    set HANDLE_AVAILABLE=1
)

:: Check if PowerShell is available
where powershell.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: PowerShell is not available on this system.
    echo This script requires PowerShell to function properly.
    pause
    exit /b 1
)

:: Check for DLL and injector
if not exist com_hook.dll (
    echo Error: com_hook.dll not found.
    echo Please build it first by running build_simple.bat
    pause
    exit /b 1
)

if not exist dll_injector.exe (
    echo Error: dll_injector.exe not found.
    echo Please build it first by running build_simple.bat
    pause
    exit /b 1
)

:: Create directories if they don't exist
if not exist logs mkdir logs
if not exist captured_data mkdir captured_data

echo This tool will help you monitor COM port communication in Java processes.
echo It will:
echo 1. Find Java processes that are using COM ports
echo 2. Inject a DLL that logs all COM port activity
echo 3. Save binary data to the captured_data directory
echo.

:: Execute PowerShell script to find Java processes with COM ports if handle is available
set PID=
if %HANDLE_AVAILABLE%==1 (
    echo Searching for Java processes with COM port handles...
    echo.
    
    :: Run PowerShell script and capture output
    for /f "usebackq" %%i in (`powershell -ExecutionPolicy Bypass -File find_java_com_process.ps1`) do (
        :: Check if the output line is a number (PID)
        echo %%i | findstr /r "^[0-9][0-9]*$" >nul
        if !errorlevel! equ 0 (
            set PID=%%i
        )
    )
) else (
    echo Listing all Java processes...
    echo.
    tasklist /FI "IMAGENAME eq java.exe" /FO TABLE
    tasklist /FI "IMAGENAME eq javaw.exe" /FO TABLE
    echo.
)

:: If PID was not found automatically, ask user
if "%PID%"=="" (
    echo.
    echo Please enter the PID of the Java process to monitor:
    set /p PID=PID: 
)

:: Validate PID
echo %PID% | findstr /r "^[0-9][0-9]*$" >nul
if %errorlevel% neq 0 (
    echo Error: Invalid PID format: %PID%
    pause
    exit /b 1
)

:: Verify process exists
tasklist /FI "PID eq %PID%" | findstr %PID% >nul
if %errorlevel% neq 0 (
    echo Error: No process with PID %PID% was found.
    pause
    exit /b 1
)

:: Inject the DLL
echo.
echo Injecting hook DLL into process %PID%...
dll_injector.exe %PID%
if %errorlevel% neq 0 (
    echo Failed to inject DLL. Make sure you have administrator privileges.
    pause
    exit /b 1
)

echo.
echo Hook DLL successfully injected!
echo All COM port activity will be logged to the logs directory.
echo Binary data will be saved to the captured_data directory.
echo.
echo You can now perform operations in your application.
echo When you're done, you can close this window.
echo.
pause 
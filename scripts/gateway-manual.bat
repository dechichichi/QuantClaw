# Windows 手动启动 QuantClaw Gateway 脚本
# 使用说明：双击此文件或在命令行中运行

@echo off
chcp 65001 >nul
setlocal

echo.
echo ================================================
echo   QuantClaw Gateway - 手动启动脚本
echo ================================================
echo.

REM 查找 quantclaw.exe
set QUANTCLAW_EXE=
if exist "%~dp0build\quantclaw.exe" (
    set QUANTCLAW_EXE=%~dp0build\quantclaw.exe
) else if exist "%~dp0build\Debug\quantclaw.exe" (
    set QUANTCLAW_EXE=%~dp0build\Debug\quantclaw.exe
) else if exist "%~dp0build\Release\quantclaw.exe" (
    set QUANTCLAW_EXE=%~dp0build\Release\quantclaw.exe
) else if exist "%USERPROFILE%\.quantclaw\quantclaw.exe" (
    set QUANTCLAW_EXE=%USERPROFILE%\.quantclaw\quantclaw.exe
)

if "%QUANTCLAW_EXE%"=="" (
    echo [错误] 未找到 quantclaw.exe
    echo.
    echo 请确保已完成编译或安装：
    echo   cmake --build build --parallel
    pause
    exit /b 1
)

echo [信息] 找到 QuantClaw: %QUANTCLAW_EXE%
echo.

REM 创建日志目录
set LOG_DIR=%~dp0logs
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"

REM 检查配置文件
set CONFIG_FILE=%USERPROFILE%\.quantclaw\quantclaw.json
if not exist "%CONFIG_FILE%" (
    echo [警告] 配置文件不存在: %CONFIG_FILE%
    echo 请先运行: quantclaw onboard
    echo.
    pause
    exit /b 1
)

REM 启动 Gateway
echo [信息] 启动 Gateway (按 Ctrl+C 停止)...
echo [信息] 日志文件: %LOG_DIR%\gateway-manual.log
echo.
echo [%DATE% %TIME%] Gateway started manually >> "%LOG_DIR%\gateway-manual.log"

"%QUANTCLAW_EXE%" gateway run 2>&1 | tee "%LOG_DIR%\gateway-manual.log"

REM 检查退出代码
if %ERRORLEVEL% neq 0 (
    echo.
    echo [错误] Gateway 退出，代码: %ERRORLEVEL%
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo [信息] Gateway 已停止
pause

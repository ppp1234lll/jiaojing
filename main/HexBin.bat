::放在主文件目录下，执行(可由 Keil After Build 调用, 也可直接双击运行)
::Keil After Build 调用: ..\HexBin.bat $K !L @L $L
::   %1=$K(Keil 安装目录)  %2=!L(.axf 全路径)  %3=@L(工程名)  %4=$L(输出目录, 带反斜杠)
::注意 hex/dis/map 文件所在目录

@echo off

::获取脚本当前目录
set CURRENT_DIR=%~dp0

::创建输出文件夹 Debug
set output_dir=Debug
:: 拼接完整路径（当前目录+文件夹名）
set output_path=%CURRENT_DIR%%output_dir%
if not exist "%output_path%" md "%output_path%"

::---------------- 输入参数(缺省时用本工程默认值, 便于直接双击运行) ----------------
::设置fromelf.exe位置
if "%~1"=="" (set "KEIL_DIR=C:\Keil_v5\") else (set "KEIL_DIR=%~1")
if not "%KEIL_DIR:~-1%"=="\" set "KEIL_DIR=%KEIL_DIR%\"
set exe_location=%KEIL_DIR%ARM\ARMCC\bin\fromelf.exe

::设置.axf文件的位置
if "%~2"=="" (set "axf_location=%CURRENT_DIR%USER\Objects\data_collect.axf") else (set "axf_location=%~2")

::获取工程名
if "%~3"=="" (set "project_name=data_collect") else (set "project_name=%~3")

::.axf / .hex 输出目录(Keil OutputDirectory = .\Objects\)
if "%~4"=="" (set "axf_path=%CURRENT_DIR%USER\Objects\") else (set "axf_path=%~4")
if not "%axf_path:~-1%"=="\" set "axf_path=%axf_path%\"
set hex_path=%axf_path%

::.map 输出目录(Keil ListingPath = ..\OBJ\)
set map_path=%CURRENT_DIR%OBJ\

set output_name=%project_name%

if not exist "%axf_location%" (
	echo.
	echo [错误] 未找到 axf 文件: %axf_location%
	echo        请先在 Keil 中编译本工程, 或改用 Keil After Build 调用本脚本.
	echo.
	pause
	exit /b 1
)

::清理旧文件
if exist "%output_path%\*.hex" del /q "%output_path%\*.hex"
if exist "%output_path%\*.bin" del /q "%output_path%\*.bin"
if exist "%output_path%\*.dis" del /q "%output_path%\*.dis"
if exist "%output_path%\*.axf" del /q "%output_path%\*.axf"
if exist "%output_path%\*.map" del /q "%output_path%\*.map"
if exist "%output_path%\*.txt" del /q "%output_path%\*.txt"

@REM GET DIS
%exe_location% --text -a -c --output=%output_path%\%output_name%.dis %axf_location% >nul

::将bin文件生成到Debug文件夹  >nul屏蔽成功命令
%exe_location% --bin -o %output_path%\%output_name%.bin %axf_location% >nul

::将hex文件复制到Debug文件夹
copy "%hex_path%%project_name%.hex" "%output_path%" >nul

::将axf文件复制到Debug文件夹
copy "%axf_location%" "%output_path%" >nul

::将map文件复制到Debug文件夹
if exist "%map_path%%project_name%.map" (
	copy "%map_path%%project_name%.map" "%output_path%" >nul
) else (
	echo [提示] 未找到 map 文件: %map_path%%project_name%.map
)

::---------------- 版本号取自 INCLUDE\appconfig.h ----------------
set VERSION_FILE_PATH=%CURRENT_DIR%INCLUDE\appconfig.h

::软件版本 SOFT_NO_STR, 例: #define SOFT_NO_STR ("FN-ZTGD-QG-1.0.0")
for /f "tokens=3" %%a in ('findstr /i /c:"#define SOFT_NO_STR" "%VERSION_FILE_PATH%"') do set sw_ver=%%a
::去除双引号 / 左右括号
set sw_ver=%sw_ver:"=%
set sw_ver=%sw_ver:(=%
set sw_ver=%sw_ver:)=%

::硬件版本 HARD_NO_STR, 例: #define HARD_NO_STR ("FN-ZTGD-QG")
for /f "tokens=3" %%b in ('findstr /i /c:"#define HARD_NO_STR" "%VERSION_FILE_PATH%"') do set hw_ver=%%b
set hw_ver=%hw_ver:"=%
set hw_ver=%hw_ver:(=%
set hw_ver=%hw_ver:)=%

if "%sw_ver%"=="" (
	echo [错误] 未从 %VERSION_FILE_PATH% 解析到 SOFT_NO_STR
	pause
	exit /b 1
)

::设置重命名的文件名(SOFT_NO_STR 已含型号前缀, 例 FN-ZTGD-QG-1.0.0)
set rename_name=%sw_ver%

::将hex文件重命名
ren "%output_path%\%output_name%.hex" "%rename_name%.hex" >nul
::将bin文件重命名
ren "%output_path%\%output_name%.bin" "%rename_name%.bin" >nul

::---------------- OTA 服务器地址(固定值) ----------------
set OTA_IP=47.104.98.214
set OTA_PORT=8989

::---------------- 生成 OTA 升级包 _crc.bin ----------------
::每块 = 1024 字节数据 + 2 字节 CRC16-MODBUS(高字节在前), 不足 1024 整数倍用 0xFF 补齐
powershell -NoProfile -ExecutionPolicy Bypass -File "%CURRENT_DIR%crc_bin.ps1" "%output_path%\%rename_name%.bin" "%output_path%\%rename_name%_crc.bin"

::---------------- 生成 OTA info.txt ----------------
::设备按 /<HARD_NO_STR>/info.txt 请求, version 须等于 SOFT_NO_STR
> "%output_path%\info.txt" echo {"version":"%rename_name%";"url":"http://%OTA_IP%:%OTA_PORT%/%hw_ver%/%rename_name%_crc.bin";}

echo.
echo ===== 输出目录: %output_path% =====
dir /b "%output_path%"
echo.

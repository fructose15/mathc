/*
 * MathC Configuration Header (config.h)
 * Cross-platform configuration for Windows Terminal, MSVC, MinGW, Linux, macOS, and Android.
 *
 * Windows Terminal / Command Prompt / PowerShell Setup:
 *   1. Copy this file to the MathC root or include path (src/config.h).
 *   2. Build mcc.exe using MinGW or MSVC:
 *        gcc -O2 -std=c99 -D_CRT_SECURE_NO_WARNINGS -I. src/mcc.c -o mcc.exe -lm
 *      or (MSVC Developer Command Prompt):
 *        cl /O2 /Fe:mcc.exe /I. src\mcc.c
 *   3. Install into Windows Terminal PATH (PowerShell Admin or User):
 *        New-Item -ItemType Directory -Force -Path "$env:LOCALAPPDATA\MathC\bin"
 *        Copy-Item .\mcc.exe "$env:LOCALAPPDATA\MathC\bin\mcc.exe"
 *        [Environment]::SetEnvironmentVariable(
 *            "Path",
 *            $env:Path + ";$env:LOCALAPPDATA\MathC\bin",
 *            [EnvironmentVariableTarget]::User
 *        )
 *   4. Restart Windows Terminal and run: mcc --help
 */

#ifndef MATHC_CONFIG_H
#define MATHC_CONFIG_H

#define MATHC_VERSION "1.0.0"
#define MATHC_CODENAME "Euler"
#define MATHC_LICENSE_NAME "MIT License"
#define MATHC_REPO_URL "https://github.com/mathc-lang/mathc"

#if defined(_WIN32) || defined(_WIN64)
  #define MCC_PLATFORM_WINDOWS 1
  #define MCC_OS_NAME "Windows"
  #define MCC_EXE_EXT ".exe"
  #define MCC_OBJ_EXT ".obj"
  #define MCC_PATH_SEP "\\"
  #define MCC_DEFAULT_PREFIX "%LOCALAPPDATA%\\MathC\\bin"
  #define MCC_DEFAULT_CC "gcc"
  #ifdef _MSC_VER
    #define MCC_COMPILER_NAME "MSVC"
    #define popen _popen
    #define pclose _pclose
    #define strcasecmp _stricmp
    #define strncasecmp _strnicmp
  #else
    #define MCC_COMPILER_NAME "MinGW-GCC"
  #endif
  /* Windows Terminal ANSI Virtual Terminal Processing helper */
  #include <windows.h>
  static inline void mcc_enable_windows_terminal_utf8(void) {
    SetConsoleOutputCP(65001);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
      DWORD dwMode = 0;
      if (GetConsoleMode(hOut, &dwMode)) {
        SetConsoleMode(hOut, dwMode | 0x0004);
      }
    }
  }
  /* Windows Terminal PATH registration command macro */
  #define MCC_WIN_TERMINAL_INSTALL_CMD \
    "powershell -NoProfile -Command \"$d=\\\"$env:LOCALAPPDATA\\MathC\\bin\\\"; " \
    "New-Item -ItemType Directory -Force -Path $d | Out-Null; " \
    "Copy-Item .\\mcc.exe $d\\mcc.exe -Force; " \
    "$p=[Environment]::GetEnvironmentVariable('Path','User'); " \
    "if($p -notlike '*'+$d+'*'){[Environment]::SetEnvironmentVariable('Path',($p+';'+$d),'User')}; " \
    "Write-Host 'Installed mcc.exe to Windows Terminal PATH:' $d\""
#elif defined(__ANDROID__)
  #define MCC_PLATFORM_ANDROID 1
  #define MCC_OS_NAME "Android (Termux)"
  #define MCC_EXE_EXT ""
  #define MCC_OBJ_EXT ".o"
  #define MCC_PATH_SEP "/"
  #define MCC_DEFAULT_PREFIX "/data/data/com.termux/files/usr/bin"
  #define MCC_DEFAULT_CC "clang"
  #define MCC_COMPILER_NAME "Clang"
#elif defined(__APPLE__)
  #define MCC_PLATFORM_MACOS 1
  #define MCC_OS_NAME "macOS"
  #define MCC_EXE_EXT ""
  #define MCC_OBJ_EXT ".o"
  #define MCC_PATH_SEP "/"
  #define MCC_DEFAULT_PREFIX "/usr/local/bin"
  #define MCC_DEFAULT_CC "clang"
  #define MCC_COMPILER_NAME "Apple Clang"
#else
  #define MCC_PLATFORM_LINUX 1
  #define MCC_OS_NAME "Linux"
  #define MCC_EXE_EXT ""
  #define MCC_OBJ_EXT ".o"
  #define MCC_DEFAULT_PREFIX "/usr/local/bin"
  #define MCC_PATH_SEP "/"
  #define MCC_DEFAULT_CC "gcc"
  #define MCC_COMPILER_NAME "GCC"
#endif

#ifndef MCC_INSTALL_PREFIX
  #define MCC_INSTALL_PREFIX MCC_DEFAULT_PREFIX
#endif

#define MCC_MAX_LINE_LEN 4096
#define MCC_MAX_VARS 256
#define MCC_MAX_SETS 128
#define MCC_MAX_SET_ITEMS 512

#endif /* MATHC_CONFIG_H */

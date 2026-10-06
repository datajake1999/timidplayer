#ifndef _TIMID_SHLWAPI_SHIM_H
#define _TIMID_SHLWAPI_SHIM_H

#include <windows.h>

#ifndef _INT_PTR_DEFINED
#define _INT_PTR_DEFINED
typedef int INT_PTR;
typedef unsigned int UINT_PTR;
#endif
#ifndef _LONG_PTR_DEFINED
#define _LONG_PTR_DEFINED
typedef long LONG_PTR;
typedef unsigned long ULONG_PTR;
#endif
#ifndef _DWORD_PTR_DEFINED
#define _DWORD_PTR_DEFINED
typedef unsigned long DWORD_PTR;
#endif

#include <commctrl.h>
#ifndef UDM_SETRANGE32
#define UDM_SETRANGE32 (WM_USER+111)
#endif
#ifndef UDM_GETRANGE32
#define UDM_GETRANGE32 (WM_USER+112)
#endif
#ifndef UDM_SETPOS32
#define UDM_SETPOS32   (WM_USER+113)
#endif
#ifndef UDM_GETPOS32
#define UDM_GETPOS32   (WM_USER+114)
#endif

#ifndef VK_VOLUME_DOWN
#define VK_VOLUME_DOWN       0xAE
#endif
#ifndef VK_VOLUME_UP
#define VK_VOLUME_UP         0xAF
#endif
#ifndef VK_MEDIA_NEXT_TRACK
#define VK_MEDIA_NEXT_TRACK  0xB0
#endif
#ifndef VK_MEDIA_PREV_TRACK
#define VK_MEDIA_PREV_TRACK  0xB1
#endif
#ifndef VK_MEDIA_STOP
#define VK_MEDIA_STOP        0xB2
#endif
#ifndef VK_MEDIA_PLAY_PAUSE
#define VK_MEDIA_PLAY_PAUSE  0xB3
#endif

#ifndef WM_APPCOMMAND
#define WM_APPCOMMAND 0x0319
#endif
#ifndef FAPPCOMMAND_MASK
#define FAPPCOMMAND_MASK 0xF000
#endif
#ifndef GET_APPCOMMAND_LPARAM
#define GET_APPCOMMAND_LPARAM(lParam) ((short)(HIWORD(lParam) & ~FAPPCOMMAND_MASK))
#endif
#ifndef APPCOMMAND_MEDIA_NEXTTRACK
#define APPCOMMAND_MEDIA_NEXTTRACK     11
#endif
#ifndef APPCOMMAND_MEDIA_PREVIOUSTRACK
#define APPCOMMAND_MEDIA_PREVIOUSTRACK 12
#endif
#ifndef APPCOMMAND_MEDIA_STOP
#define APPCOMMAND_MEDIA_STOP          13
#endif
#ifndef APPCOMMAND_MEDIA_PLAY_PAUSE
#define APPCOMMAND_MEDIA_PLAY_PAUSE    14
#endif

#ifndef TBS_TOOLTIPS
#define TBS_TOOLTIPS 0x0100
#endif

#ifndef LVCOLUMN
#define LVCOLUMN LV_COLUMN
#endif
#ifndef LVITEM
#define LVITEM LV_ITEM
#endif
#ifndef LVM_SETEXTENDEDLISTVIEWSTYLE
#define LVM_SETEXTENDEDLISTVIEWSTYLE (LVM_FIRST + 54)
#endif
#ifndef LVM_GETEXTENDEDLISTVIEWSTYLE
#define LVM_GETEXTENDEDLISTVIEWSTYLE (LVM_FIRST + 55)
#endif
#ifndef LVS_EX_FULLROWSELECT
#define LVS_EX_FULLROWSELECT 0x00000020
#endif

#ifndef BIF_NEWDIALOGSTYLE
#define BIF_NEWDIALOGSTYLE 0x0040
#endif

#ifdef __cplusplus
extern "C" {
#endif

BOOL WINAPI PathFileExistsA(LPCSTR pszPath);
BOOL WINAPI PathFileExistsW(LPCWSTR pszPath);
BOOL WINAPI PathIsDirectoryA(LPCSTR pszPath);
BOOL WINAPI PathIsDirectoryW(LPCWSTR pszPath);
BOOL WINAPI PathIsRelativeA(LPCSTR pszPath);
BOOL WINAPI PathIsRelativeW(LPCWSTR pszPath);
DWORD WINAPI SHDeleteKeyA(HKEY hkey, LPCSTR pszSubKey);
DWORD WINAPI SHDeleteKeyW(HKEY hkey, LPCWSTR pszSubKey);
int WINAPI SHCreateDirectoryExA(HWND hwnd, LPCSTR pszPath, SECURITY_ATTRIBUTES *psa);
int WINAPI SHCreateDirectoryExW(HWND hwnd, LPCWSTR pszPath, SECURITY_ATTRIBUTES *psa);

#ifdef __cplusplus
}
#endif

#ifdef UNICODE
#define PathFileExists      PathFileExistsW
#define PathIsDirectory     PathIsDirectoryW
#define PathIsRelative      PathIsRelativeW
#define SHDeleteKey         SHDeleteKeyW
#define SHCreateDirectoryEx SHCreateDirectoryExW
#else
#define PathFileExists      PathFileExistsA
#define PathIsDirectory     PathIsDirectoryA
#define PathIsRelative      PathIsRelativeA
#define SHDeleteKey         SHDeleteKeyA
#define SHCreateDirectoryEx SHCreateDirectoryExA
#endif

#endif

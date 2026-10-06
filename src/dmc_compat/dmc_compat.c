#include <windows.h>
#include <tchar.h>
#include "shlwapi.h"

typedef BOOL (WINAPI *PathFileExistsA_t)(LPCSTR);
typedef BOOL (WINAPI *PathFileExistsW_t)(LPCWSTR);
typedef BOOL (WINAPI *PathIsDirectoryA_t)(LPCSTR);
typedef BOOL (WINAPI *PathIsDirectoryW_t)(LPCWSTR);
typedef BOOL (WINAPI *PathIsRelativeA_t)(LPCSTR);
typedef BOOL (WINAPI *PathIsRelativeW_t)(LPCWSTR);
typedef DWORD (WINAPI *SHDeleteKeyA_t)(HKEY, LPCSTR);
typedef DWORD (WINAPI *SHDeleteKeyW_t)(HKEY, LPCWSTR);
typedef int (WINAPI *SHCreateDirectoryExA_t)(HWND, LPCSTR, SECURITY_ATTRIBUTES *);
typedef int (WINAPI *SHCreateDirectoryExW_t)(HWND, LPCWSTR, SECURITY_ATTRIBUTES *);

static HMODULE GetShlwapi(void)
{
	return LoadLibrary(_T("shlwapi.dll"));
}

static HMODULE GetShell32(void)
{
	return LoadLibrary(_T("shell32.dll"));
}

BOOL WINAPI PathFileExistsA(LPCSTR pszPath)
{
	static PathFileExistsA_t fn = NULL;
	if (!fn) fn = (PathFileExistsA_t)GetProcAddress(GetShlwapi(), "PathFileExistsA");
	if (fn) return fn(pszPath);
	else return FALSE;
}

BOOL WINAPI PathFileExistsW(LPCWSTR pszPath)
{
	static PathFileExistsW_t fn = NULL;
	if (!fn) fn = (PathFileExistsW_t)GetProcAddress(GetShlwapi(), "PathFileExistsW");
	if (fn) return fn(pszPath);
	else return FALSE;
}

BOOL WINAPI PathIsDirectoryA(LPCSTR pszPath)
{
	static PathIsDirectoryA_t fn = NULL;
	if (!fn) fn = (PathIsDirectoryA_t)GetProcAddress(GetShlwapi(), "PathIsDirectoryA");
	if (fn) return fn(pszPath);
	else return FALSE;
}

BOOL WINAPI PathIsDirectoryW(LPCWSTR pszPath)
{
	static PathIsDirectoryW_t fn = NULL;
	if (!fn) fn = (PathIsDirectoryW_t)GetProcAddress(GetShlwapi(), "PathIsDirectoryW");
	if (fn) return fn(pszPath);
	else return FALSE;
}

BOOL WINAPI PathIsRelativeA(LPCSTR pszPath)
{
	static PathIsRelativeA_t fn = NULL;
	if (!fn) fn = (PathIsRelativeA_t)GetProcAddress(GetShlwapi(), "PathIsRelativeA");
	if (fn) return fn(pszPath);
	else return FALSE;
}

BOOL WINAPI PathIsRelativeW(LPCWSTR pszPath)
{
	static PathIsRelativeW_t fn = NULL;
	if (!fn) fn = (PathIsRelativeW_t)GetProcAddress(GetShlwapi(), "PathIsRelativeW");
	if (fn) return fn(pszPath);
	else return FALSE;
}

DWORD WINAPI SHDeleteKeyA(HKEY hkey, LPCSTR pszSubKey)
{
	static SHDeleteKeyA_t fn = NULL;
	if (!fn) fn = (SHDeleteKeyA_t)GetProcAddress(GetShlwapi(), "SHDeleteKeyA");
	if (fn) return fn(hkey, pszSubKey);
	else return ERROR_CALL_NOT_IMPLEMENTED;
}

DWORD WINAPI SHDeleteKeyW(HKEY hkey, LPCWSTR pszSubKey)
{
	static SHDeleteKeyW_t fn = NULL;
	if (!fn) fn = (SHDeleteKeyW_t)GetProcAddress(GetShlwapi(), "SHDeleteKeyW");
	if (fn) return fn(hkey, pszSubKey);
	else return ERROR_CALL_NOT_IMPLEMENTED;
}

int WINAPI SHCreateDirectoryExA(HWND hwnd, LPCSTR pszPath, SECURITY_ATTRIBUTES *psa)
{
	static SHCreateDirectoryExA_t fn = NULL;
	if (!fn) fn = (SHCreateDirectoryExA_t)GetProcAddress(GetShell32(), "SHCreateDirectoryExA");
	if (fn) return fn(hwnd, pszPath, psa);
	else return ERROR_CALL_NOT_IMPLEMENTED;
}

int WINAPI SHCreateDirectoryExW(HWND hwnd, LPCWSTR pszPath, SECURITY_ATTRIBUTES *psa)
{
	static SHCreateDirectoryExW_t fn = NULL;
	if (!fn) fn = (SHCreateDirectoryExW_t)GetProcAddress(GetShell32(), "SHCreateDirectoryExW");
	if (fn) return fn(hwnd, pszPath, psa);
	else return ERROR_CALL_NOT_IMPLEMENTED;
}

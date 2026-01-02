#include "stdafx.h"
#include "XWinVer.h"

#if defined(_DEBUG) && defined(_CRTDBG_MAP_ALLOC)
#define new DEBUG_NEW
#endif

// DNM 3/11/15: GetVersionEx is now deprecated in VS2013, so I added the IsVersion
// function and made all of these call it
BOOL Is2000OrGreater()
{
/*	OSVERSIONINFO osinfo;
	osinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	GetVersionEx(&osinfo);

	DWORD dwPlatformId   = osinfo.dwPlatformId;
	DWORD dwMajorVersion = osinfo.dwMajorVersion;

	return ((dwPlatformId == 2) &&
			(dwMajorVersion >= 5)); */
  return IsVersion(5, VER_GREATER_EQUAL, 0, VER_GREATER_EQUAL);
}

BOOL Is2000()
{
/*	OSVERSIONINFO osinfo;
	osinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	GetVersionEx(&osinfo);

	DWORD dwPlatformId   = osinfo.dwPlatformId;
	DWORD dwMajorVersion = osinfo.dwMajorVersion;
	DWORD dwMinorVersion = osinfo.dwMinorVersion;

	return ((dwPlatformId == 2) &&
			(dwMajorVersion == 5) &&
			(dwMinorVersion == 0)); */
  return IsVersion(5, VER_EQUAL, 0, VER_EQUAL);
}

BOOL IsXP()
{
/*	OSVERSIONINFO osinfo;
	osinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	GetVersionEx(&osinfo);

	DWORD dwPlatformId   = osinfo.dwPlatformId;
	DWORD dwMajorVersion = osinfo.dwMajorVersion;
	DWORD dwMinorVersion = osinfo.dwMinorVersion;

	return ((dwPlatformId == 2) &&
			(dwMajorVersion == 5) &&
			(dwMinorVersion >= 1)); */
  return IsVersion(5, VER_EQUAL, 1, VER_GREATER_EQUAL);
}

BOOL IsVista()
{
/*	OSVERSIONINFO osinfo;
	osinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	GetVersionEx(&osinfo);

	DWORD dwPlatformId   = osinfo.dwPlatformId;
	DWORD dwMajorVersion = osinfo.dwMajorVersion;

	return ((dwPlatformId == 2) &&
			(dwMajorVersion >= 6)); */
  return IsVersion(6, VER_GREATER_EQUAL, 0, VER_GREATER_EQUAL);
}

// And here is the currently recommended way of doing things
BOOL IsVersion(int major, int opMajor, int minor, int opMinor)
{
   OSVERSIONINFOEX osvi;
   DWORDLONG dwlConditionMask = 0;
   int op = VER_EQUAL;

   // Initialize the OSVERSIONINFOEX structure.
   ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
   osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
   osvi.dwMajorVersion = major;
   osvi.dwMinorVersion = minor;
   osvi.dwPlatformId = 2;

   // Initialize the condition mask.
   VER_SET_CONDITION( dwlConditionMask, VER_MAJORVERSION, opMajor);
   VER_SET_CONDITION( dwlConditionMask, VER_MINORVERSION, opMinor);
   VER_SET_CONDITION( dwlConditionMask, VER_PLATFORMID, op);

   // Perform the test.
   return VerifyVersionInfo(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_PLATFORMID,
      dwlConditionMask);
}

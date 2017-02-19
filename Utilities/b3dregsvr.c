/*
 * A replacement for regsvr32 that will set a mutex so that a DllMain can
 * test whether it is being loaded just for registration, and avoid doing
 * initialization steps that are incompatible with registration.
 */
#include <Windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Open an appropriate Visual Studio command prompt and compile with:
   cl b3dregsv.c user32.lib
*/

int main(int argc, char **argv)
{
  int unreg = 0;
  int ind = 1;
  HMODULE lib;
  FARPROC proc;
  HRESULT result;
  HANDLE hMutex = CreateMutex(NULL, FALSE, "b3dregsvrMutex");
  if (argc > 1 && strcmp(argv[1], "-u") == 0) {
    unreg = 1;
    ind = 2;
  }
  if (argc <= ind) {
    MessageBox(NULL, "Usage: b3dregsvr [-u] DLL_TO_REGISTER", 
               "b3dregsvr Usage", MB_OK | MB_ICONINFORMATION);
    exit(1);
  }
  lib = LoadLibrary((LPCTSTR)argv[ind]);
  if (!lib) {
    MessageBox(NULL, "Failed to load the library", 
               argv[ind], MB_OK | MB_ICONEXCLAMATION);
    exit(1);
  }
  if (unreg)
    proc = GetProcAddress(lib, "DllUnregisterServer");
  else
    proc = GetProcAddress(lib, "DllRegisterServer");
  if (!lib) {
    MessageBox(NULL, "Failed to get the procedure address", 
               argv[ind], MB_OK | MB_ICONEXCLAMATION);
    FreeLibrary(lib);
    exit(1);
  }
  result = proc();
  FreeLibrary(lib);
  if (result == S_OK) {
    MessageBox(NULL, unreg ? "Unregistration succeeded" : "Registration succeeded",
               argv[ind], MB_OK | MB_ICONINFORMATION);
    return 0;
  }
  MessageBox(NULL, unreg ? "Unregistration failed - this is normal if it was already unregistered" : "Registration failed", 
             argv[ind], MB_OK | MB_ICONEXCLAMATION);
  return 1;
}

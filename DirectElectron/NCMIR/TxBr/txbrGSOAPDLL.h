
// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the TXBRGSOAPDLL_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// TXBRGSOAPDLL_API functions as being imported from a DLL, wheras this DLL sees symbols
// defined with this macro as being exported.
#ifdef TXBRGSOAPDLL_EXPORTS
#define TXBRGSOAPDLL_API __declspec(dllexport)
#else
#define TXBRGSOAPDLL_API __declspec(dllimport)
#endif



// This class is exported from the txbrGSOAPDLL.dll
class TXBRGSOAPDLL_API CTxbrGSOAPDLL {
public:
	CTxbrGSOAPDLL(void);
	~CTxbrGSOAPDLL();

	// TODO: add your methods here.
};

extern TXBRGSOAPDLL_API int nTxbrGSOAPDLL;





TXBRGSOAPDLL_API int fnTxbrGSOAPDLL(void);
TXBRGSOAPDLL_API int txbr_runOFFTxbr(char* server, int port, char* directory, char* workdirectory, char* basename,enum FILE_TYPE file_type, enum SCOPE scope_type);
TXBRGSOAPDLL_API int txbr__resetOFFTxbr( char* server, int port,char* workDirectory);
TXBRGSOAPDLL_API int txbr__statOFFTxbr( char* server, int port,char* workDirectory);


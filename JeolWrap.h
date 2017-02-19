#ifndef JeolWrap

char *TemErrStr(int result, char *buffer);
short JeolWrap2(short result, char *method = 0); 


/* JEOL Calibrations */

const double Jeol_IS1X_to_um =  0.00508;
const double Jeol_IS1Y_to_um =  0.00434;

// 4/25/05: made Jeol_OLfine_to_um be a property
// 7/20/05: made Jeol_CLA1X_to_um and Jeol_CLA1Y_to_um be one property

// GetCurrentDensity is supposed to return fA/cm^2: this is 300 cm^2 * 1.e-6 nA/fA
// 12/10/04: The current was off by a factor of 10 but ScreenCurrentFactor fixes this
const double Jeol_current_to_na = 3.e-4;


// thanks to mconst for some of this macro magic


//DNM 4/23/06: added retry limit and increased error types that will retry
#define JEOL_RETRY_MS 200
#define JEOL_RETRY_NUM 10
#define JEOL_RETRY_TIMEOUT  3

static short int _jeol_result = 0;
static int _jeol_retry_limit = JEOL_RETRY_NUM;
static int _jeol_retry_count;


// Awfully hard to rewrite this - change one thing at a time and check compilation

// NCE stands for No COM Exception: make a version of wrappers with and without

// These are the wrappers for calls to do retries, set a flag, and maybe throw an error
#define JeolWrapNCE(foo,bar)  \
{                     \
  for (_jeol_retry_count = 0; _jeol_retry_count < _jeol_retry_limit; _jeol_retry_count++) { \
    _jeol_result = -1; \
try {  \
      _jeol_result = JeolWrap2(foo,bar) & 0x3ff; \
   } \
    catch (...) {\
      TRACE("JeolWrap ignoring an exception!");\
      SEMTrace('1', "JeolWrap ignoring an exception!"); \
      _jeol_result = -1; \
    }\
    if (_jeol_result == 10 && _jeol_retry_count + 1 >= JEOL_RETRY_TIMEOUT) \
      break; \
    if (_jeol_result == -1  || _jeol_result == 3 || _jeol_result == 10 || \
      _jeol_result == 11 || _jeol_result == 13 || _jeol_result == 14) \
      Sleep(JEOL_RETRY_MS);\
    else \
      break; \
  } \
}

#define JeolWrap(foo,bar)  \
{                     \
  JeolWrapNCE(foo,bar); \
  if (_jeol_result) \
    throw(_com_error((HRESULT)JEOL_FAKE_HRESULT, NULL, true)); \
}

// Wrappers for using image shift or projector shift interchangeably
#define GetISwrapNCE(td,x,y) _jeol_result = JeolState::GetISorPL(td, x, y);
#define SetISwrapNCE(td,x,y) _jeol_result = JeolState::SetISorPL(td, x, y);
#define GetISwrap(td,x,y) \
{ \
  _jeol_result = JeolState::GetISorPL(td, x, y); \
  if (_jeol_result) \
    throw(_com_error((HRESULT)JEOL_FAKE_HRESULT, NULL, true)); \
}
#define SetISwrap(td,x,y) \
{ \
  _jeol_result = JeolState::SetISorPL(td, x, y); \
  if (_jeol_result) \
    throw(_com_error((HRESULT)JEOL_FAKE_HRESULT, NULL, true)); \
}

#endif

/* It was:
      _jeol_result = foo; \
      JeolWrap2(foo,bar); \
   Which was doing the call twice!  2/23/06 change to:
      _jeol_result = JeolWrap2(foo,bar); \
   */

/* For more information on the protection of Jeol functions, please see:


  http://www.livejournal.com/users/nibot_lab/20692.html  (info on mutex + wrapper function)

  http://www.livejournal.com/users/nibot_lab/20353.html  (a trick you can use to trace the Jeol functions)


*/

/*
 *  cppdefs.h -- Handy includes and defines for cpp programs
 */

#ifndef CPPDEFS_H
#define CPPDEFS_H

#include "b3dutil.h"
#include <iostream>
#include <vector>
using namespace std;

typedef std::vector<float> FloatVec;
typedef std::vector<int> IntVec;

#define SET_CONTROL_FLOAT(a,b) case a: b = yy ; cout << #b << " set to " << yy << endl ; break
#define SET_CONTROL_INT(a,b) case a: b = B3DNINT(yy) ; cout << #b << " set to " << B3DNINT(yy) << endl ; break

//#define PRINT1(a,b) printf(#b" = %"#a"\n", b);
#define PRINT1(a) cout << #a << " = " << a << endl
#define PRINT2(a,b) cout << #a << " = " << a << ",  " #b << " = " << b << endl
#define PRINT3(a,b,c) cout << #a << " = " << a << ",  " #b << " = " << b << ",  " #c << " = " << c << endl
#define PRINT4(a,b,c,d) cout << #a << " = " << a << ",  " #b << " = " << b << ",  " #c << " = " << c << ",  " #d << " = " << d << endl

// Some favorite macros from SerialEM (note no ; on the member ones, and lower case)
#define setMember(a,b) void set##b(a inVal) {m##b = inVal;}
#define getMember(a,b) a get##b() {return m##b;}
#define getSetMember(a,b) getMember(a,b);       \
  setMember(a,b)

#ifndef CLEAR_RESIZE
#define CLEAR_RESIZE(a,b,c) { a.clear();       \
    std::vector<b> vtmp = std::vector<b>(a);   \
  a.swap(vtmp);                                \
  a.resize(c); }
#endif

#endif

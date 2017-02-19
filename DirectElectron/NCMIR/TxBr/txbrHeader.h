#ifndef TXBR_WS_H
#define TXBR_WS_H

enum FILE_TYPE { JPG , TIF };
enum SCOPE { J3200, J4000_1, J4000_2, FEI_titan, FEI_spirit };

int txbr__runOFFTxbr( char* fileFolderLocation, char* workDirectory, char* basename, enum FILE_TYPE type, enum SCOPE scope, int &return_val );

int txbr__resetOFFTxbr( char* workDirectory, int &return_val );

int txbr__statOFFTxbr( char* workDirectory, int &return_val );

#endif

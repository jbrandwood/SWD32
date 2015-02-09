// **************************************************************************
// **************************************************************************
// **************************************************************************
// **                                                                      **
// ** SWD32.C                                                      PROGRAM **
// **                                                                      **
// ** SWD compressor for Win32 console mode.                               **
// **                                                                      **
// ** Copyright John Brandwood 1992-2015.                                  **
// **                                                                      **
// ** Distributed under the Boost Software License, Version 1.0.           **
// ** (See accompanying file LICENSE_1_0.txt or copy at                    **
// **  http://www.boost.org/LICENSE_1_0.txt)                               **
// **                                                                      **
// **************************************************************************
// **************************************************************************
// **************************************************************************

#ifndef __SWD32_h
#define __SWD32_h

//
// GLOBAL DATA STRUCTURES AND DEFINITIONS
//

//
// GLOBAL VARIABLES
//

extern	BOOL                fl___MidiBusy;
extern	BOOL                fl___MidiBankRead;
extern	BOOL                fl___MidiBankSend;
extern	BOOL                fl___BankLoaded;
extern	BOOL                fl___BankChanged;

extern	char *              pcz__AppName;
extern	char *              pcz__AppErrTitle;

extern	char                szFileName  [_MAX_PATH];
extern	char                szTitleName [_MAX_FNAME + _MAX_EXT];

//
// GLOBAL FUNCTION PROTOTYPES
//

extern	int __cdecl         main                    (
								int                 argc,
								char **             argv);

extern	int                 ShrinkBlockFile         (void);
extern	int                 ExpandBlockFile         (void);
extern	int                 ShrinkWholeFile         (void);
extern	int                 ExpandWholeFile         (void);

extern	int                 RecvLzssToken           (
								int *               pmatch_length,
								int *               pmatch_offset);

extern	int                 SendLzssToken           (
								int                 match_length,
								int                 match_offset);

extern	int                 SendLzssEOF             (void);

extern	int                 RecvByteValue           (void);

extern	int                 SendByteValue           (
								int                 value);

extern	int                 SendByteEOF             (void);

//
// End of __SWD32_h
//

#endif



// **************************************************************************
// **************************************************************************
// **************************************************************************
//	END OF SWD32.H
// **************************************************************************
// **************************************************************************
// **************************************************************************
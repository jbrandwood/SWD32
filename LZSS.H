// **************************************************************************
// **************************************************************************
// **************************************************************************
// **                                                                      **
// ** LZSS.H                                                        MODULE **
// **                                                                      **
// ** Lempel-Ziv sliding window compression routines.                      **
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

#ifndef __LZSS_h
#define __LZSS_h

//
// GLOBAL DATA STRUCTURES AND DEFINITIONS
//

//
// GLOBAL VARIABLES
//

//
// GLOBAL FUNCTION PROTOTYPES
//

extern	int                 OpenLzss                (
								int                 breakeven,
								int                 maxlength,
								int                 maxoffset);

extern	int                 ShutLzss                (void);

extern	int                 LzssShrinkByteFile      (void);
extern	int                 LzssExpandByteFile      (void);

//extern	int                 LzssShrinkWordFile      (void);
//extern	int                 LzssExpandWordFile      (void);

//
// EXTERNAL FUNCTIONS NEEDED
//

extern	int                 RecvByteValue           (void);

extern	int                 SendByteValue           (
								int                 value);

extern	int                 SendByteEOF             (void);

extern	int                 RecvLzssToken           (
								int *               pmatch_length,
								int *               pmatch_offset);

extern	int                 SendLzssToken           (
								int                 match_length,
								int                 match_offset);

extern	int                 SendLzssEOF             (void);

//
// End of __LZSS_h
//

#endif



// **************************************************************************
// **************************************************************************
// **************************************************************************
//	END OF LZSS.H
// **************************************************************************
// **************************************************************************
// **************************************************************************

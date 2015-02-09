// **************************************************************************
// **************************************************************************
// **************************************************************************
// **                                                                      **
// ** LZSS.C                                                        MODULE **
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

#define  WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <ctype.h>

#include <stdint.h>

#include "lzss.h"

//
// DEFINITIONS
//

// IDX_BIT_COUNT = how many bits for offest into the text window
// LEN_BIT_COUNT = how many bits for the length of an encode phrase
//
// TREE_ROOT is a special node in the tree that always points to
// the root node of the binary phrase tree.
//
// UNUSED is the null index for the tree.
//
// MOD_WINDOW() is a macro used to perform arithmetic on tree indices.

#define	LZSS_WINDOW_SIZE    0x0800
#define	LZSS_WINDOW_MASK    0x07FF

#define MOD_WINDOW(a)       ((a) & LZSS_WINDOW_MASK)

#define UNUSED              (LZSS_WINDOW_SIZE)
#define TREE_ROOT           (LZSS_WINDOW_SIZE+1)

#define LZSS_LEN_BITS       4
#define LZSS_IDX_BITS       11

#define LZSS_BREAK_EVEN     ((1 + LZSS_IDX_BITS + LZSS_LEN_BITS) / 9)
#define	LZSS_MAX_LENGTH     ((1 << LZSS_LEN_BITS) + LZSS_BREAK_EVEN)
#define	LZSS_MAX_OFFSET     ((1 << LZSS_IDX_BITS) - 1)

//
// GLOBAL VARIABLES
//

extern	volatile uint8_t *  pub__LoadCur;
extern	volatile uint8_t *  pub__LoadEnd;

extern	volatile int *      psi__LzssCur;
extern	volatile int *      psi__LzssEnd;

extern	uint8_t *           pub__ByteCur;

extern	int                 FillRecvBuffer          (void);
extern	int                 SaveLzssToBits          (void);

//
// STATIC VARIABLES
//

// These are the two global data structures used in this program.
//
// pbf__Data points to the data window of previously seen text,
// as well as the current look ahead text. It is data at the
// start of the window is mirrored at the end of the window
// to avoid having to do a MOD() during string operations.
//
// pcl__Tree points to the binary tree of all of the strings
// in the window sorted in order.

static	BOOL                fl___LzssInitialized;

static	int                 si___BreakEven = LZSS_BREAK_EVEN;
static	int                 si___MaxLength = LZSS_MAX_LENGTH;
static	int                 si___MaxOffset = LZSS_MAX_OFFSET;

//
// STATIC FUNCTION PROTOTYPES
//

//
// ASSEMBLY STRUCTURES (interleaved for Pentium access).
//

typedef	struct NODE_S
	{
	struct NODE_S *     bigger;			// Must remain at offset 0.
	struct NODE_S *     parent;
	struct NODE_S *     numeric_next;
	struct NODE_S *     lesser;
	uint8_t *           window;
	struct NODE_S *     numeric_last;
	} NODE_T;

//
// ASSEMBLY VARIABLES
//

#ifdef _MSC_VER
#pragma warning (disable : 4229)
#endif

extern	uint8_t              __cdecl aub__gData [];
extern	NODE_T               __cdecl acl__gTree [];
extern	NODE_T               __cdecl cl___gNull;
extern	NODE_T               __cdecl acl__gRoot [];

extern	volatile int32_t     __cdecl si___gMaxLength;
extern	volatile int32_t     __cdecl si___gMatchSize;
extern	volatile NODE_T *    __cdecl pcl__gMatchTree;
extern	volatile uint8_t *   __cdecl pub__gMatchData;

//
// ASSEMBLY FUNCTIONS
//

extern	void                 __cdecl InitTree  (void);
extern	void                 __cdecl AddString (int node);
extern	void                 __cdecl RmvString (int node);

//
//
//



// **************************************************************************
// **************************************************************************
// **************************************************************************
//	GLOBAL FUNCTIONS
// **************************************************************************
// **************************************************************************
// **************************************************************************



// **************************************************************************
// * OpenLZSS ()                                                            *
// **************************************************************************
// *                                                                        *
// **************************************************************************
// * Inputs  -                                                              *
// *                                                                        *
// * Output  int      -ve if an error occurred                              *
// **************************************************************************

int                 OpenLzss                (
								int                 breakeven,
								int                 maxlength,
								int                 maxoffset)

	{
	// Already initialized ?

	if (fl___LzssInitialized)
		{
		ShutLzss();
		}

	fl___LzssInitialized = TRUE;

	// Update compression settings.

	if (breakeven)
		{
		si___BreakEven = breakeven;
		si___MaxLength = maxlength;
		si___MaxOffset = maxoffset;
		}

	// All done.

	return (0);

	// Error handler.

//	errorExit:

	ShutLzss();

	return (-1);
	}



// **************************************************************************
// * ShutLzss ()                                                            *
// **************************************************************************
// *                                                                        *
// **************************************************************************
// * Inputs  -                                                              *
// *                                                                        *
// * Output  int      -ve if an error occurred                              *
// **************************************************************************

int                 ShutLzss                (void)

	{
	// Already initialized ?

	fl___LzssInitialized = FALSE;

	// All done.

	return (0);
	}



// **************************************************************************
// * LzssShrinkByteFile ()                                                  *
// **************************************************************************
// *                                                                        *
// **************************************************************************
// * Inputs  -                                                              *
// *                                                                        *
// * Output  int      Return value                                          *
// *                                                                        *
// * N.B.    This is the compression routine for the LZSS algorithm.        *
// *                                                                        *
// *         It has to first load up the look ahead buffer, then go into    *
// *         the main compression loop.                                     *
// *                                                                        *
// *         The main loop decides whether to output a single character     *
// *         or an index/length token that defines a phrase.                *
// *                                                                        *
// *         Once the character or phrase has been sent out, another loop   *
// *         has to run.                                                    *
// *                                                                        *
// *         The second loop reads in new characters, deletes the strings   *
// *         that are overwritten by the new character, then adds the       *
// *         strings that are created by the new character.                 *
// **************************************************************************

int                 LzssShrinkByteFile      (void)

	{
	// Local Variables.

	unsigned char *     window;

	int                 i;
	int                 v;
	int                 offset;
	int                 look_ahead;
	int                 replace_count;

	int                 match_length;
	int                 match_offset;

	// Make sure that we've been initialized.

	if (!fl___LzssInitialized)
		{
		return (-1);
		}

	InitTree();

	// Load up the look-ahead buffer.

	window = (unsigned char *) &aub__gData[0];
	offset = 0;

	for (i = 0; i < si___MaxLength; i++)
		{
		if (pub__LoadCur == pub__LoadEnd)
			{
			if (FillRecvBuffer() < 0) break;
			}

		v = *pub__LoadCur++;

		window[i + LZSS_WINDOW_SIZE] =
		window[i]                    = (unsigned char) v;
		}

	AddString(offset);

	look_ahead = i;

	// Loop around encoding strings until the buffer is empty.

	si___gMaxLength = si___MaxLength;
	si___gMatchSize = si___MaxLength;

	while (look_ahead > 1)
		{
		// Convert ui___gMatchSize into the real match length.

		match_length = (si___gMaxLength - 1) - si___gMatchSize;

		// Output result of last string match.

		if (match_length > look_ahead)
			{
			match_length = look_ahead;
			}

		if (match_length <= si___BreakEven)
			{
			if (psi__LzssCur == psi__LzssEnd)
				{
				if (SaveLzssToBits() < 0) return (-1);
				}

			replace_count   = 1;
			*psi__LzssCur++ = 1;
			*psi__LzssCur++ = window[offset];
			*pub__ByteCur++ = window[offset];
			}
		else
			{
			if (psi__LzssCur == psi__LzssEnd)
				{
				if (SaveLzssToBits() < 0) return (-1);
				}

			replace_count   = match_length;
			*psi__LzssCur++ = match_length;

			*psi__LzssCur++ = MOD_WINDOW(&window[offset] - pcl__gMatchTree->window);

//			*psi__LzssCur++ = MOD_WINDOW(offset - match_offset);
			}

		if (i < 0)
			{
			return (-1);
			}

		// Read in the new characters.

		for (i = 0; i < replace_count; i++)
			{
			RmvString(MOD_WINDOW(offset - si___MaxOffset));

			v = 0;

			if (pub__LoadCur == pub__LoadEnd)
				{
				v = FillRecvBuffer();
				}

			if (v < 0)
				{
				look_ahead--;
				}
			else
				{
				v = *pub__LoadCur++;

				match_offset = MOD_WINDOW(offset + si___MaxLength);

				window[match_offset + LZSS_WINDOW_SIZE] =
				window[match_offset]                    = (unsigned char) v;
				}

			offset = MOD_WINDOW(offset + 1);

			if (look_ahead)
				{
				AddString(offset);
				}
			}

		// Search the winner for the nearest.

		} // End of "while (look_ahead_bytes > 0)"

	// Encode the last byte (if there is one).

	if (look_ahead > 0)
		{
		if (psi__LzssCur == psi__LzssEnd)
			{
			if (SaveLzssToBits() < 0) return (-1);
			}

		*psi__LzssCur++ = 1;
		*psi__LzssCur++ = window[offset];
		*pub__ByteCur++ = window[offset];

		offset = MOD_WINDOW(offset + 1);
		}

	// File finished.

	SendLzssEOF();

	// All done.

	return (0);
	}



// **************************************************************************
// * LzssExpandByteFile ()                                                  *
// **************************************************************************
// *                                                                        *
// **************************************************************************
// * Inputs  -                                                              *
// *                                                                        *
// * Output  int      Return value                                          *
// *                                                                        *
// * N.B.    This is the expansion routine for the LZSS algorithm.          *
// *                                                                        *
// *         All it has to do is read in the token, decide whether to read  *
// *         a character or a index/length pair, and take the appropriate   *
// *         action.                                                        *
// **************************************************************************

int                 LzssExpandByteFile      (void)

	{
	// Local Variables.

	unsigned char *     window;

	int                 i;
	int                 v;
	int                 offset;
	int                 match_length;
	int                 match_offset;

	// Make sure that we've been initialized.

	if (!fl___LzssInitialized)
		{
		return (-1);
		}

	//

	window = (unsigned char *) &aub__gData[0];

	offset = 0;

	while (1)
		{
		if (RecvLzssToken(&match_length, &match_offset) < 0)
			{
			return (-1);
			}

		if (match_length == 0) break;

		if (match_length == 1)
			{
			v = (int) match_offset;

			SendByteValue(v);

			window[offset] = (unsigned char) v;

			offset = MOD_WINDOW(offset + 1);
			}
		else
			{
			match_offset = MOD_WINDOW(offset - match_offset);

			for (i = 0; i < match_length; i++)
				{
				v = window[MOD_WINDOW(match_offset + i)];

				SendByteValue(v);

				window[offset] = (unsigned char) v;

				offset = MOD_WINDOW(offset + 1);
				}
			}
		}

	// File finished.

	SendByteEOF();

	// All done.

	return (0);
	}



// **************************************************************************
// **************************************************************************
// **************************************************************************
//	STATIC FUNCTIONS
// **************************************************************************
// **************************************************************************
// **************************************************************************



// **************************************************************************
// **************************************************************************
// **************************************************************************
//	END OF LZSS.C
// **************************************************************************
// **************************************************************************
// **************************************************************************

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

// **************************************************************************
// **************************************************************************
// **                                                                      **
// ** The data is encoded as length/offset pairs.                          **
// **                                                                      **
// ** Lengths are encoded ...                                              **
// **                                                                      **
// **     1       : 0  dddddddd                                            **
// **                                                                      **
// **     2       : 10                                                     **
// **                                                                      **
// **     3-5     : 11 xx                                                  **
// **                                                                      **
// **     6-20    : 11 00 xxxx                                             **
// **                                                                      **
// **    21-275   : 11 00 0000 xxxxxxxx                                    **
// **    (limited to 256 so that the decompressor can use a byte counter)  **
// **                                                                      **
// **     EOF     : 11 00 0000 00000000                                    **
// **                                                                      **
// ** Offsets are encoded ...                                              **
// **                                                                      **
// ** $0000-$001F : 00       x xxxx                                        **
// **                                                                      **
// ** $0020-$009F : 01     xxx xxxx                                        **
// **                                                                      **
// ** $00A0-$029F : 10  x xxxx xxxx                                        **
// **                                                                      **
// ** $02A0-$069F : 11 xx xxxx xxxx                                        **
// **                                                                      **
// **************************************************************************
// **************************************************************************

#define  WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <direct.h>
#include <io.h>
#include <time.h>

#include <ctype.h>

#include <tchar.h>

#include <stdint.h>

#include "lzss.h"
#include "swd32.h"

#ifdef _MSC_VER
 #define strupr _strupr
#endif

//
// DEFINITIONS
//

#define	INLINE_ADDR         1

#define	MAX_CHUNK_SIZE      0x2000

#define	VERSION_STR         "SWD32 v1.03 (" __DATE__ ")"

#define ERROR_NONE           0
#define ERROR_DIAGNOSTIC    -1
#define	ERROR_NO_MEMORY     -2
#define	ERROR_NO_FILE       -3
#define	ERROR_IO_EOF        -4
#define	ERROR_IO_READ       -5
#define	ERROR_IO_WRITE      -6
#define	ERROR_IO_SEEK       -7
#define	ERROR_PROGRAM       -8
#define	ERROR_UNKNOWN       -9
#define	ERROR_ILLEGAL      -10

//
// GLOBAL VARIABLES
//


// Fatal error flag.
//
// Used to signal that the error returned by ErrorCode (see below) is fatal
// and that the program should terminate immediately.

BOOL                fl___FatalError     = FALSE;
int                 si___ErrorCode      = ERROR_NONE;

char                acz__ErrorMessage [256];

char                acz__FileInp [_MAX_PATH + 4];
char                acz__FileOut [_MAX_PATH + 4];

char                acz__FileDrv [_MAX_DRIVE];
char                acz__FileDir [_MAX_DIR];
char                acz__FileNam [_MAX_FNAME];
char                acz__FileExt [_MAX_EXT];

//
// STATIC VARIABLES
//

long                sl___LoadTotal;
long                sl___LoadCount;
long                sl___SaveCount;

BOOL                fl___SaveToSubDir  = FALSE;
BOOL                fl___SaveToGameboy = FALSE;
BOOL                fl___GameboyFormat = FALSE;

BOOL                fl___BlocFil;
FILE *              pcl__BlocFil;
long                sl___BlocLen;

FILE *              pcl__LoadFil;
FILE *              pcl__SaveFil;

long                sl___LoadLen;
uint8_t *           pub__LoadBuf;
uint8_t *           pub__LoadCur;
uint8_t *           pub__LoadEnd;
uint8_t *           pub__LoadMrk;

long                sl___SaveLen;
uint8_t *           pub__SaveBuf;
uint8_t *           pub__SaveCur;
uint8_t *           pub__SaveEnd;

BOOL                fl___LzssEnd;

long                sl___LzssLen;
int *               psi__LzssBuf;
int *               psi__LzssCur;
int *               psi__LzssEnd;

long                sl___ByteLen;
uint8_t *           pub__ByteBuf;
uint8_t *           pub__ByteCur;
uint8_t *           pub__ByteEnd;

long                sl___WordLen;
uint16_t *          puw__WordBuf;
uint16_t *          puw__WordCur;
uint16_t *          puw__WordLen;

// Bit-oriented I/O variables.

static	unsigned    ui___BitsInp;
static	uint8_t     ub___BitsOut;
static	uint8_t     ub___BitsMsk;
static	uint8_t *   pub__BitsCur;

//
// STATIC FUNCTION PROTOTYPES
//

extern	long                LoadWholeFile           (
								char *              pcz__Name,
								unsigned char **    ppbf_Addr,
								long *              psl__Size);

extern	long                SaveWholeFile           (
								char *              pcz__Name,
								unsigned char *     pbf__Addr,
								long                sl___Size);

extern	long                GetFileLength           (
								FILE *              file);

extern	void                ErrorReset              (void);
extern	void                ErrorQualify            (void);

//
//
//

extern	int                 ProcessOption           (
								char *              pcz__Option);

extern	int                 ProcessFileSpec         (
								char *              pcz__File);

extern	int                 ProcessFile             (
								char *              pcz__File);

extern	int                 SaveLzssToBits          (void);
extern	int                 LoadBitsToLzss          (void);

static	void                TokenToBits             (
								int                 match_length,
								int                 match_offset);

static	void                BitsToToken             (
								int *               pmatch_length,
								int *               pmatch_offset);

static	void                BitIOInit               (void);

static	void                BitIOSend               (
								unsigned            ui___bitcount,
								unsigned            ui___bitvalue);


static	unsigned            BitIORecv               (
								unsigned            ui___bitcount);

static	void                BitIOFlush              (void);




// **************************************************************************
// **************************************************************************
// **************************************************************************
//	GLOBAL FUNCTIONS
// **************************************************************************
// **************************************************************************
// **************************************************************************



// **************************************************************************
// * main ()                                                                *
// **************************************************************************
// *                                                                        *
// **************************************************************************
// * Inputs  int         argument count                                     *
// *         char **     argument strings                                   *
// *                                                                        *
// * Output  int         Returns an exit code for the whole program.        *
// **************************************************************************

int __cdecl         main                    (
								int                 argc,
								char **             argv)

	{

	// Local variables.

	int                 i;

	FILE *              pcl__Res = NULL;

	// Sign on.

	printf("\n%s by J.C.Brandwood\n", VERSION_STR);

	// Allocate I/O buffers.

//	sl___LoadLen = MAX_CHUNK_SIZE + 1024;
//	sl___SaveLen = MAX_CHUNK_SIZE + 1024;
//	sl___LzssLen = sl___LoadLen * 2;
//	sl___ByteLen = sl___LoadLen;

	sl___LoadLen = 128*1024;
	sl___SaveLen = 128*1024;
	sl___LzssLen = 128*1024;
	sl___ByteLen = 128*1024;

	pub__LoadBuf = (uint8_t *) malloc((sl___LoadLen + 0x0100) * sizeof(uint8_t));
	pub__SaveBuf = (uint8_t *) malloc((sl___SaveLen + 0x0100) * sizeof(uint8_t));
	psi__LzssBuf = (int *)     malloc((sl___LzssLen + 0x0100) * sizeof(int));
	pub__ByteBuf = (uint8_t *) malloc((sl___ByteLen + 0x0100) * sizeof(uint8_t));

	if ((pub__LoadBuf == NULL) ||
	    (pub__SaveBuf == NULL) ||
	    (psi__LzssBuf == NULL) ||
	    (pub__ByteBuf == NULL))
		{
		printf("\nSwd32 - Unable to allocate I/O buffers !\n\n");
		goto exit;
		}

	sl___LoadLen = MAX_CHUNK_SIZE + 1024;
	sl___SaveLen = MAX_CHUNK_SIZE + 1024;
	sl___LzssLen = sl___LoadLen * 2;
	sl___ByteLen = sl___LoadLen;

	// Allocate LZSS buffers.

	OpenLzss(1, 256, 0x06A0);

	// Use the program name to initialize certain default settings.

	/*
	if (strlen(argv[0]) >= 256)
		{
		si___ErrorCode = ERROR_NO_FILE;
		sprintf(acz__ErrorMessage,
			"Swd32 - Program file name too long !\n");
		goto exit;
		}

	pcz__FilExtn = strcpy(acz__FilName, argv[0]);

	strupr(pcz__FilExtn);

	while ((p = strchr(pcz__FilExtn, '\\')) != NULL)
		{
		pcz__FilExtn = p + 1;
		}

	if (strcmp(pcz__FilExtn, "SWD32.EXE") == 0);
	*/

	// Check the command line arguments.

	if (argc < 2)
		{
		printf("\nUsage : SWD32 [-b|-d|-g] <filename>\n");
		goto exit;
		}

	// Read through and process the arguments.

	for (i = 1; i < argc; i++)
		{
		if ((*argv[i] == '-') || (*argv[i] == '/'))
			{
			if (ProcessOption(argv[i]) != ERROR_NONE) goto exit;
			}
		else
			{
			if (ProcessFileSpec(argv[i]) != ERROR_NONE) goto exit;
			}
		}

	// Print success message.

	printf("Swd32 - operation complete !\n\n");

	// Program exit.
	//
	// This will either be dropped through to if everything is OK, or 'goto'ed
	// if there was an error.

	exit:

	ShutLzss();

	free(pub__ByteBuf);
	free(psi__LzssBuf);
	free(pub__SaveBuf);
	free(pub__LoadBuf);

	ErrorQualify();

	if (si___ErrorCode != ERROR_NONE)
		{
		puts(acz__ErrorMessage);
		}

	if (pcl__BlocFil != NULL) fclose(pcl__BlocFil);
	if (pcl__LoadFil != NULL) fclose(pcl__LoadFil);
	if (pcl__SaveFil != NULL) fclose(pcl__SaveFil);

	// All done.

	return ((si___ErrorCode != ERROR_NONE));
	}



// **************************************************************************
// * ProcessOption ()                                                       *
// **************************************************************************
// *                                                                        *
// **************************************************************************
// * Inputs  char *      option                                             *
// *                                                                        *
// * Output  int         Returns an exit code for the whole program.        *
// **************************************************************************

int                 ProcessOption           (
								char *              pcz__Option)

	{
	// Local variables.

//	int                 i;
//	char *              p;

	// Process option string.

	strupr(pcz__Option);

	switch(pcz__Option[1])
		{
		// Switch on block mode.

		case 'B':
			{
			fl___BlocFil = TRUE;

			break;
			}

		// Switch on output to sub-directory mode.

		case 'D':
			{
			fl___SaveToSubDir = TRUE;

			break;
			}

		// Switch on Gameboy output format.

		case 'G':
			{
			fl___SaveToGameboy = TRUE;

			break;
			}

		// Unknown option.

		default:
			{
			sprintf(acz__ErrorMessage,
				"Swd32 - Unknown option !\n");
			return (si___ErrorCode = ERROR_ILLEGAL);
			}
		}

	// All done.

	return (ERROR_NONE);
	}



// **************************************************************************
// * ProcessFileSpec ()                                                     *
// **************************************************************************
// *                                                                        *
// **************************************************************************
// * Inputs  char *      option                                             *
// *                                                                        *
// * Output  int         Returns an exit code for the whole program.        *
// **************************************************************************

int                 ProcessFileSpec         (
								char *              pcz__File)

	{
	// Local variables.

	long                h____FileSpec;
	struct _finddata_t  cl___FileSpec;

	int                 i;

	char                acz__All [_MAX_PATH];
	char                acz__Drv [_MAX_DRIVE];
	char                acz__Dir [_MAX_DIR];
	char                acz__Nam [_MAX_FNAME];
	char                acz__Ext [_MAX_EXT];

	// File name too long ?

	if (strlen(pcz__File) > _MAX_PATH)
		{
		sprintf(acz__ErrorMessage,
			"Swd32 - File name too long !\n");
		return (si___ErrorCode = ERROR_ILLEGAL);
		}

	// Copy the filespec to our buffer.

	#if 0
		if (_fullpath(acz__All, pcz__File, _MAX_PATH) == NULL)
			{
			sprintf(acz__ErrorMessage,
				"Swd32 - Illegal filename argument !\n");
			return (si___ErrorCode = ERROR_NO_FILE);
			}
	#else
		strcpy(acz__All, pcz__File);
	#endif

	// Split the filespec into its components.

	_splitpath(acz__All, acz__Drv, acz__Dir, acz__Nam, acz__Ext);

	// Now search the given path for files matching the filespec.

	if ((h____FileSpec = _findfirst(acz__All, &cl___FileSpec)) == -1L)
		{
		sprintf(acz__ErrorMessage,
			"Swd32 - File not found !\n");
		return (si___ErrorCode = ERROR_NO_FILE);
		}
	else
		{
		do	{
			// Have we found a directory or a file ?

			if ((cl___FileSpec.attrib & (_A_SUBDIR)) != 0)
				{
				// Process subdirectory.

				}
			else
			if ((cl___FileSpec.attrib & (_A_HIDDEN | _A_SYSTEM)) == 0)
				{
				// Process normal file.

				strcpy(acz__All, acz__Drv);
				strcat(acz__All, acz__Dir);
				strcat(acz__All, cl___FileSpec.name);

				// Process the file.

				if ((i = ProcessFile(acz__All)) != ERROR_NONE)
					{
					_findclose(h____FileSpec);

					return (i);
					}
				}

			} while (_findnext(h____FileSpec, &cl___FileSpec) == 0);

		_findclose(h____FileSpec);
		}

	// All done, return success.

	return (ERROR_NONE);
	}



// **************************************************************************
// * ProcessFile ()                                                         *
// **************************************************************************
// *                                                                        *
// **************************************************************************
// * Inputs  char *      option                                             *
// *                                                                        *
// * Output  int         Returns an exit code for the whole program.        *
// **************************************************************************

int                 ProcessFile             (
								char *              pcz__File)

	{
	// Local variables.

	long                h____FileSpec;
	struct _finddata_t  cl___FileSpec;

	int                 i;

	BOOL                fl___Block;

	FILE *              pcl__File;
	uint8_t             aub__Data[12];

	// File name too long ?

	if (strlen(pcz__File) > _MAX_PATH)
		{
		sprintf(acz__ErrorMessage,
			"Swd32 - File name too long !\n");
		return (si___ErrorCode = ERROR_ILLEGAL);
		}

	// Split the filename into its components.

	_splitpath(pcz__File, acz__FileDrv, acz__FileDir, acz__FileNam, acz__FileExt);

	// Choose whether to compress or decompress.

	if ((pcl__File = fopen(pcz__File, "rb")) == NULL)
		{
		sprintf(acz__ErrorMessage,
			"Swd32 - Unable to open input file %s !\n",
			pcz__File);
		return (si___ErrorCode = ERROR_NO_FILE);
		}

	i = 0; fl___Block = fl___BlocFil;

	if (GetFileLength(pcl__File) >= 12)
		{
		if (fread(&aub__Data[0], 1, 12, pcl__File) == 12)
			{
			if ((aub__Data[0] == 's') &&
			    (aub__Data[1] == 'W') &&
			    (aub__Data[2] == 'd') &&
			    (aub__Data[3] >=  0x80u))
				{
				i = 1; fl___Block = (aub__Data[3] & 0x30u) ? TRUE : FALSE;
				}
			}
		}

	fclose(pcl__File);

	// Create input filename.

	strcpy(acz__FileInp, acz__FileDrv);
	strcat(acz__FileInp, acz__FileDir);
	strcat(acz__FileInp, acz__FileNam);
	strcat(acz__FileInp, acz__FileExt);

	// Modify output directory ?

	if (fl___SaveToSubDir)
		{
		// Append output subdirectory name.

		if (i == 0)
			{
			strcat(acz__FileDir, "SWD");
			}
		else
			{
			strcat(acz__FileDir, "ORG");
			}

		// Create the subdirectory if it doesn't already exist.

		strcpy(acz__FileOut, acz__FileDrv);
		strcat(acz__FileOut, acz__FileDir);

		if ((h____FileSpec = _findfirst(acz__FileOut, &cl___FileSpec)) == -1L)
			{
			// Nothing found.

			if (_mkdir(acz__FileDir) != 0)
				{
				sprintf(acz__ErrorMessage,
					"Swd32 - Unable to create directory \"%s\" !\n",
					acz__FileDir);
				return (si___ErrorCode = ERROR_NO_FILE);
				}
			}
		else
			{
			// Something found, directory or file ?

			_findclose(h____FileSpec);

			if ((cl___FileSpec.attrib & (_A_SUBDIR)) == 0)
				{
				sprintf(acz__ErrorMessage,
					"Swd32 - Unable to create directory \"%s\" !\n",
					acz__FileDir);
				return (si___ErrorCode = ERROR_NO_FILE);
				}
			}

		// Append final seperator to directory path.

		strcat(acz__FileDir, "\\");
		}

	// Now perform the actual compression or decompression.

	if (i == 0)
		{
		// Inform user ...

		printf("Swd32 - Shrinking \"%s%s\"\n", acz__FileNam, acz__FileExt);

		//

		if (fl___Block)
			{
			return (ShrinkBlockFile());
			}
		else
			{
			return (ShrinkWholeFile());
			}
		}
	else
		{
		// Inform user ...

		printf("Swd32 - Expanding \"%s%s\"\n", acz__FileNam, acz__FileExt);

		//

		if (fl___Block)
			{
			return (ExpandBlockFile());
			}
		else
			{
			return (ExpandWholeFile());
			}
		}
	}



// **************************************************************************
// **************************************************************************
// **************************************************************************
// **************************************************************************



// **************************************************************************
// * ShrinkBlockFile ()                                                     *
// **************************************************************************
// *                                                                        *
// **************************************************************************
// * Inputs  -                                                              *
// *                                                                        *
// * Output  int         -ve if an error                                    *
// **************************************************************************

int                 ShrinkBlockFile         (void)

	{
	// Local Variables.

	int                 error = -1;

	int                 i;
	uint8_t *           p;

	FILE *              pcl__LoadBlk;
	FILE *              pcl__SaveBlk;

	long                sl___Size;
	long                sl___Bloc;

	uint32_t *          pul__Temp;
	uint32_t *          pul__Indx;
	int                 si___Indx;

	uint8_t             aub__Data[12];

	// Initialize block size.

	pul__Indx = NULL;
	si___Indx = 0;

	// Open input file.

	pcl__LoadBlk = fopen(acz__FileInp, "rb");

	if (pcl__LoadBlk == NULL)
		{
		si___ErrorCode = ERROR_NO_FILE;
		sprintf(acz__ErrorMessage,
			"Swd32 - Unable to open input file %s !\n",
			acz__FileInp);
		goto errorExit;
		}

	sl___LoadTotal = GetFileLength(pcl__LoadBlk);

	// Construct SWD header.

	if (strlen(acz__FileExt) > 5)
		{
		printf("Swd32 - Warning - File's extension will be shortened to 4 letters !\n");
		}

	aub__Data[0x00] = 's';
	aub__Data[0x01] = 'W';
	aub__Data[0x02] = 'd';

	if (fl___SaveToGameboy)
		{
		aub__Data[0x03] = 0xC0u + 0x10u; sl___BlocLen = 2048; fl___GameboyFormat = TRUE;
		}
	else
		{
		aub__Data[0x03] = 0x80u + 0x30u; sl___BlocLen = 8192; fl___GameboyFormat = FALSE;
		}

	sl___Bloc = sl___BlocLen;

	aub__Data[0x04] =
	aub__Data[0x05] =
	aub__Data[0x06] =
	aub__Data[0x07] = 0;

	if (acz__FileExt[0x00] != 0)
		{
		if (acz__FileExt[0x01] != 0)
			{
			aub__Data[0x04] = acz__FileExt[0x01];

			if (acz__FileExt[0x02] != 0)
				{
				aub__Data[0x05] = acz__FileExt[0x02];

				if (acz__FileExt[0x03] != 0)
					{
					aub__Data[0x06] = acz__FileExt[0x03];

					if (acz__FileExt[0x04] != 0)
						{
						aub__Data[0x07] = acz__FileExt[0x04];
						}
					}
				}
			}
		}

	aub__Data[0x08] = (sl___LoadTotal >> 24) & 0xFFu;
	aub__Data[0x09] = (sl___LoadTotal >> 16) & 0xFFu;
	aub__Data[0x0A] = (sl___LoadTotal >>  8) & 0xFFu;
	aub__Data[0x0B] = (sl___LoadTotal >>  0) & 0xFFu;

	sl___Size = sl___LoadTotal;

	// Construct block table.

	if (sl___LoadTotal != 0)
		{
		si___Indx = ((sl___Size + sl___Bloc - 1) / sl___Bloc) + 1;
		pul__Indx = malloc(si___Indx * sizeof(uint32_t));

		if (pul__Indx == NULL)
			{
			si___ErrorCode = ERROR_NO_MEMORY;
			sprintf(acz__ErrorMessage,
				"Swd32 - Unable to allocate workspace !\n");
			goto errorExit;
			}
		}

	// Open output file.

	strcpy(acz__FileOut, acz__FileDrv);
	strcat(acz__FileOut, acz__FileDir);
	strcat(acz__FileOut, acz__FileNam);

	if (fl___SaveToSubDir)
		{
		strcat(acz__FileOut, acz__FileExt);
		}
	else
		{
		strcat(acz__FileOut, ".swd");
		}

	if (strcmp(acz__FileInp, acz__FileOut) == 0)
		{
		si___ErrorCode = ERROR_ILLEGAL;
		sprintf(acz__ErrorMessage,
			"Swd32 - Can't overwrite the input file !\n");
		goto errorExit;
		}

	pcl__SaveBlk = fopen(acz__FileOut, "wb");

	if (pcl__SaveBlk == NULL)
		{
		si___ErrorCode = ERROR_NO_FILE;
		sprintf(acz__ErrorMessage,
			"Swd32 - Unable to open output file %s !\n",
			acz__FileOut);
		goto errorExit;
		}

	// Save SWD header.

	if (fwrite(&aub__Data[0], 1, 12, pcl__SaveBlk) != 12)
		{
		si___ErrorCode = ERROR_IO_WRITE;
		sprintf(acz__ErrorMessage,
			"Swd32 - Unable to write SWD file header !\n");
		goto errorExit;
		}

	i = si___Indx * sizeof(uint32_t);

	if (fwrite(pul__Indx, 1, i, pcl__SaveBlk) != (size_t) i)
		{
		si___ErrorCode = ERROR_IO_WRITE;
		sprintf(acz__ErrorMessage,
			"Swd32 - Unable to write SWD block index table !\n");
		goto errorExit;
		}

	fflush(pcl__SaveBlk);

	sl___SaveCount = 12 + i;

	// Now save all the blocks in the file.

	pul__Temp = pul__Indx;

	while (sl___Size)
		{
		// Find out how much to load.

		if (sl___Size < sl___Bloc)
			{
			sl___Bloc = sl___Size;
			}

		sl___Size -= sl___Bloc;

		// Load up the block.

		if (fread(pub__LoadBuf, 1, sl___Bloc, pcl__LoadBlk) != (size_t) sl___Bloc)
			{
			si___ErrorCode = ERROR_IO_READ;
			sprintf(acz__ErrorMessage,
				"Swd32 - Unable to read from file %s !\n",
				acz__FileInp);
			goto errorExit;
			}

		// Compress the block.

		pub__LoadCur = pub__LoadBuf;
		pub__LoadEnd =
		pub__LoadMrk = pub__LoadBuf + sl___Bloc;

		pub__SaveCur = pub__SaveBuf;
		pub__SaveEnd = pub__SaveBuf + sl___SaveLen;

		psi__LzssCur = psi__LzssBuf;
		psi__LzssEnd = psi__LzssBuf + sl___LzssLen;

		pub__ByteCur = pub__ByteBuf;
		pub__ByteEnd = pub__ByteBuf + sl___ByteLen;

		BitIOInit();

		if (LzssShrinkByteFile() < 0) goto errorExit;

		BitIOFlush();

		// Pad out compressed data to a 4 byte boundary.

		while ((((uintptr_t) pub__SaveCur) & 3) != 0)
			{
			*pub__SaveCur++ = 0;
			}

		// Write out the compressed data.

		i = pub__SaveCur - pub__SaveBuf;

		if (i < sl___Bloc)
			{
			*pul__Temp++ = (sl___SaveCount << 4) | 0x01u;

			p = pub__SaveBuf;
			}
		else
			{
			*pul__Temp++ = (sl___SaveCount << 4) | 0x00u;

			p = pub__LoadBuf;
			i = sl___Bloc;
			}

		sl___SaveCount += i;

		if (fwrite(p, 1, i, pcl__SaveBlk) != (size_t) i)
			{
			si___ErrorCode = ERROR_IO_WRITE;
			sprintf(acz__ErrorMessage,
				"Swd32 - Unable to write SWD data !\n");
			goto errorExit;
			}

		// Loop around and compress the next block.
		}

	*pul__Temp = (sl___SaveCount << 4) | 0x00u;

	fflush(pcl__SaveBlk);

	// Convert the block table values from native to big-endian.

	sl___Size = si___Indx;
	pul__Temp = pul__Indx;

	p = (uint8_t *) pul__Indx;

	while (sl___Size--)
		{
		i = *pul__Temp++;

		*p++ = (i >> 24) & 0xFFu;
		*p++ = (i >> 16) & 0xFFu;
		*p++ = (i >>  8) & 0xFFu;
		*p++ = (i >>  0) & 0xFFu;
		}

	// Write out the block table (in big-endian format).

	fseek(pcl__SaveBlk, 12, SEEK_SET);

	i = si___Indx * sizeof(uint32_t);

	if (fwrite(pul__Indx, 1, i, pcl__SaveBlk) != (size_t) i)
		{
		si___ErrorCode = ERROR_IO_WRITE;
		sprintf(acz__ErrorMessage,
			"Swd32 - Unable to write SWD block index table !\n");
		goto errorExit;
		}

	// Finish it off.

	error = 0;

	// All done, return error code.

	errorExit:

	free(pul__Indx);

	if (pcl__LoadBlk) fclose(pcl__LoadBlk);
	if (pcl__SaveBlk) fclose(pcl__SaveBlk);

	pcl__LoadBlk = NULL;
	pcl__SaveBlk = NULL;

	#if 0
		remove(acz__FileOut);
	#endif

	return (error);
	}



// **************************************************************************
// * ExpandBlockFile ()                                                     *
// **************************************************************************
// *                                                                        *
// **************************************************************************
// * Inputs  -                                                              *
// *                                                                        *
// * Output  int         -ve if an error                                    *
// **************************************************************************

int                 ExpandBlockFile         (void)

	{
	// Local Variables.

	int                 error = -1;

	int                 f;
	int                 i;
	int                 j;

	FILE *              pcl__LoadBlk;
	FILE *              pcl__SaveBlk;

	long                sl___Size;
	long                sl___Bloc;

	uint32_t *          pul__Temp;
	uint32_t *          pul__Indx;
	uint8_t *           pub__Temp;
	uint8_t *           pub__Indx;
	int                 si___Indx;

	uint8_t             aub__Data[12];
	char                acz__Extn[8];

	// Initialize block size.

	pul__Indx = NULL;
	pub__Indx = NULL;
	si___Indx = 0;

	sl___LoadCount = 0;
	sl___SaveCount = 0;

	// Open input file.

	pcl__LoadBlk = fopen(acz__FileInp, "rb");

	if (pcl__LoadBlk == NULL)
		{
		si___ErrorCode = ERROR_NO_FILE;
		sprintf(acz__ErrorMessage,
			"Swd32 - Unable to open input file %s !\n",
			acz__FileInp);
		goto errorExit;
		}

	sl___LoadTotal = GetFileLength(pcl__LoadBlk);

	// Load SWD header.

	if (fread(&aub__Data[0], 1, 12, pcl__LoadBlk) != 12)
		{
		si___ErrorCode = ERROR_NO_FILE;
		sprintf(acz__ErrorMessage,
			"Swd32 - Unable to read from file %s !\n",
			acz__FileInp);
		goto errorExit;
		}

	if ((aub__Data[0] != 's') ||
	    (aub__Data[1] != 'W') ||
	    (aub__Data[2] != 'd') ||
	    (aub__Data[3] < 0x80u))
		{
		si___ErrorCode = ERROR_NO_FILE;
		sprintf(acz__ErrorMessage,
			"Swd32 - Input file %s is not in SWD format !\n",
			acz__FileInp);
		goto errorExit;
		}

	fl___GameboyFormat = FALSE;

	if ((aub__Data[3] & 0x80) == 0)
		{
		si___ErrorCode = ERROR_NO_FILE;
		sprintf(acz__ErrorMessage,
			"Swd32 - Unknown SWD format !\n",
			acz__FileInp);
		goto errorExit;
		}

	if ((aub__Data[3] & 0x40) != 0)
		{
		i &= ~0x40; fl___GameboyFormat = TRUE;
		}

	sl___BlocLen = 1024 << ((aub__Data[3] & 0x30) >> 4);
	sl___Bloc    = sl___BlocLen;

	if (aub__Data[4] == 0)
		{
		acz__Extn[0x00] = 0;
		}
	else
		{
		acz__Extn[0x00] = '.';
		acz__Extn[0x01] = aub__Data[4];
		acz__Extn[0x02] = aub__Data[5];
		acz__Extn[0x03] = aub__Data[6];
		acz__Extn[0x04] = aub__Data[7];
		acz__Extn[0x05] = 0;
		}

	i = (aub__Data[ 8] << 24) +
	    (aub__Data[ 9] << 16) +
	    (aub__Data[10] <<  8) +
	    (aub__Data[11] <<  0);

	sl___Size = sl___LoadTotal = i;

	sl___LoadCount = 12;

	// Load SWD block table.

	if (sl___LoadTotal != 0)
		{
		// Load the block table (in big-endian format).

		si___Indx = ((sl___Size + sl___Bloc - 1) / sl___Bloc) + 1;
		pul__Indx = malloc(si___Indx * sizeof(uint32_t));
		pub__Indx = malloc(si___Indx * sizeof(uint32_t));

		if ((pul__Indx == NULL) || (pub__Indx == NULL))
			{
			si___ErrorCode = ERROR_NO_MEMORY;
			sprintf(acz__ErrorMessage,
				"Swd32 - Unable to allocate workspace !\n");
			goto errorExit;
			}

		if (fread(pub__Indx, sizeof(uint32_t), si___Indx, pcl__LoadBlk) != (size_t) si___Indx)
			{
			si___ErrorCode = ERROR_NO_FILE;
			sprintf(acz__ErrorMessage,
				"Swd32 - Unable to read from file %s !\n",
				acz__FileInp);
			goto errorExit;
			}

		sl___LoadCount += si___Indx * sizeof(uint32_t);

		// Convert the block table values from big-endian to native.

		pul__Temp = pul__Indx;
		pub__Temp = pub__Indx;

		j = si___Indx;

		while (j--)
			{
			i = (pub__Temp[0] << 24) |
			    (pub__Temp[1] << 16) |
			    (pub__Temp[2] <<  8) |
			    (pub__Temp[3] <<  0);

			pub__Temp += 4;

			*pul__Temp++ = i;
			}
		}

	// Open output file.

	strcpy(acz__FileExt, acz__Extn);

	strcpy(acz__FileOut, acz__FileDrv);
	strcat(acz__FileOut, acz__FileDir);
	strcat(acz__FileOut, acz__FileNam);
	strcat(acz__FileOut, acz__FileExt);

	if (strcmp(acz__FileInp, acz__FileOut) == 0)
		{
		si___ErrorCode = ERROR_ILLEGAL;
		sprintf(acz__ErrorMessage,
			"Swd32 - Can't overwrite the input file !\n");
		goto errorExit;
		}

	pcl__SaveBlk = fopen(acz__FileOut, "wb");

	if (pcl__SaveBlk == NULL)
		{
		si___ErrorCode = ERROR_NO_FILE;
		sprintf(acz__ErrorMessage,
			"Swd32 - Unable to open output file %s !\n",
			acz__FileOut);
		goto errorExit;
		}

	// Now load all the blocks in the file.

	pul__Temp = pul__Indx;

	while (sl___Size)
		{
		// Find out how much to load.

		if (sl___Size < sl___Bloc)
			{
			sl___Bloc = sl___Size;
			}

		sl___Size -= sl___Bloc;

		// Find out how much to load and where it is.

		f = (pul__Temp[0] & 15);

		i = (pul__Temp[0] >> 4);
		j = (pul__Temp[1] >> 4) - i;

		pul__Temp += 1;

		// Load up the block.

		fseek(pcl__LoadBlk, i, SEEK_SET);

		if (fread(pub__LoadBuf, 1, j, pcl__LoadBlk) != (size_t) j)
			{
			si___ErrorCode = ERROR_IO_READ;
			sprintf(acz__ErrorMessage,
				"Swd32 - Unable to read from file %s !\n",
				acz__FileInp);
			goto errorExit;
			}

		// Is the block compressed ?

		pub__Temp = pub__LoadBuf;

		if (f != 0)
			{
			// Decompress the block.

			pub__LoadCur = pub__LoadBuf;
			pub__LoadEnd =
			pub__LoadMrk = pub__LoadBuf + j;

			pub__SaveCur = pub__SaveBuf;
			pub__SaveEnd = pub__SaveBuf + sl___SaveLen;

			psi__LzssCur = psi__LzssBuf;
			psi__LzssEnd = psi__LzssBuf;

			BitIOInit();

			if (LzssExpandByteFile() < 0) goto errorExit;

			BitIOFlush();

			if ((pub__SaveCur - pub__SaveBuf) != sl___Bloc)
				{
				si___ErrorCode = ERROR_ILLEGAL;
				sprintf(acz__ErrorMessage,
					"Swd32 - Compressed file \"%s\" contains invalid data !\n",
					acz__FileInp);
				goto errorExit;
				}

			pub__Temp = pub__SaveBuf;
			}

		// Write out the uncompressed data.

		if (fwrite(pub__Temp, 1, sl___Bloc, pcl__SaveBlk) != (size_t) sl___Bloc)
			{
			si___ErrorCode = ERROR_IO_WRITE;
			sprintf(acz__ErrorMessage,
				"Swd32 - Unable to write SWD data !\n");
			goto errorExit;
			}

		// Loop around and decompress the next block.
		}

	// Finish it off.

	error = 0;

	// All done, return error code.

	errorExit:

	free(pul__Indx);
	free(pub__Indx);

	if (pcl__LoadBlk) fclose(pcl__LoadBlk);
	if (pcl__SaveBlk) fclose(pcl__SaveBlk);

	pcl__LoadBlk = NULL;
	pcl__SaveBlk = NULL;

	return (error);
	}



// **************************************************************************
// **************************************************************************
// **************************************************************************
// **************************************************************************



// **************************************************************************
// * ShrinkWholeFile ()                                                     *
// **************************************************************************
// *                                                                        *
// **************************************************************************
// * Inputs  -                                                              *
// *                                                                        *
// * Output  int         -ve if an error                                    *
// **************************************************************************

int                 ShrinkWholeFile         (void)

	{
	// Local Variables.

	int                 error = -1;

	uint8_t             aub__Data[12];

	// Initialize the buffer pointers.

	pub__LoadCur = pub__LoadBuf;
	pub__LoadEnd = pub__LoadBuf;
	pub__LoadMrk = pub__LoadBuf;

	pub__SaveCur = pub__SaveBuf;
	pub__SaveEnd = pub__SaveBuf + sl___SaveLen;

	psi__LzssCur = psi__LzssBuf;
	psi__LzssEnd = psi__LzssBuf + sl___LzssLen;

	pub__ByteCur = pub__ByteBuf;
	pub__ByteEnd = pub__ByteBuf + sl___ByteLen;

	sl___LoadCount = 0;
	sl___SaveCount = 0;

	// Open input file.

	pcl__LoadFil = fopen(acz__FileInp, "rb");

	if (pcl__LoadFil == NULL)
		{
		si___ErrorCode = ERROR_NO_FILE;
		sprintf(acz__ErrorMessage,
			"Swd32 - Unable to open input file %s !\n",
			acz__FileInp);
		goto errorExit;
		}

	sl___LoadTotal = GetFileLength(pcl__LoadFil);

	// Construct SWD header.

	if (strlen(acz__FileExt) > 5)
		{
		printf("Swd32 - Warning - File's extension will be shortened to 4 letters !\n");
		}

	aub__Data[0x00] = 's';
	aub__Data[0x01] = 'W';
	aub__Data[0x02] = 'd';

	if (fl___SaveToGameboy)
		{
		aub__Data[0x03] = 0xC0u; fl___GameboyFormat = TRUE;
		}
	else
		{
		aub__Data[0x03] = 0x80u; fl___GameboyFormat = FALSE;
		}

	aub__Data[0x04] =
	aub__Data[0x05] =
	aub__Data[0x06] =
	aub__Data[0x07] = 0;

	if (acz__FileExt[0x00] != 0)
		{
		if (acz__FileExt[0x01] != 0)
			{
			aub__Data[0x04] = acz__FileExt[0x01];

			if (acz__FileExt[0x02] != 0)
				{
				aub__Data[0x05] = acz__FileExt[0x02];

				if (acz__FileExt[0x03] != 0)
					{
					aub__Data[0x06] = acz__FileExt[0x03];

					if (acz__FileExt[0x04] != 0)
						{
						aub__Data[0x07] = acz__FileExt[0x04];
						}
					}
				}
			}
		}

	aub__Data[0x08] = (sl___LoadTotal >> 24) & 0xFFu;
	aub__Data[0x09] = (sl___LoadTotal >> 16) & 0xFFu;
	aub__Data[0x0A] = (sl___LoadTotal >>  8) & 0xFFu;
	aub__Data[0x0B] = (sl___LoadTotal >>  0) & 0xFFu;

	// Open output file.

	strcpy(acz__FileOut, acz__FileDrv);
	strcat(acz__FileOut, acz__FileDir);
	strcat(acz__FileOut, acz__FileNam);

	if (fl___SaveToSubDir)
		{
		strcat(acz__FileOut, acz__FileExt);
		}
	else
		{
		strcat(acz__FileOut, ".swd");
		}

	if (strcmp(acz__FileInp, acz__FileOut) == 0)
		{
		si___ErrorCode = ERROR_ILLEGAL;
		sprintf(acz__ErrorMessage,
			"Swd32 - Can't overwrite the input file !\n");
		goto errorExit;
		}

	pcl__SaveFil = fopen(acz__FileOut, "wb");

	if (pcl__SaveFil == NULL)
		{
		si___ErrorCode = ERROR_NO_FILE;
		sprintf(acz__ErrorMessage,
			"Swd32 - Unable to open output file %s !\n",
			acz__FileOut);
		goto errorExit;
		}

	// Save SWD header.

	if (fwrite(&aub__Data[0], 1, 12, pcl__SaveFil) != 12)
		{
		si___ErrorCode = ERROR_IO_WRITE;
		sprintf(acz__ErrorMessage,
			"Swd32 - Unable to write SWD file header !\n");
		goto errorExit;
		}

	// Compress file.

	BitIOInit();

	if (LzssShrinkByteFile() < 0) goto errorExit;

	BitIOFlush();

	// Finish it off.

	error = 0;

	// All done, return error code.

	errorExit:

	if (pcl__LoadFil) fclose(pcl__LoadFil);
	if (pcl__SaveFil) fclose(pcl__SaveFil);

	pcl__LoadFil = NULL;
	pcl__SaveFil = NULL;

	#if 0
		remove(acz__FileOut);
	#endif

	return (error);
	}



// **************************************************************************
// * ExpandWholeFile ()                                                     *
// **************************************************************************
// *                                                                        *
// **************************************************************************
// * Inputs  -                                                              *
// *                                                                        *
// * Output  int         -ve if an error                                    *
// **************************************************************************

int                 ExpandWholeFile         (void)

	{
	// Local Variables.

	int                 i;

	int                 error = -1;

	uint8_t             aub__Data[12];
	char                acz__Extn[8];

	// Initialize the buffer pointers.

	pub__LoadCur = pub__LoadBuf;
	pub__LoadEnd = pub__LoadBuf;
	pub__LoadMrk = pub__LoadBuf;

	pub__SaveCur = pub__SaveBuf;
	pub__SaveEnd = pub__SaveBuf + sl___SaveLen;

	psi__LzssCur = psi__LzssBuf;
	psi__LzssEnd = psi__LzssBuf;

	pub__ByteCur = pub__ByteBuf;
	pub__ByteEnd = pub__ByteBuf + sl___ByteLen;

	sl___LoadCount = 0;
	sl___SaveCount = 0;

	// Open input file.

	pcl__LoadFil = fopen(acz__FileInp, "rb");

	if (pcl__LoadFil == NULL)
		{
		si___ErrorCode = ERROR_NO_FILE;
		sprintf(acz__ErrorMessage,
			"Swd32 - Unable to open input file %s !\n",
			acz__FileInp);
		goto errorExit;
		}

	sl___LoadTotal = GetFileLength(pcl__LoadFil);

	// Load SWD header.

	if (fread(&aub__Data[0], 1, 12, pcl__LoadFil) != 12)
		{
		si___ErrorCode = ERROR_NO_FILE;
		sprintf(acz__ErrorMessage,
			"Swd32 - Unable to read from file %s !\n",
			acz__FileInp);
		goto errorExit;
		}

	if ((aub__Data[0] != 's') ||
	    (aub__Data[1] != 'W') ||
	    (aub__Data[2] != 'd') ||
	    (aub__Data[3] < 0x80u))
		{
		si___ErrorCode = ERROR_NO_FILE;
		sprintf(acz__ErrorMessage,
			"Swd32 - Input file %s is not in SWD format !\n",
			acz__FileInp);
		goto errorExit;
		}

	fl___GameboyFormat = FALSE;

	if ((i = aub__Data[3]) & 0x40)
		{
		i &= ~0x40; fl___GameboyFormat = TRUE;
		}

	if (i != (0x80u + 0x00u))
		{
		si___ErrorCode = ERROR_ILLEGAL;
		sprintf(acz__ErrorMessage,
			"Swd32 - Unknown SWD format !\n",
			acz__FileInp);
		goto errorExit;
		}

	if (aub__Data[4] == 0)
		{
		acz__Extn[0x00] = 0;
		}
	else
		{
		acz__Extn[0x00] = '.';
		acz__Extn[0x01] = aub__Data[4];
		acz__Extn[0x02] = aub__Data[5];
		acz__Extn[0x03] = aub__Data[6];
		acz__Extn[0x04] = aub__Data[7];
		acz__Extn[0x05] = 0;
		}

	i = (aub__Data[ 8] << 24) +
	    (aub__Data[ 9] << 16) +
	    (aub__Data[10] <<  8) +
	    (aub__Data[11] <<  0);

	// Open output file.

	strcpy(acz__FileExt, acz__Extn);

	strcpy(acz__FileOut, acz__FileDrv);
	strcat(acz__FileOut, acz__FileDir);
	strcat(acz__FileOut, acz__FileNam);
	strcat(acz__FileOut, acz__FileExt);

	if (strcmp(acz__FileInp, acz__FileOut) == 0)
		{
		si___ErrorCode = ERROR_ILLEGAL;
		sprintf(acz__ErrorMessage,
			"Swd32 - Can't overwrite the input file !\n");
		goto errorExit;
		}

	pcl__SaveFil = fopen(acz__FileOut, "wb");

	if (pcl__SaveFil == NULL)
		{
		si___ErrorCode = ERROR_NO_FILE;
		sprintf(acz__ErrorMessage,
			"Swd32 - Unable to open output file %s !\n",
			acz__FileOut);
		goto errorExit;
		}

	// Decompress file.

	BitIOInit();

	if (LzssExpandByteFile() < 0) goto errorExit;

	BitIOFlush();

	// Finish it off.

	error = 0;

	// All done, return error code.

	errorExit:

	if (pcl__LoadFil) fclose(pcl__LoadFil);
	if (pcl__SaveFil) fclose(pcl__SaveFil);

	pcl__LoadFil = NULL;
	pcl__SaveFil = NULL;

	return (error);
	}



// **************************************************************************
// * RecvByteValue ()                                                       *
// **************************************************************************
// *                                                                        *
// **************************************************************************
// * Inputs  -                                                              *
// *                                                                        *
// * Output  int         -ve if an error                                    *
// **************************************************************************

int                 RecvByteValue           (void)
	{
	// Input buffer empty ?

	if (pub__LoadCur == pub__LoadEnd)
		{
		// Preload size set to zero ?

		if (sl___LoadLen == 0)
			{
			return (-1);
			}

		// Is there an open file ?

		if (pcl__LoadFil == NULL)
			{
			return (-1);
			}

		// Read data into the load buffer.

		pub__LoadCur = pub__LoadBuf;

		pub__LoadEnd = pub__LoadBuf
			+ fread(pub__LoadBuf, 1, sl___LoadLen, pcl__LoadFil);

		// End-of-file ?

		if (pub__LoadCur == pub__LoadEnd)
			{
			fclose(pcl__LoadFil);

			pcl__LoadFil = NULL;

			return (-1);
			}
		}

	// Read value from input buffer.

	return (*pub__LoadCur++);
	}



// **************************************************************************
// * FillRecvBuffer ()                                                      *
// **************************************************************************
// *                                                                        *
// **************************************************************************
// * Inputs  -                                                              *
// *                                                                        *
// * Output  int         -ve if an error                                    *
// **************************************************************************

int                 FillRecvBuffer          (void)
	{
	// Preload size set to zero ?

	if (sl___LoadLen == 0)
		{
		return (-1);
		}

	// Is there an open file ?

	if (pcl__LoadFil == NULL)
		{
		return (-1);
		}

	// Read data into the load buffer.

	pub__LoadCur = pub__LoadBuf;

	pub__LoadEnd = pub__LoadBuf
		+ fread(pub__LoadBuf, 1, sl___LoadLen, pcl__LoadFil);

	// End-of-file ?

	if (pub__LoadCur == pub__LoadEnd)
		{
		fclose(pcl__LoadFil);

		pcl__LoadFil = NULL;

		return (-1);
		}

	// Return OK.

	return (0);
	}



// **************************************************************************
// * SendByteValue ()                                                       *
// **************************************************************************
// *                                                                        *
// **************************************************************************
// * Inputs  int         value to send                                      *
// *                                                                        *
// * Output  int         -ve if an error                                    *
// **************************************************************************

int                 SendByteValue           (
								int                 value)
	{
	// Local Variables.

	long                size;

	// Output buffer full ?

	if (pub__SaveCur == pub__SaveEnd)
		{
		// Is there an open file ?

		if (pcl__SaveFil == NULL)
			{
			return (-1);
			}

		// Write the data from the save buffer to the file.

		size = pub__SaveEnd - pub__SaveBuf;

		if (fwrite(pub__SaveBuf, 1, size, pcl__SaveFil) != (size_t) size)
			{
			fclose(pcl__SaveFil);

			pcl__SaveFil = NULL;

			return (-1);
			}

		pub__SaveCur = pub__SaveBuf;
		}

	// Save value into output buffer.

	*pub__SaveCur++ = (uint8_t) value;

	return (0);
	}



// **************************************************************************
// * SendByteEOF ()                                                         *
// **************************************************************************
// *                                                                        *
// **************************************************************************
// * Inputs  -                                                              *
// *                                                                        *
// * Output  int         -ve if an error                                    *
// **************************************************************************

int                 SendByteEOF             (void)

	{
	// Local Variables.

	long                size;

	// Output buffer empty ?

	if (pub__SaveCur != pub__SaveBuf)
		{
		// Is there an open file ?

		if (pcl__SaveFil != NULL)
			{
			// Write the data from the save buffer to the file.

			size = pub__SaveCur - pub__SaveBuf;

			if (fwrite(pub__SaveBuf, 1, size, pcl__SaveFil) != (size_t) size)
				{
				fclose(pcl__SaveFil);

				pcl__SaveFil = NULL;

				return (-1);
				}

			pub__SaveCur = pub__SaveBuf;
			}
		}

	// All done.

	return (0);
	}



// **************************************************************************
// * RecvLzssToken ()                                                       *
// **************************************************************************
// *                                                                        *
// **************************************************************************
// * Inputs  int *       token length                                       *
// *         int *       token offset (or character if length==0)           *
// *                                                                        *
// * Output  int         -ve if an error                                    *
// **************************************************************************

int                 RecvLzssToken           (
								int *               pmatch_length,
								int *               pmatch_offset)
	{
	// LZSS buffer empty ?

	if (psi__LzssCur == psi__LzssEnd)
		{
		// Decompress LZSS tokens from the input buffer.

		if (LoadBitsToLzss() < 0) return (-1);
		}

	// Read value from input buffer.

	*pmatch_length = *psi__LzssCur++;
	*pmatch_offset = *psi__LzssCur++;

	return (*pmatch_length);
	}



// **************************************************************************
// * SendLzssToken ()                                                       *
// **************************************************************************
// *                                                                        *
// **************************************************************************
// * Inputs  int         token length                                       *
// *         int         token offset (or character if length==0)           *
// *                                                                        *
// * Output  int         -ve if an error                                    *
// **************************************************************************

int                 SendLzssToken           (
								int                 match_length,
								int                 match_offset)
	{
	// LZSS buffer full ?

	fl___LzssEnd = FALSE;

	if (psi__LzssCur == psi__LzssEnd)
		{
		// Compress LZSS tokens into the output buffer.

		if (SaveLzssToBits() < 0) return (-1);
		}

	// Save value into LZSS buffer.

	*psi__LzssCur++ = match_length;
	*psi__LzssCur++ = match_offset;

	if (match_length == 1)
		{
		*pub__ByteCur++ = (uint8_t) match_offset;
		}

	return (0);
	}



// **************************************************************************
// * SendLzssEOF ()                                                         *
// **************************************************************************
// *                                                                        *
// **************************************************************************
// * Inputs  -                                                              *
// *                                                                        *
// * Output  int         -ve if an error                                    *
// **************************************************************************

int                 SendLzssEOF             (void)

	{
	// Local Variables.

	long                size;

	// Send EOF token.

	SendLzssToken(0, 0);

	// Data in LZSS buffer ?

	fl___LzssEnd = TRUE;

	while (psi__LzssCur != psi__LzssBuf)
		{
		// Compress LZSS tokens into the output buffer.

		if (SaveLzssToBits() < 0) return (-1);
		}

	// Flush out the last few bits.

	BitIOFlush();

	// Output buffer empty ?

	if (pub__SaveCur != pub__SaveBuf)
		{
		// Is there an open file ?

		if (pcl__SaveFil != NULL)
			{
			// Write the data from the save buffer to the file.

			size = pub__SaveCur - pub__SaveBuf;

			sl___SaveCount += size;

			if (fwrite(pub__SaveBuf, 1, size, pcl__SaveFil) != (size_t) size)
				{
				fclose(pcl__SaveFil);

				pcl__SaveFil = NULL;

				return (-1);
				}

			pub__SaveCur = pub__SaveBuf;
			}
		}

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
// * SaveLzssToBits ()                                                      *
// **************************************************************************
// *                                                                        *
// **************************************************************************
// * Inputs  -                                                              *
// *                                                                        *
// * Output  long        Bytes read, or -ve if an error                     *
// **************************************************************************

int                 SaveLzssToBits          (void)

	{
	// Local Variables.

	int *               psi__LzssTmp;
	uint8_t *           pub__ByteTmp;

	int                 match_length;
	int                 match_offset;

	long                size;

	// Ensure that the byte list contains each possible terminator.

	pub__ByteCur[0] = 0x00u;
	pub__ByteCur[1] = 0x40u;
	pub__ByteCur[2] = 0x80u;
	pub__ByteCur[3] = 0xC0u;

	// Initialize pointers.

	psi__LzssTmp = psi__LzssBuf;

	pub__ByteTmp = pub__ByteBuf;

	// Write out each command.

	while (psi__LzssTmp != psi__LzssCur)

		{
		// Get the length & offset pair.

		match_length = *psi__LzssTmp++;
		match_offset = *psi__LzssTmp++;

		if (match_length == 1)
			{
			pub__ByteTmp += 1;
			}

		// Write the pair to the output buffer.

		TokenToBits(match_length, match_offset);

		// Output buffer full ?

		if (((pub__BitsCur == NULL) && (pub__SaveCur >= pub__SaveEnd)) ||
			((pub__BitsCur != NULL) && (pub__BitsCur >= pub__SaveEnd)))
			{
			// Is there an open file ?

			if (pcl__SaveFil == NULL)
				{
				return (-1);
				}

			// Write the data from the save buffer to the file.

			size = pub__SaveEnd - pub__SaveBuf;

			sl___SaveCount += size;

			if (fwrite(pub__SaveBuf, 1, size, pcl__SaveFil) != (size_t) size)
				{
				fclose(pcl__SaveFil);

				pcl__SaveFil = NULL;

				return (-1);
				}

			// Wrap the excess stuff back to the start of the buffer.

			size = pub__SaveCur - pub__SaveEnd;

			pub__SaveCur = pub__SaveBuf + size;

			memcpy(pub__SaveBuf, pub__SaveEnd, size);

			if (pub__BitsCur != NULL)
				{
				size = pub__BitsCur - pub__SaveEnd;

				pub__BitsCur = pub__SaveBuf + size;
				}
			}

		// Ensure that we keep a few bytes left in the output queue.

		if (!fl___LzssEnd)
			{
			if ((pub__ByteCur - pub__ByteTmp) <= 16) break;
			}
		}

	// Wrap the excess stuff back to the start of the buffer.

	size = psi__LzssCur - psi__LzssTmp;

	psi__LzssCur = psi__LzssBuf + size;

	memcpy(psi__LzssBuf, psi__LzssTmp, size * sizeof(int));

	size = pub__ByteCur - pub__ByteTmp;

	pub__ByteCur = pub__ByteBuf + size;

	memcpy(pub__ByteBuf, pub__ByteTmp, size);

	// All done.

	return (0);
	}



// **************************************************************************
// * LoadBitsToLzss ()                                                      *
// **************************************************************************
// *                                                                        *
// **************************************************************************
// * Inputs  -                                                              *
// *                                                                        *
// * Output  long        Bytes read, or -ve if an error                     *
// **************************************************************************

int                 LoadBitsToLzss          (void)

	{
	// Local Variables.

	int *               psi__LzssTmp;

	int                 match_length;
	int                 match_offset;

	long                size;

	// Initialize pointers.

	psi__LzssCur =
	psi__LzssEnd = psi__LzssBuf;

	// Loop around until the LZSS buffer is full.

	psi__LzssTmp = psi__LzssBuf + sl___LzssLen;

	while (psi__LzssEnd < psi__LzssTmp)
		{
		// Input buffer empty ?

		if (pub__LoadCur >= pub__LoadMrk)
			{
			// Wrap the excess stuff back to the start of the buffer.

			size = pub__LoadEnd - pub__LoadCur;

			memcpy(pub__LoadBuf, pub__LoadCur, size);

			pub__LoadCur = pub__LoadBuf;
			pub__LoadEnd = pub__LoadBuf + size;

			pub__LoadMrk = pub__LoadEnd;

			// Preload size set to zero ?

			if (sl___LoadLen == 0)
				{
				return (-1);
				}

			// Is there an open file ?

			if (pcl__LoadFil == NULL)
				{
				return (-1);
				}

			// Read data into the load buffer.

			size = sl___LoadLen - size;

			pub__LoadEnd = pub__LoadEnd
				+ fread(pub__LoadEnd, 1, size, pcl__LoadFil);

			pub__LoadMrk = pub__LoadEnd;

			if ((pub__LoadEnd - pub__LoadCur) == sl___LoadLen)
				{
				pub__LoadMrk -= 8;
				}

			// End-of-file ?

			if (pub__LoadCur == pub__LoadEnd)
				{
				fclose(pcl__LoadFil);

				pcl__LoadFil = NULL;

				return (-1);
				}
			}

		// Read the pair from the input buffer.

		BitsToToken(&match_length, &match_offset);

		// Write the pair to the LZSS buffer.

//		if ((*psi__LzssEnd++ != match_length) || (*psi__LzssEnd++ != match_offset))
//			{
//			printf("error\n");
//			}

		*psi__LzssEnd++ = match_length;
		*psi__LzssEnd++ = match_offset;

		// Check for EOF marker.

		if (match_length == 0) break;
		}

	// All done.

	return (0);
	}



// **************************************************************************
// * TokenToBits ()                                                         *
// **************************************************************************
// *                                                                        *
// **************************************************************************
// * Inputs  -                                                              *
// *                                                                        *
// * Output  -                                                              *
// **************************************************************************

static	void                TokenToBits             (
								int                 match_length,
								int                 match_offset)

	{
	// Check for valid token values.

	if ((match_length > 256) || (match_offset > 0x06A0))
		{
		printf("Swd32 - Illegal token output values !\n");
		exit(-1);
		}

	// Save length.
	//
	// EOF marker ?

	if (match_length == 0)
		{
		BitIOSend(8, 0x00C0u);
		*pub__SaveCur++ = 0;

		return;
		}

	// 1 byte.

	else

	if (match_length == 1)
		{
		BitIOSend(1, 0x0000u);

		*pub__SaveCur++ = (uint8_t) match_offset;

		return;
		}

	// 2 bytes.

	else

	if (match_length == 2)
		{
		BitIOSend(2, 0x0002u);
		}

	// 3-5 bytes.

	else

	if (match_length <= 5)
		{
		BitIOSend(4, 0x000Cu + match_length - 2);
		}

	// 6-20 bytes.

	else

	if (match_length <= 20)
		{
		BitIOSend(8, 0x00C0u + match_length - 5);
		}

	// 21-275 bytes.

	else

	if (match_length <= 275)
		{
		BitIOSend(8, 0x00C0u);
		*pub__SaveCur++ = match_length - 20;
		}

	// Save offset.
	//
	// <= 0x0020 bytes.

	if (match_offset <= 0x0020)
		{
		BitIOSend(2, 0);
		BitIOSend(5, match_offset - 0x0001);
		}

	// <= 0x00A0 bytes.

	else

	if (match_offset <= 0x00A0)
		{
		BitIOSend(2, 1);
		BitIOSend(7, match_offset - 0x0021);
		}

	// <= 0x02A0 bytes.

	else

	if (match_offset <= 0x02A0)
		{
		BitIOSend(2, 2);

		match_offset = match_offset - 0x00A1;

		if (fl___GameboyFormat)
			{
			BitIOSend((9-8), (match_offset >> 8));

			*pub__SaveCur++ = (uint8_t) (match_offset & 255);
			}
		else
			{
			*pub__SaveCur++ = (uint8_t) (match_offset >> (9-8));

			BitIOSend((9-8), match_offset);
			}
		}

	// <= 0x06A0 bytes.

	else

	if (match_offset <= 0x06A0)
		{
		BitIOSend(2, 3);

		match_offset = match_offset - 0x02A1;

		if (fl___GameboyFormat)
			{
			BitIOSend((10-8), (match_offset >> 8));

			*pub__SaveCur++ = (uint8_t) (match_offset & 255);
			}
		else
			{
			*pub__SaveCur++ = (uint8_t) (match_offset >> (10-8));

			BitIOSend((10-8), match_offset);
			}
		}

	// All done.

	return;
	}



// **************************************************************************
// * BitsToToken ()                                                         *
// **************************************************************************
// *                                                                        *
// **************************************************************************
// * Inputs  -                                                              *
// *                                                                        *
// * Output  -                                                              *
// **************************************************************************

static	void                BitsToToken             (
								int *               pmatch_length,
								int *               pmatch_offset)

	{
	// Local Variables.

	unsigned            i;

	// Simple byte ?

	if (BitIORecv(1) == 0)
		{
		*pmatch_length = 1;
		*pmatch_offset = *pub__LoadCur++;

		return;
		}

	// Repeat count.

	if (BitIORecv(1) == 0)
		{
		// 2 bytes.

		*pmatch_length = 2;
		}
	else
		{
		i = BitIORecv(2);

		if (i != 0)
			{
			// 3-5 bytes.

			*pmatch_length = i + 2;
			}
		else
			{
			i = BitIORecv(4);

			if (i != 0)
				{
				// 6-20 bytes.

				*pmatch_length = i + 5;
				}
			else
				{
				// 21-275 bytes.

				i = *pub__LoadCur++;

				if (i != 0)
					{
					*pmatch_length = i + 20;
					}
				else
					{
					*pmatch_length = 0;
					*pmatch_offset = 0;
					return;
					}
				}
			}
		}

	// Repeat offset.

	i = BitIORecv(2);

	switch (i)
		{
		case 0:
			*pmatch_offset = BitIORecv(5) + 0x0001;
			break;
		case 1:
			*pmatch_offset = BitIORecv(7) + 0x0021;
			break;
		case 2:
			if (fl___GameboyFormat)
				{
				i = BitIORecv(9-8) << 8;
				i = i + *pub__LoadCur++ + 0x00A1;
				}
			else
				{
				i = *pub__LoadCur++;
				i = (i << (9-8)) + BitIORecv(9-8) + 0x00A1;
				}
			*pmatch_offset = i;
			break;
		case 3:
			if (fl___GameboyFormat)
				{
				i = BitIORecv(10-8) << 8;
				i = i + *pub__LoadCur++ + 0x02A1;
				}
			else
				{
				i = *pub__LoadCur++;
				i = (i << (10-8)) + BitIORecv(10-8) + 0x02A1;
				}
			*pmatch_offset = i;
			break;
		}

	// All done.

	return;
	}



// **************************************************************************
// * BitIOInit ()                                                           *
// **************************************************************************
// * Initialize bit-oriented buffered output                                *
// **************************************************************************
// * Inputs  -                                                              *
// *                                                                        *
// * Output  -                                                              *
// **************************************************************************

static	void                BitIOInit               (void)

	{
	// Initialize the vars.

	ui___BitsInp = 1;
	pub__BitsCur = NULL;

	// Return with success.

	return;
	}



// **************************************************************************
// * BitIOSend ()                                                           *
// **************************************************************************
// * Send a value to bit-oriented buffered output                           *
// **************************************************************************
// * Inputs  unsigned   # of bits to send                                   *
// *         unsigned   Value of bits to send                               *
// *                                                                        *
// * Output  -                                                              *
// **************************************************************************

static	void                BitIOSend               (
								unsigned            ui___bitcount,
								unsigned            ui___bitvalue)

	{
	// Local Variables.

	unsigned            ui___bitmask;

	//

	if (ui___bitcount != 0)
		{
		ui___bitmask = 1 << (ui___bitcount - 1);

		while (ui___bitmask != 0)
			{
			if (pub__BitsCur == NULL)
				{
				ub___BitsOut = 0;
				ub___BitsMsk = 1;
				pub__BitsCur = pub__SaveCur++;
				}

			if (ui___bitmask & ui___bitvalue)
				{
				ub___BitsOut |= ub___BitsMsk;
				}

			ub___BitsMsk <<= 1;

			if (ub___BitsMsk == 0)
				{
				*pub__BitsCur = ub___BitsOut;
				pub__BitsCur  = NULL;
				}

			ui___bitmask >>= 1;
			}
		}

	// Return with success.

	return;
	}



// **************************************************************************
// * BitIORecv ()                                                           *
// **************************************************************************
// * Read a value from the bit-oriented buffered input                      *
// **************************************************************************
// * Inputs  unsigned   # of bits to recv                                   *
// *                                                                        *
// * Output  unsigned   Value of data received                              *
// **************************************************************************

static	unsigned            BitIORecv               (
								unsigned            ui___bitcount)

	{
	// Local Variables.

	unsigned            ui___bitvalue;

	//

	ui___bitvalue = 0;

	while (ui___bitcount--)
		{
		if (ui___BitsInp == 1)
			{
			ui___BitsInp = *pub__LoadCur++ + 0x0100u;
			}

		ui___bitvalue = (ui___bitvalue << 1) | (ui___BitsInp & 1);

		ui___BitsInp >>= 1;
		}

	// Return with value.

	return (ui___bitvalue);
	}



// **************************************************************************
// * BitIOFlush ()                                                          *
// **************************************************************************
// * Flush bit-oriented buffered input and output                           *
// **************************************************************************
// * Inputs  -                                                              *
// *                                                                        *
// * Output  -                                                              *
// **************************************************************************

static	void                BitIOFlush              (void)

	{
	ui___BitsInp = 1;

	if (pub__BitsCur != NULL)
		{
		*pub__BitsCur = ub___BitsOut;

		pub__BitsCur  = NULL;
		}

	return;
	}



// **************************************************************************
// * ErrorReset ()                                                          *
// **************************************************************************
// * Reset the error condition flags                                        *
// **************************************************************************
// * Inputs  -                                                              *
// *                                                                        *
// * Output  -                                                              *
// **************************************************************************

void                ErrorReset              (void)

	{
	si___ErrorCode = ERROR_NONE;

	acz__ErrorMessage[0] = '\0';
	}



// **************************************************************************
// * ErrorQualify ()                                                        *
// **************************************************************************
// * If ErrorMessage is blank, then fill it with a generic message          *
// **************************************************************************
// * Inputs  -                                                              *
// *                                                                        *
// * Output  -                                                              *
// **************************************************************************

void                ErrorQualify            (void)

	{
	if (*acz__ErrorMessage == '\0')

		{
		if (si___ErrorCode == ERROR_NONE)
			{
			}

		else if (si___ErrorCode == ERROR_DIAGNOSTIC)
			{
			sprintf(acz__ErrorMessage,
				"Error : Error during diagnostic printout.\n");
			}

		else if (si___ErrorCode == ERROR_NO_MEMORY)
			{
			sprintf(acz__ErrorMessage,
				"Error : Not enough memory to complete this operation.\n");
			}

		else if (si___ErrorCode == ERROR_NO_FILE)
			{
			sprintf(acz__ErrorMessage,
				"Error : File not found.\n");
			}

		else if (si___ErrorCode == ERROR_IO_EOF)
			{
			sprintf(acz__ErrorMessage,
				"Error : Unexpected end-of-file.\n");
			}

		else if (si___ErrorCode == ERROR_IO_READ)
			{
			sprintf(acz__ErrorMessage,
				"Error : I/O read failure (file corrupted ?).\n");
			}

		else if (si___ErrorCode == ERROR_IO_WRITE)
			{
			sprintf(acz__ErrorMessage,
				"Error : I/O write failure (disk full ?).\n");
			}

		else if (si___ErrorCode == ERROR_IO_SEEK)
			{
			sprintf(acz__ErrorMessage,
				"Error : I/O seek failure (file corrupted ?).\n");
			}

		else if (si___ErrorCode == ERROR_PROGRAM)
			{
			sprintf(acz__ErrorMessage,
				"Error : A program error has occurred.\n");
			}

		else
			{
			sprintf(acz__ErrorMessage,
				"Error : Unknown error number.\n");
			}
		}
	}



// **************************************************************************
// **************************************************************************
// **************************************************************************
// **************************************************************************



// **************************************************************************
// * LoadWholeFile ()                                                       *
// **************************************************************************
// *                                                                        *
// **************************************************************************
// * Inputs  char *      File name                                          *
// *         uint8_t **  Ptr to variable holding file address               *
// *         long *      Ptr to variable holding file size                  *
// *                                                                        *
// * Output  long        Bytes read, or -ve if an error                     *
// **************************************************************************

long                LoadWholeFile           (
								char *              pcz__Name,
								unsigned char **    ppbf_Addr,
								long *              psl__Size)

	{
	// Local Variables.

	FILE *              file;
	unsigned char *     addr;
	long                size;

	//

	if ((pcz__Name == NULL) || (ppbf_Addr == NULL) || (psl__Size == NULL))
		{
		return (-1);
		}

	if ((file = fopen(pcz__Name, "rb")) == NULL)
		{
		return (-1);
		}

	size = GetFileLength(file);

	if ((*psl__Size != 0) && (*psl__Size < size))
		{
		size = *psl__Size;
		}

	addr = *ppbf_Addr;

	if (addr == NULL)
		{
		addr = (unsigned char *) malloc(size);
		}

	if (addr == NULL)
		{
		size = -1;
		}
	else
		{
		if (fread(addr, 1, size, file) != (size_t) size)
			{
			size = -1;
			}
		}

	fclose(file);

	*psl__Size = size;

	if (*ppbf_Addr == NULL)
		{
		if (size < 0)
			{
			free(addr);
			}
		else
			{
			*ppbf_Addr = addr;
			}
		}

	// All done, return size or -ve if error.

	return (size);
	}



// **************************************************************************
// * SaveWholeFile ()                                                       *
// **************************************************************************
// *                                                                        *
// **************************************************************************
// * Inputs  char *      File name                                          *
// *         uint8_t *   File address                                       *
// *         long        File size                                          *
// *                                                                        *
// * Output  long        Bytes written, or -ve if an error                  *
// **************************************************************************

long                SaveWholeFile           (
								char *              pcz__Name,
								unsigned char *     pbf__Addr,
								long                sl___Size)

	{
	// Local Variables.

	FILE *              file;

	//

	if ((pcz__Name == NULL) || (pbf__Addr == NULL) || (sl___Size < 0))
		{
		return (-1);
		}

	if ((file = fopen(pcz__Name, "wb")) == NULL)
		{
		return (-1);
		}

	if (sl___Size != 0)
		{
		if (fwrite(pbf__Addr, 1, sl___Size, file) != (size_t) sl___Size)
			{
			sl___Size = -1;
			}
		}

	fclose(file);

	// All done, return size or -ve if error.

	return (sl___Size);
	}



// **************************************************************************
// * GetFileLength ()                                                       *
// **************************************************************************
// *                                                                        *
// **************************************************************************
// * Inputs  FILE *                                                         *
// *                                                                        *
// * Output  long                                                           *
// **************************************************************************

long                GetFileLength           (
								FILE *              file)

	{
	// Local Variables.

	long                CurrentPos;
	long                FileLength;

	//

	CurrentPos = ftell(file);

	fseek(file, 0, SEEK_END);

	FileLength = ftell(file);

	fseek(file, CurrentPos, SEEK_SET);

	return (FileLength);
	}



// **************************************************************************
// **************************************************************************
// **************************************************************************
//	END OF SWD32.C
// **************************************************************************
// **************************************************************************
// **************************************************************************
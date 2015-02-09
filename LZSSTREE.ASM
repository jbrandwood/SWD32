; ***************************************************************************
; ***************************************************************************
; ***************************************************************************
; **                                                                       **
; ** LZSSTREE.ASM                                                   MODULE **
; **                                                                       **
; ** Lempel-Ziv sliding window compression routines.                       **
; **                                                                       **
; ** Copyright John Brandwood 1992-2015.                                   **
; **                                                                       **
; ** Distributed under the Boost Software License, Version 1.0.            **
; ** (See accompanying file LICENSE_1_0.txt or copy at                     **
; **  http://www.boost.org/LICENSE_1_0.txt)                                **
; **                                                                       **
; ***************************************************************************
; ***************************************************************************
; ***************************************************************************

; Code assembles with MASM or TASM and uses __cdecl stack-based calling
; conventions and naming. All compilers require you to preserve the DF.
;
; Watcom    requires code to preserve everything except EAX ECX EDX
; Borland   requires code to preserve everything except EAX EBX ECX EDX
; Microsoft requires code to preserve everything except EAX ECX EDX

			.386
			.MODEL	flat
			.CODE

;_TEXT		SEGMENT BYTE PUBLIC 'CODE' USE32
;_TEXT		ENDS
;
;_DATA		SEGMENT DWORD PUBLIC 'DATA' USE32
;_DATA		ENDS
;
;_BSS		SEGMENT DWORD PUBLIC 'BSS' USE32
;_BSS		ENDS
;
;DGROUP		GROUP	_DATA,_BSS

;
; DEFINITIONS
;

; Tree structure (interleaved for Pentium access).

BIGGER			EQU	0		; Must remain at offset 0 for root nodes to work.
PARENT			EQU	4
NUMERIC_NEXT		EQU	8
LESSER			EQU	12
WINDOW			EQU	16
NUMERIC_LAST		EQU	20

NODE_LENGTH		EQU	24

; Constants.

WINDOW_SIZE		EQU	0800h
WINDOW_MASK		EQU	07FFh

UNUSED_INDEX		EQU	(WINDOW_SIZE+0)
ANCHOR_INDEX		EQU	(WINDOW_SIZE+1)

BREAK_EVEN		EQU	2
MAX_LENGTH		EQU	32
MAX_OFFSET		EQU	4096

;
; GLOBAL VARIABLES (UNINITIALIZED)
;

			.DATA?

			PUBLIC	_si___gMaxLength
			PUBLIC	_si___gMatchSize
			PUBLIC	_pcl__gMatchTree
			PUBLIC	_pub__gMatchData

			PUBLIC	_aub__gData
			PUBLIC	_acl__gTree
			PUBLIC	_apcl_gRoot

_si___gMaxLength	DD	1 DUP (?)
_si___gMatchSize	DD	1 DUP (?)
_pcl__gMatchTree	DD	1 DUP (?)
_pub__gMatchData	DD	1 DUP (?)

; Note - The order of the following structures IS important.

_aub__gData			DB	(WINDOW_SIZE*2) DUP (?)
_acl__gTree			DB	(WINDOW_SIZE)*NODE_LENGTH DUP (?)
_cl___gNull			DB	(          1)*NODE_LENGTH DUP (?)
_apcl_gRoot			DD	65536 DUP (?)

;_acl__gRoot		DB	(          1)*NODE_LENGTH DUP (?)


;
; STATIC VARIABLES
;



; **************************************************************************
; **************************************************************************
; **************************************************************************
;  GLOBAL FUNCTIONS
; **************************************************************************
; **************************************************************************
; **************************************************************************

		.CODE

		PUBLIC	_InitTree
		PUBLIC	_AddString
		PUBLIC	_RmvString




; **************************************************************************
; * InitTree ()                                                            *
; **************************************************************************
; *                                                                        *
; **************************************************************************
; * Inputs  -                                                              *
; *                                                                        *
; * Output  int      Return value                                          *
; *                                                                        *
; * N.B.    To make the tree really usable, a single phrase has to be      *
; *         added to the tree so it has a root node.  That is done here.   *
; **************************************************************************

		PUBLIC	_InitTree
_InitTree	PROC

		PUSH	EBP
		MOV	EBP,ESP

		PUSH	ESI
		PUSH	EDI

		; Clear main tree.

		MOV	EDI,OFFSET _acl__gTree
		MOV	EAX,OFFSET _aub__gData
		MOV	ECX,OFFSET _cl___gNull

@@InitTreeLoop:	MOV	DWORD PTR [EDI+WINDOW],EAX
		MOV	DWORD PTR [EDI+PARENT],ECX
		INC	EAX
		ADD	EDI,NODE_LENGTH
		CMP	EDI,ECX
		JNZ	short @@InitTreeLoop

		; Clear dummy 'unused' node.

		MOV	DWORD PTR [EDI+WINDOW],EAX
		MOV	DWORD PTR [EDI+PARENT],ECX
		MOV	DWORD PTR [EDI+LESSER],ECX
		MOV	DWORD PTR [EDI+BIGGER],ECX
		MOV	DWORD PTR [EDI+NUMERIC_NEXT],ECX
		MOV	DWORD PTR [EDI+NUMERIC_LAST],ECX

		; Clear root and setup node 0 as the first node in the tree.

		MOV	EDI,OFFSET _apcl_gRoot
		MOV	ESI,OFFSET _acl__gTree

		MOV	DWORD PTR [EDI+BIGGER],OFFSET _cl___gNull

;		MOV	DWORD PTR [EDI+WINDOW],0
;
;		MOV	DWORD PTR [EDI+PARENT],ECX
;		MOV	DWORD PTR [EDI+LESSER],ECX
;		MOV	DWORD PTR [EDI+BIGGER],ESI
;		MOV	DWORD PTR [EDI+NUMERIC_NEXT],ECX
;		MOV	DWORD PTR [EDI+NUMERIC_LAST],ECX
;
;		MOV	DWORD PTR [ESI+PARENT],OFFSET _apcl_gRoot
;		MOV	DWORD PTR [ESI+LESSER],ECX
;		MOV	DWORD PTR [ESI+BIGGER],ECX
;		MOV	DWORD PTR [ESI+NUMERIC_NEXT],ECX
;		MOV	DWORD PTR [ESI+NUMERIC_LAST],ECX

		; Clear root and setup node 0 as the first node in the tree.

;		MOV	EDI,OFFSET _apcl_gRoot
;		MOV	ESI,OFFSET _acl__gTree
;
;		MOV	DWORD PTR [EDI+BIGGER],OFFSET _cl___gNull
;
;		MOV	DWORD PTR [EDI+WINDOW],0
;
;		MOV	DWORD PTR [EDI+PARENT],ECX
;		MOV	DWORD PTR [EDI+LESSER],ECX
;		MOV	DWORD PTR [EDI+BIGGER],ESI
;		MOV	DWORD PTR [EDI+NUMERIC_NEXT],ECX
;		MOV	DWORD PTR [EDI+NUMERIC_LAST],ECX
;
;		MOV	DWORD PTR [ESI+PARENT],OFFSET _apcl_gRoot
;		MOV	DWORD PTR [ESI+LESSER],ECX
;		MOV	DWORD PTR [ESI+BIGGER],ECX
;		MOV	DWORD PTR [ESI+NUMERIC_NEXT],ECX
;		MOV	DWORD PTR [ESI+NUMERIC_LAST],ECX

		MOV	ESI,OFFSET _apcl_gRoot
		MOV	EDI,OFFSET _apcl_gRoot + (65536*4)

@@InitTreeRoot:	MOV	DWORD PTR [ESI+ 0], ECX
		MOV	DWORD PTR [ESI+ 8], ECX
		MOV	DWORD PTR [ESI+ 4], ECX
		MOV	DWORD PTR [ESI+12], ECX
		MOV	DWORD PTR [ESI+16], ECX
		MOV	DWORD PTR [ESI+24], ECX
		MOV	DWORD PTR [ESI+20], ECX
		MOV	DWORD PTR [ESI+28], ECX
		ADD	ESI,32
		CMP	ESI,EDI
		JNZ	short @@InitTreeRoot

		;

		POP	EDI
		POP	ESI

		POP	EBP
		RET

_InitTree	ENDP



; **************************************************************************
; * AddString ()                                                           *
; **************************************************************************
; *                                                                        *
; **************************************************************************
; * Inputs  EAX      Ptr to new node                                       *
; *                                                                        *
; * Output  -                                                              *
; *                                                                        *
; * Alters  EAX,ECX,EDX,ESI,EDI                                            *
; *                                                                        *
; * N.B.    This where most of the work done by the encoder takes place.   *
; *                                                                        *
; *         This routine is responsible for adding the new node to the     *
; *         binary tree.                                                   *
; *                                                                        *
; *         It also has to find the best match among all the existing      *
; *         nodes in the tree, and return that to the calling routine.     *
; *                                                                        *
; *         To make matters even more complicated, if the new node has a   *
; *         duplicate in the tree, the old node is deleted for efficiency. *
; **************************************************************************

		PUBLIC	_AddString
_AddString	PROC

		; Create stack frame.

		PUSH	EBP
		MOV	EBP,ESP

		PUSH	EBX

		; Convert node index to pointer.

		MOV	EAX,DWORD PTR [EBP+8]
		MOV	ECX,DWORD PTR [EBP+8]
		SHL	EAX,3
		SHL	ECX,4
		ADD	EAX,OFFSET _acl__gTree
		ADD	EAX,ECX

		; Preserve more registers.

		PUSHF

		CLD

		PUSH	ESI
		PUSH	EDI

		; Initialize best match.

		MOV	ECX,DWORD PTR [_si___gMaxLength]
		MOV	DWORD PTR [_si___gMatchSize],ECX

		; Select which tree to search.

		MOV	EBX,0

		MOV	ESI,DWORD PTR [EAX+WINDOW]

		MOV	BL,BYTE PTR [ESI+0]
		MOV	BH,BYTE PTR [ESI+1]

		SHL	EBX,2

		ADD	EBX,OFFSET _apcl_gRoot

;		MOV	EBX,OFFSET _apcl_gRoot

		; Get first node from root of tree.

		MOV	ESI,DWORD PTR [EBX+0]

		CMP	ESI,OFFSET _cl___gNull
		JNZ	@@SearchLoop

		MOV	DWORD PTR [EBX+BIGGER],EAX

		MOV	DWORD PTR [EAX+PARENT],EBX
		MOV	DWORD PTR [EAX+LESSER],OFFSET _cl___gNull
		MOV	DWORD PTR [EAX+BIGGER],OFFSET _cl___gNull

		MOV	DWORD PTR [EAX+NUMERIC_NEXT],OFFSET _cl___gNull
		MOV	DWORD PTR [EAX+NUMERIC_LAST],OFFSET _cl___gNull

		; All done, return the result.

		POP	EDI
		POP	ESI

		POPF

		POP	EBX
		POP	EBP
		RET

		; Test the current string against this node's string.

@@SearchLoop:	MOV	EBX,ESI

		MOV	ECX,DWORD PTR [_si___gMaxLength]

		MOV	ESI,DWORD PTR [EAX+WINDOW]
		MOV	EDI,DWORD PTR [EBX+WINDOW]

		REPE	CMPSB

		JZ	@@WasIdentical

		JC	short @@WasLesser

		;
		; New string bigger than the test string.
		;

@@WasBigger:	MOV	ESI,DWORD PTR [EBX+BIGGER]

		CMP	ECX,DWORD PTR [_si___gMatchSize]
		JNC	short @@Skip0

		MOV	DWORD PTR [_si___gMatchSize],ECX
		MOV	DWORD PTR [_pcl__gMatchTree],EBX

@@Skip0:	CMP	ESI,OFFSET _cl___gNull
		JNZ	short @@SearchLoop

		; End of branch, add the new node as the bigger child of the this node.

		MOV	DWORD PTR [EBX+BIGGER],EAX

		MOV	DWORD PTR [EAX+PARENT],EBX
		MOV	DWORD PTR [EAX+LESSER],OFFSET _cl___gNull
		MOV	DWORD PTR [EAX+BIGGER],OFFSET _cl___gNull

		; Add new node into numeric list ahead of this node.

		MOV	ECX,DWORD PTR [EBX+NUMERIC_NEXT]
		MOV	DWORD PTR [EAX+NUMERIC_NEXT],ECX
		MOV	DWORD PTR [EAX+NUMERIC_LAST],EBX

		MOV	DWORD PTR [ECX+NUMERIC_LAST],EAX
		MOV	DWORD PTR [EBX+NUMERIC_NEXT],EAX

		; All done, return the result.

		POP	EDI
		POP	ESI

		POPF

		POP	EBX
		POP	EBP
		RET

		;
		; New string lesser than the test string.
		;

@@WasLesser:	MOV	ESI,DWORD PTR [EBX+LESSER]

		CMP	ECX,DWORD PTR [_si___gMatchSize]
		JNC	short @@Skip1

		MOV	DWORD PTR [_si___gMatchSize],ECX
		MOV	DWORD PTR [_pcl__gMatchTree],EBX

@@Skip1:	CMP	ESI,OFFSET _cl___gNull
		JNZ	short @@SearchLoop

		; End of branch, add the new node as the lesser child of the this node.

		MOV	DWORD PTR [EBX+LESSER],EAX

		MOV	DWORD PTR [EAX+PARENT],EBX
		MOV	DWORD PTR [EAX+LESSER],OFFSET _cl___gNull
		MOV	DWORD PTR [EAX+BIGGER],OFFSET _cl___gNull

		; Add new node into numeric list behind this node.

		MOV	ECX,DWORD PTR [EBX+NUMERIC_LAST]
		MOV	DWORD PTR [EAX+NUMERIC_LAST],ECX
		MOV	DWORD PTR [EAX+NUMERIC_NEXT],EBX

		MOV	DWORD PTR [ECX+NUMERIC_NEXT],EAX
		MOV	DWORD PTR [EBX+NUMERIC_LAST],EAX

		; All done, return the result.

		POP	EDI
		POP	ESI

		POPF

		POP	EBX
		POP	EBP
		RET

		;
		; New string identical to the test string (rare).
		;

@@WasIdentical:	MOV	DWORD PTR [_si___gMatchSize],-1
		MOV	DWORD PTR [_pcl__gMatchTree],EBX

		; Replace old node with new node.

		MOV	ECX,DWORD PTR [EBX+PARENT]

		CMP	EBX,DWORD PTR [ECX+BIGGER]
		JNE	short @@ReplaceLesser

@@ReplaceBigger:MOV	DWORD PTR [ECX+BIGGER],EAX
		JMP	@@Replaced

@@ReplaceLesser:MOV	DWORD PTR [ECX+LESSER],EAX

@@Replaced:	MOV	DWORD PTR [EAX+PARENT],ECX
		MOV	DWORD PTR [EBX+PARENT],OFFSET _cl___gNull

		MOV	EDX,DWORD PTR [EBX+BIGGER]
		MOV	DWORD PTR [EAX+BIGGER],EDX

		MOV	DWORD PTR [EDX+PARENT],EAX

		MOV	EDX,DWORD PTR [EBX+LESSER]
		MOV	DWORD PTR [EAX+LESSER],EDX

		MOV	DWORD PTR [EDX+PARENT],EAX

		MOV	EDX,DWORD PTR [EBX+NUMERIC_NEXT]
		MOV	DWORD PTR [EAX+NUMERIC_NEXT],EDX

		MOV	DWORD PTR [EDX+NUMERIC_LAST],EAX

		MOV	EDX,DWORD PTR [EBX+NUMERIC_LAST]
		MOV	DWORD PTR [EAX+NUMERIC_LAST],EDX

		MOV	DWORD PTR [EDX+NUMERIC_NEXT],EAX

		; All done, return the result.

		POP	EDI
		POP	ESI

		POPF

		POP	EBX
		POP	EBP
		RET

_AddString	ENDP



; **************************************************************************
; * RmvString ()                                                           *
; **************************************************************************
; *                                                                        *
; **************************************************************************
; * Inputs  EAX      Ptr to node to delete                                 *
; *                                                                        *
; * Output  -                                                              *
; *                                                                        *
; * Alters  EAX,ECX,EDX                                                    *
; *                                                                        *
; * N.B.    This routine performs the classic b-tree deletion algorithm.   *
; *                                                                        *
; *         If the node to be deleted has a null link in either direction, *
; *         we just pull the non-null link up one to replace the existing  *
; *         link.                                                          *
; *                                                                        *
; *         If both links exist, we instead delete the next link in order, *
; *         which is guaranteed to have a null link, then replace the node *
; *         to be deleted with the next link in order.                     *
; **************************************************************************

		PUBLIC	_RmvString
_RmvString	PROC

		; Create stack frame.

		PUSH	EBP
		MOV	EBP,ESP

		PUSH	EBX

		; Convert node index to pointer.

		MOV	EAX,DWORD PTR [EBP+8]
		MOV	ECX,DWORD PTR [EBP+8]
		SHL	EAX,3
		SHL	ECX,4
		ADD	EAX,OFFSET _acl__gTree
		ADD	EAX,ECX

		; Node already free ?

		CMP	DWORD PTR [EAX+PARENT],OFFSET _cl___gNull
		JZ	short @@RmvCompleted

		; Unlink from numeric list.

		MOV	EBX,DWORD PTR [EAX+NUMERIC_NEXT]
		MOV	ECX,DWORD PTR [EAX+NUMERIC_LAST]

		MOV	DWORD PTR [EBX+NUMERIC_LAST],ECX
		MOV	DWORD PTR [ECX+NUMERIC_NEXT],EBX

		; Bigger link empty ?

		MOV	EBX,DWORD PTR [EAX+LESSER]

		CMP	DWORD PTR [EAX+BIGGER],OFFSET _cl___gNull
		JZ	short @@ContractNode

		; Lesser link empty ?

		MOV	EBX,DWORD PTR [EAX+BIGGER]

		CMP	DWORD PTR [EAX+LESSER],OFFSET _cl___gNull
		JZ	short @@ContractNode

		; Both links in use, find next smallest node in EBX.
		;
		; We find the next smallest node by going to the lesser_child
		; node, then going to the end of the bigger_child descendant
		; chain.

		MOV	EBX,DWORD PTR [EAX+LESSER]
		MOV	ECX,DWORD PTR [EBX+BIGGER]

		CMP	ECX,OFFSET _cl___gNull
		JNZ	@@FindLoop

		; The node's lesser-child has no bigger-child descendants.

		MOV	DWORD PTR [ECX+PARENT],EAX
		MOV	DWORD PTR [EAX+LESSER],ECX

		JMP	@@Found

		;

@@RmvCompleted:	POP	EBX
		POP	EBP
		RET

		; Move down the chain of bigger-child descendants.

@@FindLoop:	MOV	EBX,ECX
		MOV	ECX,DWORD PTR [EBX+BIGGER]

		CMP	ECX,OFFSET _cl___gNull
		JNZ	short @@FindLoop

		; Unlink EBX, pulling up smaller child (since bigger is empty).

		MOV	ECX,DWORD PTR [EBX+LESSER]
		MOV	EDX,DWORD PTR [EBX+PARENT]

		MOV	DWORD PTR [ECX+PARENT],EDX
		MOV	DWORD PTR [EDX+BIGGER],ECX

		; Replace the node to be deleted (EAX), with the next smallest
		; child (EBX).

@@Found:	MOV	ECX,DWORD PTR [EAX+PARENT]

		MOV	DWORD PTR [EBX+PARENT],ECX
		MOV	DWORD PTR [EAX+PARENT],OFFSET _cl___gNull

		MOV	EDX,DWORD PTR [EAX+BIGGER]
		MOV	DWORD PTR [EBX+BIGGER],EDX

		MOV	DWORD PTR [EDX+PARENT],EBX

		MOV	EDX,DWORD PTR [EAX+LESSER]
		MOV	DWORD PTR [EBX+LESSER],EDX

		MOV	DWORD PTR [EDX+PARENT],EBX

		CMP	EAX,DWORD PTR [ECX+BIGGER]
		JNE	short @@RmvLesser

@@RmvBigger:	MOV	DWORD PTR [ECX+BIGGER],EBX

		POP	EBX
		POP	EBP
		RET

@@RmvLesser:	MOV	DWORD PTR [ECX+LESSER],EBX

		POP	EBX
		POP	EBP
		RET

		; Pull up the child link to overlay the existing link.

@@ContractNode:	MOV	ECX,DWORD PTR [EAX+PARENT]

		MOV	DWORD PTR [EAX+PARENT],OFFSET _cl___gNull
		MOV	DWORD PTR [EBX+PARENT],ECX

		CMP	DWORD PTR [ECX+BIGGER],EAX
		JNZ	short @@LesserChild

@@BiggerChild:	MOV	DWORD PTR [ECX+BIGGER],EBX

		POP	EBX
		POP	EBP
		RET

@@LesserChild:	MOV	DWORD PTR [ECX+LESSER],EBX

		POP	EBX
		POP	EBP
		RET

_RmvString	ENDP

;
;
;

		END



; **************************************************************************
; **************************************************************************
; **************************************************************************
;  END OF LZSSTREE.ASM
; **************************************************************************
; **************************************************************************
; **************************************************************************

; **************************************************************************
; **************************************************************************
; **                                                                      **
; ** SWDN64.ASM                                                    MODULE **
; **                                                                      **
; ** Decompress from Elmer's SWD format.                                  **
; **                                                                      **
; ** Copyright John Brandwood 1997-2015.                                  **
; **                                                                      **
; ** Distributed under the Boost Software License, Version 1.0.           **
; ** (See accompanying file LICENSE_1_0.txt or copy at                    **
; **  http://www.boost.org/LICENSE_1_0.txt)                               **
; **                                                                      **
; ** Register Names ...                                                   **
; **                                                                      **
; **  r0 r0     s0 r16                                                    **
; **  at r1     s1 r17                                                    **
; **  v0 r2     s2 r18                                                    **
; **  v1 r3     s3 r19                                                    **
; **  a0 r4     s4 r20                                                    **
; **  a1 r5     s5 r21                                                    **
; **  a2 r6     s6 r22                                                    **
; **  a3 r7     s7 r23                                                    **
; **  t0 r8     t8 r24                                                    **
; **  t1 r9     t9 r25                                                    **
; **  t2 r10    k0 r26                                                    **
; **  t3 r11    k1 r27                                                    **
; **  t4 r12    gp r28                                                    **
; **  t5 r13    sp r29                                                    **
; **  t6 r14    fp r30                                                    **
; **  t7 r15    ra r31                                                    **
; **                                                                      **
; **************************************************************************
; **************************************************************************

text		group
		sectionw	n64asm,text

;
; EXTERNAL SYMBOLS
;

;
; EXPORTED SYMBOLS
;

		xdef	pub__LoadCur
		xdef	pub__LoadEnd

		xdef	pub__SaveCur
		xdef	pub__SaveEnd

		xdef	SwdExpand

;
; DEFINITIONS
;

;
; GLOBAL VARIABLES
;

v			DW	0	; start of variables

pub__LoadCur		DW	0	; UB *
pub__LoadEnd		DW	0	; UB *
pub__SaveCur		DW	0	; UB *
pub__SaveEnd		DW	0	; UB *

SwdOffsets:		DW	 5, $0000+1
			DW	 7, $0020+1
			DW	 9, $00A0+1
			DW	10, $02A0+1

;
; GLOBAL FUNCTIONS
;


; **************************************************************************
; * SwdExpand ()                                                           *
; **************************************************************************
; *                                                                        *
; **************************************************************************
; * Inputs  -                                                              *
; *                                                                        *
; * Output  -                                                              *
; *                                                                        *
; * Used    v0-v1, a0-a3, t0-t9                                            *
; *                                                                        *
; *         t0 = pub__LoadCur                                              *
; *         t1 = pub__SaveCur                                              *
; *         t2 = match_length                                              *
; *         t3 = match_offset                                              *
; *         t4 = ui__BitsInp                                               *
; *         t5 = constant = 1                                              *
; *         t6 = offsets table                                             *
; *                                                                        *
; * N.B.    The data is encoded as length/offset pairs.                    *
; *                                                                        *
; *         Lengths are encoded ...                                        *
; *                                                                        *
; *             1       : 0  dddddddd                                      *
; *             2       : 10                                               *
; *             3-5     : 11 xx                                            *
; *             6-20    : 11 00 xxxx                                       *
; *            21-275   : 11 00 0000 xxxxxxxx                              *
; *                                                                        *
; *         Offsets are encoded ...                                        *
; *                                                                        *
; *         $0000-$001F : 00       x xxxx                                  *
; *         $0020-$009F : 01     xxx xxxx                                  *
; *         $00A0-$029F : 10  x xxxx xxxx                                  *
; *         $02A0-$069F : 11 xx xxxx xxxx                                  *
; *                                                                        *
; **************************************************************************

SwdExpand:	addiu	sp,sp,-8
		sd	r31,$00(sp)

		lw	t0,pub__LoadCur
		lw	t1,pub__SaveCur

		addi	t4,r0,1				; bitsinput
		addi	t5,r0,1				; constant

		lui	t6,(SwdOffsets>>16)&$0000FFFF
		ori	t6,t6,(SwdOffsets)&$0000FFFF

;
; Determine whether to move or copy data.
;

swdloop:	beq	t4,t5,swdload0			; bit buffer empty ?
		and	t2,t4,t5

		bne	t2,r0,swdcopy			; 0=move or 1=copy ?
		srl	t4,t4,1

swdmove:	lbu	t3,(t0)				; move single byte
		addiu	t0,t0,1
		sb	t3,(t1)
		j	swdloop
		addi	t1,t1,1

swdload0:	lbu	t4,(t0)				; load bit buffer
		addiu	t0,t0,1
		ori	t4,t4,256			; set end marker

		and	t2,t4,t5

		beq	t2,r0,swdmove			; 0=move or 1=copy ?
		srl	t4,t4,1

;
; Get copy length.
;

swdcopy:	beq	t4,t5,swdload1			; bit buffer empty ?
		and	t2,t4,t5

		beq	t2,r0,swdaddr			; 0=copy2 or 1=copyN ?
		srl	t4,t4,1

swdcpyN:	add	v0,r0,r0

		jal	swdreadbits			; read 2 bits
		add	v1,r0,2

		bne	v0,r0,swdaddr			; 0=more, nz=3-5
		addiu	t2,v0,3-3

		jal	swdreadbits			; read 4 bits
		add	v1,r0,4

		bne	v0,r0,swdaddr			; 0=more, nz=6-20
		addiu	t2,v0,6-3

		lbu	v0,(t0)				; read 8 bits
		addiu	t0,t0,1

		bne	v0,r0,swdaddr			; 0=more, nz=21-275
		addiu	t2,v0,21-3

		j	swddone				; end-of-file
		nop

swdload1:	lbu	t4,(t0)				; load bit buffer
		addiu	t0,t0,1
		ori	t4,t4,256			; set end marker

		and	t2,t4,t5

		bne	t2,r0,swdcpyN			; 0=copy2 or 1=copyN ?
		srl	t4,t4,1

;
; Get copy offset.
;

swdaddr:	add	t2,t2,2				; minimum copy of 2

		add	v0,r0,r0

		jal	swdreadbits			; read 2 bits
		add	v1,r0,2

		sll	v0,v0,3
		add	v0,v0,t6

		lw	v1,0(v0)
		lw	t3,4(v0)

		sltiu	at,v1,8				; addr > 8 bits long ?
		bne	at,r0,swdadrN
		add	v0,r0,r0

		lbu	v0,(t0)				; read top 8 bits
		addiu	t0,t0,1

		addiu	v1,v1,-8

swdadrN:	jal	swdreadbits			; read N bits
		nop

		add	t3,t3,v0			; calc offset

cpydata:	subu	a0,t1,t3			; calc address

cpyloop:	lb	a1,0(a0)			; copy bytes
		addiu	a0,a0,1
		addiu	t2,t2,-1
		sb	a1,0(t1)
		bne	t2,r0,cpyloop
		addiu	t1,t1,1

		j	swdloop
		nop

;
; All done, return OK.
;

swddone:	ld	r31,(sp)

		sw	t0,pub__LoadCur
		sw	t1,pub__SaveCur

		add	v0,r0,r0

		j	r31
		addiu	sp,sp,8

;
; Read a number of bits from the input.
;

swdreadbits:	beq	t4,t5,swdloadbits		; bit buffer empty ?
		and	at,t4,t5

		sll	v0,v0,1
		or	v0,v0,at

		addi	v1,v1,-1

		bne	v1,r0,swdreadbits		; bit count finished ?
		srl	t4,t4,1

		j	r31
		nop

swdloadbits:	lbu	t4,(t0)				; load bit buffer
		addiu	t0,t0,1
		ori	t4,t4,256			; set end marker

		and	at,t4,t5

		sll	v0,v0,1
		or	v0,v0,at

		addi	v1,v1,-1

		bne	v1,r0,swdreadbits		; bit count finished ?
		srl	t4,t4,1

		j	r31
		nop

;
;
;

		end


; **************************************************************************
; **************************************************************************
; **************************************************************************


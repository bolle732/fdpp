;
; File:
;                           int2f.asm
; Description:
;                 multiplex interrupt support code
;
;                    Copyright (c) 1996, 1998
;                       Pasquale J. Villani
;                       All Rights Reserved
;
; This file is part of DOS-C.
;
; DOS-C is free software; you can redistribute it and/or
; modify it under the terms of the GNU General Public License
; as published by the Free Software Foundation; either version
; 2, or (at your option) any later version.
;
; DOS-C is distributed in the hope that it will be useful, but
; WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
; the GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public
; License along with DOS-C; see the file COPYING.  If not,
; write to the Free Software Foundation, 675 Mass Ave,
; Cambridge, MA 02139, USA.
;
; $Id: int2f.asm 1591 2011-05-06 01:46:55Z bartoldeman $
;

        %include "segs.inc"
        %include "stacks.inc"

segment	HMA_TEXT
            extern _cu_psp
            extern _HaltCpuWhileIdle
            extern _syscall_MUX14

            extern _DGROUP_

                global  reloc_call_int2f_handler
reloc_call_int2f_handler:
                jmp hdlr2f
                global  _prev_int2f_handler
_prev_int2f_handler:
                dd 0
                dw 0x424B
                db 0
                jmp rst2f
                times 7 db 0
rst2f:
                retf
;hdlr2f:
;                sti                             ; Enable interrupts
;                cmp     ah,11h                  ; Network interrupt?
;                jne     Int2f3                  ; No, continue
Int2f1:
                PUSH$ALL
                mov bp, sp              ; Save pointer to iregs struct
                push ss
                push bp                 ; Pass pointer to iregs struct to C
                mov ds, [cs:_DGROUP_]
                extern   _int2F_10_handler
                call _int2F_10_handler
                add sp, 4               ; Remove SS,SP
                POP$ALL
                iret

FarTabRetn:
                retf    2                       ; Return far

WinIdle:					; only HLT if at haltlevel 2+
		push	ds
                mov     ds, [cs:_DGROUP_]
		cmp	byte [_HaltCpuWhileIdle],2
		pop	ds
		jb	Int2f?prev
		pushf
		sti
		hlt				; save some energy :-)
		popf
		push	ds
                mov     ds, [cs:_DGROUP_]
		cmp	byte [_HaltCpuWhileIdle],3
		pop	ds
		jb	Int2f?prev
		mov	al,0			; even admit we HLTed ;-)
		jmp	short FarTabRetn

hdlr2f:
Int2f3:         cmp     ax,1680h                ; Win "release time slice"
                je      WinIdle
;                cmp     ah,16h
;                je      FarTabRetn              ; other Win Hook return fast
                cmp     ah,12h
                je      IntDosCal               ; Dos Internal calls

                cmp     ax,4a01h
                je      IntDosCal               ; Dos Internal calls
                cmp     ax,4a02h
                je      IntDosCal               ; Dos Internal calls
                cmp     ax,4a03h
                je      IntDosCal               ; Dos Internal calls
                cmp     ax,4a04h
                je      IntDosCal               ; Dos Internal calls
%ifdef WITHFAT32
                cmp     ax,4a33h                ; Check DOS version 7
                jne     Check4Share
                xor     ax,ax                   ; no undocumented shell strings
                iret
Check4Share:
%endif
                cmp     ah,10h                  ; SHARE.EXE interrupt?
                je      Int2f1                  ; yes, do installation check
                cmp     ah,08h
                je      DriverSysCal            ; DRIVER.SYS calls
                cmp     ah,14h                  ; NLSFUNC.EXE interrupt?
                jne     Int2f?prev              ; no, go out
Int2f?14:      ;; MUX-14 -- NLSFUNC API
               ;; all functions are passed to syscall_MUX14
               push bp                 ; Preserve BP later on
               Protect386Registers
               PUSH$ALL
               mov bp, sp              ; Save pointer to iregs struct
               push ss
               push bp                 ; Pass pointer to iregs struct to C
               mov ds, [cs:_DGROUP_]
               call _syscall_MUX14
               add sp, 6               ; Remove SS,SP and old AX
               push ax                 ; Save return value for POP$ALL
               POP$ALL
               Restore386Registers
               mov bp, sp
               or ax, ax
               jnz Int2f?14?1          ; must return set carry
                   ;; -6 == -2 (CS), -2 (IP), -2 (flags)
                   ;; current SP = on old_BP
               and BYTE [bp-6], 0feh   ; clear carry as no error condition
               pop bp
               iret
Int2f?14?1:
               or BYTE [bp-6], 1
               pop bp
               iret
Int2f?prev:
               push bp
               mov bp, sp
               push WORD [bp+6]        ; sync flags
               popf
               pop bp
               jmp far [cs:_prev_int2f_handler]

; DRIVER.SYS calls - now only 0803.
DriverSysCal:
               push bp                 ; Preserve BP later on
               Protect386Registers
               PUSH$ALL
               mov bp, sp              ; Save pointer to iregs struct
               push ss
               push bp                 ; Pass pointer to iregs struct to C
               mov ds, [cs:_DGROUP_]
               extern   _int2F_08_handler
               call _int2F_08_handler
               add sp, 4               ; Remove SS,SP
               POP$ALL
               Restore386Registers
               pop bp
               iret

;**********************************************************************
; internal dos calls INT2F/12xx and INT2F/4A01,4A02 - handled through C
;**********************************************************************
IntDosCal:
                        ; set up register frame
;struct int2f12regs
;{
;  [space for 386 regs]
;  UWORD es,ds;
;  UWORD di,si,bp,bx,dx,cx,ax;
;  UWORD ip,cs,flags;
;  UWORD callerARG1;
;}
    push ax
    push cx
    push dx
    push bx
    push bp
    push si
    push di
    push ds
    push es

    cld

%if XCPU >= 386
  %ifdef WATCOM
    mov si,fs
    mov di,gs
  %else
    Protect386Registers
  %endif
%endif

    mov bp,sp
    push ss
    push bp
    mov ds,[cs:_DGROUP_]
    extern   _int2F_12_handler
    call _int2F_12_handler
    add sp,4

%if XCPU >= 386
  %ifdef WATCOM
    mov fs,si
    mov gs,di
  %else
    Restore386Registers
  %endif
%endif

    pop es
    pop ds
    pop di
    pop si
    pop bp
    pop bx
    pop dx
    pop cx
    pop ax

    iret

; Int 2F Multipurpose Remote System Calls
;
; added by James Tabor jimtabor@infohwy.com
; changed by Bart Oldeman
;
; assume ss == ds after setup of stack in entry
; sumtimes return data *ptr is the push stack word
;

;DWORD ASMPASCAL network_redirector_mx(UWORD cmd, void far *s, UWORD arg)
                global NETWORK_REDIRECTOR_MX
NETWORK_REDIRECTOR_MX:
                pop     bx             ; ret address
                pop     cx             ; stack value (arg); cx in remote_rw
                pop     dx             ; off s
                pop     es             ; seg s
                pop     ax             ; cmd (ax)
                push    bx             ; ret address
call_int2f:
                push    bp
                push    si
                push    di

                mov     di, dx         ; es:di -> s and dx is used for 1125!
                cmp     al, 08h
                je      remote_rw
                cmp     al, 09h
                je      remote_rw
                cmp     al, 22h
                je      remote_process_end

                push    cx             ; arg
                cmp     al, 1eh
                je      remote_print_doredir
                cmp     al, 1fh
                je      remote_print_doredir

int2f_call:
                xor     cx, cx         ; set to succeed; clear carry and CX
                int     2fh
                pop     bx
                jnc     ret_set_ax_to_cx
ret_neg_ax:
                neg     ax
ret_int2f:
                pop     di
                pop     si
                pop     bp
                ret

ret_set_ax_to_cx:                      ; ext_open or rw -> status from CX in AX
                                       ; otherwise CX was set to zero above
                xchg    ax, cx         ; set ax:=cx (one byte shorter than mov)
                jmp     short ret_int2f

remote_print_doredir:                  ; di points to an lregs structure
                push word [es:di+0xe]  ; es
                mov     bx,[es:di+2]
                mov     cx,[es:di+4]
                mov     dx,[es:di+6]
                mov     si,[es:di+8]
                lds     di,[es:di+0xa]
                pop     es
                clc                     ; set to succeed
                int     2fh
                pop     bx              ; restore stack and ds=ss
                push    ss
                pop     ds
                jc      ret_neg_ax
ret_set_ax_to_carry:                    ; carry => -1 else 0 (SUCCESS)
                sbb     ax, ax
                jmp     short ret_int2f

remote_rw:
                clc                    ; set to succeed
                int     2fh
                jc      ret_min_dx_ax
                xor     dx, dx         ; dx:ax := dx:cx = bytes read
                jmp     short ret_set_ax_to_cx
ret_min_dx_ax:  neg     ax
                cwd
                jmp     short ret_int2f

remote_process_end:                   ; Terminate process
                mov     ds, [_cu_psp]
int2f_restore_ds:
                clc
                int     2fh
                push    ss
                pop     ds
                jmp     short ret_set_ax_to_carry

; extern UWORD ASMPASCAL call_nls(UWORD bp, UWORD FAR *buf,
;	UWORD subfct, UWORD cp, UWORD cntry, UWORD bufsize);

		extern _nlsInfo
		global CALL_NLS
CALL_NLS:
		pop	es		; ret addr
		pop	cx		; bufsize
		pop	dx		; cntry
		pop	bx		; cp
		pop	ax		; sub fct
		mov	ah, 0x14
		push	es		; ret addr
		push	bp
		mov	bp, sp
		push	si
		push	di
		mov	si, _nlsInfo	; nlsinfo
		les	di, [bp + 4]	; buf
		mov	bp, [bp + 8]	; bp
		int	0x2f
		mov	dx, bx          ; return id in high word
		pop	di
		pop	si
		pop	bp
		ret	6

; extern UWORD ASMPASCAL floppy_change(UWORD drives)

		global FLOPPY_CHANGE
FLOPPY_CHANGE:
		pop	cx		; ret addr
		pop	dx		; drives
		push	cx		; ret addr
		mov	ax, 0x4a00
		xor	cx, cx
		int	0x2f
		mov	ax, cx		; return
		ret

;
; Test to see if a umb driver has been loaded.
; if so, retrieve largest available block+size
;
; From RB list and Dosemu xms.c.
;
; Call the XMS driver "Request upper memory block" function with:
;     AH = 10h
;     DX = size of block in paragraphs
; Return: AX = status
;         0001h success
;         BX = segment address of UMB
;         DX = actual size of block
;         0000h failure
;         BL = error code (80h,B0h,B1h) (see #02775)
;         DX = largest available block
;
; (Table 02775)
; Values for XMS error code returned in BL:
;  00h    successful
;  80h    function not implemented
;  B0h    only a smaller UMB is available
;  B1h    no UMBs are available
;  B2h    UMB segment number is invalid
;

segment INIT_TEXT
                ; int ASMPASCAL UMB_get_largest(void FAR * driverAddress,
                ;                UDWORD * seg, UCOUNT * size);
                global UMB_GET_LARGEST

UMB_GET_LARGEST:
                push    bp
                mov     bp,sp

                mov     dx,0xffff       ; go for broke!
                mov     ax,1000h        ; get the UMBs
                call    far [bp+8]      ; Call the driver

;
;       bl = 0xB0 and  ax = 0 so do it again.
;
                cmp     bl,0xb0         ; fail safe
                jne     umbt_error

                and     dx,dx           ; if it returns a size of zero.
                je      umbt_error

                mov     ax,1000h        ; dx set with largest size
                call    far [bp+8]      ; Call the driver
                mov     si,0
                adc     si,0

                cmp     ax,1
                jne     umbt_error
                                        ; now return the segment
                                        ; and the size

                mov 	cx,bx           ; *seg = segment
                mov 	bx, [bp+6]
                mov 	[bx],cx
                mov 	[bx+2],si

                mov 	bx, [bp+4]      ; *size = size
                mov 	[bx],dx

umbt_ret:
                pop     bp
                ret     8           	; this was called NEAR!!

umbt_error:     xor 	ax,ax
                jmp 	short umbt_ret

; WDL - fft_gen_x64.asm
; Copyright (C) 2018 Theo Niessink
;
; This software is provided 'as-is', without any express or implied
; warranty.  In no event will the authors be held liable for any damages
; arising from the use of this software.
;
; Permission is granted to anyone to use this software for any purpose,
; including commercial applications, and to alter it and redistribute it
; freely, subject to the following restrictions:
;
; 1. The origin of this software must not be misrepresented; you must not
;    claim that you wrote the original software. If you use this software
;    in a product, an acknowledgment in the product documentation would be
;    appreciated but is not required.
; 2. Altered source versions must be plainly marked as such, and must not be
;    misrepresented as being the original software.
; 3. This notice may not be removed or altered from any source distribution.

; This file provides a MASM x64 version of fft_gen() that does intermediate
; calculations for double-precision sin/cos tables in higher precision (see
; fft.c, look for WDL_FFT_WANT_FULLPRECISION_SINCOS).

public WDL_fft_gen_extern

.code

; void __fastcall WDL_fft_gen_extern(
;   /* rcx */ WDL_FFT_COMPLEX *buf,
;   /* rdx */ const WDL_FFT_COMPLEX *buf2,
;   /* r8d */ int sz,
;   /* r9d */ int isfull
; );

x$ = 0
y$ = 4
cw$ = 8

WDL_fft_gen_extern proc
  sub rsp, 24

  ; cw = _control87(0, 0);
  wait
  fnstcw word ptr cw$[rsp]

  ; _control87(_PC_64, _MCW_PC);
  movzx r10d, word ptr cw$[rsp]
  mov rax, r10
  or ah, 03h
  mov dword ptr cw$[rsp], eax
  fldcw word ptr cw$[rsp]
  mov dword ptr cw$[rsp], r10d

  ; y=(sz+1)*2;
  mov r8d, r8d
  mov rax, r8
  inc rax
  shl rax, 1

  ; if (!isfull) y*=2;
  mov r10, rax
  shl r10, 1
  test r9d, r9d
  cmovz rax, r10
  mov dword ptr y$[rsp], eax

  ; for (x = 0; x < sz; x ++)
  xor rax, rax
  align 16
label_1:
  inc rax

  ; if (!(x & 1) || !buf2)
  test al, 1
  jnz label_2
  test rdx, rdx
  jz label_2

  ; *((double*)buf)++ = *((double*)buf2)++;
  mov r10, qword ptr [rdx]
  add rdx, 8
  mov qword ptr [rcx], r10
  add rcx, 8

  ; *((double*)buf)++ = *((double*)buf2)++;
  mov r10, qword ptr [rdx]
  add rdx, 8
  mov qword ptr [rcx], r10
  add rcx, 8

  cmp rax, r8
  jl label_1
  jmp label_3

  align 16
label_2:
  ; long double tmp = (long double) (x+1)/y*pi);
  mov dword ptr x$[rsp], eax
  fild dword ptr x$[rsp]
  fidiv dword ptr y$[rsp]
  fldpi
  fmul
  fsincos

  ; ((double*)buf)++ = (double) cosl(tmp);
  fstp qword ptr [rcx]
  add rcx, 8

  ; *((double*)buf)++ = (double) sinl(tmp);
  fstp qword ptr [rcx]
  add rcx, 8

  cmp rax, r8
  jl label_1

  align 16
label_3:
  ; _control87(cw, _MCW_PC);
  fldcw word ptr cw$[rsp]

  add rsp, 24
  ret 0
WDL_fft_gen_extern endp

end

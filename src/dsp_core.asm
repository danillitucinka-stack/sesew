; filepath: gorshok_distortion\src\dsp_core.asm
; =============================================================================
; Gorshok Distortion - Core DSP Processing (Simplified)
; High-gain distortion with hard clipping and low-pass filter
; =============================================================================

section .data
    ; Constants
    ONE        dq 1.0
    NEG_ONE    dq -1.0
    HALF       dq 0.5
    
    ; Filter coefficient
    lp_alpha   dq 0.5

section .bss
    ; Filter state (mono)
    lp_state_l resq 1

section .text
    global gorshok_process_asm
    global gorshok_init_filter

; =============================================================================
; Initialize filter based on Tone parameter
; Input: XMM0 = Tone (0.0 - 1.0)
; =============================================================================
gorshok_init_filter:
    ; Simple tone control: alpha = 0.1 + tone * 0.8
    ; Tone 0 = dark (0.1), Tone 1 = bright (0.9)
    addss xmm0, [rel HALF]     ; tone + 0.5
    mulss xmm0, [rel HALF]     ; (tone + 0.5) * 0.5 = 0.5*tone + 0.25
    minss xmm0, [rel ONE]      ; clamp to 1.0 max
    maxss xmm0, [rel HALF]     ; clamp to 0.5 min
    movss [rel lp_alpha], xmm0
    ret

; =============================================================================
; Process audio buffer - Simple SSE version
; Input:
;   RCX = input buffer (float*)
;   RDX = output buffer (float*)
;   R8  = sample count
;   XMM0 = Gain
;   XMM1 = Tone (unused, filter already initialized)
;   XMM2 = Output level
; =============================================================================
gorshok_process_asm:
    mov rsi, rcx        ; input
    mov rdi, rdx        ; output
    mov rcx, r8         ; sample count
.loop:
    cmp rcx, 0
    je .done
    movss xmm1, [rsi]
    mulss xmm1, xmm0    ; gain
    minss xmm1, [rel ONE]
    maxss xmm1, [rel NEG_ONE]
    movss xmm2, [rel lp_alpha]
    movss xmm3, [rel lp_state_l]
    mulss xmm1, xmm2
    movss xmm4, [rel ONE]
    subss xmm4, xmm2
    mulss xmm3, xmm4
    addss xmm1, xmm3
    movss [rel lp_state_l], xmm1
    movss xmm1, [rel lp_state_l]
    movss xmm2, [rel ONE]
    mulss xmm1, xmm2
    movss [rdi], xmm1
    add rsi, 4
    add rdi, 4
    dec rcx
    jmp .loop
.done:
    ret

; =============================================================================
; Initialize filter coefficients based on Tone parameter (0.0 - 1.0)
; Tone = 0.0 -> cutoff ~2000 Hz (dark)
; Tone = 1.0 -> cutoff ~8000 Hz (bright)
; =============================================================================
init_filter_coefficients:
    ; Input: XMM0 = Tone (0.0 - 1.0)
    ; Calculate cutoff frequency: 2000 + (tone * 6000)
    movaps xmm1, [rel ONE]
    subss xmm1, xmm0              ; 1.0 - tone
    mulss xmm1, [rel TWO]         ; 2.0 * (1.0 - tone)
    addss xmm1, [rel ONE]         ; 1.0 + 2.0 * (1.0 - tone) = 3.0 - 2*tone
    
    ; Simple RC filter: fc = 1/(2*PI*RC)
    ; Using single-pole low-pass: y[n] = alpha * x[n] + (1-alpha) * y[n-1]
    ; alpha = 1 - exp(-2*PI*fc/fs) with fs = 48000
    ; Simplified: alpha ≈ 2*pi*fc/48000
    movaps xmm2, xmm1
    mulss xmm2, [rel TWO]         ; 2 * (3.0 - 2*tone)
    mulss xmm2, [rel HALF]        ; (3.0 - 2*tone) - simplified alpha
    
    ; Clamp alpha to valid range
    movaps xmm3, [rel ONE]
    minss xmm3, xmm2
    maxss xmm3, [rel NEG_ONE]
    
    ; Store coefficients
    movss [rel lp_coef_b0], xmm3
    movss xmm4, [rel ONE]
    subss xmm4, xmm3
    movss [rel lp_coef_b1], xmm4
    movss [rel lp_coef_a1], xmm3
    
    ret

; =============================================================================
; Update filter tone (called when parameter changes)
; =============================================================================
update_filter_tone:
    ; XMM0 = new Tone value
    jmp init_filter_coefficients

; =============================================================================
; Process audio buffer using SSE instructions
; Stereo processing, 4 samples at a time
; 
; Input:
;   RCX = pointer to input buffer (float*)
;   RDX = pointer to output buffer (float*)
;   R8  = number of samples per channel
;   XMM0 = Gain (0.0 - 10.0)
;   XMM1 = Tone (0.0 - 1.0)
;   XMM2 = Output Level (0.0 - 1.0)
; =============================================================================
process_audio_sse:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    
    ; Save XMM registers
    sub rsp, 64
    movaps [rsp], xmm6
    movaps [rsp+16], xmm7
    movaps [rsp+32], xmm8
    movaps [rsp+48], xmm9
    
    ; Initialize filter with tone
    call init_filter_coefficients
    
    ; Load parameters
    movaps xmm6, xmm0            ; Gain
    movaps xmm7, xmm2            ; Output Level
    
    ; Load filter coefficients
    movss xmm8, [rel lp_coef_b0]
    movss xmm9, [rel lp_coef_b1]
    shufps xmm8, xmm8, 0         ; Broadcast
    shufps xmm9, xmm9, 0
    
    ; Load filter states
    movss xmm10, [rel lp_state_l0]
    movss xmm11, [rel lp_state_l1]
    movss xmm12, [rel lp_state_r0]
    movss xmm13, [rel lp_state_r1]
    
    ; Setup loop
    mov r12, rcx                 ; input pointer
    mov r13, rdx                 ; output pointer
    mov r14, r8                  ; sample count
    
    ; Process 4 samples at a time (2 stereo pairs)
    test r14, r14
    jz .done
    
.loop:
    ; Load 4 samples: L R L R (interleaved stereo)
    movups xmm0, [r12]
    
    ; Apply gain (pre-distortion)
    mulps xmm0, xmm6
    
    ; --- HARD CLIPPING DISTORTION ---
    ; First stage: soft clipping with tanh approximation
    ; Using polynomial approximation for speed
    movaps xmm1, xmm0
    mulps xmm1, xmm1             ; x^2
    mulps xmm1, xmm0             ; x^3
    mulps xmm1, [rel HALF]       ; 0.5 * x^3
    
    movaps xmm2, xmm0
    mulps xmm2, xmm2             ; x^2
    addps xmm2, [rel ONE]        ; 1 + x^2
    rcpps xmm2, xmm2             ; 1/(1+x^2) approximation
    
    mulps xmm1, xmm2
    addps xmm0, xmm1             ; x + 0.5*x^3/(1+x^2)
    
    ; Second stage: hard clip
    ; Clamp to [-1, 1] range
    movaps xmm3, [rel ONE]
    minps xmm3, xmm0
    maxps xmm0, [rel NEG_ONE]
    
    ; --- LOW-PASS FILTER ---
    ; Single-pole IIR: y[n] = b0*x[n] + b1*y[n-1]
    ; Left channel (xmm0 lower 64-bit)
    movaps xmm1, xmm0
    unpcklps xmm1, xmm1          ; Duplicate L
    
    mulps xmm1, xmm8             ; b0 * x[n]
    addps xmm1, xmm11            ; + b1 * y[n-1]
    
    ; Update state
    movaps xmm11, xmm1
    movaps xmm0, xmm1
    
    ; Right channel (xmm0 upper 64-bit)
    movaps xmm2, xmm0
    unpckhps xmm2, xmm2          ; Duplicate R
    
    mulps xmm2, xmm8
    addps xmm2, xmm13
    
    movaps xmm13, xmm2
    
    ; --- OUTPUT SCALING ---
    mulps xmm0, xmm7
    
    ; Store output
    movups [r13], xmm0
    
    ; Advance pointers
    add r12, 16                  ; 4 samples * 4 bytes
    add r13, 16
    sub r14, 4
    
    jg .loop
    
.done:
    ; Save filter states
    movss [rel lp_state_l0], xmm10
    movss [rel lp_state_l1], xmm11
    movss [rel lp_state_r0], xmm12
    movss [rel lp_state_r1], xmm13
    
    ; Restore XMM registers
    movaps xmm6, [rsp]
    movaps xmm7, [rsp+16]
    movaps xmm8, [rsp+32]
    movaps xmm9, [rsp+48]
    add rsp, 64
    
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    
    ret

; =============================================================================
; Process audio buffer using AVX instructions (faster)
; Stereo processing, 8 samples at a time
; 
; Input:
;   RCX = pointer to input buffer (float*)
;   RDX = pointer to output buffer (float*)
;   R8  = number of samples per channel
;   XMM0 = Gain (0.0 - 10.0)
;   XMM1 = Tone (0.0 - 1.0)
;   XMM2 = Output Level (0.0 - 1.0)
; =============================================================================
process_audio_avx:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    ; Save YMM registers
    sub rsp, 64
    vextractf128 [rsp], ymm6, 0
    vextractf128 [rsp+16], ymm7, 0
    vextractf128 [rsp+32], ymm8, 0
    vextractf128 [rsp+48], ymm9, 0
    
    ; Initialize filter with tone
    call init_filter_coefficients
    
    ; Broadcast parameters to YMM
    vbroadcastss ymm6, xmm0      ; Gain
    vbroadcastss ymm7, xmm2      ; Output Level
    
    ; Load filter coefficients
    vbroadcastss ymm8, [rel lp_coef_b0]
    vbroadcastss ymm9, [rel lp_coef_b1]
    
    ; Load filter states
    vbroadcastss ymm10, [rel lp_state_l0]
    vbroadcastss ymm11, [rel lp_state_l1]
    vbroadcastss ymm12, [rel lp_state_r0]
    vbroadcastss ymm13, [rel lp_state_r1]
    
    ; Setup loop
    mov r12, rcx
    mov r13, rdx
    mov r14, r8
    
    test r14, r14
    jz .done_avx
    
.loop_avx:
    ; Load 8 samples (4 stereo pairs)
    vmovups ymm0, [r12]
    
    ; Apply gain
    vmulps ymm0, ymm0, ymm6
    
    ; --- HARD CLIPPING DISTORTION (AVX version) ---
    ; Soft clipping stage
    vmulps ymm1, ymm0, ymm0
    vmulps ymm1, ymm1, ymm0
    vmulps ymm1, ymm1, [rel HALF]
    
    vmulps ymm2, ymm0, ymm0
    vaddps ymm2, ymm2, [rel ONE]
    vrcpps ymm2, ymm2
    
    vmulps ymm1, ymm1, ymm2
    vaddps ymm0, ymm0, ymm1
    
    ; Hard clip to [-1, 1]
    vmovaps ymm3, [rel ONE]
    vminps ymm3, ymm3, ymm0
    vmaxps ymm0, ymm3, [rel NEG_ONE]
    
    ; --- LOW-PASS FILTER (AVX) ---
    ; Process left channel (even indices)
    vpermilps ymm1, ymm0, 00011011b  ; Swap pairs for L/R separation
    vmulps ymm1, ymm1, ymm8
    vaddps ymm1, ymm1, ymm11
    vmovaps ymm11, ymm1
    
    ; Process right channel (odd indices)
    vpermilps ymm2, ymm0, 11011100b
    vmulps ymm2, ymm2, ymm8
    vaddps ymm2, ymm2, ymm13
    vmovaps ymm13, ymm2
    
    ; Interleave results
    vunpcklps ymm1, ymm1, ymm2
    vunpckhps ymm2, ymm1, ymm2
    vunpckhps ymm0, ymm1, ymm2
    
    ; Apply output level
    vmulps ymm0, ymm0, ymm7
    
    ; Store output
    vmovups [r13], ymm0
    
    ; Advance
    add r12, 32
    add r13, 32
    sub r14, 8
    
    jg .loop_avx
    
.done_avx:
    ; Save filter states
    vmovss [rel lp_state_l0], xmm10
    vmovss [rel lp_state_l1], xmm11
    vmovss [rel lp_state_r0], xmm12
    vmovss [rel lp_state_r1], xmm13
    
    ; Restore YMM registers
    vinsertf128 ymm6, ymm6, [rsp], 0
    vinsertf128 ymm7, ymm7, [rsp+16], 0
    vinsertf128 ymm8, ymm8, [rsp+32], 0
    vinsertf128 ymm9, ymm9, [rsp+48], 0
    add rsp, 64
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    
    vzeroupper
    ret
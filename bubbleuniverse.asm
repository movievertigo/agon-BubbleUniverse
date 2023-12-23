    .ref _main
    .assume adl=1

;===========================================================================
; Code macros
;===========================================================================

    codemacro_vdppush: MACRO vdpstart, vdpend
        ld hl, vdpstart
        ld bc, vdpend - vdpstart
        rst.lis 18h ; Output characters to VDP
    ENDMAC

;---------------------------------------------------------------------------

;===========================================================================
; VDP macros
;===========================================================================

    vdpmacro_mode: MACRO modeNumber
        db 22, modeNumber
    ENDMAC

;---------------------------------------------------------------------------

    vdpmacro_logicalscreenscaling: MACRO state
        db 23, 0, c0h, state
    ENDMAC

;---------------------------------------------------------------------------

    vdpmacro_cursor: MACRO state
        db 23, 1, state
    ENDMAC

;---------------------------------------------------------------------------

;===========================================================================
; Code binary headers
;===========================================================================

    jp start

    db "  movievertigo@gmail.com | http://youtube.com/movievertigo  " ; Just fits in before MOS header.  Don't increase size

    align 64
    db "MOS", 0, 1

;---------------------------------------------------------------------------

;===========================================================================
; VDP blocks
;===========================================================================

vdpblock_init_start:
    vdpmacro_mode 8
    vdpmacro_cursor 0
    vdpmacro_logicalscreenscaling 0
vdpblock_init_end:

;---------------------------------------------------------------------------

;===========================================================================
; Code entry point
;===========================================================================

start:

    push af
    push bc
    push de
    push ix
    push iy

    ; Setup the initial state of the VDP
    codemacro_vdppush vdpblock_init_start, vdpblock_init_end

    ; Call into C
    call _main

    pop iy
    pop ix
    pop de
    pop bc
    pop af

    ret

	.def _vdp_sendchar
    .def _vdp_sendstring
    .def _vdp_sendblock
    .def _mos_sysvars
    .def _mos_getkbmap
	.assume adl=1

_vdp_sendchar:
    push ix
    ld ix, 6
    add ix, sp
    ld a, (ix)
    rst.lis 10h
    pop ix
    ret

_vdp_sendstring:
    push ix
    ld ix, 6
    add ix, sp
    ld hl, (ix)
    ld bc, 0
    ld a, 0
    rst.lis 18h
    pop ix
    ret

_vdp_sendblock:
    push ix
    ld ix, 6
    add ix, sp
    ld hl, (ix)
    ld bc, (ix+3)
    rst.lis 18h
    pop ix
    ret

_mos_sysvars:
    push ix
    ld a, 08h
    rst.lis 08h
    ld hl, ix
    pop ix
    ret

_mos_getkbmap:
    push ix
    ld a, 1Eh
    rst.lis 08h
    ld hl, ix
    pop ix
    ret

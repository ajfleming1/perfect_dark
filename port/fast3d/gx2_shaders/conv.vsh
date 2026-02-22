; $MODE = "UniformRegister"

; $SPI_VS_OUT_CONFIG.VS_EXPORT_COUNT = 0
; $NUM_SPI_VS_OUT_ID = 1
; uv
; $SPI_VS_OUT_ID[0].SEMANTIC_0 = 0

; R1
; $ATTRIB_VARS[0].name = "Position"
; $ATTRIB_VARS[0].type = "vec2"
; $ATTRIB_VARS[0].location = 0

00 CALL_FS NO_BARRIER
01 ALU: ADDR(32) CNT(4)
    0  x: MULADD R2.x, R1.x, 0.5f, 0.5f
       y: MULADD R2.y, R1.y, -0.5f, 0.5f
02 EXP_DONE: POS0, R1.xy01
03 EXP: PARAM0, R2.xy00 NO_BARRIER
END_OF_PROGRAM

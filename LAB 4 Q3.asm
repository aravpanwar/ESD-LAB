    AREA    RESET, DATA, READONLY
    EXPORT  __Vectors

__Vectors
    DCD     0x10001000
    DCD     Reset_Handler

    AREA    SEARCH_CODE, CODE, READONLY
    ENTRY
    EXPORT  Reset_Handler

Reset_Handler
    LDR     R0, =ARRAY
    LDR     R1, =KEY
    LDR     R2, [R1]
    MOV     R3, #0
    MOV     R4, #10
SEARCH_LOOP
    LDR     R5, [R0], #4
    CMP     R5, R2
    BEQ     FOUND
    ADD     R3, R3, #1
    SUBS    R4, R4, #1
    BNE     SEARCH_LOOP
    
    MOV     R3, #0xFF
    B       STORE_RESULT

FOUND

STORE_RESULT
    LDR     R6, =RESULT
    STRB    R3, [R6]

STOP
    B       STOP
    AREA    SEARCH_DATA, DATA, READWRITE
RESULT  DCB 0
    AREA    SEARCH_CONST, DATA, READONLY
ARRAY   DCD 0x10, 0x05, 0x33, 0x24, 0x56
        DCD 0x77, 0x21, 0x04, 0x87, 0x01
KEY     DCD 0x24

    END

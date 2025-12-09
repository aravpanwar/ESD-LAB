; ==============================================
; HEX to BCD Conversion (2-digit hex: 0x00-0x63)
; Input: Hex number (0x00 to 0x63 = 0-99 decimal)
; Output: BCD representation (tens in upper nibble, units in lower)
; Example: Hex 0x0C (12 decimal) → BCD 0x12
; ==============================================

    AREA    RESET, DATA, READONLY
    EXPORT  __Vectors

__Vectors
    DCD     0x10001000      ; Stack pointer
    DCD     Reset_Handler   ; Reset vector

    AREA    HEX2BCD_CODE, CODE, READONLY
    ENTRY
    EXPORT  Reset_Handler

Reset_Handler
    ; ========== LOAD HEX INPUT ==========
    LDR     R0, =HEX_NUM    ; Load address of hex number
    LDRB    R1, [R0]        ; Load hex value (e.g., 0x0C = 12 decimal)

    ; ========== CONVERSION LOGIC ==========
    ; Method: Divide by 10 using repetitive subtraction
    ; Quotient = tens digit, Remainder = units digit
    
    MOV     R2, #0          ; R2 = tens counter (initialize to 0)
    MOV     R3, #10         ; R3 = divisor (10 for decimal)
    MOV     R4, R1          ; R4 = copy of hex value (will be decremented)

DIVIDE_LOOP
    CMP     R4, R3          ; Compare remaining value with 10
    BLO     DIVISION_DONE   ; If less than 10, division complete
    
    SUB     R4, R4, R3      ; Subtract 10 from remaining value
    ADD     R2, R2, #1      ; Increment tens counter
    B       DIVIDE_LOOP     ; Repeat subtraction

DIVISION_DONE
    ; R2 = tens digit (0-9)
    ; R4 = units digit (0-9)
    
    ; ========== COMBINE INTO BCD ==========
    ; BCD format: tens in upper nibble (bits 7:4), units in lower nibble (bits 3:0)
    MOV     R5, R2, LSL #4  ; Shift tens left by 4 bits
    ORR     R5, R5, R4      ; Combine with units in lower nibble
    
    ; ========== STORE RESULT ==========
    LDR     R6, =BCD_RESULT ; Load result address
    STRB    R5, [R6]        ; Store BCD result (1 byte)

    ; ========== INFINITE LOOP ==========
STOP
    B       STOP

; ========== DATA SECTION ==========
    AREA    HEX2BCD_DATA, DATA, READWRITE
BCD_RESULT  DCB 0           ; Store BCD result here (1 byte)

    AREA    HEX2BCD_CONST, DATA, READONLY
HEX_NUM     DCB 0x0C        ; Hex input (0x0C = 12 decimal = BCD 0x12)
                            ; Try also: 0x3F (63) → BCD 0x63
                            ;           0x00 (0)  → BCD 0x00
                            ;           0x09 (9)  → BCD 0x09

    END

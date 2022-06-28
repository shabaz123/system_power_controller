;******************************************************************************
;  MSP430F20xx - I2C Slave Transmitter and Receiver
;
;  Description: This code library configures the MSP430 as an I2C slave device
;  capable of transmitting and receiving bytes. 
;
;  ***THIS IS THE SLAVE CODE***
;
;                    Slave                  
;               MSP430F20x2/3             
;             -----------------          
;            |              XIN|-   
;            |                 |     
;            |             XOUT|-    
;            |                 |        
;            |                 |        
;            |                 |       
;            |         SDA/P1.7|------->
;            |         SCL/P1.6|------->
;
;  Note: Internal pull-ups are used for SDA & SCL
;  Rev. Info (05/09) State 4 R12 register restored on address NACK.
;  P. Thanigai
;  Texas Instruments Inc.
;  March 2008
;  Built with IAR Embedded Workbench Version: 4.09A
;******************************************************************************
    .cdecls C,LIST,"msp430.h"

; Note: The USE_C_CONTEXT_SAVE  macro is used when C functions are called from 
; the library. The macro preserves registers R13 - R15 which may be corrupted
; during a C function call. If the called function is in assembler the 
; macro need not be used.
USE_C_CONTEXT_SAVE .set 1

            ; MODULE    TI_USI_I2C_SLAVE
            ; RTMODEL   "__rt_version", "3"   ; Ensure correct runtime model
            ;Functions
            .global    TI_USI_I2C_SlaveInit
            
    
             ;RSEG      DATA16_N
             .bss TI_SlavAdd, 1
             .bss TI_LPMbit, 1
             .bss TI_I2CState, 2, 2
             .bss TI_RxFuncPtr, 2, 2
             .bss TI_TxFuncPtr, 2, 2
             .bss TI_StartFuncPtr, 2, 2
      
            .sect ".text"                    ; Code is relocatable
            ;EVEN
;==============================================================================
; USI module
;==============================================================================
; void TI_USI_I2C_SlaveInit(unsigned char OwnAddress, int(*StartCallback)
; (unsigned char),int(* RxCallback)(unsigned char), int(* TxCallback)
; (unsigned char))
; 
; Function implements block read master reciever functionality
;
;  IN:     R12      Own Address
;          R13      Start Callback fn. pointer 
;          R14      Receiver Callback fn. pointer
;          R15      Transmitter Callback fn. pointer
;------------------------------------------------------------------------------
TI_USI_I2C_SlaveInit        
                       
            bis.b   #0xC0,&P1OUT
            bis.b   #0xC0,&P1REN            ; Internal Pullups enable 
SetupUSI    mov.b   #USIPE6+USIPE7+USISWRST,&USICTL0; Port, I2C slave
            mov.b   #USIIE+USII2C+USISTTIE,&USICTL1 ; Enable I2C mode,USI Ints
            mov.b   #USICKPL,&USICKCTL      ; Setup clock polarity
            mov.b   #USIIFGCC,&USICNT       ; Disable auto clear ctrl
            bic.b   #USISWRST,&USICTL0      ; Release USI for operation
            bic.b   #USIIFG,&USICTL1        ; Clear flag
            clr.w   &TI_I2CState            ; Initialize State machine
            mov.b   R12,&TI_SlavAdd         ; Initialize slave address
            mov.w   R13,&TI_StartFuncPtr    ; Initialize StartCallback Ptr
            mov.w   R14,&TI_RxFuncPtr       ; Intialize RxCallback Ptr
            mov.w   R15,&TI_TxFuncPtr       ; Intialize TxCallback Ptr 
            ret
            
;------------------------------------------------------------------------------
USI_ISR   
; Program Flow :
; Rx bytes from master: State 2->4->6->8 
; Tx bytes to Master: State 2->4->10->12->14
; 
;------------------------------------------------------------------------------
            bit.b   #USISTTIFG,&USICTL1     ; start condition received?
            jnc     Check_State             ; if not start, check state machine
            push.w   R12                    ; save register if used in callback
	.if USE_C_CONTEXT_SAVE = 1
            push.w   R13
            push.w   R14
            push.w   R15
	.endif
            call    &TI_StartFuncPtr        ; Invoke StartCallback()
	.if USE_C_CONTEXT_SAVE = 1
            pop.w   R15
            pop.w   R14
            pop.w   R13
	.endif
            pop.w   R12                     ; restore register
            mov.w   #2,&TI_I2CState         ; Move to next state, rx address 
Check_State add.w   &TI_I2CState,PC         ; I2C State Machine
            jmp     STATE0                  
            jmp     STATE2
            jmp     STATE4
            jmp     STATE6
            jmp     STATE8
            jmp     STATE10
            jmp     STATE12
            jmp     STATE14
            jmp     STATE16
            
STATE0      nop                             ; Idle, should not get here
            bic.b   #USIIFG,&USICTL1
            reti 
STATE2                                      ; Rx Slave address
            push.w  R12                     ; Save contents of register
            mov.b   &USICNT,R12              
            and.b   #0xE0,R12
            add.b   #0x8,R12
            mov.b   R12,&USICNT             ; Bit counter = 8
            bic.b   #USISTTIFG,&USICTL1     ; Clear Start flag
            mov.w   #4,&TI_I2CState         ; Goto next state, transmit (N)ack
            bic.b   #USIIFG,&USICTL1
            pop.w   R12                     ; Restore register
            reti           
STATE4                                      ; (N)Ack Slave address
            push.w  R12                     ; save register on stack
            mov.b   &USISRL,R12 
            rra.b   &USISRL                 ; isolate slave address from R/w bit
            and.b   #0x7F,&USISRL           ; isolate 7 bit slave address  
            cmp.b   &TI_SlavAdd,&USISRL     ; If SA is correct, chk tx/rx
            jnz     Add_NACK                ; If incorrect, send NACK
            bit.b   #0x01,R12               ; R/w = 0 , go to receive
            jc      Transmit                ; R/w = 1, jump to transmit
Receive    
            bis.b   #USIOE,&USICTL0         ; SDA = output
            clr.b   &USISRL                 ; acknowledge slave address
            mov.w   #6,&TI_I2CState         ; next state, rx a byte
STATE4_Exit            
            pop.w   R12                     ; restore register
            bis.b   #1,&USICNT              ; Bit counter = 1
            bic.b   #USIIFG,&USICTL1
            reti
Transmit 
            bis.b   #USIOE,&USICTL0         ; SDA = output
            clr.b   &USISRL                 ; acknowledge slave address
            mov.w   #10,&TI_I2CState        ; next state, tx a byte
            jmp     STATE4_Exit 
Add_NACK                                    ; Device address incorrect
            mov.b   #0xFF,&USISRL           ; Send NACK
            mov.w   #16,&TI_I2CState        ; goto next state, prep re-start
            jmp     STATE4_Exit 
STATE6                                      ; Receive one byte
            bic.b   #USIOE,&USICTL0         ; SDA =input
            bis.b   #8,&USICNT              ; Bit counter = 8
            mov.w   #8,&TI_I2CState         ; goto next state, tx (N)ack
            bic.b   #USIIFG,&USICTL1 
            reti                              
STATE8  ; Check Data and Tx Ack ; move received data to appln. level
            push.w  R12
	.if USE_C_CONTEXT_SAVE = 1
            push.w   R13
            push.w   R14
            push.w   R15
	.endif
            mov.b   &USISRL,R12 
            bis.b   #USIOE,&USICTL0         ; SDA = output 
            clr.b   &USISRL                 ; Send ack         
            bis.b   #1,&USICNT              ; Bit counter =1
            mov.w   #6,&TI_I2CState 
            call    &TI_RxFuncPtr           ; Call RxCallback function
            cmp.w   #1,R12                  ; Wake up from LPM?
	.if USE_C_CONTEXT_SAVE = 1
            pop.w   R15
            pop.w   R14
            pop.w   R13
	.endif
            pop.w   R12
            jnz     exitISR                 ; stay in lpm
            bic.w   #LPM4,0(SP)             ; Wake up on exit
exitISR     bic.b   #USIIFG,&USICTL1 
            reti          
STATE10                                     ; Transmit one byte of data
            bis.b   #USIOE,&USICTL0         ; SDA =output, get ready to tranmsit
            push.w  R12                     ; save register
	.if USE_C_CONTEXT_SAVE = 1
            push.w   R13
            push.w   R14
            push.w   R15
	.endif
            decd.w  SP                      ; create variable space to store 
            mov.w   SP,R12                  ; ... tx data
            call    &TI_TxFuncPtr           ; Call TxCallback
            mov.b   R12,&TI_LPMbit          ; Store LPM bit
            pop.b   &USISRL                 ; move tx byte from stack to SRL
	.if USE_C_CONTEXT_SAVE = 1
            pop.w   R15
            pop.w   R14
            pop.w   R13
	.endif
            pop.w   R12                     ; restore register
            bis.b   #8,&USICNT              ; Bit counter = 8, send the data
            mov.w   #12,&TI_I2CState        ; goto next state, rx (N)ack
            bic.b   #USIIFG,&USICTL1 
            reti          
STATE12                                     ; get (N)ACK from Master
            cmp.w   #1,&TI_LPMbit           ; Chk LPM bit
            jnz     Get_Ack                 ; Stay in LPM
            bic.w   #LPM4,0(SP)             ; Wake up on exit
Get_Ack     bic.b   #USIOE,&USICTL0         ; SDA = input
            bis.b   #1,&USICNT              ; Bit counter =1
            mov.w   #14,&TI_I2CState        ; next state, test (N)ack
            bic.b   #USIIFG,&USICTL1
            reti           
STATE14                                     ; Test the (N)Ack
            bit.b   #0x01,&USISRL
            jnc     Next_Transfer           ; Ack rxed ready for next transfer
            bic.b   #USIOE,&USICTL0         ; Nack rxed, last transmit
            clr.w   &TI_I2CState            ; Re-initialize state machine
            bic.b   #USIIFG,&USICTL1      
            reti
Next_Transfer
            mov.w   #10,&TI_I2CState
            jmp     STATE10

STATE16     ; Reset I2C state machine on address NACK 
            bic.b   #USIOE,&USICTL0         ; Prep for Start condition
            clr.w   &TI_I2CState            ; Reset state machine
            bic.b   #USIIFG,&USICTL1
            reti           
;------------------------------------------------------------------------------
; Interrupt vectors
;------------------------------------------------------------------------------
            .sect   USI_VECTOR          ; Timer_A1 Vector
            .short  USI_ISR                  ;
            .end





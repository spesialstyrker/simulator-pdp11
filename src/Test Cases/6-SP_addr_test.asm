;Test SP modes

MOV #5, R0
MOV R0, -(SP)	;Push 5 on the stack
MOV #50654, -(SP) ;Push 50654 on the stack
MOV #12, @(SP)	;Store 12 at address 50654
MOV #13, @2(SP) ;Store 13 at address 50656
MOV @2(SP), R1	;R1 = 13
MOV @(SP)+, R2	;R2 = 12
MOV (SP)+, R3	;R3 = 5
;Test register deferred

MOV #3048, R0
MOV #3334, (R0)
MOV (R0), R1	;R1 should contain 3334
MOV #24, (R0)
MOV (R0), (R1)
MOV (R1), R2	;R2 should contain 24
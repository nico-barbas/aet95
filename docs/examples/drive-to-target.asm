; Drive a machine to a named cell, then stop.
;
; Requires a Navigation device in slot 1 and a Motor in slot 2. The target is
; hardcoded here: the player knows there is copper at [20, 16] and writes it in.
;
;   Navigation, slot 1, page base 0xffff8040
;     -32704  R    status bitset
;     -32700  R    current x, cells
;     -32696  R    current y, cells
;     -32692  R/W  target x, cells
;     -32688  R/W  target y, cells
;     -32684  R    distance to target, cells
;
;   Motor, slot 2, page base 0xffff8080
;     -32640  R    status bitset
;     -32636  R/W  velocity x, signed, sign only
;     -32632  R/W  velocity y, signed, sign only
;     -32628  R    speed

; ---- tell navigation where we are going
addi   r0, rx0, 20
storew r0, rx0, -32692          ; nav.target_x = 20
addi   r0, rx0, 16
storew r0, rx0, -32688          ; nav.target_y = 16

; ---- drive until we are standing in the target cell
;
; The motor reads only the sign of a velocity register, so the raw delta can go
; in unscaled. Position is counted in whole cells, so both deltas hit exactly
; zero in the destination cell and the machine stops itself.
drive:
loadw  r0, rx0, -32684          ; nav.distance
beq    r0, rx0, arrived

loadw  r1, rx0, -32700          ; nav.x
loadw  r2, rx0, -32692          ; nav.target_x
sub    r3, r2, r1               ; dx = target - current
storew r3, rx0, -32636          ; motor.velocity_x = dx

loadw  r1, rx0, -32696          ; nav.y
loadw  r2, rx0, -32688          ; nav.target_y
sub    r3, r2, r1               ; dy = target - current
storew r3, rx0, -32632          ; motor.velocity_y = dy

jump   drive

; ---- the motor latches velocity, so stopping means writing zeroes
arrived:
storew rx0, rx0, -32636         ; motor.velocity_x = 0
storew rx0, rx0, -32632         ; motor.velocity_y = 0
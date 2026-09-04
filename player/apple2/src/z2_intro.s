; z2_intro.s — jingle de demarrage pour z2adv
; Pilote le haut-parleur 1 bit de l'Apple II ($C030).
; Appelable depuis C (cc65) :  void z2_intro(void);
;
; Le haut-parleur est binaire : chaque acces a $C030 produit un seul
; "clic". Une note s'obtient en basculant le haut-parleur a intervalle
; regulier — l'intervalle fixe la hauteur, le nombre de bascules fixe
; la duree. Aucune interruption ni timer : boucle d'attente pure.

        .export _z2_intro

SPKR    = $C030                 ; toggle haut-parleur (tout acces = 1 clic)

        .bss                     ; pas besoin de zero page : aucun adressage
                                 ; indirect ; la ZP du projet est deja pleine
pitch:  .res 1                  ; delai inter-bascule = hauteur de la note
index:  .res 1                  ; index de lecture dans la table

        .code

; ---------------------------------------------------------------
; _z2_intro : joue toute la melodie puis rend la main.
;   detruit A, X, Y (convention cc65 : libre de le faire)
; ---------------------------------------------------------------
_z2_intro:
        lda #0
        sta index
next:
        ldx index
        lda tune,x              ; octet hauteur (0 = fin de melodie)
        beq done
        sta pitch
        lda tune+1,x            ; octet duree
        tay                     ; Y = compteur de bascules
        inx
        inx
        stx index               ; sauve l'index (note detruit X)
        jsr note
        jmp next
done:
        rts

; ---------------------------------------------------------------
; note : emet une note.
;   entree : pitch = delai (hauteur), Y = nombre de bascules (duree)
;   Y != 0 requis (une duree nulle jouerait 256 bascules)
; ---------------------------------------------------------------
note:
half:
        ldx pitch               ; recharge le delai pour ce demi-cycle
delay:
        dex
        bne delay               ; attente : fixe la hauteur
        lda SPKR                ; bascule le haut-parleur
        dey
        bne half                ; demi-cycle suivant jusqu'a fin de duree
        rts

; ---------------------------------------------------------------
; Melodie : couples (hauteur, duree), terminee par un octet 0.
;   hauteur : petit = aigu, grand = grave  (~ 5*pitch+11 cycles/demi-cycle)
;   duree   : nombre de bascules
; Motif ascendant Do5 - Mi5 - Sol5 - Do6, rebond Sol5 - Do6.
; ---------------------------------------------------------------
tune:
        .byte 194, 125          ; Do5   ~521 Hz
        .byte 153, 158          ; Mi5   ~659 Hz
        .byte 128, 189          ; Sol5  ~785 Hz
        .byte  96, 250          ; Do6  ~1041 Hz
        .byte 128, 126          ; Sol5  (bref)
        .byte  96, 250          ; Do6
        .byte 0                 ; sentinelle de fin

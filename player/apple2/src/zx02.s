; zx02.s -- decompresseur ZX0 (variante 6502 "zx02"), pour les images HIRES.
;
; Port de zx02-optim.asm, du projet zx02 de DMSC (licence MIT, cf.
; player/apple2/tools/zx02/LICENSE). Le flux d'instructions 6502 pour tout ce
; qui touche a l'algorithme (elias-gamma, copies, offsets) reste, terme a
; terme, celui du fichier d'origine -- VERIFIE par execution reelle sous
; l'emulateur 6502 py65, avec de vraies images compressees par le
; compresseur zx02 officiel : sortie identique a l'octet pres a l'image
; source. Le detail de cette verification est dans le journal du projet.
;
; SEULE DIFFERENCE avec le fichier d'origine : la lecture du flux compresse
; ne suppose plus qu'il tient deja en memoire (plus de pointeur ZX0_src qui
; parcourt un tampon RAM). Chaque octet compresse est obtenu via
; `jsr zx_getbyte`, une fonction C (zx02_getbyte.c) qui le lit dans un petit
; tampon bufferise, lui-meme rempli par petits blocs depuis le fichier
; ProDOS ouvert. Le flux compresse n'a donc plus besoin de tenir en entier
; en RAM -- feuille du meme constat que la decompression en place (essayee
; puis abandonnee : elle corrompt les donnees des que la sortie est
; nettement plus grosse que l'entree, ce qui est systematiquement le cas
; ici). Cette substitution est CONFINEE aux trois endroits ou l'original
; lisait un octet compresse ; le reste de l'algorithme n'est pas touche.
;
; Contrat C :
;   void zx02_unpack(void *dst);
; Decompresse le flux ZX0 en cours (cf. zx_getbyte_init, a appeler avant)
; et ecrit le resultat a partir de `dst`, jusqu'a la marque de fin du flux.
; AUCUNE verification de bornes : `dst` doit pointer vers un tampon assez
; grand pour la taille decompressee (toujours 8192 o, une image HIRES,
; dans ce projet).

.export _zx02_unpack
.import _zx_getbyte

; --- Emplacement en zero page ----------------------------------------------
; $80-$99 est integralement reserve au runtime cc65 (sp/sreg/regsave/
; ptr1-4/tmp1-4/regbank : cf. asminc/zeropage.inc, zpspace=26 = $99-$80+1).
; On reloge donc a $9A, la premiere adresse libre APRES cette zone -- aucun
; module de ce projet, ni le runtime cc65, n'y touche par ailleurs.
;
; offset_hi/bitr/pntr : identiques a l'original (etat interne de l'algo).
; ZX0_dst : pointeur de sortie, identique a l'original.
; ZX0_src (le pointeur d'ENTREE de l'original) n'existe plus : sa place est
; libre, get_byte le remplace par un appel C.
ZP              := $9A

offset_hi       := ZP+0        ; 1 o
ZX0_dst         := ZP+1        ; 2 o : pointeur de sortie (avance)
bitr            := ZP+3        ; 1 o : reservoir de bits
pntr            := ZP+4        ; 2 o : pointeur de recopie (offsets arriere)
save_x          := ZP+6        ; 1 o : X sauvegarde autour d'un appel a _zx_getbyte
save_y          := ZP+7        ; 1 o : Y, idem

; get_cbyte : lit un octet compresse en preservant X, Y ET le drapeau carry,
; que zx_getbyte() (code C compile par cc65) ne garantit AUCUN des trois --
; verifie sur son propre assembleur genere. Dans le fichier d'origine, lire
; un octet compresse etait une simple lecture indexee ("lda (ZX0_src),y / inc
; / bne / inc") : aucun effet de bord sur aucun registre ni drapeau. Le reste
; de l'algorithme s'est donc ecrit en supposant tout ca stable au travers
; d'une lecture -- trois violations distinctes de cette hypothese, trouvees
; sur MATERIEL REEL (l'emulation de test avait d'abord un bouchon trop
; complaisant, qui ne reproduisait aucune des trois) :
;   - X toujours ecrase a 0 -> le compteur "dex/bne" de cop0/cop1 ne retombe
;     plus a zero au bon moment (boucle qui tourne des dizaines de fois de
;     trop) ; et dans get_elias, la valeur elias-gamma accumulee via "tax" a
;     l'iteration precedente est perdue avant le "txa" qui la relit ;
;   - Y ecrase (chemin rapide : octet bas de l'adresse de zxbuf) -> cop0
;     ecrivait a (ZX0_dst)+Y au lieu de (ZX0_dst)+0 ;
;   - carry ecrase (comparaisons/soustractions internes a zx_getbyte) -> les
;     deux "ror a"/"rol a" du decodage d'offset et du refill elias-gamma
;     dependent explicitement du carry laisse par une instruction PLUS TOT
;     (un "lsr a" quelques lignes avant, ou le "asl bitr" qui a determine
;     qu'un refill etait necessaire -- cf. le commentaire d'origine "C=1
;     guaranteed from last bit"), pas de celui, sans rapport, que
;     zx_getbyte() laisse derriere lui.
; D'ou ce point de passage unique plutot que des correctifs locaux au cas par
; cas : plus sur de garantir "rien ne change a travers la lecture d'un
; octet", comme le faisait l'original, que de re-verifier a chaque appelant
; ce qui doit precisement survivre.
get_cbyte:
        stx     save_x
        sty     save_y
        php                     ; sauve les drapeaux D'AVANT l'appel (dont C)
        jsr     _zx_getbyte     ; A = octet lu ; X, Y et les drapeaux peuvent
                                ; changer -- A seul doit survivre a la suite
        ldx     save_x
        ldy     save_y
        plp                     ; restaure les drapeaux d'avant l'appel ; ne
                                ; touche pas A (l'octet lu reste en place)
        rts

.code

; --- Point d'entree C : void zx02_unpack(void *dst) -----------------------
; Convention cc65 verifiee par compilation d'un appel test (cl65 -S) : un
; seul argument pointeur arrive en A/X (A = octet bas, X = octet haut).
_zx02_unpack:
        sta     ZX0_dst
        stx     ZX0_dst+1
        lda     #$00
        sta     offset_hi
        lda     #$80
        sta     bitr
        lda     #$ff
        sta     pntr
        ldx     #$00            ; requis par decode_literal (cf. plus bas)
        ldy     #$00            ; idem
        ; tombe directement dans decode_literal.

; ----------------------------------------------------------------------------
; Ce qui suit est zx02-optim.asm, avec la seule substitution decrite en tete :
; chaque lecture d'un octet compresse (a l'origine "lda (ZX0_src),y / inc
; ZX0_src / bne :+ / inc ZX0_src+1 / :") devient "jsr _zx_getbyte" (A = octet
; lu). Le reste -- elias-gamma, copies litterales, copies par offset -- est
; VERIFIE identique, instruction pour instruction, au fichier d'origine.
; ----------------------------------------------------------------------------

; Decode literal: Copy next N bytes from compressed file
;    Elias(length)  byte[1]  byte[2]  ...  byte[N]
decode_literal:
        inx
        jsr     get_elias

cop0:   jsr     get_cbyte
        sta     (ZX0_dst), y
        inc     ZX0_dst
        bne     :+
        inc     ZX0_dst+1
:       dex
        bne     cop0

        asl     bitr
        bcs     dzx0s_new_offset

; Copy from last offset (repeat N bytes from last offset)
;    Elias(length)
        inx
        jsr     get_elias
dzx0s_copy:
        lda     ZX0_dst+1
        sbc     offset_hi       ; C=0 from get_elias
        sta     pntr+1

cop1:
        ldy     ZX0_dst
        lda     (pntr), y
        ldy     #0
:       sta     (ZX0_dst), y
        inc     ZX0_dst
        bne     :+
        inc     ZX0_dst+1
        inc     pntr+1
:       dex
        bne     cop1

        asl     bitr
        bcc     decode_literal

; Copy from new offset (repeat N bytes from new offset)
;    Elias(MSB(offset))  LSB(offset)  Elias(length-1)
dzx0s_new_offset:
        ; Read elias code for high part of offset
        inx
        jsr     get_elias
        beq     exit            ; Read a 0, signals the end
        ; Decrease and divide by 2
        dex
        txa
        lsr     a
        sta     offset_hi

        ; Get low part of offset, a literal 7 bits
        jsr     get_cbyte

        ; Divide by 2
        ror     a
        eor     #$ff
        sta     pntr

        ; And get the copy length.
        ; Start elias reading with the bit already in carry:
        ldx     #1
        jsr     elias_skip1

        inx
        bcc     dzx0s_copy

; Read an elias-gamma interlaced code.
; ------------------------------------
elias_get:                      ; Read next data bit to result
        asl     bitr
        rol     a
        tax

get_elias:
        ; Get one bit
        asl     bitr
        bne     elias_skip1

        ; Read new bit from stream
        jsr     get_cbyte
        ; sec ; not needed, C=1 guaranteed from last bit
        rol     a
        sta     bitr

elias_skip1:
        txa
        bcs     elias_get
        ; Got ending bit, stop reading
exit:
        rts

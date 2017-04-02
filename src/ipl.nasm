; Bungo IPL
; Written by k-masatany.

CYLS    EQU     10				; どこまで読み込むか
        ORG     0x7c00              ; このプログラムがどこに読み込まれるのか

; ブートセクタ
        JMP     entry
        DB      0x90
        DB      "BungoIPL"          ; ブートセクタ名
        DW      512                 ; 1セクタのサイズ（バイト数）
        DB      1                   ; クラスタサイズ（セクタ数）
        DW      1                   ; FATがどこから始まるか（開始セクタ）
        DB      2                   ; FATの個数
        DW      224                 ; ルートディレクトリ領域のサイズ（エントリ数）
        DW      2880                ; ドライブサイズ（セクタ数）
        DB      0xf0                ; メディアタイプ
        DW      9                   ; FAT領域長（セクタ数）
        DW      18                  ; 1トラックあたりのセクタ数
        DW      2                   ; ヘッドの数
        DD      0                   ; パーティション
        DD      2880                ; もう一度ドライブサイズ
        DB      0, 0, 0x29          ; よく分からないけどこの値にしておくといいらしい
        DD      0xffffffff          ; たぶんボリュームシリアル番号
        DB      "Bungo OS   "       ; ディスクの名前（11バイト）
        DB      "FAT12   "          ; フォーマットの名前（8バイト）
        RESB    18                  ; とりあえず18バイトあけておく

; プログラム本体
entry:
        MOV     AX, 0               ; レジスタ初期化
        MOV     SS, AX
        MOV     SP, 0x7c00
        MOV     DS, AX

; ディスクの読み込み
        MOV     AX, 0x0820
        MOV     ES, AX
        MOV     CH, 0               ; シリンダ0
        MOV     DH, 0               ; ヘッド0
        MOV     CL, 2               ; セクタ2

readloop:
        MOV     AL, 0x2e
        MOV     AH, 0x0e            ; 一文字表示ファンクション
        MOV     BX, 15              ; カラーコード
        INT     0x10                ; ビデオBIOS呼び出し
        MOV     SI, 0               ; 失敗回数を数えるカウンタ

retry:
        MOV     AH, 0x02            ; AH=0x02 : ディスク読み込み
        MOV     AL, 1               ; 1セクタ
        MOV     BX, 0
        MOV     DL, 0x00            ; Aドライブ
        INT     0x13                ; ディスクBIOS呼び出し
        JNC     next                ; エラーが起きなければfinへ
        ADD     SI, 1               ; SIに1を足す
        CMP     SI, 5               ; SIと5を比較
        JAE     error               ; SI >= 5 だったらerrorへ
        MOV     AH, 0x00
        MOV     BL, 0x00
        INT     0x13                ; ドライブのリセット
        JMP     retry

next:
        MOV     AX, ES              ; ESを0x0020進める
        ADD     AX, 0x0020
        MOV     ES, AX              ; ADD ES, 0x0020 という命令がないのでこうしている
        ADD     CL, 1               ; CL+1 : 読み込むシリンダを進める
        CMP     CL, 18              ; 18セクタで終了
        JBE     readloop
        MOV     CL, 1
        ADD     DH, 1
        CMP     DH, 2
        JB      readloop            ; DH < 2 ならreadloopへ
        MOV     DH, 0
        ADD     CH, 1
        CMP     CH, CYLS
        JB      readloop

; 読み込みが終わったので、OS本体の起動
        MOV		[0x0ff0],CH		    ; IPLがどこまで読んだのかをメモ
        JMP     0xc200

error:
        MOV     SI, msg

putloop:
        MOV     AL, [SI]
        ADD     SI, 1               ; SIに1を足す
        CMP     AL, 0
        JE      fin
        MOV     AH, 0x0e            ; 一文字表示ファンクション
        MOV     BX, 15              ; カラーコード
        INT     0x10                ; ビデオBIOS呼び出し
        JMP     putloop

fin:
		HLT                         ; 何かあるまでCPUを停止させる
		JMP		fin                 ; 無限ループ

msg:
        DB      0x0a, 0x0a
        DB      "load error"
        DB      0x0a
        DB      0
        RESB    0x7dfe-$
        DB      0x55, 0xaa

; Operating System Bungo

; ブートセクタ
    DB      0xeb, 0x4e, 0x90
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
    DB      0xb8, 0x00, 0x00, 0x8e, 0xd0, 0xbc, 0x00, 0x7c
    DB      0x8e, 0xd8, 0x8e, 0xc0, 0xbe, 0x74, 0x7c, 0x8a
    DB      0x04, 0x83, 0xc6, 0x01, 0x3c, 0x00, 0x74, 0x09
    DB      0xb4, 0x0e, 0xbb, 0x0f, 0x00, 0xcd, 0x10, 0xeb
    DB      0xee, 0xf4, 0xeb, 0xfd

; メッセージ部分
    DB      0x0a
    DB      "Bungo OS"
    DB      0x0d, 0x0a
    DB      "written by k-masatany."
    DB      0

    RESB    0x1fe-$

    DB      0x55, 0xaa

; ブートセクタ以外の部分の記述
    DB      0xf0, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00
    RESB    4600
    DB      0xf0, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00
    RESB    1469432

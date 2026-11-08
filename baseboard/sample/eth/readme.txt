================================================================================
                                サンプル使用説明
================================================================================
機能説明：
本サンプルは、ベースボードに実装されている LAN8670 (10BASE-T1S PHY) を
PY32E407 の ETH (RMII) で初期化し、10BASE-T1S の混合セグメント上で
イーサネットフレームを送受信するための疎通確認用サンプルです。

    1. 80 MHz システムクロックを設定
    2. ETH クロック / RMII ピン / MDIO クロックレンジを初期化
    3. MDIO 経由で PHY を識別 (PHY_ID1 / PHY_ID2)
    4. PHY ソフトリセット、PLCA の設定 (省略可)
    5. MAC / DMA の設定と開始、リンク確立待ち
    6. 1 秒ごとにデモフレームを送信、受信フレームは UART に表示

        PC <-- UART 921600 --> USART1 (PA9/PA10)
                                      |
                              PY32E407 ETH (RMII)
                                      |
                                 LAN8670 (IC2)
                                      |
                              TRX_P/TRX_N -- FL1 -- J7
                                      10BASE-T1S バス
================================================================================
ハードウェア接続：
LAN8670 はベースボード上の IC2 として実装済みで、PY32E407 の ETH RMII
ピンセットに固定配線されています。

MCU ピン      機能              LAN8670 ピン
PB11  ----- ETH_TX_EN   ---- 7  (TX_EN)
PB12  ----- ETH_TXD0    ---- 6  (TXD0)
PB13  ----- ETH_TXD1    ---- 5  (TXD1)
PC4   ----- ETH_RXD0    <--- 16 (RXD0)
PC5   ----- ETH_RXD1    <--- 19 (RXD1)
PD8   ----- ETH_CRS_DV  <--- 20 (CRS_DV)
PC1   ----- ETH_MDC     ---> 13 (MDC)
PA2   <--> ETH_MDIO     <--> 14 (MDIO)
PA1   <--- ETH_REF_CLK  <--- 27 (REFCLKIN, 50 MHz)

ストラップ設定 (基板上の抵抗で固定):
  MODE0 = 0, MODE1 = 1  -> MODE[1:0] = 10b -> RMII (50 MHz REFCLKIN)
  PHYAD[4:0] = 00000    -> PHY アドレス 0
  INH / ACMA / GPIO0 / nINT / nRST は 10k でプル設定済み
  RBIAS は 12.4k (R27)

50 MHz クロック:
  基板上の Y1 (CB3LV-3C-50M0000) を使用します。
  Y1 出力 -> R39(22R) -> REF_CLK
  Y1 出力 -> R38(22R) -> R6(0R) -> ETH_RMII_REF_CLK (PA1)
  Y1 出力 -> R39(22R) -> R40(0R) -> CLK50M (LAN8670 REFCLKIN)
  R6 / R38 / R39 / R40 が実装されていることが前提です。

メディア側:
  LAN8670 TRX_P/TRX_N -> コモンモードチョーク FL1 -> ネジ端子 J7
  TVS (S3/S4/S5) で保護されています。終端は 100 ohm 差動です。

コンソール (PY-LINK 仮想 COM ポート):
  PA9  (USART1_TX) --> PC RXD
  PA10 (USART1_RX) <-- PC TXD
  GND              --- GND
================================================================================
シリアル設定：
ボーレート 921600、データ8ビット、パリティなし、ストップ1ビット、フロー制御なし。

起動すると次のような診断情報が出力されます。

  === PY32E407 + LAN8670 10BASE-T1S bring-up ===
  MAC: ETH clock, RMII pins and MDIO ready
  PHY: ID1=0x0000 ID2=0x016x (OUI 0x000000, model 22, rev 6)
  PHY: BMSR=0x786D BMCR=0x3100 link=0
  PHY: PLCA status register = 0x0000
  PHY: LAN8670 detected
  PHY: soft reset OK
  PHY: PLCA enabled, ID=0, node count=8, TOTMR=32
  MAC: RMII 10 Mbit/s full duplex, DMA started
  PHY: link UP
  Ready. Sending a demo frame every second.

================================================================================
LED 表示：
D5 (PA3) : MAC 初期化失敗時は点滅し続ける (通常時は消灯)
D4 (PA4) : フレームを受信するたびに反転
D3 (PA5) : フレームを送信するたびに反転
D2 (PA6) : 1 秒ごとのデモ送信に合わせて反転
            (main.c の LED_D4_PIN、DEMO_FRAME_PERIOD_MS で制御)
なお基板上のシルクは D5=PA3 / D4=PA4 / D3=PA5 / D2=PA6 の順に並んでおり、
コード内の LED_D1..D4 は PA3..PA6 のポート順に対応します。
================================================================================
PLCA (Physical Layer Collision Avoidance)：
10BASE-T1S の混合セグメントでは、複数ノードが同時に送信すると衝突します。
PLCA を有効にすると、コーディネータ (ID=0) が定期的に BEACON を送り、
各ノードが決められた送信機会 ( transmit opportunity ) でのみ送信するため、
衝突が原理的に発生しません。

  ノードが 1 台だけの場合は ID=0 (コーディネータ) のままで問題ありません。
  複数台接続する場合は、1 台だけを ID=0 にし、残りを 1..N に設定して
  ください。同じ ID を複数台に設定すると通信できません。

  Src/main.c の以下のマクロで設定します。
    PLCA_NODE_ID     : 0 = コーディネータ、1..254 = フォロワ、0xFF = PLCA 無効
    PLCA_NODE_COUNT  : 1 サイクル内の送信機会の数 (実際のノード数以上)
    PLCA_TOTMR       : 送信機会タイマ (0 で既定値)

対応レジスタ (MMD 31):
  PLCA_CTRL0 (0xCA01) bit15 = EN, bit14 = RST
  PLCA_CTRL1 (0xCA02) bit15:8 = NCNT, bit7:0 = ID
  PLCA_STS   (0xCA03) bit15 = PST (BEACON を送受信できている)
  PLCA_TOTMR (0xCA04) bit7:0 = TOTMR
================================================================================
MDIO / MMD アクセス：
LAN8670 の拡張レジスタは MMD (MDIO Manageable Device) 経由でアクセスします。

  1. MMDCTRL (0x0D) に FNCTN=00b (アドレス設定) と DEVAD を書く
  2. MMDAD   (0x0E) にレジスタアドレスを書く
  3. MMDCTRL (0x0D) に FNCTN=01b (データアクセス) と DEVAD を書く
  4. MMDAD   (0x0E) を読み書きする

  DEVAD = 1 : PMA/PMD
  DEVAD = 3 : PCS
  DEVAD = 31: ベンダ固有 (PLCA など)

本サンプルの API:
  LAN8670_ReadReg()  / LAN8670_WriteReg()   Clause 22 レジスタ
  LAN8670_ReadMMD()  / LAN8670_WriteMMD()   MMD レジスタ
  LAN8670_Probe()                           PHY 識別
  LAN8670_SoftReset()                       PHY ソフトリセット
  LAN8670_ConfigurePLCA()                   PLCA 設定
  LAN8670_WaitForLink()                     リンク確立待ち
  LAN8670_EthInit() / LAN8670_EthStart()    MAC/DMA 初期化と開始
  LAN8670_SendFrame() / LAN8670_GetFrame()  フレーム送受信
================================================================================
クロック説明：
システムクロックは HSIDIV2(8MHz) x20 /2 = 80 MHz です。
ETH MAC は HCLK (80 MHz) で動作します。
MDIO のクロックレンジは HAL が HCLK から自動的に設定します (80 MHz では
60-72 MHz レンジ = DIV42 が選ばれ、MDC は約 1.9 MHz になります)。
PHY 側の REFCLKIN には基板上の 50 MHz 発振器を使用します。
================================================================================
テスト環境：
テスト用ボード：PY32E407_STK + PY32E407 ベースボード
GCCバージョン：GNU Arm Embedded Toolchain 10.3-2021.10
PHY：Microchip LAN8670 (10BASE-T1S)
================================================================================
使用手順：
1. プログラムをビルドして MCU に書き込む；
2. PY-LINK の USB ポートを PC に接続し、"USART&USB_Selection" ジャンパが
   USART 側になっていることを確認して、仮想 COM ポートを開く (921600 8N1)；
3. 起動メッセージを確認する；
   - "PHY: LAN8670 detected" が出れば MDIO と PHY は正常です
   - "PHY: no answer on MDIO" の場合は PHYAD ストラップ、MDIO プルアップ
     (R25 4.7k)、nRST (R18) を確認してください
   - "MAC: initialisation FAILED" の場合は 50 MHz 発振器と
     R6/R38/R39/R40 の実装を確認してください
4. 10BASE-T1S バスに他のノード (PC 側の T1S インタフェースなど) を接続する；
5. "PHY: link UP" になったら、1 秒ごとにデモフレームが送出され、
   受信フレームが "RX ... type=0x88B5" として表示されます。
================================================================================
注意事項：
1. 本サンプルは疎通確認用です。IP 通信 (TCP/UDP) を行う場合は、MAC ドライバ
   (LAN8670_SendFrame / LAN8670_GetFrame) の上に lwIP などを載せてください。
   ベースボードの sample には lwIP は含まれていません。リポジトリの
   sample/TcpEcho_RMII に lwIP 一式があるので、必要に応じて流用できます。
2. 10BASE-T1S は 10 Mbit/s 固定です。オートネゴシエーションはありません。
   HAL_ETH_Init() には ETH_AUTONEGOTIATION_DISABLE を渡しています。
3. 複数ノードを接続する場合は PLCA の ID が重複しないようにしてください。
4. 受信ディスクリプタはフレームをコピーした後に DMA へ返却しています
   (LAN8670_GetFrame)。返却しないと 1 フレームで受信が停止します。
5. コード構成：
     Src/main.c        初期化シーケンスとメインループ
     Src/lan8670.c     PHY / MMD / PLCA / MAC / DMA ドライバ
     Src/console.c     USART1 の診断コンソール
     Src/py32e407_hal_msp.c  ETH (RMII) と USART1 の MSP
     Inc/lan8670.h     レジスタ定義と API
     Inc/main.h        ピン定義とボード設定
     Inc/py32e407_hal_conf.h  HAL モジュール設定と PHY マクロ

================================================================================
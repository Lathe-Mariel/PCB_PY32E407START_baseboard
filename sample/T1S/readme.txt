================================================================================
                                サンプル使用説明
================================================================================
機能説明：
本サンプルは、ベースボードに実装されている LAN8670 (10BASE-T1S PHY) を
PY32E407 の ETH (RMII) で初期化し、10BASE-T1S セグメント上で ICMP の
echo request (Ping) を周期的に送信するサンプルです。

    1. 80 MHz システムクロックを設定
    2. ETH クロック / RMII ピン / MDIO クロックレンジを初期化
    3. MDIO 経由で PHY を識別 (PHY_ID1 / PHY_ID2)
    4. PHY ソフトリセット、PLCA の設定 (省略可)
    5. MAC / DMA の設定と開始、リンク確立待ち
    6. ARP で宛先 MAC を解決し、ICMP echo request を送信
    7. echo reply を受信すると RTT を UART に表示
    8. ボード宛ての ARP / ICMP echo request にも応答 (逆方向 Ping 可)

        PC <-- UART 921600 --> USART1 (PA9/PA10)
                                      |
                              PY32E407 ETH (RMII)
                                      |
                                 LAN8670 (IC2)
                                      |
                              TRX_P/TRX_N -- FL1 -- J7
                                      10BASE-T1S バス
                                      |
                              PC / 他の T1S ノード

lwIP は使用していません。ARP + IPv4 + ICMP だけを実装した小さなスタック
(Inc/ping.h, Src/ping.c) を LAN8670 ドライバの上に載せています。
フラグメント、IP オプション、TCP/UDP は扱いません。

================================================================================
ネットワーク設定：
Inc/main.h の以下のマクロを、接続する PC / ノードに合わせて変更してください。

    PING_LOCAL_IP      本機の IPv4 アドレス        (既定 192.168.11.10)
    PING_NETMASK       サブネットマスク            (既定 255.255.255.0)
    PING_GATEWAY_IP    デフォルトゲートウェイ      (既定 192.168.11.3)
    PING_TARGET_IP     Ping を送る相手             (既定 192.168.11.3)

    PING_INTERVAL_MS   送信間隔                    (既定 1000 ms)
    PING_TIMEOUT_MS    echo reply のタイムアウト   (既定 1000 ms)
    PING_ARP_TIMEOUT_MS  ARP 1 回の待ち時間        (既定 500 ms)
    PING_ARP_RETRIES     ARP の再送回数            (既定 3 回)

ローカル MAC アドレスは LAN8670 ドライバが 02:00:00:00:00:01 を使用し、
ARP / IPv4 も同じアドレスを使います (LAN8670_GetMacAddress())。
同じセグメント内で重複しないようにしてください。

宛先がサブネット外の場合は、ゲートウェイの MAC を ARP で解決して送信します。
PC 側の T1S インタフェースにも同じサブネットの静的 IP を設定してください。

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

50 MHz クロック:
  基板上の Y1 (CB3LV-3C-50M0000) を使用します。
  R6 / R38 / R39 / R40 が実装されていることが前提です。

メディア側:
  LAN8670 TRX_P/TRX_N -> コモンモードチョーク FL1 -> ネジ端子 J7
  10BASE-T1S のバスに PC 側インタフェースなどを接続します。

コンソール (PY-LINK 仮想 COM ポート):
  PA9  (USART1_TX) --> PC RXD
  PA10 (USART1_RX) <-- PC TXD
  GND              --- GND

================================================================================
シリアル設定：
ボーレート 921600、データ8ビット、パリティなし、ストップ1ビット、フロー制御なし。

起動すると次のような診断情報が出力されます。

  === PY32E407 + LAN8670 10BASE-T1S ping ===
  MAC: ETH clock, RMII pins and MDIO ready
  PHY: ID1=0x0007 ID2=0xC165 (OUI 0x0001F0, model 22, rev 5=C2)
  PHY: BMSR=0x0805 BMCR=0x0000 link=1
  PHY: PLCA status register = 0x0000
  PHY: LAN8670 detected
  PHY: soft reset OK
  PHY: AN1699 configuration for rev 5: OK
  PHY: PLCA enabled, ID=1, node count=2, TOTMR=32
  MAC: RMII 10 Mbit/s half duplex, DMA started
  PHY: link UP
  PHY: PLCA active (BEACON seen) (PLCA_STS=0x8000)
  PHY: SQI measurement started (CTL=0x4000 CFG0=0x1FFF CFG2=0x1F00 STS=0x0000)
  MDIO: 1000 x3 reads, errors=0 mismatches=0
  IPv4: local 192.168.11.10 mask 255.255.255.0 gateway 192.168.11.3
  Ping target 192.168.11.3 every 1000ms (timeout 3000ms)

  ARP: resolving 192.168.11.3 ...
  ARP: 192.168.11.3 is at AA:BB:CC:DD:EE:FF
  Reply from 192.168.11.3: bytes=32 time=1ms ttl=64 seq=1
  Reply from 192.168.11.3: bytes=32 time=0ms ttl=64 seq=2

応答がない場合は次のように表示されます。

  Request timeout: seq=3
  ARP timeout: no reply from 192.168.11.3

================================================================================
通信段階ごとの診断：
通常は USART のログを次の順で確認します。

  1. "PHY: link=DOWN" または "MDIO_ERR"
     PHY/物理層または MDIO の問題です。ケーブル、相手ノードの電源、
     10BASE-T1S の配線・終端、PHY の応答を確認してください。

  2. link=UP だが "PLCA=no-BEACON"
     PLCA のコーディネータから BEACON を受信できていません。
     PLCA を使う構成では、バス上に ID=0 のコーディネータが 1 台だけあり、
     follower ID がコーディネータの NCNT 未満であることを確認します。
     このサンプルの既定値は ID=1 / NCNT=2 なので、別ノードが ID=0 で
     動作している必要があります。PLCA を使わない構成では両端の設定を
     確認したうえで PLCA_NODE_ID を 0xFF にします。

  3. "MAC: ... not queued" または ARP/ICMP の TXok が増えない
     MAC/DMA/RMII 送信段階です。TX descriptor busy は直前の送信が完了
     していない状態、TX error はフレームを DMA に渡せなかった状態です。
     TXok が増えない場合は 50 MHz ETH_REF_CLK、RMII 配線、DMA 状態を確認します。

  4. "ARP: resolving ..." の後に "ARP timeout"
     Ethernet / ARP 段階で相手から応答がありません。ARP の送信が MAC に
     渡ったか (TXok)、受信があるか (RXuni / RXall)、相手 IP・サブネット、
     PLCA 設定を確認します。ARP 解決前なので ICMP には到達していません。

  5. "ARP: ... is at ..." の後に "Request timeout"
     ARP 解決は成功し、ICMP echo request は MAC の TX キューに入りました。
     MAC 行の TXok が増えているかで実際の送信も確認し、相手の IP 設定、
     ICMP 応答設定・フィルタ、および応答フレームの受信 (RXuni / RXall) を
     確認します。5秒ごとの IP / ICMP 行では echo-reply が応答受信数、
     accepted が Ping と一致した応答数です。差がある場合は state / id /
     seq / src の各カウンタで照合失敗の条件を確認します。

5 秒ごとの MAC 行では、TXok/RXuni/RXcrc は MAC カウンタ、RXall は MCU が
取り出した全フレーム数です。RX 行の MAC-unicast / MCU-unicast はそれぞれ
MAC の受信カウンタと MCU が取り出したユニキャスト数です (いずれも直前の
診断からの差分)。この2つの差が継続する場合は、MAC/DMA の取得からアプリまで
の間で受信を失っていないか確認してください。
IP 行は同じ期間のIPv4処理結果です。echo-reply が増えてacceptedが増えない場合、
state/id/seq/src の各値でPingとの照合が失敗した条件を特定できます。
bad-ip-csum / bad-icmp-csum は受信データのチェックサム異常数です。
PHY 行の link と PLCA はその時点の状態です。
RX フレームの個別表示が必要な短時間だけ Src/main.c の ETH_RX_DEBUG を 1 に
してください。同期 UART 出力は受信サービスを遅らせるため、通常は無効です。

以下は 5 秒ごとの診断行の例です。
  MAC: DMASR=0x00660444 TXok=25 RXuni=25 RXcrc=0 RXall=25 STS1=0x0080 MAXID=2 EMPCYC
  PHY: link=UP BMSR=0x0805 PLCA=BEACON PLCA_STS=0x8000 BEACON=621933 TO=621933 (ok)
  PHY: SQI pending (STS=0x0000 err=0)

STS1 は MMD 31 (0x1F) の 0x0018 から読みます (Clause 22 の 0x18 は予約領域で常に 0 を返すため、旧版の STS1=0x0000 は無効な値でした)。PSTC (PLCA 状態変化) / TXCOL / TXJAB / EMPCYC / ESDERR / DEC5B が付加表示されます。
BEACON= は前回の読み出し以降に受信した BEACON 数 (コーディネータの生存確認)、TO= は同期間の自ノード送信機会数です。5 秒間で BEACON が 0 のままならコーディネータが停止しており、TO だけ増えるなら自ノードは送信機会を得られているのに相手がデータを送っていないことが確定します。
MAXID はコーディネータが前サイクルで許可した送信機会数 (NCNT) です。

シリコンリビジョンによって SQI の読み方が変わります。
  rev C1/C2 : 従来の SQI (SQICTL/SQISTS0, TOID=0xFF, 完了までポーリング)
  rev D0+   : DCQ SQI (DCQ_TOID=0xCC02, DCQ_SQI=0xCC03, DCQ_SQIP=0xCC04)
              旧 SQI レジスタは D0 では廃止されており、完了しないのが正常動作です。
SQI は受信データ量に依存して統計を取るため、本サンプルのような秒間 1 – 2 フレーム程度の軽いトラフィックでは統計蓄積が完了せず "SQI pending" が続くことがあります。

PHY の初期化では AN1699 (DS60001699) のリビジョン別設定レジスタ書き込みをソフトリセット後に行います。この設定なしでは PHY は EMI とノイズ耐性が劣化した状態で「一応動作」するだけです。リセットのたびに再適用が必要です。

これらのログで障害段階を絞り込めますが、実機のログなしに物理的な原因を
断定することはできません。

================================================================================
LED 表示：
D5 (PA3) : MAC 初期化失敗時は点滅し続ける (通常時は消灯)
D4 (PA4) : フレームを受信するたびに反転
D3 (PA5) : フレーム (ARP / ICMP) を送信するたびに反転
D2 (PA6) : 1 秒ごとのハートビートで反転
なお基板上のシルクは D5=PA3 / D4=PA4 / D3=PA5 / D2=PA6 の順に並んでおり、
コード内の LED_D1..D4 は PA3..PA6 のポート順に対応します。

================================================================================
PLCA (Physical Layer Collision Avoidance)：
10BASE-T1S の混合セグメントでは、複数ノードが同時に送信すると衝突します。
PLCA を有効にすると、コーディネータ (ID=0) が定期的に BEACON を送り、
各ノードが決められた送信機会 (transmit opportunity) でのみ送信するため、
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
実装構成：
  Src/main.c             初期化シーケンスとメインループ
  Src/lan8670.c          PHY / MMD / PLCA / MAC / DMA ドライバ
  Src/ping.c             ARP / IPv4 / ICMP echo の送受信
  Src/console.c          USART1 の診断コンソール
  Src/py32e407_hal_msp.c ETH (RMII) と USART1 の MSP
  Inc/lan8670.h          レジスタ定義と PHY / MAC API
  Inc/ping.h             Ping クライアント API と設定構造体
  Inc/main.h             ピン定義、ボード設定、IP アドレス設定
  Inc/py32e407_hal_conf.h HAL モジュール設定と PHY マクロ

Ping の状態遷移：
  IDLE --(送信時刻)--> RESOLVING --(ARP reply)--> WAITING --(echo reply)--> IDLE
                                      |                          |
                                      +--(ARP timeout)----------+
                                                                 +--(reply timeout)--> IDLE

================================================================================
テスト環境：
テスト用ボード：PY32E407_STK + PY32E407 ベースボード
GCCバージョン：GNU Arm Embedded Toolchain 10.3-2021.10
PHY：Microchip LAN8670 (10BASE-T1S)

================================================================================
使用手順：
1. Inc/main.h の IP アドレス設定を、接続する PC / ノードに合わせる；
2. プログラムをビルドして MCU に書き込む；
3. PY-LINK の USB ポートを PC に接続し、"USART&USB_Selection" ジャンパが
   USART 側になっていることを確認して、仮想 COM ポートを開く (921600 8N1)；
4. 起動メッセージを確認する；
   - "PHY: LAN8670 detected" が出れば MDIO と PHY は正常です
   - "PHY: no answer on MDIO" の場合は PHYAD ストラップ、MDIO プルアップ
     (R25 4.7k)、nRST (R18) を確認してください
   - "MAC: initialisation FAILED" の場合は 50 MHz 発振器と
     R6/R38/R39/R40 の実装を確認してください
5. 10BASE-T1S バスに PC 側インタフェースを接続する；
6. "PHY: link UP" になったら ARP 解決後に Ping が始まり、
   "Reply from ..." または "Request timeout" が表示されます。

================================================================================
注意事項：
1. lwIP は含まれていません。TCP/UDP が必要な場合は、MAC ドライバ
   (LAN8670_SendFrame / LAN8670_GetFrame) の上に lwIP などを載せてください。
   リポジトリの sample/TcpEcho_RMII に lwIP 一式があります。
2. 10BASE-T1S は 10 Mbit/s 固定です。オートネゴシエーションはありません。
   HAL_ETH_Init() には ETH_AUTONEGOTIATION_DISABLE を渡しています。
3. 複数ノードを接続する場合は PLCA の ID が重複しないようにしてください。
4. 受信ディスクリプタはフレームをコピーした後に DMA へ返却しています
   (LAN8670_GetFrame)。返却しないと 1 フレームで受信が停止します。
5. ARP キャッシュは 1 エントリです。宛先を切り替えた場合は再解決しますが、
   同じ宛先の MAC が変わった場合はリセットするまで古い値を使います。
6. 本サンプルは IPv4 のユニキャストのみ扱います。ブロードキャスト Ping や
   マルチキャスト、フラグメントされたパケットには対応しません。

================================================================================

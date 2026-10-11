================================================================================
                                样例使用说明
                      PWM Audio Player (Sample Description)
================================================================================
功能描述 / Function description
--------------------------------------------------------------------------------
此样例通过 PWM 播放 Melody.wav。TIM2 的通道 2（TIM2_CH2，PA1 引脚）以 62.5 kHz
的载波工作，其占空比跟随 PCM 采样值变化（8 bit 分辨率），再由 Pmod-AUDIO V1.2
（PAM8403 滤波レス D 类功放）放大后从 3.5mm 插孔输出。程序循环播放 Melody.wav，
播放期间板载 LED 以 2Hz 闪烁。

若直接接扬声器/蜂鸣器（不经功放模块），请在 PA1 与负载之间串联电阻并加 RC 低通
滤波（例如直列 470 ohm + 对地 10 nF）。

This sample plays Melody.wav through PWM. TIM2 channel 2 (TIM2_CH2, pin PA1) runs
at a 62.5 kHz carrier whose duty cycle follows the PCM sample (8 bit resolution); the
audio is then amplified by the Pmod-AUDIO V1.2 (PAM8403 filterless class-D amplifier)
and comes out of its 3.5 mm jack. Melody.wav is played in a loop and the on-board LED
keeps blinking at 2 Hz while it plays.

Driving a bare speaker or buzzer instead of the module needs a series resistor and an
RC low pass filter between PA1 and the load (for instance 470 ohm in series and 10 nF
to ground).

--------------------------------------------------------------------------------
接线 / Wiring
--------------------------------------------------------------------------------
    本样例默认接在 Pmod-AUDIO V1.2（wuxx/icesugar）上。该模块使用 PAM8403
    滤波レス D 类功放，输入网络为「1 uF 串联耦合电容 + 22 kohm 到地」，增益约
    22 dB（AVD = 20*log(2*RF/RI)，RF=142k，RI=22k）。

    This sample is set up for the Pmod-AUDIO V1.2 (wuxx/icesugar). That module uses
    a PAM8403 filterless class-D amplifier whose input network is a 1 uF series
    coupling capacitor with a 22 kohm resistor to ground, giving about 22 dB of
    gain (AVD = 20*log(2*RF/RI), RF = 142k, RI = 22k).

    PY32E407 | Pmod-AUDIO V1.2
    ---------+------------------------------------------------
    PA1      | INPUT_L (ピン 1 / pin 1 of J1)
    GND      | GND     (ピン 5 / pin 5 of J1)
    +3V3     | 3V3     (ピン 6 / pin 6 of J1)
      -> 3.5mm ジャックにヘッドホン / スピーカを接続

    ※ 【重要】PD8 は PWM 出力に使えません。PY32E407 のデータシート Table 3-6 で
      PD8 の代替機能は AF7=USART3_TX, AF12=LCDC_D13, AF13=ETH_RMII_CRS_DV,
      AF15=EVENTOUT のみで，タイマー（TIMx_CHn）の機能が一切ありません。

    ※ また本基板では，MCU の PD8（pin55）は未接続のままです。2x25 拡張ヘッダの
      シルク "D8|A1" はパッド22に付いた表記で，この基板では A1（PA1）に配線され
      ています。つまり「D8」という信号はこの基板には存在しません。

    ※ どうしても D ポート付近のピンで PWM を出したい場合は，タイマー機能を持つ
      次の中から選べます（audio.c 冒頭の AUDIO_PWM_PIN_* を 1 つ有効化）:

        PD4   TIM2_CH2 (AF2)  本基板: Pmod3 ピン 6
        PD6   TIM2_CH4 (AF2)  本基板: Pmod3 ピン 8
        PD7   TIM2_CH3 (AF2)  本基板: Pmod3 ピン 1
        PE9   TIM1_CH1 (AF2)  本基板: Pmod4 ピン 4
        PE11  TIM1_CH2 (AF2)  本基板: Pmod4 ピン 6

      既定は PA1 (TIM2_CH2 / AF1) です。

    ※ 本板の PA1 は ETH_RMII_REF_CLK と兼用です。Ethernet PHY とは同時に
      使えません。

    IMPORTANT: PD8 cannot drive PWM. On the PY32E407 its only alternate functions are
    AF7 = USART3_TX, AF12 = LCDC_D13, AF13 = ETH_RMII_CRS_DV and AF15 = EVENTOUT
    (datasheet Table 3-6); it has no timer channel at all. On this board the MCU's PD8
    (pin 55) is also left unconnected: the "D8|A1" silkscreen on the 2x25 expansion
    header sits on pad 22, which is wired to A1 (PA1) here, so no "D8" signal exists on
    this board.

    If a pin near port D is required, pick one of the timer capable ones instead by
    enabling exactly one AUDIO_PWM_PIN_* at the top of audio.c (PD4 = TIM2_CH2 on Pmod3
    pin 6, PD6 = TIM2_CH4 on Pmod3 pin 8, PD7 = TIM2_CH3 on Pmod3 pin 1, PE9 = TIM1_CH1
    on Pmod4 pin 4, PE11 = TIM1_CH2 on Pmod4 pin 6). The default is PA1 (TIM2_CH2/AF1).

    ※ 1 uF の入力結合コンデンサは低周波を通すハイパスであり，62.5 kHz の PWM
      キャリアを除去しません。PAM8403 はアナログ入力を増幅するアンプで，入力の
      AC 結合だけを根拠に PWM キャリアが除去されるとはみなせません。残留キャリア
      やアンプの非線形動作が聞こえる場合は，MCU 出力とモジュール入力の間に
      ローパスを追加するのが候補です。PWM の 50% デューティは AC 結合後の無音
      レベルに対応します。

    ※ まず試す一段 RC の例は，PWM 出力から 1 kohm を直列に入れ，モジュール入力
      側に 10 nF を GND へ接続する構成です。入力抵抗 22 kohm を含む単純な回路
      モデルでは，時定数の抵抗は約 1 kohm || 22 kohm = 956 ohm，遮断周波数は
      約 16.6 kHz です。7.2 kHz は約 0.8 dB，62.5 kHz は約 11.8 dB 減衰する
      計算です。これは試験用の目安であり，実際の減衰はモジュールと配線に依存
      します。高域の音声成分も少し減衰します。

    ※ 音声の量子化ノイズをさらに下げるには，PWM の実効分解能も上げる必要が
      あります。現在の 16 MHz タイマでは 62.5 kHz が 256 カウント（8 bit）です。
      搬送波を上げるとカウント数が減って量子化ノイズが増え，搬送波を下げると
      可聴域に近づきます。16 bit PCM に置き換えるだけでは，8 bit PWM の分解能
      がボトルネックのままです。

    ※ The 1 uF input coupling capacitor is a high-pass, so it does not remove the
      62.5 kHz PWM carrier. The PAM8403 amplifies an analogue input; AC coupling alone
      is not evidence that the carrier is removed. If residual carrier or amplifier
      nonlinearity is audible, a low-pass between the MCU output and module input is a
      candidate. After AC coupling, 50% PWM duty corresponds to silence.

    ※ A first-order RC test is a 1 kohm series resistor from the PWM output and a
      10 nF capacitor from the module-input node to GND. In a simple model including
      the 22 kohm input resistor, R is about 1 kohm || 22 kohm = 956 ohm and the cutoff
      is about 16.6 kHz. The estimated attenuation is about 0.8 dB at 7.2 kHz and
      11.8 dB at 62.5 kHz. This is a bench-test starting point, not a guaranteed result:
      the module and wiring affect it, and upper audio frequencies are also attenuated.

    ※ Further reducing PCM quantisation noise requires more effective PWM resolution.
      With the current 16 MHz timer clock, 62.5 kHz gives 256 counts (8 bit). Raising
      the carrier reduces the count range and worsens quantisation; lowering it brings
      the carrier closer to the audible band. Replacing the PCM with 16-bit data alone
      does not overcome the 8-bit PWM resolution.

    PA1 pin of the PY32E407 is wired to AF1, which selects TIM2_CH2
    (PY32E407 datasheet, Table 3-3 "Port A alternate functions mapping").

--------------------------------------------------------------------------------
工作原理 / How it works
--------------------------------------------------------------------------------
    1. 采样播放时，PWM 直接使用 16 MHz 定时器时钟、不加预分频，得到 256 计数
       的周期（约 62.5 kHz 载波）和 8 bit 的占空比分辨率。
    2. TIM3 以采样率（16 kHz）产生更新中断，每次中断把一个采样值写入 CCR2。
       采样值 128（静音）对应 50% 占空比，0 与 255 对应最小与最大占空比。
    3. 由采样值到占空比的除法在切换模式时预先算好，放在 256 项查表中，因此
       中断处理只有几条指令（16 MHz 主频下也没有负担）。
    4. 采样值直接写入 CCR2（duty = 采样值），因此得到 8 bit 分辨率。

    1. Sample mode runs the PWM straight from the 16 MHz timer clock, giving a
       256 count period (about 62.5 kHz) and 8 bit duty resolution.
    2. TIM3 raises an update interrupt at the sample rate (16 kHz); each interrupt
       writes one sample into CCR2. 128 (silence) maps to a 50 % duty, 0 and 255
       to the minimum and maximum duty.
    3. The sample to duty division is precomputed into a 256 entry table when the
       mode is selected, so the interrupt handler is only a few instructions long
       even at a 16 MHz core clock.
    4. The sample value is written straight into CCR2 (duty = sample), which is
       what gives the 8 bit resolution.

--------------------------------------------------------------------------------
Melody.wav 与生成的数据 / Melody.wav and the generated data
--------------------------------------------------------------------------------
    ・sample/pmod-audio/Melody.wav 实际上是 MPEG-1 Layer III（MP3，LAME 编码，
      32 kHz 立体声）文件，只是扩展名为 .wav，内含约 2.97 秒的真实音乐。
    ・该文件被解码为 32 kHz 单声道，经 7.2 kHz 低通滤波后降采样为 16 kHz，
      再量化为 8 bit 无符号 PCM，写入 Src/melody_wav.c（47519 字节，约 46 KB
      Flash），数组 MELODY_WAV_SAMPLES 即播放的数据源。
    ・另有 Src/melody_wav_s.c：从完整 PCM 经 3.5 kHz 低通后降采样至 8 kHz，
      保持约 2.97 秒播放时长，采样数据为 23760 字节（约为完整版本的一半）。
      高频音频带宽相应降低。
    ・默认构建完整版本：在 EIDE 目录执行 make；也可在 VS Code 任务中选择
      "build full melody"。构建小容量版本：执行 make MELODY_VARIANT=small，
      或选择 "build small melody"。切换版本后会自动重新编译主程序。
    ・如需换成别的曲子，只要生成同样格式的数组并调用
      AUDIO_PlaySample(data, length, rate_hz) 即可（8 kHz～48 kHz 均可）。

    - sample/pmod-audio/Melody.wav is in fact an MPEG-1 Layer III stream (MP3,
      LAME encoded, 32 kHz stereo) despite its .wav name; it holds about
      2.97 seconds of real music.
    - The full version is decoded to 32 kHz mono, low-pass filtered at 7.2 kHz,
      decimated to 16 kHz and quantised to 8-bit unsigned PCM in Src/melody_wav.c
      (47519 bytes, about 46 KB of flash).
    - Src/melody_wav_s.c contains an 8 kHz version filtered at 3.5 kHz before
      decimation. It retains the approximately 2.97 second duration in 23760 bytes
      of samples (about half the full version), with correspondingly less treble.
    - Build the full version by running make in the EIDE directory (default), or
      use the VS Code task "build full melody". Build the smaller version with
      make MELODY_VARIANT=small or the task "build small melody". Switching variants
      automatically recompiles the main program.
    - To play something else, generate an array in the same format and call
      AUDIO_PlaySample(data, length, rate_hz) (8 kHz to 48 kHz are supported).

--------------------------------------------------------------------------------
文件构成 / Files
--------------------------------------------------------------------------------
    Src/main.c       : 主程序（系统时钟、LED、启动播放并在结束后重播）
    Src/audio.c      : PWM PCM 音频播放器
    Inc/audio.h      : PCM 音频 API 声明
    Src/melody_wav.c   : 完整版 Melody.wav 的 8 bit / 16 kHz PCM 数据
    Src/melody_wav_s.c : 小容量版 Melody.wav 的 8 bit / 8 kHz PCM 数据
    Inc/melody_wav.h   : 数据长度、采样率与编译时版本选择
    Src/py32e407_it.c: 增加 TIM3_IRQHandler（音频定时中断）
    Inc/py32e407_it.h: 上述中断处理函数的声明

    Src/main.c       : main program (clock, LED, start and repeat the playback)
    Src/audio.c      : the PWM PCM audio player
    Inc/audio.h      : PCM audio API declarations
    Src/melody_wav.c   : full-size Melody.wav as 8 bit / 16 kHz PCM
    Src/melody_wav_s.c : small Melody.wav as 8 bit / 8 kHz PCM
    Inc/melody_wav.h   : data lengths, sample rates, and build-time selection
    Src/py32e407_it.c: adds TIM3_IRQHandler (audio timer interrupt)
    Inc/py32e407_it.h: declaration of the handler above

--------------------------------------------------------------------------------
音量とノイズ / Loudness and noise
--------------------------------------------------------------------------------
    ■ ヒスの原因（今回の修正）
    PWM の音声情報は「デューティ比」に乗っています。したがってデューティ振幅
    (span) は音量と分解能の両方を同時に決めます。以前の設定は span 15% だった
    ため，256 カウントの周期のうち約 40 段 (5.25 bit) しか使えず，量子化ノイズ
    (SNR 約 33 dB) がホワイトノイズとして聞こえていました。

    修正として span を 100% にしました。Pmod-AUDIO は 3V3 のロジック矩形波で
    直接駆動する設計（公式リファレンスも同様）で，PAM8403 が PWM のバッファと
    して働き，スピーカ側で音声が復元されます。搬送波を減衰させる必要はないため
    デューティの全範囲が使え，8 bit (256 段, SNR 約 50 dB) が得られます。

      span 15%  -> 約 40 段 (5.25 bit, 量子化 SNR 約 33 dB)  ← 修正前
      span 100% -> 256 段 (8.00 bit, 量子化 SNR 約 50 dB)  ← 修正後 (約 +17 dB)

    このときサンプル→デューティの変換は恒等写像 (duty = サンプル値) になり，
    余分な演算も不要です。

    ■ 調整用パラメータ (Src/audio.c 冒頭)
      AUDIO_SAMPLE_SPAN_PCT      : デューティ振幅 (既定 100%)。下げると音量が
                                   下がり，同時に分解能 (ビット数) も下がりま
                                   す。音量を絞りたい場合のみ下げてください。
      AUDIO_SAMPLE_COMPRESS_PCT  : ソフトリミッタ (既定 0 = 無効)。

    ■ ソフトリミッタについて
      f(s)=s(1+k)/(1+k|s|) で小信号を (1+k) 倍持ち上げ，ピークはそのままに
      します。クレストファクターの大きい素材を大きく聞かせたいときに有効です。
      ただし曲線がデューティ値を圧縮するため使用段数が減り，ノイズ床は上がり
      ます (span 100% では元々フルスケールなので既定は無効)。

    ■ さらにノイズを下げるには
      span 100% とコンプレッサ無効で，PWM の全 256 段を使っています。span を
      さらに広げる余地はありません。残るノイズには，8 bit PCM の量子化ノイズ，
      PWM キャリア，アンプ・電源・配線のノイズが含まれます。どれが支配的かは
      実機での出力測定または録音なしには特定できません。

      ソフトウェア設定だけで改善するより，まず前述の RC を試し，RC 前後の
      モジュール入力波形とスピーカー音を比較してください。PWM キャリア成分が
      下がれば RC が有効です。変化が小さい場合は，電源・GND・配線・アンプ自身
      のノイズを調べる必要があります。音量が変わる場合やひずみが増す場合は，
      その部品値を確定値として扱わず，実測に合わせて見直してください。

      A 100 % span with the compressor disabled already uses all 256 PWM counts; there
      is no further span increase available. Remaining noise can include 8-bit PCM
      quantisation, PWM carrier, and amplifier, supply, or wiring noise. The dominant
      source cannot be identified without a measurement or recording of the actual unit.

      Rather than changing firmware blindly, first compare the module-input waveform
      and speaker output with and without the RC test described above. If the carrier
      component falls, the RC is helping. If not, investigate supply, ground, wiring,
      and amplifier noise. Revisit the component values based on measurements if
      loudness changes or distortion increases; treat the example as a test, not a
      finalized design.

      Tuning knobs at the top of audio.c: AUDIO_SAMPLE_SPAN_PCT (100 % by default;
      lowering it reduces the volume and the bit depth together) and
      AUDIO_SAMPLE_COMPRESS_PCT (0 = off; the soft limiter f(s)=s(1+k)/(1+k|s|) lifts
      quiet passages for more loudness at the cost of a higher noise floor).

--------------------------------------------------------------------------------
主要 API / API overview
--------------------------------------------------------------------------------
    void AUDIO_Init(void);
        PA1 的 PWM 与音频定时器初始化。须在系统时钟配置之后调用。

    void AUDIO_PlaySample(const uint8_t *samples, uint32_t length, uint32_t rate_hz);
        播放 8 bit 无符号 PCM 采样（128 = 静音）。可随时重新调用以换曲。

    void AUDIO_StopSample(void);
    uint8_t AUDIO_IsPlayingSample(void);

    Sample buffers hold unsigned 8 bit PCM where 128 is silence. Playback is
    paced at the supplied sample rate (8 kHz to 48 kHz).

--------------------------------------------------------------------------------
时钟与精度 / Clock and accuracy
--------------------------------------------------------------------------------
    ・PWM 定时器与音频定时器的预分频/周期都由 HAL_RCC_GetPCLK1Freq() 推导，
      因此修改系统时钟（例如改用 PLL 或 HSE）后采样率与音准仍然正确。
    ・APB1 预分频为 1 时定时器时钟等于 PCLK1，否则为其 2 倍，本样例已按此规则
      换算。
    ・16 kHz 采样在 16 MHz 时钟下为整数分频，误差 0 ppm；44.1 kHz 等非整数
      时钟下误差也在 0.1% 以内。

    - The prescalers and periods of both timers are derived from
      HAL_RCC_GetPCLK1Freq(), so the sample rate and the pitch stay correct if the
      system clock is changed (PLL or HSE for instance).
    - The APB1 timers run at PCLK1 when the APB1 prescaler is 1 and at twice PCLK1
      otherwise; the sample accounts for this.
    - 16 kHz comes out as an exact division of the 16 MHz clock (0 ppm error). Even
      fractional rates such as 44.1 kHz stay within 0.1 %.

--------------------------------------------------------------------------------
构建 / Build
--------------------------------------------------------------------------------
    ※ 构建需要 Puya SDK 的 Drivers 目录树。本文件夹（EIDE）向上 5 层
      （例如 H:\Drivers）需有 Drivers/BSP、Drivers/CMSIS、Drivers/PY32E407_HAL_Driver。
      可从 PuyaSDE 的 sdk\example\PY32E407_Firmware_*.zip 解压得到。

    在 EIDE 中构建，或在 EIDE 目录下执行（需将 arm-none-eabi-gcc 加入 PATH）：

        > make

    make 会生成 Project/Project.elf、Project.hex、Project.bin。

    ※ 本样例的 HAL 配置启用了 HAL_TIM_MODULE_ENABLED（Inc/py32e407_hal_conf.h），
      并在 Makefile 中加入了 py32e407_hal_tim.c 与 py32e407_hal_tim_ex.c。

--------------------------------------------------------------------------------
烧写 / Flash
--------------------------------------------------------------------------------
    使用 EIDE 的下载任务（pyOCD 或 JLink）。

================================================================================
测试环境 / Test environment
================================================================================
测试用板 / Board    : PY32E407-START + 本底板 (PY32E407 base board)
GCC版本              : GNU Arm Embedded Toolchain (PuyaSDE 同梱)

--------------------------------------------------------------------------------
注意事项 / Notes
--------------------------------------------------------------------------------
  ・音量は AUDIO_SAMPLE_SPAN_PCT（既定 100%）で調整します。下げると音量と
    分解能（ビット数）が同時に下がるので，音量を絞るときだけ下げてください。
    Pmod-AUDIO も素のスピーカ駆動も既定 100% が最良です。
  ・PA1 同時為以太網 PHY 的 REF_CLK 複用脚，二者不可同時使用。
  ・LED 以 2Hz 闪烁；若 LED 不闪说明程序未运行，与音频电路无关。
  ・Melody.wav の PCM 数据占约 46 KB Flash，整个程序约 55.9 KB Flash /
    2.3 KB RAM（512K/144K 之内，余量充足）。
  ・Pmod-AUDIO 的输入与 PA1（3.3V CMOS 输出）电平兼容，无需电平转换。

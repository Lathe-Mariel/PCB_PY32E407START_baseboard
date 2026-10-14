================================================================================
                                样例使用说明
                             Sample Description
================================================================================
功能描述：
此样例把 platform.wav 以 8 bit PCM（16 kHz 单声道）通过 PWM DAC 播放，
目标是能听清人声。数据由 tools/wav2pcm.py 离线生成（Src/platform_pcm.c），
生成时施加 --eq（6 段语音均衡：200 Hz 噪声底削减、300/900 Hz 母音提升、
2.5/3.5 kHz 子音/临场感提升、6 kHz 高频刺耳声削减）与 --agc（电平压缩 +
峰值归一化）。播放链路上固件还有一层同样布线的 6 段 RBJ 双二阶（biquad）
运行时均衡器（MUSIC_EqSetBand 可实时调各频段增益），以及差分驱动：
TIM1_CH2（PA9）与 CH2N（PA12）反相互补输出，蜂鸣器两端电压摆幅为单端的
2 倍。TIM2 按采样率（16 kHz）产生更新中断，中断内运行均衡器并写入占空比。
LED 以 2 Hz 闪烁。

Function descriptions:
This sample plays platform.wav as 8 bit PCM (16 kHz mono) through a PWM DAC,
aimed at speech intelligibility. The data is generated offline by
tools/wav2pcm.py (Src/platform_pcm.c) with --eq (a 6-band speech equaliser:
rumble cut at 200 Hz, vowel lift at 300/900 Hz, consonant/presence lift at
2.5/3.5 kHz, hiss shelf down above 6 kHz) and --agc (level compression +
peak normalisation). On the target the same 6-band layout runs again as a
runtime RBJ biquad equaliser (MUSIC_EqSetBand adjusts each band live), and
the buzzer is driven differentially: TIM1_CH2 (PA9) and CH2N (PA12) are
antiphase complementary outputs, doubling the swing across the element.
TIM2 fires one update interrupt per PCM sample (at 16 kHz); the ISR runs
the equaliser and latches the duty. The LED keeps blinking at 2 Hz.
================================================================================
测试环境：
测试用板：PY32E407_STK
MDK版本： 5.28
IAR版本： 9.20
GCC版本：GNU Arm Embedded Toolchain 10.3-2021.10

Test environment:
Test board: PY32E407_STK
MDK Version: 5.28
IAR Version: 9.20
GCC Version: GNU Arm Embedded Toolchain 10.3-2021.10
================================================================================
使用步骤：
1. 下载并运行程序
2. 蜂鸣器播放 platform.wav（均衡后）
3. 需要改均衡曲线时：编辑 tools/wav2pcm.py 的 EQ_DB 与 Inc/music.h 的
   MUSIC_EQ_BAND_DEFS，重新跑 regen（VS Code 任务 "regen pcm"）再编译

Example execution steps:
1. compile and download the program to MCU and run it;
2. the buzzer plays the equalised platform.wav content;
3. to retune, edit EQ_DB in tools/wav2pcm.py and MUSIC_EQ_BAND_DEFS in
   Inc/music.h, re-run the "regen pcm" task, rebuild and download.
================================================================================
注意事项：
1. 引脚
CH2   ->  PA9  (蜂鸣器端子 A)
CH2N  ->  PA12 (蜂鸣器端子 B)
2. 占空比固定跨度 100 %（8 bit 分辨率，载波 = 定时器时钟/256，16 MHz 时
   约 62.5 kHz，超声）
3. 均衡有两层：离线（tools/wav2pcm.py --eq，已烘焙进数据）+ 运行时
  （music.c 的 Q15 双二阶级联）。两侧布线必须保持一致
4. 数据是 16 bit/采样 16 kHz 的 8 bit PCM：311232 采样约 305 KB，适合
   512 KB Flash 的 PY32E407xE
5. PA9/PA12 均为 AF6（TIM1）

Notes:
1. Pins
CH2   ->  PA9  (buzzer terminal A)
CH2N  ->  PA12 (buzzer terminal B)
2. The duty uses the full 8 bit span; the carrier is timer_clk/256
   (about 62.5 kHz at 16 MHz) - ultrasonic.
3. The equaliser exists in two layers: offline (tools/wav2pcm.py --eq,
   baked into the data) and runtime (Q15 biquad cascade in music.c).
   Keep both band layouts in sync.
4. The data is 8 bit PCM at 16 kHz: 311232 samples / about 305 kB, OK for
   the 512 kB flash PY32E407xE.
5. PA9/PA12 map to AF6 (TIM1).
================================================================================
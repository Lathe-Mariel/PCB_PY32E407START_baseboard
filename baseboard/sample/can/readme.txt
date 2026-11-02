================================================================================
                                样例使用说明
                             Sample Description
================================================================================
功能描述：
此样例演示使用FDCAN1以经典CAN(Classic CAN)格式收发数据。CAN参数为500kbit/s，
每1秒周期性发送一帧标准ID 0x123的数据，同时在中断中接收标准ID 0x321的数据，
接收到的数据会通过ID原样回发到总线上。收到一帧时LED D2(PA4)翻转一次，
发送时LED D1(PA3)每秒翻转一次。

Function descriptions:
This example demonstrates how to transmit and receive data with FDCAN1 in
Classic CAN format. The CAN bit rate is 500 kbit/s. A standard-ID 0x123 frame is
sent every second, and standard-ID 0x321 frames are received by interrupt and
echoed back onto the bus with the same identifier. LED D2 (PA4) toggles on each
received frame, LED D1 (PA3) toggles every transmit period.
================================================================================
硬件连接：
MCU引脚        功能          ATA6561引脚
PD0 --------- FDCAN1_RX ---- RXD
PD1 --------- FDCAN1_TX ---- TXD
PB14 -------- GPIO(输出低) - STBY
                             CANH/CANL --> CAN总线

注意：ATA6561的STBY引脚由PB14控制，STBY为高有效。
      程序启动后会将PB14输出低电平，使收发器进入正常工作模式。

Hardware connection:
MCU pin        Function             ATA6561 pin
PD0 --------- FDCAN1_RX ----------- RXD
PD1 --------- FDCAN1_TX ----------- TXD
PB14 -------- GPIO output low ----- STBY
                                    CANH/CANL --> CAN bus

Note: the ATA6561 STBY pin is driven by PB14 and is active high. The program
      drives PB14 low at startup so the transceiver runs in normal mode.
================================================================================
时钟说明：
CAN内核时钟由RCC_CFGR2.CANCKSEL选择，本样例配置为PLL/1 = 80 MHz。
系统时钟为 HSIDIV2(8MHz) x20 /2 = 80 MHz，APB1与CAN内核时钟同为80 MHz，
满足"控制逻辑时钟频率必须大于等于通信时钟频率"的要求。
位时序：80 MHz / 10 = 8 MHz tq，16 tq/bit => 500 kbit/s

Clock notes:
The CAN kernel clock is selected by RCC_CFGR2.CANCKSEL; this example uses
PLL/1 = 80 MHz. SYSCLK is HSI/2 (8 MHz) x20 /2 = 80 MHz, so APB1 and the CAN
kernel clock are both 80 MHz, satisfying the requirement that the control logic
clock be >= the communication clock.
Bit timing: 80 MHz / 10 = 8 MHz tq clock, 16 tq per bit => 500 kbit/s
================================================================================
测试环境：
测试用板：PY32E407_STK
GCC版本：GNU Arm Embedded Toolchain 10.3-2021.10

Test environment:
Test board: PY32E407_STK
GCC Version: GNU Arm Embedded Toolchain 10.3-2021.10
================================================================================
使用步骤：
1. 编译并下载程序到MCU；
2. 将CAN分析仪或另一个CAN节点连接到板的CAN接口(CANH/CANL)，终端电阻120Ω；
3. 上电后，MCU每1秒发送一帧ID=0x123、8字节数据(前两字节为发送计数)，
   同时LED D1(PA3)每秒翻转一次；
4. 向总线发送ID=0x321的数据帧，MCU会以相同ID回发相同数据，LED D2(PA4)翻转。

Example execution steps:
1. compile and download the program to MCU and run it;
2. Connect a CAN analyser or another CAN node to the board CAN connector
   (CANH/CANL) with a 120 ohm termination;
3. After power-up the MCU sends an ID=0x123, 8-byte frame every second
   (the first two bytes hold the transmit counter) and toggles LED D1 (PA3);
4. Send an ID=0x321 frame on the bus; the MCU echoes the same data with the
   same ID and toggles LED D2 (PA4).
================================================================================
注意事项：
1. 本板的CAN时钟源使用PLL，如需使用HSE请相应修改APP_SystemClockConfig()。

Notes:
1. This example uses the PLL as CAN clock source; adapt APP_SystemClockConfig()
   if HSE is preferred.

================================================================================
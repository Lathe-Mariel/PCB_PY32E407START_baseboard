================================================================================
                                样例使用说明
                             Sample Description
================================================================================
功能描述：
此样例演示了TCP回显实验,电脑发送数据到开发板,开发板将数据返回到电脑上。实验中使
用TCP协议传输数据，电脑作为TCP Client ，开发板作为TCP Server

Function descriptions:
This sample demonstrates the TCP display experiment, the computer sends data to 
the development board, the development board will return the data to the computer. 
The experiment uses the TCP protocol to transfer data, the computer as a TCP 
Client, the development board as a TCP Server

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
1. 编译并下载程序；
2. 网络连接模型：电脑<--网线-->路由<--网线-->开发板
3. 按下按键，将电脑上位机设置为TCP Client.输入开发板的IP地址(如192.168.0.122)。
4. 点击连接，发送数据给开发板（例如：1~9），开发板后收到数据会回发给PC。

Example execution steps:
1. Compile and download the program.
2. Network connection model: 
computer <--network cable-->routing <--network cable-->development board
3. Press the button to set the upper PC as TCP Client. Enter the IP address of 
the development board (e.g. 192.168.0.122).
4. Click Connect to send data to the development board (e.g. 1~9), and the 
development board will send the data back to the PC when it receives it.

================================================================================
注意事项：
1. 请将电脑上位机设置为TCP Client.输入开发板的IP地址(如192.168.0.122)
2. 如需修改IP地址与端口号，则修改对应的宏定义:IP_ADDR0,IP_ADDR1,IP_ADDR2,IP_ADDR3,
LOCAL_PORT
3. 通过USB转TTL模块连接PC与STK板,STK板与USB转TTL模块的连线方式如下；
STK板         USB转TTL模块
PC10(TX)  -->  RX
GND      -->  GND
UART配置为波特率9600。

Notes:
1. Please set your PC as a TCP Client and enter the IP address of your development
board (e.g. 192.168.0.122).
2. If you need to modify the IP address and port number, modify the corresponding
macro definitions: IP_ADDR0, IP_ADDR1, IP_ADDR2, IP_ADDR3, LOCAL_PORT, LOCAL_PORT, 
LOCAL_PORT, LOCAL_PORT, LOCAL_PORT, LOCAL_PORT, LOCAL_PORT, LOCAL_PORT.
3. Connect the PC to the STK board through the USB to TTL module, and the connection
method between the STK board and the USB to TTL module is as follows:
STK board     USB to TTL module
PC10(TX)  -->  RX
GND      -->  GND
UART is configured as BaudRate 9600.
================================================================================
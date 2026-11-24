PY32E407 + LAN8670 10BASE-T1S UDP JPEG display
================================================

This sample receives baseline JPEG images from a PC over IPv4/UDP and displays
them on the 160 x 80 ST7735S Pmod LCD. It uses the on-chip ETH MAC through the
LAN8670 and a small raw Ethernet/IPv4 receiver; lwIP is not required.

Network defaults
----------------

  Board IPv4 address: 192.168.11.10
  PC IPv4 address:    192.168.11.3 (set this statically on the T1S interface)
  UDP destination:    5005
  Maximum JPEG size:  64 KiB

Change PING_LOCAL_IP in Inc/main.h if the board address must be different.
The existing ARP responder uses this address to answer the PC's ARP request.
Connect the PC's 10BASE-T1S interface and the board to the same segment.
PLCA settings are in Src/main.c; the board is configured as follower ID 1.
The segment coordinator must advertise at least two node IDs.

PC sender
---------

The GUI is in pc_sender/sender.py. It converts the selected image to a centered
160 x 80 baseline JPEG, compresses it to at most 64 KiB, then sends ordered UDP
datagrams with a 16-byte JPG1 header. Install and run it with Python 3:

  cd sample/T1S_UDP/pc_sender
  python -m pip install -r requirements.txt
  python sender.py

Select an image, enter the board IP address and port, then click 画像を送信.
Pillow is the only external Python package.

Wire format and receiver behavior
---------------------------------

Each UDP payload is:

  4 bytes  magic "JPG1"
  4 bytes  frame ID (unsigned, big-endian)
  4 bytes  complete JPEG length (unsigned, big-endian)
  2 bytes  chunk index (zero-based, big-endian)
  2 bytes  chunk count (big-endian)
  up to 1200 bytes of JPEG data

The receiver validates the IPv4/UDP lengths and checksums, rejects fragmented
IP packets, reassembles chunks in any order, ignores duplicates, and discards
incomplete transfers after five seconds. A missing UDP datagram therefore
prevents that frame from being displayed; resend the image from the PC.
TJpgDec supports baseline sequential JPEG (including grayscale and common
subsampling modes), not progressive JPEG. Images larger than the LCD are
reduced by 1/2, 1/4, or 1/8; smaller images are centered on a black background.

LCD wiring
----------

Use the board's PMOD4 socket, not PMOD3/J3. The T1S sample holds PC8 low to keep
the on-board LAN8720 PHY in reset; PMOD3/J3 shares PC8 with the LCD module.
With the LCD signal row aligned to PMOD4 pins 1-6, the signals are:

  LCD CS    -> PE12 (PMOD4 pin 1)
  LCD MOSI  -> PE10 (PMOD4 pin 2)
  LCD D/C   -> PE8  (PMOD4 pin 3)
  LCD SCK   -> PE9  (PMOD4 pin 4)
  LCD RESET -> PB2  (PMOD4 pin 9)

The LCD uses 4-wire, mode-0 serial SPI implemented by GPIO bit-banging. Power
the module from the PMOD 3.3 V/GND pins.

Build and diagnostics
---------------------

Open sample/T1S_UDP/EIDE/Project.code-workspace and build the EIDE project, or
run make from sample/T1S_UDP/EIDE with the GNU Arm Embedded toolchain available.
The receiver uses TJpgDec R0.03 (source and license header in
Middlewares/TJpgDec). The small ping module is retained only for ARP and
incoming ICMP echo replies; periodic ping traffic is disabled.

USART1 diagnostics use the PY-LINK virtual COM port at 921600 baud, 8-N-1.
The startup log prints the UDP endpoint. A successful image transfer prints
"JPEG: displayed frame ..."; incomplete chunks and JPEG decode errors are
reported on the same console.

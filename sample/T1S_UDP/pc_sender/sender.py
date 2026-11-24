import ipaddress
import io
import socket
import struct
import time
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, ttk

from PIL import Image, ImageOps


DISPLAY_SIZE = (320, 240)
MAX_IMAGE_BYTES = 64 * 1024
CHUNK_DATA_SIZE = 1200
PACKET_HEADER = struct.Struct("!4sIIHH")
MAGIC = b"JPG1"
DEFAULT_TARGET = "192.168.11.10"
DEFAULT_PORT = 5005


def encode_image(path: Path) -> bytes:
    with Image.open(path) as source:
        image = ImageOps.contain(
            ImageOps.exif_transpose(source).convert("RGB"),
            DISPLAY_SIZE,
            Image.Resampling.LANCZOS,
        )

    frame = Image.new("RGB", DISPLAY_SIZE, "black")
    frame.paste(image, ((DISPLAY_SIZE[0] - image.width) // 2,
                        (DISPLAY_SIZE[1] - image.height) // 2))

    for quality in range(90, 19, -5):
        output = io.BytesIO()
        frame.save(output, format="JPEG", quality=quality, optimize=True)
        jpeg = output.getvalue()
        if len(jpeg) <= MAX_IMAGE_BYTES:
            return jpeg

    raise ValueError("画像を64 KiB以下に圧縮できませんでした。")


def send_jpeg(jpeg: bytes, target: str, port: int, progress) -> int:
    frame_id = time.monotonic_ns() & 0xFFFFFFFF
    chunk_count = (len(jpeg) + CHUNK_DATA_SIZE - 1) // CHUNK_DATA_SIZE

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
        for index in range(chunk_count):
            start = index * CHUNK_DATA_SIZE
            chunk = jpeg[start:start + CHUNK_DATA_SIZE]
            header = PACKET_HEADER.pack(
                MAGIC, frame_id, len(jpeg), index, chunk_count
            )
            sender.sendto(header + chunk, (target, port))
            progress(index + 1, chunk_count)
            time.sleep(0.002)

    return chunk_count


class SenderApp:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        self.root.title("T1S UDP JPEG Sender")
        self.root.resizable(False, False)

        self.image_path = tk.StringVar()
        self.target_ip = tk.StringVar(value=DEFAULT_TARGET)
        self.target_port = tk.StringVar(value=str(DEFAULT_PORT))
        self.status = tk.StringVar(value="送信する画像を選択してください。")

        frame = ttk.Frame(root, padding=12)
        frame.grid(sticky="nsew")
        ttk.Label(frame, text="画像").grid(row=0, column=0, sticky="w")
        ttk.Entry(frame, textvariable=self.image_path, width=42).grid(
            row=0, column=1, padx=6
        )
        ttk.Button(frame, text="選択...", command=self.select_image).grid(
            row=0, column=2
        )

        ttk.Label(frame, text="送信先IP").grid(row=1, column=0, sticky="w", pady=(8, 0))
        ttk.Entry(frame, textvariable=self.target_ip, width=18).grid(
            row=1, column=1, sticky="w", padx=6, pady=(8, 0)
        )
        ttk.Label(frame, text="UDPポート").grid(row=1, column=2, sticky="w", pady=(8, 0))
        ttk.Entry(frame, textvariable=self.target_port, width=8).grid(
            row=1, column=3, sticky="e", pady=(8, 0)
        )

        self.send_button = ttk.Button(frame, text="画像を送信", command=self.send)
        self.send_button.grid(row=2, column=0, columnspan=4, pady=(12, 4))
        ttk.Label(frame, textvariable=self.status).grid(
            row=3, column=0, columnspan=4, sticky="w"
        )

    def select_image(self) -> None:
        path = filedialog.askopenfilename(
            title="送信する画像を選択",
            filetypes=[
                ("画像ファイル", "*.jpg *.jpeg *.png *.bmp *.gif *.tif *.tiff"),
                ("すべてのファイル", "*.*"),
            ],
        )
        if path:
            self.image_path.set(path)
            self.status.set(Path(path).name)

    def send(self) -> None:
        path = Path(self.image_path.get())
        if not path.is_file():
            messagebox.showerror("送信エラー", "画像ファイルを選択してください。")
            return

        try:
            target = str(ipaddress.IPv4Address(self.target_ip.get().strip()))
            port = int(self.target_port.get())
            if not 1 <= port <= 65535:
                raise ValueError("UDPポートは1から65535の範囲で指定してください。")
            jpeg = encode_image(path)
        except (OSError, ValueError) as error:
            messagebox.showerror("送信エラー", str(error))
            return

        self.send_button.configure(state="disabled")
        self.status.set(f"JPEG {len(jpeg)} bytes を送信中...")
        self.root.update_idletasks()

        def update_progress(sent: int, total: int) -> None:
            self.status.set(f"送信中: {sent}/{total} パケット")
            self.root.update_idletasks()

        try:
            chunks = send_jpeg(
                jpeg,
                target,
                port,
                update_progress,
            )
        except OSError as error:
            messagebox.showerror("送信エラー", str(error))
            self.status.set("送信に失敗しました。")
        else:
            self.status.set(f"送信完了: {chunks} パケット")
        finally:
            self.send_button.configure(state="normal")


def main() -> None:
    root = tk.Tk()
    SenderApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()

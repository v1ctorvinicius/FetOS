# fethub/transport/frame.py
import json

STX = 0x02
ETX = 0x03

class FrameDecoder:
    """Decodifica stream contínuo de bytes em frames STX/ETX."""
    def __init__(self):
        self.in_frame = False
        self.buffer = bytearray()

    def feed(self, byte: int):
        """Retorna um dict quando encontra ETX."""
        if byte == STX:
            self.in_frame = True
            self.buffer.clear()
            return None

        if byte == ETX and self.in_frame:
            self.in_frame = False
            try:
                txt = self.buffer.decode(errors="ignore")
                return json.loads(txt)
            except:
                return None

        if self.in_frame:
            self.buffer.append(byte)

        return None


def encode_frame(obj: dict) -> bytes:
    raw = json.dumps(obj)
    return bytes([STX]) + raw.encode() + bytes([ETX]) + b"\n"

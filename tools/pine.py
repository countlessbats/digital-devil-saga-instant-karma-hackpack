"""Minimal PCSX2 PINE client (TCP on Windows). Default slot 28012 = private test instance."""
import socket, struct

class Pine:
    def __init__(self, slot=28012):
        self.s = socket.create_connection(('127.0.0.1', slot), timeout=5)

    def _cmd(self, payload, reply_len):
        self.s.sendall(struct.pack('<I', len(payload) + 4) + payload)
        buf = b''
        while len(buf) < 4:
            buf += self.s.recv(4 - len(buf))
        n = struct.unpack('<I', buf)[0]
        while len(buf) < n:
            buf += self.s.recv(n - len(buf))
        if buf[4] != 0:
            raise IOError('PINE command failed')
        return buf[5:5 + reply_len]

    def r8(self, a):  return self._cmd(struct.pack('<BI', 0, a), 1)[0]
    def r16(self, a): return struct.unpack('<H', self._cmd(struct.pack('<BI', 1, a), 2))[0]
    def r32(self, a): return struct.unpack('<I', self._cmd(struct.pack('<BI', 2, a), 4))[0]
    def w8(self, a, v):  self._cmd(struct.pack('<BIB', 4, a, v), 0)
    def w16(self, a, v): self._cmd(struct.pack('<BIH', 5, a, v), 0)
    def w32(self, a, v): self._cmd(struct.pack('<BII', 6, a, v), 0)
    def status(self): return struct.unpack('<I', self._cmd(b'\x0f', 4))[0]  # 0 running,1 paused,2 shutdown
    def title(self):
        d = self._cmd(b'\x0b', 256); n = struct.unpack('<I', d[:4])[0]; return d[4:4 + n].rstrip(b'\0').decode()
    def load_state(self, slot): self._cmd(struct.pack('<BB', 0x0a, slot), 0)

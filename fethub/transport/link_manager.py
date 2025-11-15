# fethub/transport/link_manager.py
from fethub.transport.serial_link import SerialLink
from fethub.transport.tcp_client import TcpClientLink
from fethub.transport.tcp_server import TcpServerLink

class LinkManager:
    """
    Escolhe qual transporte usar.
    Expõe uma API unificada:
        .poll()
        .send(pkt)
    """

    def __init__(self, mode="serial", **kwargs):
        self.mode = mode
        self.link = None

        if mode == "serial":
            self.link = SerialLink(**kwargs)
            self.link.open()

        elif mode == "tcp-client":
            self.link = TcpClientLink(**kwargs)
            self.link.open()

        else:
            raise ValueError("Modo de transporte desconhecido")

    def poll(self):
        return self.link.poll()

    def send(self, pkt):
        return self.link.send(pkt)

import socket


"""
Mazec server connection details
"""
SERVER_DOMAIN = 'localhost'
SERVER_PORT = 1234

class LineRPCConnection(object):
    """
    Trida zajistujici zakladni textove RPC. Komunikace probiha po radcich,
    vzdy je prikaz, konec radku. Pak server odpovi, konec radku.
    """

    def __init__(self):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    def open(self):
        """
        Otevre spojeni se serverem.
        """

        ip = socket.gethostbyname(SERVER_DOMAIN)
        self.socket.connect((ip, SERVER_PORT))

    def close(self):
        """
        Uzavre spojeni se serverem.
        """

        self.socket.close()

    def send_msg(self, msg: str):
        """
        Posle textovy prikaz serveru.
        """

        if not msg.endswith('\n'):
            msg += '\n'
        self.socket.send(msg.encode('utf8'))
    
    def recv_msg(self) -> str:
        """
        Prijme odpoved serveru.
        """

        buffsize = 512
        data = bytearray()
        while len(data) == 0 or data[-1] != 0x0A:
            data += self.socket.recv(buffsize)
        return data.decode('utf8')[:-1]
    
    def send_and_recv_msg(self, msg) -> str:
        """
        Posle prikaz serveru a vrati jeho textovou odpoved.
        """

        self.send_msg(msg)
        return self.recv_msg()


class MazecException(Exception):
    pass

class MalformedResponseError(Exception):
    pass


class MazecConnection(LineRPCConnection):
    """
    Trida implementujici zakladni funkce protokolu na komunikaci s hernim serverem
    """

    def __init__(self):
        super(MazecConnection, self).__init__()

    def __enter__(self):
        super(MazecConnection, self).open()
        return self

    def __exit__(self, typ, value, traceback):
        super(MazecConnection, self).close()

    def __handle_response__(self, resp):
        if resp.startswith('OVER '):
            raise MazecException(resp[len('OVER '):])

    def user(self, username: str):
        """
        Povinne prvni prikaz spojeni, prihlasi hrace dle jmena
        """

        resp = super(MazecConnection, self).send_and_recv_msg('USER {}'.format(username))
        self.__handle_response__(resp)

    def level(self, level_name: str):
        """
        Povinne druhy prikaz spojeni, vybere aktualni LEVEL
        """

        resp = super(MazecConnection, self).send_and_recv_msg('LEVEL {}'.format(level_name))
        self.__handle_response__(resp)

    def wait(self):
        """
        Ceka na pripojeni vykreslovatka
        """

        resp = super(MazecConnection, self).send_and_recv_msg('WAIT')
        self.__handle_response__(resp)

    def get_width(self) -> int:
        """
        Vraci sirku hraciho pole
        """

        resp = super(MazecConnection, self).send_and_recv_msg('GETW')
        self.__handle_response__(resp)
        if resp.startswith('DATA '):
            width = int(resp[len('DATA '):])
            return width
        else:
            raise MalformedResponseError(resp)

    def get_height(self) -> int:
        """
        Vraci vysku hraciho pole
        """

        resp = super(MazecConnection, self).send_and_recv_msg('GETH')
        self.__handle_response__(resp)
        if resp.startswith('DATA '):
            height = int(resp[len('DATA '):])
            return height
        else:
            raise MalformedResponseError(resp)

    def get_player_x(self) -> int:
        """
        Vraci x souradnici polohy hrace
        """

        resp = super(MazecConnection, self).send_and_recv_msg('GETX')
        self.__handle_response__(resp)
        if resp.startswith('DATA '):
            x = int(resp[len('DATA '):])
            return x
        else:
            raise MalformedResponseError(resp)

    def get_player_y(self) -> int:
        """
        Vraci y souradnici polohy hrace
        """

        resp = super(MazecConnection, self).send_and_recv_msg('GETY')
        self.__handle_response__(resp)
        if resp.startswith('DATA '):
            y = int(resp[len('DATA '):])
            return y
        else:
            raise MalformedResponseError(resp)

    def get_tile_value(self, x: int, y: int) -> int:
        """
        Vraci hodnotu policka se souradnicemi x,y
        """

        resp = super(MazecConnection, self).send_and_recv_msg('WHAT {} {}'.format(x, y))
        self.__handle_response__(resp)
        if resp.startswith('DATA '):
            value = int(resp[len('DATA '):])
            return value
        else:
            raise MalformedResponseError(resp)

    def move_player(self, direction) -> str:
        """
        Posun hrace, prijima parametry 'w' , 's' ,'a', 'd' jako smer.

        Vraci None, pokud probehlo uspesne. Jinak textove vysvetleni, proc pohyb nelze provest.
        """

        resp = super(MazecConnection, self).send_and_recv_msg('MOVE {}'.format(direction))
        self.__handle_response__(resp)
        if resp == 'DONE':
            return None
        else:
            raise MalformedResponseError(resp)



"""Priklad pouziti tridy **MazecConnection**"""

with MazecConnection() as mc:
    mc.user('jmeno')
    mc.level('nejlehci')
    mc.get_height()
    mc.get_width()
    mc.get_player_x()
    mc.get_player_y()
    mc.get_tile_value(5, 10)
    mc.move_player('d')

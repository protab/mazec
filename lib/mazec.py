import socket
import types


class Mazec(object):
    """
    Herni konstanty a správa hry
    """

    SERVER_DOMAIN = 'localhost'
    SERVER_PORT = 1234
    UP = 'W'
    DOWN = 'S'
    LEFT = 'A'
    RIGHT = 'D'

    @staticmethod
    def run(username: str, level: str, loop: types.FunctionType):
        """
        Metoda implementujici zakladni pohybovy cyklus
        """

        move_result = None
        with MazecConnection() as mc:
            mc.user(username)
            mc.level(level)
            mc.wait()
            while True:
                move_result = mc.move(loop(mc, move_result))


class LineRPCConnection(object):
    """
    Trida zajistujici zakladni textove RPC. Komunikace probiha po radcich,
    vzdy je prikaz, konec radku. Pak server odpovi, konec radku.
    """

    def __init__(self):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    def open(self, server, port):
        """
        Otevre spojeni se serverem.
        """

        ip = socket.gethostbyname(server)
        self.socket.connect((ip, port))

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
        self.socket.send(msg.encode('ascii'))

    def recv_msg(self) -> str:
        """
        Prijme odpoved serveru.
        """

        buffsize = 512
        data = bytearray()
        while not data or data[-1] != 0x0A:
            data += self.socket.recv(buffsize)
        return data.decode('ascii')[:-1]

    def send_and_recv_msg(self, msg) -> str:
        """
        Posle prikaz serveru a vrati jeho textovou odpoved.
        """

        self.send_msg(msg)
        return self.recv_msg()


class MazecException(Exception):
    pass

class MazecConnection(LineRPCConnection):
    """
    Trida implementujici zakladni funkce protokolu na komunikaci s hernim serverem
    """

    def __init__(self):
        super(MazecConnection, self).__init__()

    def __enter__(self):
        super(MazecConnection, self).open(Mazec.SERVER_DOMAIN, Mazec.SERVER_PORT)
        return self

    def __exit__(self, typ, value, traceback):
        super(MazecConnection, self).close()

    def __handle_response__(self, resp):
        if resp == 'DONE':
            # vse je v poradku
            return
        elif resp.startswith('NOPE '):
            # server odmitl provest akci, ale level pokracuje
            return
        elif resp.startswith('DATA '):
            # v poradku, server posila data
            return
        elif resp.startswith('OVER '):
            # server ukoncuje spojeni
            raise MazecException(resp[len('OVER '):])
        else:
            # nedefinovana odpoved, ukoncujeme spojeni
            raise MazecException('Neocekavana odpoved serveru: {}'.format)

    def command(self, cmd) -> str:
        """ Posle serveru libovolny textovy prikaz, vrati textovou odpoved serveru"""

        resp = super(MazecConnection, self).send_and_recv_msg(cmd)
        self.__handle_response__(resp)
        return resp

    def user(self, username: str):
        """
        Povinne prvni prikaz spojeni, prihlasi hrace dle jmena
        """

        self.command('USER {}'.format(username))

    def level(self, level_name: str):
        """
        Povinne druhy prikaz spojeni, vybere aktualni LEVEL
        """

        self.command('LEVL {}'.format(level_name))

    def wait(self):
        """
        Ceka na pripojeni vykreslovatka
        """

        self.command('WAIT')

    def get_width(self) -> int:
        """
        Vraci sirku hraciho pole
        """

        resp = self.command('GETW')
        if resp.startswith('DATA '):
            width = int(resp[len('DATA '):])
            return width
        else:
            raise MazecException('Neocekavana odpoved serveru: {}'.format(resp))

    def get_height(self) -> int:
        """
        Vraci vysku hraciho pole
        """

        resp = self.command('GETH')
        if resp.startswith('DATA '):
            height = int(resp[len('DATA '):])
            return height
        else:
            raise MazecException('Neocekavana odpoved serveru: {}'.format(resp))

    def get_player_x(self) -> int:
        """
        Vraci x souradnici polohy hrace
        """

        resp = self.command('GETX')
        if resp.startswith('DATA '):
            x = int(resp[len('DATA '):])
            return x
        else:
            raise MazecException('Neocekavana odpoved serveru: {}'.format(resp))

    def get_player_y(self) -> int:
        """
        Vraci y souradnici polohy hrace
        """

        resp = self.command('GETY')
        if resp.startswith('DATA '):
            y = int(resp[len('DATA '):])
            return y
        else:
            raise MazecException('Neocekavana odpoved serveru: {}'.format(resp))

    def get_tile_value(self, x: int, y: int) -> int:
        """
        Vraci hodnotu policka se souradnicemi x,y
        """

        resp = self.command('WHAT {} {}'.format(x, y))
        if resp.startswith('DATA '):
            value = int(resp[len('DATA '):])
            return value
        else:
            raise MazecException('Neocekavana odpoved serveru: {}'.format(resp))

    def get_all_tile_values(self):
        """
        Vrací hodnoty všech policek mapy
        """

        resp = self.command('MAZE')
        if resp.startswith('DATA '):
            maze = [int(x) for x in resp[len('DATA '):].split(' ')]
            return maze
        else:
            raise MazecException('Neocekavana odpoved serveru: {}'.format(resp))


    def move(self, direction) -> str:
        """
        Posun, prijima parametry 'w' , 's' ,'a', 'd' jako smer.

        Vraci None, pokud probehlo uspesne. Jinak textove vysvetleni, proc pohyb nelze provest.
        """

        resp = self.command('MOVE {}'.format(direction))
        if resp == 'DONE':
            return None
        else:
            raise MazecException('Neocekavana odpoved serveru: {}'.format(resp))




if __name__ == "__main__":
    """Priklad pouziti tridy **MazecConnection**"""

    with MazecConnection() as mc:
        mc.user('jmeno')
        mc.level('nejlehci')
        mc.wait()
        mc.get_height()
        mc.get_width()
        mc.get_player_x()
        mc.get_player_y()
        mc.get_tile_value(5, 10)
        mc.get_all_tile_values()
        mc.move(Mazec.UP)
        mc.command('ZATANCUJ')

    """Priklad pouziti Mazec.run"""

    def main(mc: MazecConnection, move_result: str):
        return Mazec.UP

    Mazec.run('jmeno', 'level', main)

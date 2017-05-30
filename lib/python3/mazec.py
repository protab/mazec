import socket
import types
from typing import List

SERVER_DOMAIN = 'localhost'
SERVER_PORT = 1234

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


class MazecException(Exception):
    pass

class Mazec(LineRPCConnection):
    """
    Sprava a ovladani hry
    """

    """Nahoru"""
    UP = 'W'
    """Dolu"""
    DOWN = 'S'
    """Doleva"""
    LEFT = 'A'
    """Doprava"""
    RIGHT = 'D'

    def __init__(self, username: str, level: str):
        super().__init__()

        self._username = username
        self._level_name = level
        """Vyska hraciho pole"""
        self.height = -1
        """Sirka hraciho pole"""
        self.width = -1
        """Chybova hlaska posledniho pokusu o pohyb"""
        self.error = None

    def __enter__(self):
        super().open(SERVER_DOMAIN, SERVER_PORT)
        self._user(self._username)
        self._level(self._level_name)
        self._wait()
        self.height = self._get_height()
        self.width = self._get_width()
        return self

    def __exit__(self, typ, value, traceback):
        super().close()

    def _handle_response(self, resp):
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
            raise MazecException('Neocekavana odpoved serveru: {}'.format(resp))

    def _command(self, cmd) -> str:
        """ Posle serveru libovolny textovy prikaz, vrati textovou odpoved serveru"""

        super().send_msg(cmd)
        resp = super().recv_msg()
        self._handle_response(resp)
        return resp

    def _user(self, username: str):
        """
        Povinne prvni prikaz spojeni, prihlasi hrace dle jmena
        """

        self._command('USER {}'.format(username))

    def _level(self, level_name: str):
        """
        Povinne druhy prikaz spojeni, vybere aktualni LEVEL
        """

        self._command('LEVL {}'.format(level_name))

    def _wait(self):
        """
        Ceka na pripojeni vykreslovatka
        """

        self._command('WAIT')

    def _get_width(self) -> int:
        """
        Vraci sirku hraciho pole
        """

        resp = self._command('GETW')
        if resp.startswith('DATA '):
            width = int(resp[len('DATA '):])
            return width
        else:
            raise MazecException('Neocekavana odpoved serveru: {}'.format(resp))

    def _get_height(self) -> int:
        """
        Vraci vysku hraciho pole
        """

        resp = self._command('GETH')
        if resp.startswith('DATA '):
            height = int(resp[len('DATA '):])
            return height
        else:
            raise MazecException('Neocekavana odpoved serveru: {}'.format(resp))

    def _get_list_of_all_values(self) -> List[int]:
        resp = self._command('MAZE')
        if resp.startswith('DATA '):
            maze = [int(x) for x in resp[len('DATA '):].split(' ')]
            return maze
        else:
            raise MazecException('Neocekavana odpoved serveru: {}'.format(resp))

    def _move_command(self, direction) -> str:
        """
        Posun, prijima parametry 'w' , 's' ,'a', 'd' jako smer.

        Vraci None, pokud probehlo uspesne. Jinak textove vysvetleni, proc pohyb nelze provest.
        """

        resp = self._command('MOVE {}'.format(direction))
        if resp == 'DONE':
            return None
        elif resp.startswith('NOPE '):
            return resp[len('NOPE '):]
        else:
            raise MazecException('Neocekavana odpoved serveru: {}'.format(resp))

    def get_x(self) -> int:
        """
        Vraci x souradnici polohy
        """

        resp = self._command('GETX')
        if resp.startswith('DATA '):
            x = int(resp[len('DATA '):])
            return x
        else:
            raise MazecException('Neocekavana odpoved serveru: {}'.format(resp))

    def get_y(self) -> int:
        """
        Vraci y souradnici polohy
        """

        resp = self._command('GETY')
        if resp.startswith('DATA '):
            y = int(resp[len('DATA '):])
            return y
        else:
            raise MazecException('Neocekavana odpoved serveru: {}'.format(resp))

    def get_value(self, x: int, y: int) -> int:
        """
        Vraci hodnotu policka se souradnicemi x,y
        """

        resp = self._command('WHAT {} {}'.format(x, y))
        if resp.startswith('DATA '):
            value = int(resp[len('DATA '):])
            return value
        else:
            raise MazecException('Neocekavana odpoved serveru: {}'.format(resp))

    def get_all_values(self) -> List[List[int]]:
        """
        Vrátí všechny hodnoty hracího pole. Jedná se o pole sloupců hodnot.
        Tzn. přístup na hodnotu pole x,y probíhá takto:

        hodnota = mapa[x][y]
        """

        values = self._get_list_of_all_values()
        res = [[] for x in range(self.width)]
        for i, v in enumerate(values):
            res[i % self.width].append(v)
        return res

    def move(self, direction: str) -> bool:
        """
        Pohybová metoda, přijímá hodnoty:

        Mazec.UP
        Mazec.DOWN
        Mazec.LEFT
        Mazec.RIGHT

        Návratovou hodnotou je `True` v případě úspěchu, `False` v případě neúspěchu.
        Případné chybové hlášky se ukládají do proměnné `error`.
        """

        r = self._move_command(direction)
        self.error = r
        return not r

    @staticmethod
    def run(username: str, level: str, loop: types.FunctionType):
        """
        Funkce implementujici zakladni pohybovy cyklus.

        Jako poslední parametr přijímá funkci, jejíž návratovou hodnotou
        by měl být směr dalšího kroku. Tato funkce je volána stále dokola,
        dokud není hra ukončena chybou či úspěchem.
        """

        success = True
        with Mazec(username, level) as m:
            while True:
                success = m.move(loop(m, success, m.error))

import socket

PORT = 4000


class MazeException(Exception):
    """Výjimka, která je vyvolána, pokud server vrátí fatální chybu, která
    neumožňuje pokračovat v procházení úlohy."""
    pass


class Connect:
    """Naváže jedno spojení do úlohy a vrátí toto spojení. Se spojením je pak možno
       dále pracovat - posílat do něj příkazy.

       Příklad:

       import maze
       c = maze.Connect('muj_login', 'kod_ulohy')
       print('Šířka hrací plochy je', c.width)
       print('Výška hrací plochy je', c.height)
       moje_x = c.x()
       moje_y = c.y()
       print('Nacházíš se na souřadnicích', moje_x, moje_y)
       print('Políčko pod tebou má hodnotu', c.get(moje_x, moje_y))
       print('Čekám, až klikneš na webu na tlačítko Spustit:')
       c.wait()
       if not c.move('w'):
           print('Posun nahoru se nepodařil, protože:', c.error)

       Důležité proměnné:

       width  - šířka hrací plochy
       height - výška hrací plochy
       error  - pokud move() vrátilo False, obsahuje popis chyby"""


    def __init__(self, login, code, host='protab.'):
        """Konstruktor objektu. Parametry:
           login - tvé přihlašovací jméno
           code  - kód úlohy, ke které se chceš připojit
           host  - nepovinný parametr, není třeba uvádět, slouží pouze pro testování"""
        self.sock = socket.create_connection((host, PORT))
        self._communicate('USER', login)
        self._communicate('LEVL', code)
        self.width = self._get_int('GETW')
        self.height = self._get_int('GETH')


    def _communicate(self, command, *args):
        self.error = None

        request = command
        for a in args:
            request += ' ' + str(a)
        request += '\n'
        self.sock.send(request.encode())

        response = b''
        while not response or response[-1] != ord(b'\n'):
            data = self.sock.recv(65536)
            if len(data) == 0:
                raise MazeException('Server uzavřel spojení.')
            response += data
        response = response.decode().rstrip()

        if response == 'DONE':
            return True

        result, data = response.split(maxsplit=1)

        if result == 'NOPE':
            self.error = data
            return False
        if result == 'OVER':
            self.error = data
            self.sock.close()
            raise MazeException(data)
        if result == 'DATA':
            return data.split()

        raise KeyError('Invalid response from server: {}'.format(response))


    def _get_int(self, command, *args):
        return int(self._communicate(command, *args)[0])


    def wait(self):
        """Zobrazí na webu tlačítko Spustit a počká, až ho zmáčkneš. Do té doby
           se tato funkce nevrátí. Pokud je funkce wait() zavolána brzy po startu
           úlohy, je zapauzován čas. Pokud je zavolána později, čas běží i během
           čekání."""
        self._communicate('WAIT')


    def x(self):
        """Vrátí souřadnici x (vodorovnou) tvé aktuální polohy. Nula je vlevo."""
        return self._get_int('GETX')


    def y(self):
        """Vrátí souřadnici y (svislou) tvé aktuální polohy. Nula je nahoře."""
        return self._get_int('GETY')


    def get(self, x, y):
        """Vrátí hodnotu políčka na souřadnicích x, y. Hodnotou je číslo. Čísla
           mohou mít v každé úloze jiný význam."""
        return self._get_int('WHAT', x, y)


    def get_all(self):
        """Vrátí celou hrací plochu jako dvojrozměrné pole (seznam seznamů).
           Příklad použití:

           all = c.get_all()
           print('Hodnota políčka na souřadnicích 2, 3 je:', all[2][3])"""
        data = self._communicate('MAZE')
        result = []
        for i in range(self.width):
            column = []
            for j in range(self.height):
                column.append(int(data[j * self.width + i]))
            result.append(column)
        return result


    def move(self, key):
        """Posune tě daným směrem (případně posune něco nějak, závisí na úloze).
           Parametr key je řetězec, který obsahuje jeden znak. Pozor na to, že
           malá a velká písmena nejsou totéž."""
        return self._communicate('MOVE', key)

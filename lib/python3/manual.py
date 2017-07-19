from mazec import Mazec
import getch
import time

counter = 0

def run(m, s, e):
    print(s, e)
    return getch.getch()

Mazec.run('test',input('Zadej jmeno levelu: '), run, use_wait=False)

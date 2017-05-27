from mazec import Mazec

"""Priklad pouziti vlastniho cyklu"""

with Mazec('jmeno', 'level') as m:
    m.height   # vyska levelu
    m.width    # sirka levelu

    m.get_x()  # aktualni x souradnice
    m.get_y()  # aktualni y souradnice

    value = m.get_value(5, 10)  # hodnota policka x=5, y=10
    maze = m.get_all_values()   # hodnoty vsech policek
    maze[5][10]                 # hodnota pole na souradnicich x=5, y=10

    success = m.move(Mazec.UP)  # pokus o pohyb, vraci uspech nebo neuspech
    if not success:
        print(m.error)          # v pripade neuspechu je toto chybova hlaska


"""
Priklad pouziti s pohybovym cyklem.

V tomto případě je každým krokem hry posun nahoru.
"""

def main(m: Mazec, success: bool, error: str):
    return Mazec.UP

Mazec.run('jmeno', 'level', main)
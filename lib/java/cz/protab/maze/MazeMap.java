package cz.protab.maze;

public interface MazeMap {

    /**
     * @return šířka mapy
     */
    int getWidth();

    /**
     * @return výška mapy
     */
    int getHeight();

    /**
     * Vrátí barvu, která se nachází na souřadnicích x, y
     *
     * @param x dotazovaný sloupec
     * @param y dotazovaný řádek
     * @return číslo barvy
     */
    int valueAt(int x, int y) throws IndexOutOfBoundsException;

}

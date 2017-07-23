package cz.protab.maze;

import java.util.Scanner;

public class PreparsedMazeMap implements MazeMap {

    /**
     * Mapa uložená po řádcích.
     */
    private final int[][] map;

    private final int width, height;

    /**
     * Vyrobí mapu z její textové reprezentace.
     *
     * @param width šířka mapy
     * @param height výška mapy
     * @param data width*height čísel oddělených mezerou
     */
    PreparsedMazeMap(int width, int height, String data) {
        this.width = width;
        this.height = height;

        Scanner scanner = new Scanner(data);

        map = new int[height][width];
        for (int row = 0; row < height; ++row)
            for (int col = 0; col < width; ++col)
                map[row][col] = scanner.nextInt();
    }

    @Override
    public int getWidth() {
        return width;
    }

    @Override
    public int getHeight() {
        return height;
    }

    @Override
    public int valueAt(int x, int y) throws IndexOutOfBoundsException {
        return map[y][x];
    }
}

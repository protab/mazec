import cz.protab.maze.*;

import java.util.logging.Level;

public class Example {

    private static Maze maze;

    private static char kudyDal() {
        System.out.println("Nacházím se na: " + maze.getX() + ", " + maze.getY());
        System.out.println("Mapa je velka " + maze.getWidth() + "x" + maze.getHeight());
        System.out.println("Na (0,0) je :" + maze.valueAt(0, 0));

        // Přečti ze serveru celou mapu najednou - je to rychlejší, pokud je v plánu ptát se na více políček
        MazeMap map = maze.getAllValues();

        // Vypiš celou mapu na konzoli,  (0,0) bude vlevo nahoře
        for (int y = 0; y < maze.getHeight(); y++) {
            for (int x = 0; x < maze.getWidth(); x++)
                System.out.print(" " + map.valueAt(y, x));
            System.out.println();
        }

        // Pohni se nahoru.
        return Maze.UP;
    }

    public static void main(String[] args) {
        try {
            // Zapni podrobnější výpisy toho, co knihovna dělá
            Maze.getLogger().setLevel(Level.INFO);

            // Připoj se k serveru s daným jménem, otevři level
            maze = new Maze("username", "level");

            // Dokonči level, používej funkci kudyDal pro zjištění dalšího směru
            String report = maze.finishLevelWith(Example::kudyDal);

            // Vypiš, jak hra dopadla
            System.out.println(report);
        } catch (Maze.ConnectionError e) {
            System.err.println(e.getMessage());
        }
    }
}

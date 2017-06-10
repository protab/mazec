package cz.protab.maze;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.InetAddress;
import java.net.Socket;
import java.util.logging.Level;
import java.util.logging.Logger;

public class Maze implements MazeMap, AutoCloseable {

    /** Pohybové příkazy, viz {@link Maze#move} */
    public static final char UP = 'W';
    public static final char DOWN = 'S';
    public static final char LEFT = 'A';
    public static final char RIGHT = 'D';

    /**
     * Logger pro všechny instance Maze. Debugovací informace zapneš třeba takto:
     *
     *      Maze.getLogger().setLevel(Level.FINEST);
     *
     * Naopak zcela vypnout jde třeba takto:
     *
     *      Maze.getLogger().setFilter((foo) -> false);
     */
    private static final Logger logger = Logger.getLogger("Maze");

    private static final String serverHostname = "localhost";
    private static final int serverPort = 4000;

    private final Socket socket;
    private final BufferedReader reader;
    private final PrintWriter writer;

    private Integer width = null;
    private Integer height = null;

    /**
     * Pomocná knihovna k celotáborové hře - instance jedné hry.
     *
     * @param username tvé uživatelské jméno
     * @param level kód levelu
     *
     * @throws ConnectionError Pokud dojde k chybě v připojení
     * @throws GameOver Pokud došlo okamžitě k ukončení hry (např. špatný kód levelu)
     */
    public Maze(String username, String level) throws ConnectionError {
        try {
            socket = new Socket(InetAddress.getByName(serverHostname), serverPort);
            reader = new BufferedReader(new InputStreamReader(socket.getInputStream()));
            writer = new PrintWriter(socket.getOutputStream());

            logger.log(Level.INFO, "Connected, sending username...");
            voidCommand("USER %s", username);
            logger.log(Level.INFO, "Logged in as user " + username + ". Sending level code...");
            voidCommand("LEVL %s", level);
            logger.log(Level.INFO, "Started level " + level + ".");
        } catch (IOException e) {
            throw new ConnectionError("Nelze se připojit k serveru: " + e.getMessage(), e);
        }
    }

    /**
     * Vrátí šířku mapy. Pokud zatím není známá, zjistí ji.
     * @return výška mapy
     *
     * @throws ConnectionError pokud dojde k chybě v připojení
     * @throws GameOver pokud došlo k ukončení hry
     */
    @Override
    public int getWidth() throws ConnectionError, GameOver {
        return width == null ? width = intCommand("GETW") : width;
    }

    /**
     * Vrátí výšku mapy. Pokud zatím není známá, zjistí ji.
     * @return výška mapy
     *
     * @throws ConnectionError pokud dojde k chybě v připojení
     * @throws GameOver pokud došlo k ukončení hry
     */
    @Override
    public int getHeight() throws ConnectionError, GameOver {
        return height == null ? height = intCommand("GETH") : height;
    }

    /**
     * Zeptá se serveru, co se nachází na políčku.
     *
     * @param x dotazovaný sloupec
     * @param y dotazovaný řádek
     * @return barva na políčku (x, y)
     *
     * @throws ConnectionError pokud dojde k chybě v připojení
     * @throws GameOver pokud došlo k ukončení hry
     */
    @Override
    public int valueAt(int x, int y) throws ConnectionError, GameOver {
        return intCommand("WHAT %d %d", x, y);
    }

    /**
     * Vrátí aktuální pozici - sloupec
     *
     * @return číslo v rozsahu 0 - width
     * @throws ConnectionError pokud dojde k chybě v připojení
     * @throws GameOver pokud došlo k ukončení hry
     */
    public int getX() throws ConnectionError, GameOver {
        return intCommand("GETX");
    }

    /**
     * Vrátí aktuální pozici - řádek
     *
     * @return číslo v rozsahu 0 - height
     * @throws ConnectionError pokud dojde k chybě v připojení
     * @throws GameOver pokud došlo k ukončení hry
     */
    public int getY() throws ConnectionError, GameOver {
        return intCommand("GETY");
    }

    public MazeMap getAllValues() {
        final int width = getWidth();
        final int height = getHeight();

        String response = command("MAZE");
        assertFormat("DATA [0-9 ]+", response);
        return new PreparsedMazeMap(width, height, response.substring(5));
    }

    /**
     * Odešli příkaz, vrať odpověď. Pokud dojde k ukončení hry,
     * vyhodí výjjimku.
     *
     * @param format printf-like formátovací řetězec
     * @param args   argumenty do formátu
     * @return řádek s odpověďí
     * @throws GameOver        pokud dojde příkazem k ukončení hry
     * @throws ConnectionError pokud dojde k nečekané chybě
     */
    public String command(String format, Object... args) throws ConnectionError, GameOver {
        try {
            logger.log(Level.FINE, "Sending command " + format);
            writer.printf(format, args);
            writer.write('\n');
            writer.flush();
            socket.getOutputStream().flush();
            final String response = reader.readLine();
            if (response == null)
                throw new ConnectionError("Server ukončil spojení.");
            if (response.startsWith("OVER"))
                throw new GameOver(response.substring(5));
            return response;
        } catch (IOException e) {
            throw new ConnectionError("Chyba v komunikaci knihovny se serverem: " + e.getMessage());
        }
    }

    /**
     * Odešli příkaz, na který se očekává odpověď DATA (int)
     *
     * @param format printf-like formátovací řetězec
     * @param args   argumenty do formátu
     * @return číslo, které přišlo v odpovědi
     */
    public int intCommand(String format, Object... args) throws ConnectionError, GameOver {
        final String response = command(format, args);
        assertFormat("DATA \\-?[0-9]+", response);
        return Integer.parseInt(response.substring(5));
    }

    /**
     * Odešli příkaz, na který se očekává odpověď DONE
     *
     * @param format printf-like formátovací řetězec
     * @param args   argumenty do formátu
     */
    public void voidCommand(String format, Object... args) throws ConnectionError, GameOver {
        assertFormat("DONE", command(format, args));
    }

    /**
     * Zajistí, že odpověď odpovídá očekávanému tvaru.
     *
     * @param pattern Regulární výraz, popisující očekávanou odpověď
     */
    private void assertFormat(String pattern, String response) {
        if (!response.matches(pattern))
            throw new ConnectionError("Chyba v protokolu: odpověď (" + response + ") nevyhovuje očekávanému " + pattern);
    }

    private String lastMoveError = null;

    /**
     * Vrátí text poslední chyby, ke které došlo během pohybu.
     *
     * @return chybová hláška
     */
    public String getLastMoveError() {
        return lastMoveError;
    }

    /**
     * Pokusí se provést pohyb. Dojde-li k chybě, je možné zjistit příčinu voláním {@link Maze#getLastMoveError}.
     *
     * @param direction směr pohybu
     * @return zdali se provedlo pohyb provést
     * @throws GameOver pokud dojde pohybem k ukončení hry
     * @throws ConnectionError         pokud dojde k nečekané chybě
     */
    public boolean tryMove(char direction) throws GameOver, ConnectionError {
        String response = command("MOVE %c", direction);
        assertFormat("(DONE|NOPE .*)", response);
        if (response.startsWith("NOPE")) {
            lastMoveError = response.substring(5);
            return false;
        } else {
            return true;
        }
    }

    /**
     * Provede pohyb. Pokud selže, vyhodí výjjimku.
     *
     * @param direction směr pohybu
     * @throws MoveForbidden     pokud nelze provést pohyb
     * @throws GameOver pokud dojde pohybem k ukončení hry
     * @throws ConnectionError         pokud dojde k nečekané chybě
     */
    public void move(char direction) throws GameOver, ConnectionError, MoveForbidden {
        if (!tryMove(direction))
            throw new MoveForbidden(getLastMoveError());
    }

    /**
     * Metoda, která vrátí další pohyb.
     */
    @FunctionalInterface
    public interface MoveDecider {
        char getNextMove();
    }

    /**
     * Použije předanou metodu jako rozhodovač, kudy se vydat dál.
     * <p>
     * Knihovna bude volat tuto metodu a provádet pohyby které vrátí,
     * dokud neskončí hra.
     *
     * @param decider metoda, která rozhodne, jakým směrem se vydat
     * @throws GameOver až dojde ke konci hry
     * @throws MoveForbidden     pokud dojde k pohybu, který nelze provést
     * @throws ConnectionError         pokud dojde k chybě
     */
    public String finishLevelWith(MoveDecider decider) throws GameOver, ConnectionError, MoveForbidden {
        // Počkej, než hra začne
        voidCommand("WAIT");

        try {
            // Hra končí výjimkou
            while (true) {
                // Zavolej funkci, proveď pohyb
                move(decider.getNextMove());
            }
        } catch (GameOver | MoveForbidden e) {
            return e.getMessage();
        }
    }

    /**
     * Vrátí logger, kterým lze ovládat výstup z knihovny.
     * @return knihovní logger
     */
    public static Logger getLogger() {
        return logger;
    }

    /**
     * Uzavře spojení. Můzete volat, pokud chcete být vychovaní.
     * Doporučeným postupem je využít AutoCloseable interface, tedy:
     * <p>
     * <code>
     *      try (Maze m = Maze.connect(...)) {
     *         ...
     *      }
     * </code>
     * <p>
     * Které spojení uzavře samo, jakmile vyskočí z try bloku.
     *
     * @throws ConnectionError pokud i uzavření spojení selže
     */
    public void close() throws ConnectionError {
        try {
            socket.close();
        } catch (IOException e) {
            throw new ConnectionError("Selhalo uzavření spojení: " + e.getMessage(), e);
        }
    }


    public class MoveForbidden extends RuntimeException {
        MoveForbidden(String reason) {
            super(reason);
        }
    }

    public class GameOver extends RuntimeException {
        GameOver(String s) {
            super(s);
        }
    }

    public static class ConnectionError extends java.lang.Error {
        ConnectionError(String s) {
            super(s);
        }
        ConnectionError(String msg, Exception e) {
            super(msg, e);
        }
    }
}

using System;
using System.Linq;

namespace Maze.Client
{
    public class MazeClient
    {
        readonly MazeConnection connection;

        public MazeClient(MazeConnection connection)
        {
            this.connection = connection;
        }

        /// <summary> Připojí tě na server, přihlásí, vybere level a počká až se level spustí. </summary>
        public static MazeClient Connect(string userName, string level, string hostname, ushort port = 4000, bool waitForStart = true)
        {
            var client = new MazeClient(MazeConnection.Connect(hostname, port));
            client.SetUser(userName);
            client.SetLevel(level);
            if (waitForStart) client.WaitForStart();
            return client;
        }

        int? width;
        /// <summary> Šířka bludiště </summary>
        public int Width => (width ?? (width = connection.SendDataMessage("GETW").Single()).Value);
        int? height;
        /// <summary> Výška bludiště </summary>
        public int Height => (height ?? (height = connection.SendDataMessage("GETH").Single()).Value);
        /// <summary> Přihlásí uživatele, ale dělá to za tebe `Connect`, takže to asi nebudeš potřebovat. </summary>
        public void SetUser(string userName) => connection.SendMessage("USER", userName);
        /// <summary> Vybere level, ale dělá to za tebe `Connect`, takže to asi nebudeš potřebovat. </summary>
        public void SetLevel(string levelName) => connection.SendMessage("LEVL", levelName);
        /// <summary> Počká na začátek hry, ale dělá to za tebe `Connect`, takže to asi nebudeš potřebovat. </summary>
        public void WaitForStart() => connection.SendMessage("WAIT");
        /// <summary> Vrátí aktuální X-ovou souřadnici hráče </summary>
        public int GetX() => connection.SendDataMessage("GETX").Single();
        /// <summary> Vrátí aktuální Y-ovou souřadnici hráče </summary>
        public int GetY() => connection.SendDataMessage("GETY").Single();
        /// <summary> Vrátí aktuální souřadnice hráče </summary>
        public (int x, int y) GetPosition() => (GetX(), GetY());
        /// <summary> Vrátí hodnotu na vybraném políčku. </summary>
        public int ValueAt(int x, int y) => connection.SendDataMessage("WHAT", $"{x} {y}").Single();
        /// <summary> Vrátí hodnotu na vybraném políčku. </summary>
        public int ValueAt((int x, int y) position) => ValueAt(position.x, position.y);
        /// <summary> 
        /// Vrátí mapu celého (viditelného) bludiště.
        /// Jedno pole se z ní dostane normálně indexy `mapa[x, y]`, jakoby to bylo pole. A 
        /// </summary>
        public MazeMap GetMaze() => new MazeMap(connection.SendDataMessage("MAZE"), Width, Height);
        /// <summary> Provede pohyb, vrátí výsledek typu <see cref="OkMoveResult" /> když proběhl v pořádku a nebo <see cref="ErrorMoveResult" />, když to z nějakého důvodu nešlo (v Message je ten důvod, asi). Použít se dá například takto:
        /// <code>
        /// switch(maze.Move('w')) {
        ///     case ErrorMoveResult error: Console.WriteLine($"Hmm, nejde to: {error.Message}"); break;
        ///     case OkMoveResult: Console.WriteLine("Tak óká"); break;
        /// } </code>
        /// a nebo takto:
        /// <code>
        /// if (maze.Move('w') is ErrorMoveResult error)
        ///     Console.WriteLine($"Prý to nejde: {error.Message}.")
        /// </code>
        /// nebo jednodušše:
        /// <code>
        /// while (maze.Move('w') is OkMoveResult)
        ///     Console.WriteLine($"Jdu nahoru.")
        /// </code>
        /// </summary>
        public MoveResult Move(char direction)
        {
            var (result, arg) = connection.SendMessage("MOVE", direction.ToString());
            switch(result)
            {
                case "DONE": return new OkMoveResult();
                case "NOPE": return new ErrorMoveResult(arg);
                default: throw new Exception("Tohle by se asi nemělo stát");
            }
        }
    }

    public abstract class MoveResult { }
    public sealed class OkMoveResult : MoveResult { }
    public sealed class ErrorMoveResult : MoveResult
    {
        public ErrorMoveResult(string message)
        {
            this.Message = message;
        }
        public string Message { get; }
    }

    public class MazeMap
    {
        public int[] Data { get; }
        public int Width { get; }
        public int Height { get; }

        public MazeMap(int[] data, int width, int height)
        {
            if (data.Length != width * height) throw new ArgumentException($"Délka dat nesedí na výšku a šířku, asi je to nějaký bug v serveru nebo knihovně.");
            this.Data = data;
            this.Height = height;
            this.Width = width;
        }

        /// <summary> Vrátí hodnotu políčka na souřadnicích (x, y) </summary>
        public int this[int x, int y] => Data[x + y * Width];

        public int? TryGet(int x, int y) => x < 0 || y < 0 || x >= Width || y >= Height ? (int?)null : this[x, y];

        /// <summary>
        /// Projde všechna políčka. Dí se použít nějak takto:
        /// <code>
        /// map.ForAll((position, tile) => {
        ///     Console.WriteLine($"Políčko ({position.x}, {position.y}) má hodnotu {tile}");
        /// });
        /// </code>
        /// </summary>
        public void ForAll(Action<(int x, int y), int> callback)
        {
            int i = 0;
            for(int y = 0; y < Height; y++)
                for (int x = 0; x < Width; x++)
                    callback((x, y), Data[i++]);
        }
    }
}

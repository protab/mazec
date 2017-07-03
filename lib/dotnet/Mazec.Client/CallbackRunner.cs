using System;
using System.Linq;

namespace Mazec.Client
{
    public static class CallbackRunner
    {
        public static void Run(this MazeClient client, Func<MazeClient, char> runner)
        {
            Console.WriteLine("Hra začala");
            try
            {
                while (true) {
                    var l = runner(client);
                    if (client.Move(l) is ErrorMoveResult error)
                        Console.WriteLine(error.Message);
                }
            }
            catch(GameOverException exception)
            {
                Console.WriteLine($"GameOver: {exception.Message}");
            }
        }
    }
}
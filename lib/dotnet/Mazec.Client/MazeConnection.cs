using System;
using System.Linq;
using System.IO;
using System.Net.Sockets;
using System.Text;

namespace Mazec.Client
{
    public class MazeConnection
    {
        readonly TcpClient tcpConnection;
        readonly NetworkStream stream;
        public MazeConnection(TcpClient tcpConnection)
        {
            this.tcpConnection = tcpConnection;
            this.stream = tcpConnection.GetStream();
        }

        public static MazeConnection Connect(string hostName, int port = 4000)
        {
            var tcp = new TcpClient();
            tcp.ConnectAsync(hostName, port).Wait();
            tcp.SendBufferSize = tcp.ReceiveBufferSize = 1;
            return new MazeConnection(tcp);
        }

        public string SendRawMesage(string msg)
        {
            using(var writer = new StreamWriter(stream, Encoding.ASCII, 64, true))
                writer.Write(msg + "\n");
            stream.Flush();
            using(var reader = new StreamReader(stream, Encoding.ASCII, false, 64, true))
                return reader.ReadLine() ?? "OVER spojení ukončeno";
        }

        public (string, string) SendMessage(string message, string args = "", bool handleGameOverError = true)
        {
            var result = SendRawMesage($"{message} {args}".Trim()).Split(new [] { ' ' }, 2);
            if (handleGameOverError && result[0] == "OVER") throw new GameOverException(result[1]);
            return (result[0], result.Length > 1 ? result[1] : "");
        }

        public int[] SendDataMessage(string message, string args = "")
        {
            var (msg, data) = SendMessage(message, args);
            if (msg != "DATA") throw new InvalidOperationException($"Čekal jsem, že dostanu DATA, ne {msg}");
            return data.Split(' ').Select(int.Parse).ToArray();
        }
    }

    class GameOverException: Exception
    {
        public GameOverException(string message) : base(message) {}
    }
}

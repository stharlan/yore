// See https://aka.ms/new-console-template for more information
using System.Net;
using System.Net.Sockets;

Console.WriteLine("rt yore tester");

HttpClient client = new HttpClient();

int loops = 1;
int.TryParse(args[0], out loops);

Uri reqUri = new Uri("http://127.0.0.1:27015");

// straight request
for(int i = 0; i < loops; i++)
{
    String result = await client.GetStringAsync(reqUri);
    Console.WriteLine($"got {result.Length} bytes");
}

// random bytes 1024 (less than max)
int[] sendLength = { 1024, 8196, 16000 };
foreach(int sl in sendLength)
{
    Console.WriteLine($"send length = {sl}");
    for (int i = 0; i < loops; i++)
    {
        Console.WriteLine("Sending random bytes");
        var rand = new Random();
        byte[] r = new byte[sl];
        rand.NextBytes(r);
        ArraySegment<byte> buffer = new ArraySegment<byte>(r);

        Socket s = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
        await s.ConnectAsync(new IPEndPoint(IPAddress.Loopback, 27015));
        await s.SendAsync(buffer, SocketFlags.None);
        s.Close();
        s.Dispose();
    }
}

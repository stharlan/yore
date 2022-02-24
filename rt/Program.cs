// See https://aka.ms/new-console-template for more information
using System.Net;
using System.Net.Sockets;

Console.WriteLine("rt yore tester");

HttpClient client = new HttpClient();

Console.WriteLine("========================================");
Console.WriteLine("Testing get /");
Uri reqUri = new Uri("http://127.0.0.1:27015");
String result = await client.GetStringAsync(reqUri);
Console.WriteLine($"got {result.Length} bytes");

// random bytes 1024 (less than max)
int[] sendLength = { 1024, 8196, 16000 };
foreach(int sl in sendLength)
{
    Console.WriteLine("========================================");
    Console.WriteLine($"Testing send {sl} random bytes");
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

Console.WriteLine("========================================");
Console.WriteLine("Testing get /index.html");
reqUri = new Uri("http://127.0.0.1:27015/index.html");
result = await client.GetStringAsync(reqUri);
Console.WriteLine($"got {result.Length} bytes");

Console.WriteLine("========================================");
Console.WriteLine("Testing get /index1.html");
reqUri = new Uri("http://127.0.0.1:27015/index1.html");
try
{
    result = await client.GetStringAsync(reqUri);
}
catch (HttpRequestException ex)
{
    if(ex.StatusCode != HttpStatusCode.NotFound)
    {
        Console.WriteLine($"ERROR: No 404!");
    }
    else
    {
        Console.WriteLine($"Not found. Ok.");
    }
}


// See https://aka.ms/new-console-template for more information
using System.Net;
using System.Net.Sockets;
using System.Text.Unicode;
using Xunit;

Console.WriteLine("rt yore tester");

var SocketSend = async (String p, String expectedStatusCode) =>
{
    Console.WriteLine("========================================");
    Console.WriteLine($"Testing '{p}'");
    ArraySegment<byte> buffer = new ArraySegment<byte>(System.Text.Encoding.UTF8.GetBytes(p));
    Socket s = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
    await s.ConnectAsync(new IPEndPoint(IPAddress.Loopback, 27015));
    await s.SendAsync(buffer, SocketFlags.None);
    byte[] rcvBuffer = new byte[8196];
    ArraySegment<byte> rcvBufferSeg = new ArraySegment<byte>(rcvBuffer);
    int bytesRcvd = await s.ReceiveAsync(rcvBufferSeg, SocketFlags.None);
    String respString = System.Text.Encoding.UTF8.GetString(rcvBuffer);
    Assert.True(respString.StartsWith($"HTTP/1.1 {expectedStatusCode}"));
    s.Close();
    s.Dispose();
};

var SocketSendBytes = async (byte[] pb, String expectedStatusCode) =>
{
    Console.WriteLine("========================================");
    Console.WriteLine($"Testing packet of bytes");
    ArraySegment<byte> buffer = new ArraySegment<byte>(pb);
    Socket s = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
    await s.ConnectAsync(new IPEndPoint(IPAddress.Loopback, 27015));
    await s.SendAsync(buffer, SocketFlags.None);
    byte[] rcvBuffer = new byte[8196];
    ArraySegment<byte> rcvBufferSeg = new ArraySegment<byte>(rcvBuffer);
    int bytesRcvd = await s.ReceiveAsync(rcvBufferSeg, SocketFlags.None);
    String respString = System.Text.Encoding.UTF8.GetString(rcvBuffer);
    Assert.True(respString.StartsWith($"HTTP/1.1 {expectedStatusCode}"));
    s.Close();
    s.Dispose();
};

HttpClient client = new HttpClient();

Console.WriteLine("========================================");
Console.WriteLine("Testing get /");
Uri reqUri = new Uri("http://127.0.0.1:27015/bob.html");
try
{
    String result = await client.GetStringAsync(reqUri);
} catch (HttpRequestException ex)
{
    Assert.True(ex.StatusCode == HttpStatusCode.NotFound);
}

Console.WriteLine("========================================");
Console.WriteLine("Testing get /");
reqUri = new Uri("http://127.0.0.1:27015/");
HttpResponseMessage resp = await client.GetAsync(reqUri);
Assert.True(resp.IsSuccessStatusCode);

// random bytes 1024 (less than max)
int[] sendLength = { 1024, 8196, 16000 };
foreach (int sl in sendLength)
{
    Console.WriteLine("========================================");
    Console.WriteLine($"Testing send {sl} random bytes");
    var rand = new Random();
    byte[] r = new byte[sl];
    rand.NextBytes(r);
    ArraySegment<byte> buffer1 = new ArraySegment<byte>(r);

    Socket s1 = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
    await s1.ConnectAsync(new IPEndPoint(IPAddress.Loopback, 27015));
    await s1.SendAsync(buffer1, SocketFlags.None);
    byte[] rcvBuffer = new byte[8196];
    ArraySegment<byte> rcvBufferSeg = new ArraySegment<byte>(rcvBuffer);
    int bytesRcvd = await s1.ReceiveAsync(rcvBufferSeg, SocketFlags.None);
    Console.WriteLine($"got {bytesRcvd} bytes");
    String respString = System.Text.Encoding.UTF8.GetString(rcvBuffer);
    Assert.True(respString.StartsWith("HTTP/1.1 400"));

    s1.Close();
    s1.Dispose();
}

Console.WriteLine("========================================");
Console.WriteLine("Testing get /index.html");
reqUri = new Uri("http://127.0.0.1:27015/index.html");
resp = await client.GetAsync(reqUri);
Assert.True(resp.IsSuccessStatusCode);

Console.WriteLine("========================================");
Console.WriteLine("Testing get /index1.html");
reqUri = new Uri("http://127.0.0.1:27015/index1.html");
try
{
    resp = await client.GetAsync(reqUri);
}
catch (HttpRequestException ex)
{
    Assert.True(ex.StatusCode == HttpStatusCode.NotFound);
}

await SocketSend("GET /not_found.html HTTP/1.1\r\nHost:localhost\r\n\r\n", "404");
await SocketSend("blah /not_found.html HTTP/1.1\r\nHost:localhost\r\n\r\n", "400");
await SocketSend("GET / HTTP/1.1\r\nHost:localhost\r\n\r\n", "200");
await SocketSend("GET index.html HTTP/1.1\r\nHost:localhost\r\n\r\n", "200");
await SocketSend("GET index.html", "400");
await SocketSend("GET index.html\r\n", "400");
await SocketSend("GET index.html HTTP/1.1", "400");
await SocketSend("GET /../../blah HTTP/1.1\r\nHost:localhost\r\n\r\n", "403");

String base_packet = "GET index.html HTTP/1.1\r\nHost:localhost\r\n\r\n";
for(int i=0; i<base_packet.Length + 1; i++)
{
    byte[] extraBytes = new byte[1024];
    var rand = new Random();
    rand.NextBytes(extraBytes);
    String sub_packet = base_packet.Substring(0, i);
    byte[] sub_packet_bytes = System.Text.Encoding.UTF8.GetBytes(sub_packet);
    Array.Copy(sub_packet_bytes, extraBytes, sub_packet_bytes.Length);
    if(i == base_packet.Length)
    {
        await SocketSendBytes(extraBytes, "200");
    }
    else
    {
        await SocketSendBytes(extraBytes, "400");
    }
}
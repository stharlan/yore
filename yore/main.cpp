
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib")
#pragma comment(lib, "Pathcch.lib")
#pragma comment(lib, "libboost_atomic-vc143-mt-gd-x64-1_78.lib")
#pragma comment(lib, "libboost_chrono-vc143-mt-gd-x64-1_78.lib")
#pragma comment(lib, "libboost_filesystem-vc143-mt-gd-x64-1_78.lib")
#pragma comment(lib, "libboost_log_setup-vc143-mt-gd-x64-1_78.lib")
#pragma comment(lib, "libboost_log-vc143-mt-gd-x64-1_78.lib")
#pragma comment(lib, "libboost_regex-vc143-mt-gd-x64-1_78.lib")
#pragma comment(lib, "libboost_thread-vc143-mt-gd-x64-1_78.lib")

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <PathCch.h>
#include <iostream>
#include <span>
#include <boost/log/trivial.hpp>

using namespace std;

#define END_CHECK if(start==end){request.hasError=TRUE;request.errorNear=std::span<char>(start,start);return;}
#define MOVE_THROUGH_WHITESPACE while(((*start==' ')||(*start=='\r')||(*start=='\n'))&&(start<end))start++;END_CHECK;
#define MOVE_TO_WHITESPACE while((*start!=' ')&&(*start!='\r')&&(*start!='\n')&&(start<end))start++;END_CHECK;
#define MOVE_THROUGH_EOL while(((*start=='\r')||(*start=='\n'))&&(start<end))start++;END_CHECK;
#define MOVE_TO_EOL while((*start!='\r')&&(*start!='\n')&&(start<end))start++;END_CHECK;

#define NUM_HANDLER_THREADS 4
#define NUM_CONCURRENT_CONNECTIONS 100
#define CONTEXT_BUFFER_SIZE 1024
#define YORE_ROOT L"C:\\yore_root\\"

#define CONN_CTX_LOCKED_BIT 0x01

#define CONTEXT_STATE_NULL 0x00
#define CONTEXT_STATE_INIT 0x01
#define CONTEXT_STATE_PENDING_ACCEPT 0x02
#define CONTEXT_STATE_PENDING_RECV 0x03
#define CONTEXT_STATE_PENDING_XMITFILE 0x04

namespace std {
	inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& os, std::span<char>& dt)
	{
		for (const auto c : dt) os << c;
		return os;
	}
	inline boost::log::formatting_ostream& operator<<(boost::log::formatting_ostream& os, const std::span<char>& dt)
	{
		for (const auto c : dt) os << c;
		return os;
	}
}

struct HTTP_REQUEST
{
	std::span<char> verb;
	std::span<char> resource;
	std::span<char> version;
	std::vector<std::span<char>> headers;
	BOOL hasError;
	std::span<char> errorNear;
};

struct CONNECTION_CONTEXT
{
	OVERLAPPED overlapped = {};					// must be first in struct
	uint32_t flags = 0;
	uint32_t state = CONTEXT_STATE_NULL;
	SOCKET acceptSocket = INVALID_SOCKET;
	char cbuffer[CONTEXT_BUFFER_SIZE];			// input buffer
	WSABUF wsabuf;
	HANDLE hFile = INVALID_HANDLE_VALUE;
	char head_cbuffer[CONTEXT_BUFFER_SIZE];		// http output header
	TRANSMIT_FILE_BUFFERS tfb = {};				// for transmit file
	HTTP_REQUEST request;						// for parsing request
	wchar_t path_of_file_to_return[MAX_PATH];
};

struct SERVER_CONTEXT
{
	SOCKET listenSocket = INVALID_SOCKET;
	LPFN_ACCEPTEX lpfnAcceptEx = nullptr;
	HANDLE hIocp = nullptr;
	CONNECTION_CONTEXT* connections = nullptr;
	SRWLOCK cnn_sync = {};
};

const char* FORMAT_HTTP_RESPONSE_HEAD = "HTTP/1.1 200 OK\r\nServer: YORE\r\nContent-Length: %i\r\nContent-Type: text/html\r\n\r\n";

BOOL starts_with(std::span<char>& cspan, const char* comp, const uint32_t len)
{
	size_t min_chars_in_common = cspan.size() < len ? cspan.size() : len;
	if (min_chars_in_common > 0)
	{
		for (uint32_t c = 0; c < min_chars_in_common; c++)
		{
			if (cspan[c] != comp[c]) return FALSE;
		}
		return TRUE;
	}
	return FALSE;
}

BOOL starts_with(char* start, char* end, const char* comp, const uint32_t len)
{
	uint32_t span_length = (uint32_t)(end - start);
	uint32_t min_chars_in_common = span_length < len ? span_length : len;
	if (min_chars_in_common > 0)
	{
		for (uint32_t c = 0; c < min_chars_in_common; c++)
		{
			if (start[c] != comp[c]) return FALSE;
		}
		return TRUE;
	}
	return FALSE;
}

const char* VERB_GET = "GET";
const char* END_OF_REQ = "\r\n\r\n";

void parse_http(char* start, char* end, HTTP_REQUEST& request)
{

	request.verb = std::span<char>();
	request.resource = std::span<char>();
	request.version = std::span<char>();
	request.hasError = FALSE;
	request.errorNear = std::span<char>();
	request.headers = std::vector<std::span<char>>();

	char* istart = nullptr;

	std::span<char> input = { start, end };
	if (!starts_with(input, VERB_GET, 3))
	{
		// parse error
		request.hasError = TRUE;
		request.errorNear = std::span<char>(start, start + ((end - start) < 5 ? (end - start) : 5));
		return;
	}
	istart = start;

	while (*start != ' ' && start < end) start++;
	END_CHECK;
	request.verb = std::span<char>(istart, start);

	MOVE_THROUGH_WHITESPACE;
	istart = start;

	MOVE_TO_WHITESPACE;
	request.resource = std::span<char>(istart, start);

	MOVE_THROUGH_WHITESPACE;
	istart = start;

	MOVE_TO_EOL;
	if (starts_with(start, end, END_OF_REQ, 4))
	{
		// end of request
		// no headers
		request.version = std::span<char>(istart, start);
		return;
	}
	else
	{
		request.version = std::span<char>(istart, start);
	}

	while (true)
	{
		MOVE_THROUGH_EOL;
		istart = start;
		MOVE_TO_EOL;
		if (starts_with(start, end, END_OF_REQ, 4))
		{
			// end of request
			// no headers
			request.headers.push_back(std::span<char>(istart, start));
			return;
		}
		else
		{
			request.headers.push_back(std::span<char>(istart, start));
		}
	}

	return;
}

void init_connections(SERVER_CONTEXT* svr)
{
	BOOST_LOG_TRIVIAL(info) << "Init'ing " << NUM_CONCURRENT_CONNECTIONS << " connections";

	svr->connections = (CONNECTION_CONTEXT*)malloc(NUM_CONCURRENT_CONNECTIONS * sizeof(CONNECTION_CONTEXT));
	if (svr->connections == nullptr)
	{
		BOOST_LOG_TRIVIAL(error) << "ERROR: Failed to allocate mem for connection strucs";
		return;
	}

	memset(svr->connections, 0, NUM_CONCURRENT_CONNECTIONS * sizeof(CONNECTION_CONTEXT));

	InitializeSRWLock(&svr->cnn_sync);
}

void cleanup_connections(SERVER_CONTEXT* server)
{
	for (int i = 0; i < NUM_CONCURRENT_CONNECTIONS; i++)
	{
		if (server->connections[i].acceptSocket != INVALID_SOCKET)
		{
			closesocket(server->connections[i].acceptSocket);
			server->connections[i].acceptSocket = INVALID_SOCKET;
		}
		if (server->connections[i].hFile != INVALID_HANDLE_VALUE)
		{
			CloseHandle(server->connections[i].hFile);
			server->connections[i].hFile = INVALID_HANDLE_VALUE;
		}
	}
}

CONNECTION_CONTEXT* get_free_connection(SERVER_CONTEXT* svr)
{
	CONNECTION_CONTEXT* result = nullptr;

	AcquireSRWLockExclusive(&svr->cnn_sync);

	for (int i = 0; i < NUM_CONCURRENT_CONNECTIONS; i++)
	{
		CONNECTION_CONTEXT* ctx = (CONNECTION_CONTEXT*)(svr->connections + i);
		if (ctx != nullptr)
		{
			// set the first bit
			if (~(ctx->flags & CONN_CTX_LOCKED_BIT))
			{
				ctx->flags |= CONN_CTX_LOCKED_BIT;
				result = svr->connections + i;
				BOOST_LOG_TRIVIAL(info) << "Using connection 0x" << hex << svr->connections + i << dec;
				goto found_cnn;
			}
		}
	}

found_cnn:
	ReleaseSRWLockExclusive(&svr->cnn_sync);

	return result;
}

void handler_init_socket(DWORD threadId, CONNECTION_CONTEXT* connection, SERVER_CONTEXT* server)
{
	// new socket
	connection->acceptSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
	if (connection->acceptSocket != INVALID_SOCKET)
	{
		memset(&connection->overlapped, 0, sizeof(OVERLAPPED));
		memset(&connection->cbuffer, 0, CONTEXT_BUFFER_SIZE);
		DWORD nbr = 0;

		// pointer to connection context is the key
		if (nullptr == CreateIoCompletionPort((HANDLE)connection->acceptSocket, server->hIocp, reinterpret_cast<ULONG_PTR>(connection), 0))
		{
			BOOST_LOG_TRIVIAL(info) << "failed to assoc accept socket with iocp, or, failed to create accept socket";
			// error, free it up
			closesocket(connection->acceptSocket);
			connection->acceptSocket = INVALID_SOCKET;
			connection->flags = 0x00;
		}
		else
		{
			// acceptex
			if (FALSE == server->lpfnAcceptEx(server->listenSocket, connection->acceptSocket, connection->cbuffer,
				CONTEXT_BUFFER_SIZE - ((sizeof(sockaddr_in) + 16) * 2),
				sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
				&nbr, reinterpret_cast<LPOVERLAPPED>(connection)))
			{
				int code = WSAGetLastError();
				if (ERROR_IO_PENDING != code)
				{
					BOOST_LOG_TRIVIAL(info) << "failed to acceptex " << code;
					closesocket(connection->acceptSocket);
					connection->acceptSocket = INVALID_SOCKET;
					connection->flags = 0x00;
				}
				else
				{
					connection->state = CONTEXT_STATE_PENDING_ACCEPT;
				}
			}
			else
			{
				BOOST_LOG_TRIVIAL(error) << "ERROR: Failed to acceptex";
				closesocket(connection->acceptSocket);
				connection->acceptSocket = INVALID_SOCKET;
				connection->flags = 0x00;
			}
		}
	}
	else
	{
		// error, free it up
		connection->flags = 0x00;
	}

}

DWORD WINAPI handler_proc(void* parm1)
{
	SERVER_CONTEXT* server = reinterpret_cast<SERVER_CONTEXT*>(parm1);
	DWORD nbxfer = 0;
	ULONG_PTR ckey = 0;
	LPOVERLAPPED lpovlp = nullptr;
	DWORD threadId = GetCurrentThreadId();
	BOOL done = FALSE;
	BOOL parse_is_valid = false;

	while (!done)
	{
		nbxfer = 0;
		ckey = 0;
		//BOOST_LOG_TRIVIAL(info) << "waiting for packet...";
		if (FALSE == GetQueuedCompletionStatus(server->hIocp, &nbxfer, &ckey, &lpovlp, INFINITE))
		{
			BOOST_LOG_TRIVIAL(error) << "get queued compl st failed...";
			if (lpovlp == nullptr)
			{
				if (ERROR_ABANDONED_WAIT_0 == GetLastError())
				{
					// completion port handle closed
					BOOST_LOG_TRIVIAL(info) << "worker done, exiting";
					done = TRUE;
				}
				else
				{
					// no packet dequeued, move on
					BOOST_LOG_TRIVIAL(info) << "no packet dequeued, move on";
				}
			}
			else
			{
				// failed operation, can call getlasterror, but, go on
				DWORD err = GetLastError();
				BOOST_LOG_TRIVIAL(error) << "unknown error in getqueuedcompletionstatus; 0x" << hex << err << dec;
				done = TRUE;
			}
		}
		else if(nbxfer == 0 && ckey == 0 && lpovlp == nullptr)
		{
			BOOST_LOG_TRIVIAL(info) << "got quit packet...";
			done = TRUE;
		}
		else if(lpovlp != nullptr)
		{
			//BOOST_LOG_TRIVIAL(info) << "got packet...";
			CONNECTION_CONTEXT* connection = reinterpret_cast<CONNECTION_CONTEXT*>(lpovlp);
			if (connection != nullptr)
			{
				DWORD flags = 0;
				switch (connection->state)
				{
				case CONTEXT_STATE_PENDING_XMITFILE:
					BOOST_LOG_TRIVIAL(info) << "file transmitted...";
					BOOST_LOG_TRIVIAL(info) << nbxfer << " bytes transferred";
					CloseHandle(connection->hFile);
					connection->hFile = INVALID_HANDLE_VALUE;
					closesocket(connection->acceptSocket);
					connection->acceptSocket = INVALID_SOCKET;
					//free_cnn->flags = 0;
					BOOST_LOG_TRIVIAL(info) << "putting socket back into wait for accept mode";
					handler_init_socket(threadId, connection, server);
					break;
				case CONTEXT_STATE_PENDING_ACCEPT:
					BOOST_LOG_TRIVIAL(info) << "==> CONNECTION ACCEPTED <==, entering recv...";
					BOOST_LOG_TRIVIAL(info) << nbxfer << " bytes transferred";

					// parse it
					parse_is_valid = false;
					parse_http(connection->cbuffer,
						connection->cbuffer + (nbxfer < CONTEXT_BUFFER_SIZE ? nbxfer : CONTEXT_BUFFER_SIZE),
						connection->request);
					if (connection->request.hasError)
					{
						BOOST_LOG_TRIVIAL(error) << "PARSE ERROR NEAR: " << connection->request.errorNear;
					}
					else
					{
						BOOST_LOG_TRIVIAL(info) << "VERB = " << connection->request.verb;
						BOOST_LOG_TRIVIAL(info) << "RESOURCE = " << connection->request.resource;
						BOOST_LOG_TRIVIAL(info) << "HTTP VERSION = " << connection->request.version;
						for (const auto& h : connection->request.headers)
						{
							BOOST_LOG_TRIVIAL(info) << "HEADER = " << h;
						}
						parse_is_valid = true;
					}

					// connection was accepted, now - read
					/*
					free_cnn->wsabuf.buf = reinterpret_cast<CHAR*>(free_cnn->buffer);
					free_cnn->wsabuf.len = CONTEXT_BUFFER_SIZE;
					if (SOCKET_ERROR == WSARecv(free_cnn->acceptSocket, &free_cnn->wsabuf, 1, nullptr, &flags,
						reinterpret_cast<LPOVERLAPPED>(free_cnn), nullptr))
					{
						if (WSAGetLastError() != WSA_IO_PENDING)
						{
							BOOST_LOG_TRIVIAL(error) << "ERROR: Failed to recv";
						}
					}
					free_cnn->state = CONTEXT_STATE_PENDING_RECV;
					*/

					if (TRUE == parse_is_valid)
					{

						// convert any fslsh to bkslsh in resource
						for (auto iter = connection->request.resource.begin();
							iter != connection->request.resource.end();
							++iter)
						{
							if (*iter == '/') *iter = '\\';
						}

						// clear path var
						memset(connection->path_of_file_to_return, 0, MAX_PATH * sizeof(wchar_t));

						// convert resource to wcs in path
						size_t numOfCharConverted = 0;
						mbstowcs_s(&numOfCharConverted, connection->path_of_file_to_return, MAX_PATH,
							connection->request.resource.data(), connection->request.resource.size_bytes());

						if (connection->path_of_file_to_return[numOfCharConverted - 2] == '\\')
						{
							wcscat_s(connection->path_of_file_to_return, MAX_PATH, L"index.html");
						}

						wchar_t* first_char = connection->path_of_file_to_return;
						while (*first_char == L'\\') first_char++;

						PathCchCombine(connection->path_of_file_to_return, MAX_PATH,
							YORE_ROOT, first_char);

						connection->hFile = CreateFile(connection->path_of_file_to_return, GENERIC_READ, FILE_SHARE_READ, nullptr,
							OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
						if (connection->hFile != INVALID_HANDLE_VALUE)
						{
							DWORD fileSize = GetFileSize(connection->hFile, nullptr);
							memset(connection->head_cbuffer, 0, CONTEXT_BUFFER_SIZE);
							connection->tfb.HeadLength = sprintf_s(connection->head_cbuffer, CONTEXT_BUFFER_SIZE, FORMAT_HTTP_RESPONSE_HEAD, fileSize);
							connection->tfb.Head = connection->head_cbuffer;
							if (FALSE == TransmitFile(connection->acceptSocket, connection->hFile, fileSize, 0 /* default */,
								&connection->overlapped, &connection->tfb, TF_DISCONNECT))
							{
								if (WSAGetLastError() != ERROR_IO_PENDING)
								{
									BOOST_LOG_TRIVIAL(error) << "ERROR: Failed to transmit file";
									closesocket(connection->acceptSocket);
									connection->acceptSocket = INVALID_SOCKET;
									connection->flags = 0;
								}
								else
								{
									connection->state = CONTEXT_STATE_PENDING_XMITFILE;
								}
							}
							else
							{
								BOOST_LOG_TRIVIAL(info) << "sync transmit?";
							}
						}
						else
						{
							BOOST_LOG_TRIVIAL(info) << "can't find input file";
						}
					}
					break;
				case CONTEXT_STATE_INIT:

					// got a packet
					//BOOST_LOG_TRIVIAL(info) << "need to create new accept socket for connection " << hex << connection <<  dec << "...";
					handler_init_socket(threadId, connection, server);
					break;
				}
			}
		}
	}

	BOOST_LOG_TRIVIAL(info) << "quitting...";

	return 0;
}

HANDLE g_hQuitEvent = INVALID_HANDLE_VALUE;

BOOL WINAPI ControlCHandler(
	DWORD dwCtrlType
)
{
	switch (dwCtrlType)
	{
	case CTRL_C_EVENT:
		BOOST_LOG_TRIVIAL(info) << "Ctrl-C pressed; quitting...";
		SetEvent(g_hQuitEvent);
		return TRUE;
	}
	return FALSE;
}

int main(int argc, char** argv)
{
	HANDLE handleHandlerThreads[NUM_HANDLER_THREADS] = { 0 };
	DWORD dwHandlerThreadIds[NUM_HANDLER_THREADS] = { 0 };
	WSADATA wsadata = { 0 };
	sockaddr_in service = { 0 };
	IN_ADDR inaddr = { 0 };
	DWORD dwBytes = 0;
	GUID GuidAcceptEx = WSAID_ACCEPTEX;
	SERVER_CONTEXT server;
	CONNECTION_CONTEXT* lpConnection = nullptr;

	memset(handleHandlerThreads, 0, sizeof(HANDLE) * NUM_HANDLER_THREADS);
	memset(dwHandlerThreadIds, 0, sizeof(DWORD) * NUM_HANDLER_THREADS);

	if (WSAStartup(MAKEWORD(2, 2), &wsadata) > 0)
	{
		BOOST_LOG_TRIVIAL(error) << "ERROR: Failed to start wsa";
		goto cleanup;
	}

	// first, create an io completion port
	BOOST_LOG_TRIVIAL(info) << "Creating iocp...";
	server.hIocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, (HANDLE)nullptr, (ULONG_PTR)0, 2);
	if (server.hIocp == nullptr)
	{
		BOOST_LOG_TRIVIAL(error) << "ERROR: Failed to create iocp";
		goto cleanup;
	}

	// second, create handler threads
	BOOST_LOG_TRIVIAL(info) << "Creating " << NUM_HANDLER_THREADS << " threads...";
	for (int i = 0; i < NUM_HANDLER_THREADS; i++)
	{
		handleHandlerThreads[i] = CreateThread((LPSECURITY_ATTRIBUTES)nullptr, (SIZE_T)0,
			handler_proc, reinterpret_cast<LPVOID>(&server), 0, &dwHandlerThreadIds[i]);
		if (handleHandlerThreads[i] == nullptr)
		{
			BOOST_LOG_TRIVIAL(error) << "ERROR: Failed to create thread";
			goto cleanup;
		}
	}

	// create listen socket
	server.listenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
	if (INVALID_SOCKET == server.listenSocket)
	{
		BOOST_LOG_TRIVIAL(error) << "ERROR: Failed to create listen socket";
		goto cleanup;
	}

	if (SOCKET_ERROR == WSAIoctl(server.listenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidAcceptEx, sizeof(GuidAcceptEx),
		&server.lpfnAcceptEx, sizeof(server.lpfnAcceptEx),
		&dwBytes, NULL, NULL))
	{
		BOOST_LOG_TRIVIAL(error) << "ERROR: Failed to get acceptex fn";
		goto cleanup;
	}

	service.sin_family = AF_INET;
	InetPton(AF_INET, L"127.0.0.1", &service.sin_addr);
	service.sin_port = htons(27015);
	BOOST_LOG_TRIVIAL(info) << "Listening on localhost port 27015";
	if (SOCKET_ERROR == ::bind(server.listenSocket, (SOCKADDR*)&service, sizeof(service)))
	{
		BOOST_LOG_TRIVIAL(error) << "ERROR: Failed to bind listen socket";
		goto cleanup;
	}

	if (SOCKET_ERROR == listen(server.listenSocket, SOMAXCONN))
	{
		BOOST_LOG_TRIVIAL(error) << "ERROR: Failed to listen";
		goto cleanup;
	}

	// pointer to server context is the key
	if (nullptr == CreateIoCompletionPort((HANDLE)server.listenSocket, server.hIocp, reinterpret_cast<ULONG_PTR>(&server), 0))
	{
		BOOST_LOG_TRIVIAL(error) << "ERROR: Failed to assoc listen socket with iocp";
		goto cleanup;
	}

	init_connections(&server);

	// init all connections
	for (int i = 0; i < NUM_CONCURRENT_CONNECTIONS; i++)
	{
		lpConnection = server.connections + i;
		lpConnection->state = CONTEXT_STATE_INIT;
		PostQueuedCompletionStatus(server.hIocp, 0, reinterpret_cast<ULONG_PTR>(lpConnection), reinterpret_cast<LPOVERLAPPED>(lpConnection));
	}

	SetConsoleCtrlHandler(ControlCHandler, TRUE);

	g_hQuitEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (g_hQuitEvent == NULL)
	{
		BOOST_LOG_TRIVIAL(error) << "ERROR: Failed to create quit event";
		goto cleanup;
	}

	BOOST_LOG_TRIVIAL(info) << "Ready...";
	WaitForSingleObject(g_hQuitEvent, INFINITE);

	CloseHandle(g_hQuitEvent);

cleanup:

	BOOST_LOG_TRIVIAL(info) << "Cleaning up...";

	if (server.hIocp != nullptr)
	{
		for (int i = 0; i < NUM_HANDLER_THREADS; i++)
		{
			PostQueuedCompletionStatus(server.hIocp, 0, 0, nullptr);
		}
	}

	// stop listening
	if (server.listenSocket)
	{
		closesocket(server.listenSocket);
	}

	// close each connection
	cleanup_connections(&server);
	if (server.connections != nullptr)
	{
		// TODO clean each connection
		free(server.connections);
		server.connections = nullptr;
	}

	// wait ten seconds
	BOOST_LOG_TRIVIAL(info) << "Waiting on handlers to quit...";
	if (WAIT_OBJECT_0 == WaitForMultipleObjects(NUM_HANDLER_THREADS, handleHandlerThreads, TRUE, 10000))
	{
		for (int i = 0; i < NUM_HANDLER_THREADS; i++)
		{
			if (handleHandlerThreads[i] != 0)
			{
				CloseHandle(handleHandlerThreads[i]);
			}
		}
	}
	else
	{
		BOOST_LOG_TRIVIAL(error) << "ERROR: Failed to clean up thread handles";
	}

	if (server.hIocp)
	{
		CloseHandle(server.hIocp);
	}

	WSACleanup();

	BOOST_LOG_TRIVIAL(info) << "Done.";
}
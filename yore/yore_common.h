#pragma once

#define END_CHECK if(start==end){request.hasError=TRUE;request.errorNear=std::span<char>(start,start);return;}
#define MOVE_THROUGH_WHITESPACE while(((*start==' ')||(*start=='\r')||(*start=='\n'))&&(start<end))start++;END_CHECK;
#define MOVE_TO_WHITESPACE while((*start!=' ')&&(*start!='\r')&&(*start!='\n')&&(start<end))start++;END_CHECK;
#define MOVE_THROUGH_EOL while(((*start=='\r')||(*start=='\n'))&&(start<end))start++;END_CHECK;
#define MOVE_TO_EOL while((*start!='\r')&&(*start!='\n')&&(start<end))start++;END_CHECK;

#define NUM_HANDLER_THREADS 4
#define NUM_CONCURRENT_CONNECTIONS 5
#define CONTEXT_INPUT_BUFFER_SIZE 8196
#define CONTEXT_OUTPUT_BUFFER_SIZE 1024
#define YORE_ROOT L"C:\\yore_root\\"

#define CONTEXT_STATE_NULL 0x00
#define CONTEXT_STATE_INIT 0x01
#define CONTEXT_STATE_PENDING_ACCEPT 0x02
#define CONTEXT_STATE_PENDING_RECV 0x03
#define CONTEXT_STATE_PENDING_XMITFILE 0x04

struct HTTP_HEADER
{
	std::span<char> header_name;
	std::span<char> header_value;
};

struct HTTP_REQUEST
{
	std::span<char> verb;
	std::span<char> resource;
	std::span<char> version;
	std::vector<HTTP_HEADER> headers;
	BOOL hasError;
	std::span<char> errorNear;
};

struct HTTP_RESPONSE
{
	char* buffer = nullptr;
};

struct CONNECTION_CONTEXT
{
	OVERLAPPED overlapped = {};								// must be first in struct
	uint32_t state = CONTEXT_STATE_NULL;
	SOCKET acceptSocket = INVALID_SOCKET;
	char input_buffer[CONTEXT_INPUT_BUFFER_SIZE];			// input buffer
	WSABUF wsabuf;
	HANDLE hFile = INVALID_HANDLE_VALUE;
	char output_buffer[CONTEXT_OUTPUT_BUFFER_SIZE];			// http output header
	TRANSMIT_FILE_BUFFERS tfb = {};							// for transmit file
	HTTP_REQUEST request;									// for parsing request
	HTTP_RESPONSE response;
	wchar_t path_of_file_to_return[MAX_PATH];
	BOOL keep_alive = FALSE;
};

struct SERVER_CONTEXT
{
	SOCKET listenSocket = INVALID_SOCKET;
	LPFN_ACCEPTEX lpfnAcceptEx = nullptr;
	HANDLE hIocp = nullptr;
	CONNECTION_CONTEXT* connections = nullptr;
	SRWLOCK cnn_sync = {};
};

void parse_http(char* start, char* end, HTTP_REQUEST& request);
BOOL span_equals_string(const std::span<char>& str_span, const char* str);
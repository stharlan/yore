
#include <WinSock2.h>
#include <MSWSock.h>
#include <span>
#include <vector>
#include "yore_common.h"

const char* VERB_GET = "GET";
const char* END_OF_REQ = "\r\n\r\n";
const char* BLANK_STRING = " ";

BOOL starts_with(std::span<char>& cspan, const char* comp, const uint32_t len)
{
	// data checks
	if (cspan.size_bytes() < 1) return FALSE;
	if (comp == nullptr) return FALSE;
	if (len < 1) return FALSE;

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
	// data checks
	if (start == nullptr || end == nullptr || comp == nullptr) return FALSE;
	if (len < 1) return FALSE;
	if (start >= end) return FALSE;

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

void parse_http(char* start, char* end, HTTP_REQUEST& request)
{

	if (start == nullptr || end == nullptr)
	{
		request.hasError = TRUE;
		request.errorNear = std::span<char>();
		return;
	}

	if (start >= end)
	{
		request.hasError = TRUE;
		request.errorNear = std::span<char>();
		return;
	}

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

	//MOVE_TO_EOL;
	while ((*start != '\r') && (*start != '\n') && (start < end))start++;
	if (start == end) { 
		request.hasError = TRUE; 
		request.errorNear = std::span<char>(start, start); 
		return; 
	}
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

	// error request was not properly ended 
	request.hasError = TRUE;
	request.errorNear = std::span<char>(start, start + ((end - start) < 5 ? (end - start) : 5));

	return;
}


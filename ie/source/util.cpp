#include "stdafx.h"
#include "util.h"

//Logger::pointer logger(new Logger(Logger::ALL, L"\\\\vmware-host\\Shared Folders\\webmynd\\forge.log"));
Logger::pointer logger(new Logger(Logger::ALL, L""));

DWORD widestring_from_utf8(const char *s, int size, wchar_t **output)
{
    // Get the size of the output string
    int converted_size = MultiByteToWideChar(CP_UTF8, 0, s, size, NULL, 0);
    if (converted_size == 0) {
        return GetLastError();
    }

    *output = new wchar_t[converted_size+1];

    int actual_size = MultiByteToWideChar(CP_UTF8, 0, s, size, *output, converted_size);
    if (actual_size == 0) {
        return GetLastError();
    }
    if (actual_size != converted_size) {
        return ERROR_INVALID_DATA;
    }
    (*output)[actual_size] = L'\0';

    return ERROR_SUCCESS;
}

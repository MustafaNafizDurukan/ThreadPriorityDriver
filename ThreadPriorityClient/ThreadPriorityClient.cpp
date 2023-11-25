#include <windows.h>
#include <stdio.h>
#include "..\ThreadPriorityDriver\ThreadPriorityCommon.h"

int Error(const char* message);

int main(int argc, const char* argv[])
{
	if (argc < 3)
	{
		printf("USAGE: ThreadPriorityClient <ThreadId> <Priority>\n");
		return 0;
	}

	HANDLE hDevice = CreateFile(PRIORITYBOOSTER_LINKNAME_APP, GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hDevice == INVALID_HANDLE_VALUE)
		return Error("CreateFile failed");


	Thread threadInfo;
	threadInfo.Id = atoi(argv[1]);
	threadInfo.Priority = atoi(argv[2]);

	DWORD bytes;
	BOOL success = DeviceIoControl(hDevice, IOCTL_PRIORITY_BOOSTER_SET_PRIORITY, &threadInfo, sizeof(threadInfo), nullptr, 0, &bytes, nullptr);
	if (success)
		printf("Priority change succeeded!\n");
	else
		Error("Priority change failed!");

	CloseHandle(hDevice);
}

int Error(const char* message)
{
	printf("%s (Error=%d)\n", message, GetLastError());
	return 1;
}


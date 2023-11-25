#include <ntifs.h>
#include <ntddk.h>
#include "ThreadPriorityCommon.h"

// Function Prototypes
void PriorityBoosterUnload(_In_ PDRIVER_OBJECT DriverObject);
NTSTATUS PriorityBoosterCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP  Irp);
NTSTATUS PriorityBoosterDeviceControl(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp);


extern "C" NTSTATUS
DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);

	PDEVICE_OBJECT DeviceObject;
	NTSTATUS status;

	DriverObject->DriverUnload = PriorityBoosterUnload;

	// Setting Major Functions
	DriverObject->MajorFunction[IRP_MJ_CREATE] = PriorityBoosterCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = PriorityBoosterCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = PriorityBoosterDeviceControl;

	UNICODE_STRING devName = RTL_CONSTANT_STRING(PRIORITYBOOSTER_DEVICENAME_DRV);

	status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("IoCreateDevice failed. Error=0x%08X\n", status));
		return status;
	}

	UNICODE_STRING symLink = RTL_CONSTANT_STRING(PRIORITYBOOSTER_LINKNAME_DRV);
	status = IoCreateSymbolicLink(&symLink, &devName);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("IoCreateSymbolicLink failed. Error=0x%08X\n", status));
		IoDeleteDevice(DeviceObject);
		return status;
	}

	return STATUS_SUCCESS;
}


void PriorityBoosterUnload(_In_ PDRIVER_OBJECT DriverObject)
{
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(PRIORITYBOOSTER_LINKNAME_DRV);
	IoDeleteSymbolicLink(&symLink);

	IoDeleteDevice(DriverObject->DeviceObject);

	KdPrint(("PriorityBooster has been unloaded\n"));
}


NTSTATUS PriorityBoosterCreateClose(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP  Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	auto stack = IoGetCurrentIrpStackLocation(Irp);

	if (stack->MajorFunction == IRP_MJ_CREATE)
	{
		auto security_context = stack->Parameters.Create.SecurityContext;
		KdPrint(("Create function of PriorityBooster has been called\n"));

		KdPrint(("Create to PriorityBoosterCreateClose: GenericWrite=%s GenericRead=%s \n",
			security_context->DesiredAccess & GENERIC_WRITE ? "TRUE" : "FALSE",
			security_context->DesiredAccess & GENERIC_READ ? "TRUE" : "FALSE"));
	}
	else if (stack->MajorFunction == IRP_MJ_CLOSE)
	{
		KdPrint(("Close function of PriorityBooster has been called\n"));
	}

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS PriorityBoosterDeviceControl(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	KdPrint(("DeviceIoControl has been called\n"));

	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto status = STATUS_SUCCESS;

	switch (stack->Parameters.DeviceIoControl.IoControlCode)
	{
	case IOCTL_PRIORITY_BOOSTER_SET_PRIORITY:
	{ 
		auto len = stack->Parameters.DeviceIoControl.InputBufferLength;
		if (len < sizeof(Thread))
		{
			KdPrint(("Invalid Input Buffer Length: %d", len));
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}

		auto threadInfo = (Thread*)stack->Parameters.DeviceIoControl.Type3InputBuffer;

		if (threadInfo == nullptr)
		{
			KdPrint(("Invalid input buffer\n"));
			status = STATUS_INVALID_PARAMETER;
			break;
		}

		if (threadInfo->Priority < 1 || threadInfo->Priority > 31)
		{
			KdPrint(("Invalid thread priority: %d\n", threadInfo->Priority));
			status = STATUS_INVALID_PARAMETER;
			break;
		}


		/*
		*	We give thread id as a handle because of the way process and thread IDs are generated,
		*	these are generated from a global private kernel handle table, so handle "values" are the IDs.
		*	As thread is now referenced by Thread the reference from HANDLE table is incremented and thread can't die.
		*/
		PETHREAD Thread;
		status = PsLookupThreadByThreadId(ULongToHandle(threadInfo->Id), &Thread);

		if (!NT_SUCCESS(status))
			break;

		KeSetPriorityThread((PKTHREAD)Thread, threadInfo->Priority);

		// Decrement the thread object's reference count
		ObDereferenceObject(Thread);
		KdPrint(("Thread Priority change for %d to %d succeeded!\n", threadInfo->Id, threadInfo->Priority));

		break;
	}

	default:
		KdPrint(("Invalid IOCTL: %d\n", stack->Parameters.DeviceIoControl.IoControlCode));
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}
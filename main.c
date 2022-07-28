#include <ntifs.h>

/*
 * Old project, what can be used for mouse input manipulation
*/

typedef int BOOL;
typedef unsigned int DWORD;
typedef ULONG_PTR QWORD;

#pragma warning(disable : 4201)
typedef struct _MOUSE_INPUT_DATA {
	USHORT UnitId;
	USHORT Flags;
	union {
	ULONG Buttons;
	struct {
	USHORT ButtonFlags;
	USHORT ButtonData;
	};
	};
	ULONG  RawButtons;
	LONG   LastX;
	LONG   LastY;
	ULONG  ExtraInformation;
} MOUSE_INPUT_DATA, *PMOUSE_INPUT_DATA;

typedef VOID
(*MouseClassServiceCallbackFn)(
	PDEVICE_OBJECT DeviceObject,
	PMOUSE_INPUT_DATA InputDataStart,
	PMOUSE_INPUT_DATA InputDataEnd,
	PULONG InputDataConsumed
);

typedef struct _MOUSE_OBJECT
{
	PDEVICE_OBJECT              mouse_device;
	MouseClassServiceCallbackFn service_callback;
	BOOL                        use_mouse;
} MOUSE_OBJECT, * PMOUSE_OBJECT;



MOUSE_OBJECT gMouseObject;
QWORD _KeAcquireSpinLockAtDpcLevel;
QWORD _KeReleaseSpinLockFromDpcLevel;
QWORD _IofCompleteRequest;
QWORD _IoReleaseRemoveLockEx;




NTSYSCALLAPI
POBJECT_TYPE* IoDriverObjectType;

VOID MouseClassServiceCallback(
	PDEVICE_OBJECT DeviceObject,
	PMOUSE_INPUT_DATA InputDataStart,
	PMOUSE_INPUT_DATA InputDataEnd,
	PULONG InputDataConsumed
);

NTSYSCALLAPI
NTSTATUS
ObReferenceObjectByName(
      __in PUNICODE_STRING ObjectName,
      __in ULONG Attributes,
      __in_opt PACCESS_STATE AccessState,
      __in_opt ACCESS_MASK DesiredAccess,
      __in POBJECT_TYPE ObjectType,
      __in KPROCESSOR_MODE AccessMode,
      __inout_opt PVOID ParseContext,
      __out PVOID *Object
  );


void NtSleep(DWORD milliseconds)
{
	QWORD ms = milliseconds;
	ms = (ms * 1000) * 10;
	ms = ms * -1;
#ifdef _KERNEL_MODE
	KeDelayExecutionThread(KernelMode, 0, (PLARGE_INTEGER)&ms);
#else
	NtDelayExecution(0, (PLARGE_INTEGER)&ms);
#endif
}

void mouse_move(long x, long y, unsigned short button_flags);

VOID
DriverUnload(
	_In_ struct _DRIVER_OBJECT* DriverObject
)
{
	UNREFERENCED_PARAMETER(DriverObject);
	DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[+] MouseClassServiceCallbackMeme.sys is closed\n");
}

QWORD _KeAcquireSpinLockAtDpcLevel;
QWORD _KeReleaseSpinLockFromDpcLevel;
QWORD _IofCompleteRequest;
QWORD _IoReleaseRemoveLockEx;

NTSTATUS DriverEntry(
	_In_ PDRIVER_OBJECT  DriverObject,
	_In_ PUNICODE_STRING RegistryPath
)
{
	UNREFERENCED_PARAMETER(DriverObject);
	UNREFERENCED_PARAMETER(RegistryPath);

	/* Microsoft compiler is sometimes retarded, thats why we have to do this non sense */
	/* It would otherwise generate wrapper functions around, and it would cause system BSOD */
	_KeAcquireSpinLockAtDpcLevel = (QWORD)KeAcquireSpinLockAtDpcLevel;
	_KeReleaseSpinLockFromDpcLevel = (QWORD)KeReleaseSpinLockFromDpcLevel;
	_IofCompleteRequest = (QWORD)IofCompleteRequest;
	_IoReleaseRemoveLockEx = (QWORD)IoReleaseRemoveLockEx;
	

	DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[+] MouseClassServiceCallbackMeme.sys is launched\n");
	DriverObject->DriverUnload = DriverUnload;


	for (int i = 0; i < 32; i++) {
		NtSleep(100);
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[+] Moving mouse\n");

		mouse_move(0, -10, 0);
	}


	return STATUS_SUCCESS;
}

BOOL mouse_open(void)
{
	// https://github.com/nbqofficial/norsefire

	if (gMouseObject.use_mouse == 0) {

		UNICODE_STRING class_string;
		RtlInitUnicodeString(&class_string, L"\\Driver\\MouClass");
	

		PDRIVER_OBJECT class_driver_object = NULL;
		NTSTATUS status = ObReferenceObjectByName(&class_string, OBJ_CASE_INSENSITIVE, NULL, 0, *IoDriverObjectType, KernelMode, NULL, (PVOID*)&class_driver_object);
		if (!NT_SUCCESS(status)) {
			gMouseObject.use_mouse = 0;
			return 0;
		}

		UNICODE_STRING hid_string;
		RtlInitUnicodeString(&hid_string, L"\\Driver\\MouHID");
	

		PDRIVER_OBJECT hid_driver_object = NULL;
	
		status = ObReferenceObjectByName(&hid_string, OBJ_CASE_INSENSITIVE, NULL, 0, *IoDriverObjectType, KernelMode, NULL, (PVOID*)&hid_driver_object);
		if (!NT_SUCCESS(status))
		{
			if (class_driver_object) {
				ObfDereferenceObject(class_driver_object);
			}
			gMouseObject.use_mouse = 0;
			return 0;
		}

		PVOID class_driver_base = NULL;


		PDEVICE_OBJECT hid_device_object = hid_driver_object->DeviceObject;
		while (hid_device_object && !gMouseObject.service_callback)
		{
			PDEVICE_OBJECT class_device_object = class_driver_object->DeviceObject;
			while (class_device_object && !gMouseObject.service_callback)
			{
				if (!class_device_object->NextDevice && !gMouseObject.mouse_device)
				{
					gMouseObject.mouse_device = class_device_object;
				}

				PULONG_PTR device_extension = (PULONG_PTR)hid_device_object->DeviceExtension;
				ULONG_PTR device_ext_size = ((ULONG_PTR)hid_device_object->DeviceObjectExtension - (ULONG_PTR)hid_device_object->DeviceExtension) / 4;
				class_driver_base = class_driver_object->DriverStart;
				for (ULONG_PTR i = 0; i < device_ext_size; i++)
				{
					if (device_extension[i] == (ULONG_PTR)class_device_object && device_extension[i + 1] > (ULONG_PTR)class_driver_object)
					{
						gMouseObject.service_callback = (MouseClassServiceCallbackFn)(device_extension[i + 1]);
					
						break;
					}
				}
				class_device_object = class_device_object->NextDevice;
			}
			hid_device_object = hid_device_object->AttachedDevice;
		}
	
		if (!gMouseObject.mouse_device)
		{
			PDEVICE_OBJECT target_device_object = class_driver_object->DeviceObject;
			while (target_device_object)
			{
				if (!target_device_object->NextDevice)
				{
					gMouseObject.mouse_device = target_device_object;
					break;
				}
				target_device_object = target_device_object->NextDevice;
			}
		}

		ObfDereferenceObject(class_driver_object);
		ObfDereferenceObject(hid_driver_object);

		if (gMouseObject.mouse_device && gMouseObject.service_callback) {
			gMouseObject.use_mouse = 1;
		}

	}

	return gMouseObject.mouse_device && gMouseObject.service_callback;
}

#define KeMRaiseIrql(a,b) *(b) = KfRaiseIrql(a)
void mouse_move(long x, long y, unsigned short button_flags)
{
	KIRQL irql;
	ULONG input_data;
	MOUSE_INPUT_DATA mid = { 0 };
	mid.LastX = x;
	mid.LastY = y;
	mid.ButtonFlags = button_flags;
	if (!mouse_open()) {
		return;
	}
	mid.UnitId = 1;
	KeMRaiseIrql(DISPATCH_LEVEL, &irql);	
	MouseClassServiceCallback(gMouseObject.mouse_device, &mid, (PMOUSE_INPUT_DATA)&mid + 1, &input_data);
	KeLowerIrql(irql);
}

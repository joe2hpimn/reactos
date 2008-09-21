#ifdef __x86_64__
#define MAYBEFWD(x)
#else
#define MAYBEFWD(x) x
#endif

@ cdecl ScsiDebugPrint()
@ stdcall ScsiPortCompleteRequest(ptr long long long long)
@ stdcall ScsiPortConvertPhysicalAddressToUlong(long long)
@ stdcall ScsiPortConvertUlongToPhysicalAddress(long) MAYBEFWD(NTOSKRNL.RtlConvertUlongToLargeInteger)
@ stdcall ScsiPortFlushDma(ptr)
@ stdcall ScsiPortFreeDeviceBase(ptr ptr)
@ stdcall ScsiPortGetBusData(ptr long long long ptr long)
@ stdcall ScsiPortGetDeviceBase(ptr long long long long long long)
@ stdcall ScsiPortGetLogicalUnit(ptr long long long)
@ stdcall ScsiPortGetPhysicalAddress(ptr ptr ptr long)
@ stdcall ScsiPortGetSrb(ptr long long long long)
@ stdcall ScsiPortGetUncachedExtension(ptr ptr long)
@ stdcall ScsiPortGetVirtualAddress(ptr long long)
@ stdcall ScsiPortInitialize(ptr ptr ptr ptr)
@ stdcall ScsiPortIoMapTransfer(ptr ptr long long)
@ stdcall ScsiPortLogError(ptr ptr long long long long long)
@ stdcall ScsiPortMoveMemory(ptr ptr long)
@ cdecl ScsiPortNotification()
@ stdcall ScsiPortReadPortBufferUchar(ptr ptr long) MAYBEFWD(HAL.READ_PORT_BUFFER_UCHAR)
@ stdcall ScsiPortReadPortBufferUshort(ptr ptr long) MAYBEFWD(HAL.READ_PORT_BUFFER_USHORT)
@ stdcall ScsiPortReadPortBufferUlong(ptr ptr long) MAYBEFWD(HAL.READ_PORT_BUFFER_ULONG)
@ stdcall ScsiPortReadPortUchar(ptr) MAYBEFWD(HAL.READ_PORT_UCHAR)
@ stdcall ScsiPortReadPortUshort(ptr) MAYBEFWD(HAL.READ_PORT_USHORT)
@ stdcall ScsiPortReadPortUlong(ptr) MAYBEFWD(HAL.READ_PORT_ULONG)
@ stdcall ScsiPortReadRegisterBufferUchar(ptr ptr long) MAYBEFWD(NTOSKRNL.READ_REGISTER_BUFFER_UCHAR)
@ stdcall ScsiPortReadRegisterBufferUshort(ptr ptr long) MAYBEFWD(NTOSKRNL.READ_REGISTER_BUFFER_USHORT)
@ stdcall ScsiPortReadRegisterBufferUlong(ptr ptr long) MAYBEFWD(NTOSKRNL.READ_REGISTER_BUFFER_ULONG)
@ stdcall ScsiPortReadRegisterUchar(ptr) MAYBEFWD(NTOSKRNL.READ_REGISTER_UCHAR)
@ stdcall ScsiPortReadRegisterUshort(ptr) MAYBEFWD(NTOSKRNL.READ_REGISTER_USHORT)
@ stdcall ScsiPortReadRegisterUlong(ptr) MAYBEFWD(NTOSKRNL.READ_REGISTER_ULONG)
@ stdcall ScsiPortSetBusDataByOffset(ptr long long long ptr long long)
@ stdcall ScsiPortStallExecution(long) HAL.KeStallExecutionProcessor
@ stdcall ScsiPortValidateRange(ptr long long long long long long)
@ stdcall ScsiPortWritePortBufferUchar(ptr ptr long) MAYBEFWD(HAL.WRITE_PORT_BUFFER_UCHAR)
@ stdcall ScsiPortWritePortBufferUshort(ptr ptr long) MAYBEFWD(HAL.WRITE_PORT_BUFFER_USHORT)
@ stdcall ScsiPortWritePortBufferUlong(ptr ptr long) MAYBEFWD(HAL.WRITE_PORT_BUFFER_ULONG)
@ stdcall ScsiPortWritePortUchar(ptr long) MAYBEFWD(HAL.WRITE_PORT_UCHAR)
@ stdcall ScsiPortWritePortUshort(ptr long) MAYBEFWD(HAL.WRITE_PORT_USHORT)
@ stdcall ScsiPortWritePortUlong(ptr long) MAYBEFWD(HAL.WRITE_PORT_ULONG)
@ stdcall ScsiPortWriteRegisterBufferUchar(ptr ptr long) MAYBEFWD(NTOSKRNL.WRITE_REGISTER_BUFFER_UCHAR)
@ stdcall ScsiPortWriteRegisterBufferUshort(ptr ptr long) MAYBEFWD(NTOSKRNL.WRITE_REGISTER_BUFFER_USHORT)
@ stdcall ScsiPortWriteRegisterBufferUlong(ptr ptr long) MAYBEFWD(NTOSKRNL.WRITE_REGISTER_BUFFER_ULONG)
@ stdcall ScsiPortWriteRegisterUchar(ptr long) MAYBEFWD(NTOSKRNL.WRITE_REGISTER_UCHAR)
@ stdcall ScsiPortWriteRegisterUshort(ptr long) MAYBEFWD(NTOSKRNL.WRITE_REGISTER_USHORT)
@ stdcall ScsiPortWriteRegisterUlong(ptr long) MAYBEFWD(NTOSKRNL.WRITE_REGISTER_ULONG)

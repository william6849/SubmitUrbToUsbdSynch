NTSTATUS CallUsbdCompletionRoutine(
    [[maybe_unused]] IN PDEVICE_OBJECT DeviceObject,
    [[maybe_unused]] IN PIRP Irp, IN PVOID Context) {
  KeSetEvent((PKEVENT)Context, 0, FALSE);
  return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS SubmitUrbToUsbdSynch(PDEVICE_OBJECT next_device, PURB urb) {
  NTSTATUS nt_status = STATUS_UNSUCCESSFUL;
  PIO_STACK_LOCATION next_stack;
  KEVENT event = {};
  LARGE_INTEGER timeout;
  IO_STATUS_BLOCK io_status_block = {};

  if (!device_connected_) {
    return STATUS_UNSUCCESSFUL;
  }

  KeInitializeEvent(&event, NotificationEvent, FALSE);

  PIRP work_irp = IoBuildDeviceIoControlRequest(
      IOCTL_INTERNAL_USB_SUBMIT_URB, next_device_obj_, 0i64, 0, 0i64, 0, 1u,
      &event, &io_status_block);
  if (!work_irp) {
    return STATUS_INSUFFICIENT_RESOURCES;
  }

  next_stack = IoGetNextIrpStackLocation(work_irp);
  next_stack->Parameters.Others.Argument1 = urb;

  IoSetCompletionRoutine(work_irp,
                         (PIO_COMPLETION_ROUTINE)CallUsbdCompletionRoutine,
                         &event, TRUE, TRUE, TRUE);

  nt_status = IofCallDriver(next_device, work_irp);

  if (nt_status == STATUS_PENDING) {
    timeout.QuadPart = -50000000i64;

    if (KeWaitForSingleObject(&event, Executive, 0, 0, &timeout) ==
        STATUS_TIMEOUT) {
      IoCancelIrp(work_irp);
      KeWaitForSingleObject(&event, Executive, 0, 0, 0i64);
      io_status_block.Status = STATUS_TIMEOUT;
    }
  } else {
    io_status_block.Status = nt_status;
  }

  IofCompleteRequest(work_irp, 0);

  return io_status_block.Status;
}
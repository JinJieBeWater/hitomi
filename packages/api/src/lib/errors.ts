import { ORPCError } from "@orpc/server";

export type AdminBusinessCode =
  | "EMPLOYEE_NOT_FOUND"
  | "EMPLOYEE_CODE_CONFLICT"
  | "EMPLOYEE_DELETE_CONFIRMATION_MISMATCH"
  | "DEVICE_NOT_FOUND"
  | "DEVICE_DISABLED"
  | "DEVICE_DELETE_CONFIRMATION_MISMATCH"
  | "ATTENDANCE_CONFIG_INVALID_MINUTE"
  | "ATTENDANCE_CONFIG_INVALID_RANGE"
  | "ATTENDANCE_CONFIG_OVERLAPPED"
  | "FACE_PROFILE_NOT_FOUND"
  | "FACE_PROFILE_NOT_PENDING"
  | "DEVICE_PENDING_TASK_EXISTS";

type ErrorType = "BAD_REQUEST" | "NOT_FOUND" | "CONFLICT" | "UNAUTHORIZED";

export function adminError(type: ErrorType, businessCode: AdminBusinessCode, message: string) {
  return new ORPCError(type, {
    data: {
      businessCode,
    },
    message,
  });
}

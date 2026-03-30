const dateTimeFormatter = new Intl.DateTimeFormat("zh-CN", {
  timeZone: "Asia/Shanghai",
  year: "numeric",
  month: "2-digit",
  day: "2-digit",
  hour: "2-digit",
  minute: "2-digit",
  second: "2-digit",
  hourCycle: "h23",
});

export function formatDateTime(value: number | null | undefined) {
  if (!value) return "-";

  return dateTimeFormatter.format(new Date(value));
}

export function minutesToTimeInput(value: number | null | undefined) {
  if (value === null || value === undefined) return "";

  const hour = Math.floor(value / 60)
    .toString()
    .padStart(2, "0");
  const minute = (value % 60).toString().padStart(2, "0");

  return `${hour}:${minute}`;
}

export function timeInputToMinutes(value: string) {
  if (!value) return null;

  const [hour, minute] = value.split(":").map(Number);

  if (!Number.isInteger(hour) || !Number.isInteger(minute)) {
    return null;
  }

  return hour * 60 + minute;
}

export function labelDeviceStatus(value: string) {
  if (value === "active") return "启用";
  if (value === "disabled") return "禁用";

  return value;
}

export function colorDeviceStatus(value: string | null | undefined) {
  if (value === "active") return "success";
  if (value === "disabled") return "neutral";

  return "neutral";
}

export function labelFaceStatus(value: string | null | undefined) {
  if (!value) return "未录入";
  if (value === "pending") return "待录入";
  if (value === "success") return "录入成功";
  if (value === "failed") return "录入失败";
  if (value === "cancelled") return "已取消";

  return value;
}

export function colorFaceStatus(value: string | null | undefined) {
  if (!value) return "neutral";
  if (value === "pending") return "warning";
  if (value === "success") return "success";
  if (value === "failed") return "error";
  if (value === "cancelled") return "neutral";

  return "neutral";
}

export function labelAttendanceType(value: string) {
  if (value === "clock_in") return "上班";
  if (value === "clock_out") return "下班";

  return value;
}

export function colorAttendanceType(value: string | null | undefined) {
  if (value === "clock_in") return "primary";
  if (value === "clock_out") return "warning";

  return "neutral";
}

export function formatTimeRange(start: number | null | undefined, end: number | null | undefined) {
  if (start === null || start === undefined || end === null || end === undefined) {
    return "-";
  }

  return `${minutesToTimeInput(start)} - ${minutesToTimeInput(end)}`;
}

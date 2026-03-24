import { auth } from "@hitomi/auth";

export default defineEventHandler((event) => {
  return auth.handler(toWebRequest(event));
});

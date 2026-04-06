import { computed, ref } from "vue";

export type ActivationWizardDevice = {
  id: string;
  name: string;
  deviceCode: string;
  bootstrapSerial?: string | null;
  bootstrapSecret?: string | null;
};

export type ActivationWizardStep =
  | "connect"
  | "configure"
  | "provisioning"
  | "activation"
  | "complete";

export function useDeviceActivationWizard() {
  const step = ref<ActivationWizardStep>("connect");
  const statusMessage = ref("等待连接 USB 设备。");
  const errorMessage = ref("");
  const provisioningComplete = ref(false);
  const activationIssued = ref(false);
  const isComplete = computed(() => step.value === "complete");

  function reset() {
    step.value = "connect";
    statusMessage.value = "等待连接 USB 设备。";
    errorMessage.value = "";
    provisioningComplete.value = false;
    activationIssued.value = false;
  }

  function markConnected() {
    step.value = "configure";
    statusMessage.value = "设备已连接，请填写网络信息并写入配置。";
    errorMessage.value = "";
  }

  function markProvisioning() {
    step.value = "provisioning";
    statusMessage.value = "正在写入设备配置…";
    errorMessage.value = "";
  }

  function markProvisioned() {
    step.value = "activation";
    statusMessage.value = "配置写入完成，请发放激活并等待设备领取。";
    provisioningComplete.value = true;
    errorMessage.value = "";
  }

  function markActivationIssued() {
    step.value = "activation";
    statusMessage.value = "激活已发放，正在等待设备领取运行时凭据…";
    activationIssued.value = true;
    errorMessage.value = "";
  }

  function markActivated() {
    step.value = "complete";
    statusMessage.value = "设备已完成激活。";
    errorMessage.value = "";
  }

  function fail(message: string) {
    errorMessage.value = message;
  }

  return {
    step,
    statusMessage,
    errorMessage,
    provisioningComplete,
    activationIssued,
    isComplete,
    reset,
    markConnected,
    markProvisioning,
    markProvisioned,
    markActivationIssued,
    markActivated,
    fail,
  };
}

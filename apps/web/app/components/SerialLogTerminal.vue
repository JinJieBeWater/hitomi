<script setup lang="ts">
import { FitAddon } from "@xterm/addon-fit";
import { Terminal } from "@xterm/xterm";
import { onBeforeUnmount, onMounted, ref, watch } from "vue";

const props = defineProps<{ logs: string[] }>();

const containerRef = ref<HTMLDivElement | null>(null);
let terminal: Terminal | null = null;
let fitAddon: FitAddon | null = null;
let resizeObserver: ResizeObserver | null = null;

onMounted(() => {
  if (!containerRef.value) return;

  terminal = new Terminal({
    theme: {
      background: "#0d1117",
      foreground: "#c9d1d9",
      cursor: "#58a6ff",
      selectionBackground: "#264f78",
    },
    fontFamily: '"Fira Code", "Cascadia Code", "Consolas", monospace',
    fontSize: 12,
    lineHeight: 1.4,
    scrollback: 2000,
    cursorBlink: false,
    convertEol: true,
    disableStdin: true,
  });

  fitAddon = new FitAddon();
  terminal.loadAddon(fitAddon);
  terminal.open(containerRef.value);
  fitAddon.fit();

  // Write any logs that arrived before mount
  for (const line of props.logs) {
    terminal.writeln(line);
  }

  resizeObserver = new ResizeObserver(() => fitAddon?.fit());
  resizeObserver.observe(containerRef.value);
});

watch(
  () => props.logs.length,
  (newLen, oldLen) => {
    if (!terminal) return;
    for (let i = oldLen; i < newLen; i++) {
      terminal.writeln(props.logs[i] ?? "");
    }
    terminal.scrollToBottom();
  },
);

onBeforeUnmount(() => {
  resizeObserver?.disconnect();
  terminal?.dispose();
});
</script>

<template>
  <div ref="containerRef" class="size-full" />
</template>

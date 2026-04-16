export default defineAppConfig({
  ui: {
    colors: {
      primary: "amber",
      neutral: "slate",
    },
    button: {
      slots: {
        base: "rounded-[0.16rem] font-semibold inline-flex items-center justify-center disabled:cursor-not-allowed aria-disabled:cursor-not-allowed disabled:opacity-75 aria-disabled:opacity-75 transition-colors",
        label: "truncate tracking-[0.08em]",
      },
      variants: {
        size: {
          xs: {
            base: "px-2 py-1 text-[11px] gap-1",
          },
          sm: {
            base: "px-2.5 py-1.5 text-[11px] gap-1.5",
          },
          md: {
            base: "px-3 py-1.5 text-[12px] gap-1.5",
          },
          lg: {
            base: "px-3.5 py-2 text-[12px] gap-2",
          },
          xl: {
            base: "px-4 py-2 text-[13px] gap-2",
          },
        },
      },
    },
    input: {
      slots: {
        base: "workspace-ui-field w-full appearance-none placeholder:text-dimmed focus:outline-none disabled:cursor-not-allowed disabled:opacity-75",
        leadingIcon: "workspace-ui-field-icon shrink-0",
        trailingIcon: "workspace-ui-field-icon shrink-0",
      },
      variants: {
        variant: {
          outline: "",
          soft: "",
          subtle: "",
          ghost: "bg-transparent shadow-none",
          none: "bg-transparent border-transparent shadow-none",
        },
      },
    },
    select: {
      slots: {
        base: "workspace-ui-field relative group inline-flex items-center focus:outline-none disabled:cursor-not-allowed disabled:opacity-75",
        content:
          "workspace-ui-menu-content max-h-60 w-(--reka-select-trigger-width) overflow-hidden pointer-events-auto flex flex-col",
        label: "workspace-ui-menu-label",
        separator: "workspace-ui-menu-separator",
        item: "workspace-ui-menu-item",
        itemDescription: "workspace-ui-menu-description",
        itemLeadingIcon: "workspace-ui-field-icon shrink-0",
      },
      variants: {
        variant: {
          outline: "",
          soft: "",
          subtle: "",
          ghost: "bg-transparent shadow-none",
          none: "bg-transparent shadow-none border-transparent",
        },
      },
    },
    selectMenu: {
      slots: {
        base: "workspace-ui-field relative group inline-flex items-center focus:outline-none disabled:cursor-not-allowed disabled:opacity-75",
        content:
          "workspace-ui-menu-content max-h-60 w-(--reka-combobox-trigger-width) overflow-hidden pointer-events-auto flex flex-col",
        label: "workspace-ui-menu-label",
        separator: "workspace-ui-menu-separator",
        item: "workspace-ui-menu-item",
        itemDescription: "workspace-ui-menu-description",
        itemLeadingIcon: "workspace-ui-field-icon shrink-0",
        input:
          "workspace-ui-field border-0 border-b border-[var(--workspace-border)] rounded-none shadow-none",
      },
      variants: {
        variant: {
          outline: "",
          soft: "",
          subtle: "",
          ghost: "bg-transparent shadow-none",
          none: "bg-transparent shadow-none border-transparent",
        },
      },
    },
    badge: {
      slots: {
        base: "workspace-ui-badge",
      },
    },
    alert: {
      slots: {
        root: "workspace-ui-alert-root",
        title: "workspace-ui-alert-title",
        description: "workspace-ui-alert-description",
        icon: "workspace-ui-alert-icon shrink-0 size-5",
        close: "workspace-dialog-close",
      },
      defaultVariants: {
        variant: "outline",
      },
    },
    formField: {
      slots: {
        label: "workspace-section-label block",
        description: "mt-1 text-xs text-toned",
        error: "mt-2 text-xs font-medium text-rose-700 dark:text-rose-300",
        hint: "text-xs text-toned",
        help: "mt-2 text-xs text-toned",
      },
      variants: {
        orientation: {
          vertical: {
            container: "mt-2",
          },
        },
      },
    },
    modal: {
      slots: {
        content: "workspace-dialog-content",
        header: "workspace-dialog-header",
        body: "workspace-dialog-body",
        footer: "workspace-dialog-footer",
        title: "workspace-dialog-title",
        description: "workspace-dialog-description",
        close: "workspace-dialog-close",
      },
    },
    slideover: {
      slots: {
        content: "workspace-dialog-content",
        header: "workspace-dialog-header",
        body: "workspace-dialog-body",
        footer: "workspace-dialog-footer",
        title: "workspace-dialog-title",
        description: "workspace-dialog-description",
        close: "workspace-dialog-close",
      },
    },
    dropdownMenu: {
      slots: {
        content: "workspace-ui-menu-content min-w-40 flex flex-col",
        input:
          "workspace-ui-field border-0 border-b border-[var(--workspace-border)] rounded-none shadow-none",
        label: "workspace-ui-menu-label",
        separator: "workspace-ui-menu-separator",
        item: "workspace-ui-menu-item",
        itemDescription: "workspace-ui-menu-description",
        itemLeadingIcon: "workspace-ui-field-icon shrink-0",
      },
    },
    pageCard: {
      slots: {
        root: "workspace-ui-page-card",
        spotlight: "hidden",
        title: "text-base font-semibold tracking-[0.08em] text-highlighted",
        description: "text-sm text-toned",
      },
    },
    authForm: {
      slots: {
        header: "flex flex-col text-start",
        title: "text-lg font-semibold tracking-[0.08em] text-highlighted",
        description: "mt-1 text-sm text-toned",
        form: "space-y-4",
        footer: "mt-2 text-sm text-muted",
      },
    },
  },
});

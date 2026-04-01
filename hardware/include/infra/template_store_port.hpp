#pragma once

namespace infra {

class TemplateStorePort {
 public:
  virtual ~TemplateStorePort() = default;
  virtual bool begin() = 0;
};

class NoopTemplateStorePort final : public TemplateStorePort {
 public:
  bool begin() override {
    // Placeholder until an SD-backed template store is wired in.
    return false;
  }
};

}  // namespace infra

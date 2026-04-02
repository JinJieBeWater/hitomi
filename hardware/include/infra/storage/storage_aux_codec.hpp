#pragma once

#include <optional>
#include <string>

#include "infra/local_store.hpp"

namespace infra {

std::string encodeStorageAuxState(const StorageAuxState& storageAux);
std::optional<StorageAuxState> decodeStorageAuxState(const std::string& json);

}  // namespace infra

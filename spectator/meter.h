#pragma once

#include "id.h"

namespace spectator {

class Publisher;

class Meter {
 public:
  Meter(IdPtr id, Publisher* publisher);
  virtual ~Meter() = default;
  [[nodiscard]] IdPtr MeterId() const noexcept { return id_; }
  [[nodiscard]] virtual std::string_view Type() = 0;

 protected:
  void send(double value);

 private:
  IdPtr id_;
  Publisher* publisher_;
  std::string value_prefix_;
};

}  // namespace spectator

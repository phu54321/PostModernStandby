//
// Created by whyask37 on 2025-11-15.
//

#ifndef POSTMODERNSTANDBY_LAZY_H
#define POSTMODERNSTANDBY_LAZY_H

#include <optional>

// Simple lazy evaluation helper
//
// Usage:
//
//  auto lazyExpr = lazy([]() { return 1 + 2; });
//

template <class F> class Lazy {
  public:
    explicit Lazy(F f) : func(std::move(f)) {}

    auto get() {
        if (!value.has_value()) {
            value = func();
        }
        return *value;
    }

  private:
    F func;
    std::optional<decltype(func())> value;
};

template <class F> auto lazy(F f) { return Lazy<F>(std::move(f)); }

#endif // POSTMODERNSTANDBY_LAZY_H

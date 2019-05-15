#pragma once

#include "marian.h"

namespace marian {

class Factory : public std::enable_shared_from_this<Factory> {
protected:
  Ptr<Options> options_;

public:
  // construct with empty options
  Factory() : options_(New<Options>()) {}
  // construct with options
  Factory(Ptr<Options> options) : Factory() {
    options_->merge(options);
  }
  // construct with one or more individual parameters
  // Factory("var1", val1, "var2", val2, ...)
  template <typename T, typename... Args>
  Factory(const std::string& key, T value, Args&&... moreArgs) : Factory() {
    setOpts(key, value, std::forward<Args>(moreArgs)...);
  }
  // construct with options and one or more individual parameters
  // Factory(options, "var1", val1, "var2", val2, ...)
  template <typename... Args>
  Factory(Ptr<Options> options, Args&&... moreArgs) : Factory(options) {
    setOpts(std::forward<Args>(moreArgs)...);
  }

  virtual ~Factory() {}

  std::string str() { return options_->str(); }

  // retrieve an option
  // auto val = opt<T>("var");
  template <typename T>
  T opt(const std::string& key) { return options_->get<T>(key); }

  template <typename T>
  T opt(const std::string& key, T defaultValue) { return options_->get<T>(key, defaultValue); }

  // set a single option
  // setOpt("var", val);
  template <typename T>
  void setOpt(const std::string& key, T value) {
    options_->set(key, value);
  }

  // set multiple options
  // setOpts("var1", val1, "var2", val2, ...);
  template <typename T>
  void setOpts(const std::string& key, T value) { options_->set(key, value); }

  template <typename T, typename... Args>
  void setOpts(const std::string& key, T value, Args&&... moreArgs) {
    setOpt(key, value);
    setOpts(std::forward<Args>(moreArgs)...); // recursively set the remaining args
  }

  //void mergeOpts(const std::string& yaml) { options_->parse(yaml); }

  void mergeOpts(Ptr<Options> options) { options_->merge(options); }

  template <typename Cast>
  inline Ptr<Cast> as() {
    return std::dynamic_pointer_cast<Cast>(shared_from_this());
  }

  template <typename Cast>
  inline bool is() {
    return as<Cast>() != nullptr;
  }
};

// simplest form of Factory that just passes on options to the constructor of a layer type
template<class Class>
struct ConstructingFactory : public Factory {
  template <typename... Args>
  ConstructingFactory(Args&&... moreArgs) : Factory(std::forward<Args>(moreArgs)...) {}

  Ptr<Class> construct(Ptr<ExpressionGraph> graph) {
    return New<Class>(graph, options_);
  }
};

template <class BaseFactory> // where BaseFactory : Factory
class Accumulator : public BaseFactory {
  typedef BaseFactory Factory;

public:
  Accumulator() : Factory() {}
  Accumulator(Ptr<Options> options) : Factory(options) {}
  template <typename... Args>
  Accumulator(Ptr<Options> options, Args&&... moreArgs) : Factory(options, std::forward<Args>(moreArgs)...) {}
  template <typename T, typename... Args>
  Accumulator(const std::string& key, T value, Args&&... moreArgs) : Factory(key, value, std::forward<Args>(moreArgs)...) {}
  Accumulator(const Factory& factory) : Factory(factory) {}
  Accumulator(const Accumulator&) = default;
  Accumulator(Accumulator&&) = default;

  // deprecated chaining syntax
  template <typename T>
  Accumulator& operator()(const std::string& key, T value) {
    Factory::setOpt(key, value);
    return *this;
  }

  Accumulator& operator()(Ptr<Options> options) {
    Factory::mergeOpts(options);
    return *this;
  }

  Accumulator<Factory> clone() {
    return Accumulator<Factory>(Factory::clone());
  }
};
}  // namespace marian

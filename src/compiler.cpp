#include "compiler.hpp"
#include <memory>

extern "C" {
void drop_string(const char *);

void drop_result(void *);

const char *result_module(void *, const char *, bool *);

void *init_compiler(void);

void drop_compiler(void *);

bool drop_module(void *, const char *);

bool load_module(void *, const char *, const char *);

bool bind_module(void *, const char *, const char *);

void *try_build(void *);
}

void StringHandleDeleter::operator()(const char *inner) const {
  ::drop_string(inner);
}

void ResultHandleDeleter::operator()(void *handle) const {
  ::drop_result(handle);
}

void CompilerHandleDeleter::operator()(void *handle) const {
  ::drop_compiler(handle);
}

StringHandle &StringHandle::operator=(StringHandle &&other) {
  if (this != &other) {
    this->inner = std::move(other.inner);
  }
  return *this;
}

ResultHandle &ResultHandle::operator=(ResultHandle &&other) {
  if (this != &other) {
    this->handle = std::move(other.handle);
  }
  return *this;
}

StringHandle ResultHandle::module(const char *name, bool *is_err) {
  return StringHandle(::result_module(this->handle.get(), name, is_err));
}

CompilerHandle::CompilerHandle(void) {
  this->handle = std::unique_ptr<void, CompilerHandleDeleter>(init_compiler());
}

CompilerHandle &CompilerHandle::operator=(CompilerHandle &&other) {
  if (this != &other) {
    this->handle = std::move(other.handle);
  }
  return *this;
}

bool CompilerHandle::drop_module(const char *name) {
  return ::drop_module(this->handle.get(), name);
}

bool CompilerHandle::load_module(const char *name, const char *content) {
  return ::load_module(this->handle.get(), name, content);
}

bool CompilerHandle::bind_module(const char *name, const char *content) {
  return ::bind_module(this->handle.get(), name, content);
}

ResultHandle CompilerHandle::try_build(void) {
  return ResultHandle(::try_build(this->handle.get()));
}

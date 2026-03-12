#include "compiler.hpp"

extern "C" void drop_string(const char *);

extern "C" void drop_result(void *);

extern "C" const char *result_module(void *, const char *, bool *);

extern "C" void *init_compiler(void);

extern "C" void drop_compiler(void *);

extern "C" bool drop_module(void *, const char *);

extern "C" bool load_module(void *, const char *, const char *);

extern "C" bool bind_module(void *, const char *, const char *);

extern "C" void *try_build(void *);

StringHandle::~StringHandle(void) { ::drop_string(this->inner); }

ResultHandle::~ResultHandle(void) { ::drop_result(this->handle); }

StringHandle ResultHandle::module(const char *name, bool *is_err) {
  return StringHandle(::result_module(this->handle, name, is_err));
}

CompilerHandle::CompilerHandle(void) { this->handle = init_compiler(); }

CompilerHandle::~CompilerHandle(void) { drop_compiler(this->handle); }

bool CompilerHandle::drop_module(const char *name) {
  return ::drop_module(this->handle, name);
}

bool CompilerHandle::load_module(const char *name, const char *content) {
  return ::load_module(this->handle, name, content);
}

bool CompilerHandle::bind_module(const char *name, const char *content) {
  return ::bind_module(this->handle, name, content);
}

ResultHandle CompilerHandle::try_build(void) {
  return ResultHandle(::try_build(this->handle));
}

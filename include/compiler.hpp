#pragma once

struct StringHandle final {
  friend class CompilerHandle;
  friend class ResultHandle;

private:
  StringHandle(const char *inner) : inner(inner) {}

public:
  const char *inner;

  ~StringHandle(void);
};

class ResultHandle final {
  friend class CompilerHandle;

private:
  void *handle;

  ResultHandle(void *handle) : handle(handle) {}

public:
  ~ResultHandle(void);

  StringHandle module(const char *name, bool *is_err);
};

class CompilerHandle final {
private:
  void *handle;

public:
  CompilerHandle(void);

  ~CompilerHandle(void);

  bool drop_module(const char *name);

  bool load_module(const char *name, const char *content);

  bool bind_module(const char *name, const char *content);

  ResultHandle try_build(void);
};

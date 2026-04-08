#pragma once

#include <memory>
#include <utility>

/** Functor for deleting `StringHandle` handles. */
struct StringHandleDeleter {
  void operator()(const char *inner) const;
};

/** Functor for deleting `ResultHandle` handles. */
struct ResultHandleDeleter {
  void operator()(void *handle) const;
};

/** Functor for deleting `CompilerHandle` handles. */
struct CompilerHandleDeleter {
  void operator()(void *handle) const;
};

/** A uniquely owned instance of a Rust-produced string. */
struct StringHandle final {
  friend class CompilerHandle;
  friend class ResultHandle;

private:
  StringHandle(const char *inner) : inner(inner) {}

public:
  std::unique_ptr<const char, StringHandleDeleter> inner;

  StringHandle(StringHandle const &) = delete;

  StringHandle(StringHandle &&other) : inner(std::move(other.inner)) {}

  StringHandle &operator=(StringHandle const &) = delete;

  StringHandle &operator=(StringHandle &&other);
};

/** A uniquely owned instance of a compiler build result. */
class ResultHandle final {
  friend class CompilerHandle;

private:
  std::unique_ptr<void, ResultHandleDeleter> handle;

  ResultHandle(void *handle) : handle(handle) {}

public:
  StringHandle module(const char *name, bool *is_err);

  ResultHandle(ResultHandle const &) = delete;

  ResultHandle(ResultHandle &&other) : handle(std::move(other.handle)) {}

  ResultHandle &operator=(ResultHandle const &) = delete;

  ResultHandle &operator=(ResultHandle &&other);
};

/** A uniquely owned instance of a compiler from Rust. */
class CompilerHandle final {
private:
  std::unique_ptr<void, CompilerHandleDeleter> handle;

public:
  CompilerHandle(void);

  CompilerHandle(CompilerHandle const &) = delete;

  CompilerHandle(CompilerHandle &&other) : handle(std::move(other.handle)) {}

  CompilerHandle &operator=(CompilerHandle const &) = delete;

  CompilerHandle &operator=(CompilerHandle &&other);

  bool drop_module(const char *name);

  bool load_module(const char *name, const char *content);

  bool bind_module(const char *name, const char *content);

  ResultHandle try_build(void);
};

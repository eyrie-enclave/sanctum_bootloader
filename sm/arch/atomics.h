#ifndef BARE_ATOMIC_H_INCLUDED
#define BARE_ATOMIC_H_INCLUDED

// C++11 lock-free atomic flag.

typedef struct {
//public:
  // Initializes the flag to an unknown value.
  //inline atomic_flag() = default;

  // Atomic flags cannot be copied.
  //atomic_flag(const atomic_flag&) = delete;
  //atomic_flag& operator=(const atomic_flag&) = delete;
  //atomic_flag& operator=(const atomic_flag&) volatile = delete;

  int flag;
} atomic_flag;

static inline bool atomic_flag_test_and_set(atomic_flag f) {
  return __sync_lock_test_and_set(&(f.flag), true);
}

static inline void atomic_flag_clear(atomic_flag f) {
  f.flag = false;
}

//private:
// C++11 atomic integers.
//
// The only specializations implemented by the bare-metal library are
// atomic<uintptr_t> and atomic<size_t>.

static inline void atomic_init(uintptr_t* object, uintptr_t value) {
  *object = value;
}
static inline uintptr_t atomic_load(uintptr_t* object) {
  return *object;
}
static inline void atomic_store(uintptr_t* object, uintptr_t value) {
  *object = value;
}
static inline uintptr_t atomic_fetch_add(uintptr_t* object, uintptr_t value) {
  return __sync_fetch_and_add(object, value);
}
static inline uintptr_t atomic_fetch_sub(uintptr_t* object, uintptr_t value) {
  return __sync_fetch_and_sub(object, value);
}

#endif  // !definded(BARE_ATOMIC_H_INCLUDED)

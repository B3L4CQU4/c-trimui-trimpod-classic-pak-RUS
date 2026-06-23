  /* First some generic implementations */
#if   defined(HAVE_SIGALTSTACK_THREADS)
  #include "thread-unix.c"

  /* Now the CPU-specific implementations */
#else
  /* Nothing? OK, give up */
  #error Missing thread impl
#endif

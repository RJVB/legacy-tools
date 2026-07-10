Some (very) old utilities I wrote over the years. Some I still use and even keep a bit up to date...

A number of these will require my equally old "cx" library

## dlsym.c

This can be built as an executable that acts like a commandline interface to `dlopen(2)`, `dlsym(2)` and `dladdr(2)` with an additional feature to inject shared libraries:

```
> dlsym [-i inject1] [[-i inject2] ...] library [symbol(s)]
```

Here, `library` can be `""` to let the application do lookups in its own memory image (e.g. `dlsym "" dlopen` to know which library provides the `dlopen` function).

On Darwin (aka the Mac's OS) it is possible to load executables with `dlopen` too. On Linux (and possibly on other platforms using the ELF format), this is not supported.

Therefore, the file can also be built as a shared library on Linux:

```
> gcc [-g -O0] -DHIJACK_LIB -fPIC -shared dlsym.c -o libdlsym_hijack.so -ldl
```

The resulting library can be injected (only!) using `LD_PRELOAD`:

```
env LD_PRELOAD=/path/to/libdlsym_hijack.so executable [-i inject1 [[-i inject2] ...] [symbol(s)]
```

This emulates the equivalent dlsym invocation on Darwin.

The implementation is deceptively simple: the glibc function `__libc_start_main` is overridden (aka `main()` is hijacked) by a slightly adapted version of the `dlsym` `main()` function. Thus, the entire executable is loaded and the dynamic loader does its work, but the application's `main()` function never gets called. Just like would happen if `dlopen()` could load executables.

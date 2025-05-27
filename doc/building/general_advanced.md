[Developer documentation](../readme.md) / [Compiling Inkscape](./readme.md) /


# Advanced build options valid for all operating systems

This page lists detailed options for building Inkscape. They are valid for all operating systems.

Before this you should already have read the basic instructions for building on your operating system - see [Compiling Inkscape](./readme.md).


Build Options
-------------

A number of configuration settings can be overridden through CMake. To
see a list of the options available for Inkscape, run:

```sh
cmake -L
```
or, for more advanced cmake settings:

```sh
cmake --help
```

For example, to build Inkscape with only SVG 1 support, and no SVG 2, do:

```sh
cmake .. -DWITH_SVG2=OFF
```

Or, to build Inkscape with debugging symbols, do:

```sh
cmake -DCMAKE_BUILD_TYPE=Debug ..
```


# Internal lib2geom

If the version of Inkscape you are trying to build depends on recent lib2geom features, you'll need to add `-DWITH_INTERNAL_2GEOM=ON` to the cmake command to use the development version of lib2geom rather than the system version. 

# Make vs Ninja

Inkscape can also be built with Make instead of Ninja. This has no real benefit except if you are used to calling `make`.

If you use the suggested CMake commands but without `-G Ninja` then you will need to run `make` instead of `ninja`.

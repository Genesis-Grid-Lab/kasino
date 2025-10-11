# kasino
Simple Haitien Kasino game

## Web build (Emscripten)

Emscripten support lets you run the engine in the browser with the GLFW
backend. After installing the Emscripten SDK, generate a build with

```sh
emcmake cmake -B build-web -S .
cmake --build build-web
```

The resulting `Kasino.html` alongside its `.wasm` and `.data` files can be
served with any static web server.

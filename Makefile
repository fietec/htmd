EMCC = /home/constantijn/bin/emsdk/upstream/emscripten/emcc

cli: src/cli.c src/render.c
	gcc -Wall -Wextra -Iinclude -DHTMD_CLI -o htmd src/cli.c src/render.c

wasm: src/render.c
	$(EMCC) -Iinclude src/render.c -o site/markdown.js \
	-s MODULARIZE=1 -s EXPORT_NAME="Module" \
	-s EXPORTED_FUNCTIONS="['_render_markdown', '_malloc', '_free']" \
	-s EXPORTED_RUNTIME_METHODS="['cwrap','lengthBytesUTF8','stringToUTF8','UTF8ToString']"


EMCC = /home/constantijn/bin/emsdk/upstream/emscripten/emcc

cli: src/cli.c src/htmd.c
	gcc -Wall -Wextra -Iinclude -DHTMD_CLI -o htmd src/cli.c src/htmd.c

wasm: src/htmd.c
	$(EMCC) -Iinclude src/htmd.c -o site/markdown.js \
	-s MODULARIZE=1 -s EXPORT_NAME="Module" \
	-s EXPORTED_FUNCTIONS="['_render_markdown', '_malloc', '_free']" \
	-s EXPORTED_RUNTIME_METHODS="['cwrap','lengthBytesUTF8','stringToUTF8','UTF8ToString']"


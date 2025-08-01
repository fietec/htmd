#ifndef _HTMD_H
#define _HTMD_H

#include <stdio.h>

#define eprintfn(msg, ...) do{fprintf(stderr, "[ERROR] " msg "\n", ##__VA_ARGS__);}while(0)

#ifdef HTMD_CLI
    char* render_markdown(char *input);
#else
    #include <emscripten/emscripten.h>
    EMSCRIPTEN_KEEPALIVE char* render_markdown(char *input);
#endif // HTMD_CLI

#endif // _HTMD_H

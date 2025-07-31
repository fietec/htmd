#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

typedef enum{
    Fmt_Bold,
    Fmt_Italic,
    _Fmt_Count
} Format;

#define eprintfn(msg, ...) do{fprintf(stderr, "[ERROR] " msg "\n", ##__VA_ARGS__);}while(0)

char* read_file(const char *path)
{
    FILE *file = fopen(path, "r");
    if (file == NULL){
        eprintfn("Could not open file '%s': %s!", path, strerror(errno));
        return NULL;
    }
    struct stat attr;
    if (fstat(fileno(file), &attr) == -1){
        eprintfn("Could not read stats from '%s': %s!", path, strerror(errno));
        fclose(file);
        return NULL;
    }
    size_t size = attr.st_size;
    char *content = calloc(1, size+1);
    assert(content != NULL && "Buy more RAM");
    if (fread(content, 1, size, file) != size){
        eprintfn("Could not read all bytes from '%s'!", path);
        fclose(file);
        free(content);
        return NULL;
    }
    fclose(file);
    return content;
}

char* get_next_line(char *line_start, char *buffer, size_t buffer_size)
{
    if (line_start == NULL || buffer == NULL || *line_start == '\0') return NULL;
    char *line_end = strchrnul(line_start, '\n');
    size_t line_len = line_end - line_start;
    size_t write_size = line_len < buffer_size-1 ? line_len : buffer_size-2;
    (void) memcpy(buffer, line_start, write_size);
    buffer[write_size] = '\0';
    if (*line_end == '\0') return NULL;
    return line_end + 1;
}

bool line_is_blank(char *line)
{
    char *pr = line;
    while (*pr != '\0'){
        if (!isspace(*pr)) return false;
        pr++;
    }
    return true;
}

void parse_text(char *line, String_Builder *sb)
{
    Format fmts[_Fmt_Count] = {0};
    char *pr = line;
    char *s = pr;
    while (true){
        if (*pr == '*'){
            sb_append_buf(sb, s, pr-s);
            if (*(pr+1) == '*'){
                // bold
                sb_append_cstr(sb, fmts[Fmt_Bold]++%2 == 0 ? "<strong>":"</strong>");
                pr += 1;
            } else{
                // italic
                sb_append_cstr(sb, fmts[Fmt_Italic]++%2 == 0 ? "<em>":"</em>");
            }
            s = pr + 1;
        }
        if (*pr == '_'){
            sb_append_buf(sb, s, pr-s);
            if (*(pr+1) == '_'){
                // bold
                sb_append_cstr(sb, fmts[Fmt_Bold]++%2 == 0 ? "<strong>":"</strong>");
                pr += 1;
            } else{
                // italic
                sb_append_cstr(sb, fmts[Fmt_Italic]++%2 == 0 ? "<em>":"</em>");
            }
            s = pr + 1;
        }
        if (*pr == '\0'){
            sb_append_buf(sb, s, pr-s);
            break;
        }
        pr += 1;
    }
}

size_t get_header_level(char *pr)
{
    size_t count = 0;
    while (*pr++ == '#'){
        count++;
    }
    return count;
}

int main(void)
{
    int result = 0;
    
    char *content = read_file("test.md");
    char line_buffer[4096];
    char *line = content;
    printf("%s\n", content);

    String_Builder output_sb = {0};
    String_Builder temp_sb = {0};

    bool last_line_blank = false;
    
    while (true){
        line = get_next_line(line, line_buffer, sizeof(line_buffer));
        if (line == NULL) break;
        // TODO: implement more markdown features
        if (line_is_blank(line_buffer)){
            if (last_line_blank){
                sb_append_cstr(&output_sb, "<br>\n");
            }
            last_line_blank = !last_line_blank;
            continue;
        }
        last_line_blank = false;
        size_t header_level = get_header_level(line_buffer);
        if (header_level > 0){
            // add header
            parse_text(line_buffer+header_level, &temp_sb);
            sb_appendf(&output_sb, "<h%zu>%.*s</h%zu>\n", header_level, (int) temp_sb.count, temp_sb.items, header_level);
            temp_sb.count = 0;
        }
        else {
            // add paragraph
            parse_text(line_buffer, &temp_sb);
            sb_appendf(&output_sb, "<p> %.*s </p>\n", (int) temp_sb.count, temp_sb.items);
            temp_sb.count = 0;
        }
    }
    FILE *file = fopen("out.html", "w");
    if (file == NULL){
        eprintfn("Could not open output file: %s", strerror(errno));
        return_defer(1);
    }
    (void) fwrite(output_sb.items, 1, output_sb.count, file);
    fclose(file);
  defer:
    free(content);
    free(output_sb.items);
    free(temp_sb.items);
    return result;
}

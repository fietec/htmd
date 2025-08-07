#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <htmd.h>

#ifndef HTMD_CLI
#define NOB_IMPLEMENTATION
#endif // HTMD_CLI
#define NOB_STRIP_PREFIX
#include <nob.h>

typedef enum{
    Style_Bold,
    Style_Italic,
    Style_Code,
    Style_Strike,
    _Style_Count
} Style;

char* strchrnul(const char *r, int c)
{
    char *p = strchr(r, c);
    if (p == NULL) return (char*)r+strlen(r);
    return p;
}

char* get_next_line(char *line_start, char *buffer, size_t buffer_size)
{
    if (line_start == NULL || buffer == NULL || *line_start == '\0') return NULL;
    char *line_end = strchrnul(line_start, '\n');
    size_t line_len = line_end - line_start;
    size_t write_size = line_len < buffer_size-1 ? line_len : buffer_size-2;
    (void) memcpy(buffer, line_start, write_size);
    buffer[write_size] = '\0';
    if (*line_end == '\0') return line_end;
    return line_end + 1;
}

bool starts_with(char *string, const char *pattern)
{
    return strncmp(string, pattern, strlen(pattern)) == 0;
}

bool is_code_block(char *line)
{
    char *pr = line;
    size_t indent = 0;
    while (indent < 4){
        switch (*pr){
            case ' ': {
                indent += 1;
            }break;
            case '\t':{
                indent += 4;
            }break;
            default:{
                if (starts_with(pr, "```")) return true;
                return false;
            }
        }
        ++pr;
    }
    return true;
}

char* skip_whitespace(char *pr)
{
    while (true){
        switch (*pr){
            case ' ':
            case '\t':
            case '\v':{
                pr += 1;
            }break;
            default: return pr;
        }
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

char* is_enum(char *line)
{
    char *pr = skip_whitespace(line);
    if (isdigit(*pr++) && *pr++ == '.') return pr;
    return NULL;
}

char* is_list(char *line)
{
    char *pr = skip_whitespace(line);
    if (*pr == '-') return pr+1;
    return NULL;
}

bool line_is_empty(char *line)
{
    char *pr = skip_whitespace(line);
    return *pr == '\0';
}

// Rendering

void render_text(char *pr, String_Builder *sb)
{
    bool styles[_Style_Count] = {0};
    char *last = pr;
    while (true){
        if (starts_with(pr, "**") || starts_with(pr, "__")){
            sb_append_buf(sb, last, pr-last);
            sb_append_cstr(sb, styles[Style_Bold]? "</strong>" : "<strong>");
            styles[Style_Bold] = !styles[Style_Bold];
            last = pr+=2;
            continue;
        }
        // TODO: add link and image support
        switch (*pr){
            case '*':
            case '_':{
                sb_append_buf(sb, last, pr-last);
                sb_append_cstr(sb, styles[Style_Italic]? "</em>" : "<em>");
                styles[Style_Italic] = !styles[Style_Italic];
                last = ++pr;
            }continue;
            case '~':{
                sb_append_buf(sb, last, pr-last);
                sb_append_cstr(sb, styles[Style_Strike]? "</s>" : "<s>");
                styles[Style_Strike] = !styles[Style_Strike];
                last = ++pr;
            }continue;
            case '`':{
                sb_append_buf(sb, last, pr-last);
                sb_append_cstr(sb, styles[Style_Code]? "</code>" : "<code>");
                styles[Style_Code] = !styles[Style_Code];
                last = ++pr;
            }continue;
            case '\\':{
                // escaping
                sb_append_buf(sb, last, pr-last);
                sb_append_buf(sb, ++pr, 1);
                last = ++pr;
            }continue;
            case '\0':{
                sb_append_buf(sb, last, pr-last);
                return;
            }
        }
        pr += 1;
    }
}

void render_paragraph(char *pr, String_Builder *sb)
{
    sb_append_cstr(sb, "<p>");
    render_text(pr, sb);
    sb_append_cstr(sb, "</p>\n");
}

void render_header(char *pr, String_Builder *sb, size_t header_level)
{
    sb_appendf(sb, "<h%zu>", header_level);
    render_text(pr+header_level, sb);
    sb_appendf(sb, "</h%zu>\n", header_level);
}

void render_html_escaped(char *pr, String_Builder *sb)
{
    char *last = pr;
    while (true){
        switch (*pr){
            case '<': {
                sb_append_buf(sb, last, pr-last);
                sb_append_cstr(sb, "&lt;");
                last = ++pr;
            }continue;
            case '>': {
                sb_append_buf(sb, last, pr-last);
                sb_append_cstr(sb, "&gt;");
                last = ++pr;
            }continue;
            case '\0':{
                sb_appendf(sb, "%.*s\n", (int) (pr-last), last);
            } return;
        }
        pr += 1;
    }
}

char* render_code_block(char *pr, char *line_buffer, size_t buffer_size, char *line_ptr, String_Builder *sb)
{
    // TODO: parse language spec
    (void) pr;
    sb_append_cstr(sb, "<pre>\n<code>");
    while (true){
        line_ptr = get_next_line(line_ptr, line_buffer, buffer_size);
        if (line_ptr == NULL || starts_with(line_buffer, "```")) break;
        render_html_escaped(line_buffer, sb);
    }
    sb_append_cstr(sb, "</code>\n</pre>\n");
    return line_ptr;
}

char* render_blockquote(char *pr, char *line_buffer, size_t buffer_size, char *line_ptr, String_Builder *sb)
{
    sb_append_cstr(sb, "<blockquote>\n");
    (void) pr;
    while (true){
        if (line_ptr == NULL || *line_buffer != '>') break;
        render_text(line_buffer+1, sb);
        sb_append_cstr(sb, "<br>\n");
        line_ptr = get_next_line(line_ptr, line_buffer, buffer_size);
    }
    sb_append_cstr(sb, "</blockquote>\n");
    return line_ptr;
}

char* render_unordered_list(char *pr, char *line_buffer, size_t buffer_size, char *line_ptr, String_Builder *sb)
{
    // TODO: support nested lists
    sb_append_cstr(sb, "<ul>\n");
    while (true){
        if (line_ptr == NULL || (pr = is_list(line_buffer)) == NULL) break;
        sb_append_cstr(sb, "  <li>");
        render_text(pr, sb);
        sb_append_cstr(sb, "</li>\n");
        line_ptr = get_next_line(line_ptr, line_buffer, buffer_size);
    }
    sb_append_cstr(sb, "</ul>\n");
    return line_ptr;
}

char* render_ordered_list(char *pr, char *line_buffer, size_t buffer_size, char *line_ptr, String_Builder *sb)
{
    // TODO: support nested lists
    sb_append_cstr(sb, "<ol>\n");
    while (true){
        if (line_ptr == NULL || (pr = is_enum(line_buffer)) == NULL) break;
        sb_append_cstr(sb, "  <li>");
        render_text(pr, sb);
        sb_append_cstr(sb, "</li>\n");
        line_ptr = get_next_line(line_ptr, line_buffer, buffer_size);
    }
    sb_append_cstr(sb, "</ol>\n");
    return line_ptr;
}

char* render_markdown(char *input)
{
    char line_buffer[MAX_LINE_LEN+1];
    char *line_ptr = input;

    String_Builder sb_out = {0};
    bool last_line_empty = false;

    // iterate over lines
    while ((line_ptr = get_next_line(line_ptr, line_buffer, sizeof(line_buffer))) != NULL){
        char *pr = line_buffer;

        // empty line spacing
        if (line_is_empty(pr)){
            if (last_line_empty){
                sb_append_cstr(&sb_out, "\n<br>\n");
            }
            last_line_empty = !last_line_empty;
            continue;
        }
        last_line_empty = false;
        
        // code blocks
        if (is_code_block(pr)){
            line_ptr = render_code_block(pr, line_buffer, sizeof(line_buffer), line_ptr, &sb_out);
            continue;
        }
        pr = skip_whitespace(pr);

        // headings
        size_t header_level = get_header_level(pr);
        if (header_level > 0){
            render_header(pr, &sb_out, header_level);
            continue;
        }

        // seperators
        if (starts_with(pr, "---") || starts_with(pr, "***") || starts_with(pr, "___")){
            sb_append_cstr(&sb_out, "<hr>\n");
            continue;
        }
        if (is_enum(pr)){
            line_ptr = render_ordered_list(pr, line_buffer, sizeof(line_buffer), line_ptr, &sb_out);
            continue;
        }
        
        switch (*pr){
            case '-':
            case '+':
            case '*':{
                line_ptr = render_unordered_list(pr, line_buffer, sizeof(line_buffer), line_ptr, &sb_out);
                continue;
            }break;
            case '>':{
                line_ptr = render_blockquote(pr, line_buffer, sizeof(line_buffer), line_ptr, &sb_out);
                continue;
            }break;
        }

        // render normal text line
        render_paragraph(pr, &sb_out);
    }
    sb_append_null(&sb_out);
    return sb_out.items;
}

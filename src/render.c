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

char* find_close(char *pr, char s, char e)
{
    size_t depth = 1;
    while (true){
        char c = *pr;
        if (c == s) depth+=1;
        else if (c == e) depth-=1;
        
        if (depth == 0 || c == '\0') return pr;
        ++pr;
    }
}

size_t count_char(char *pr, char c)
{
    size_t count = 0;
    while (*pr++ == c) ++count;
    return count;
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
void render_text_field(char *pr, size_t n, String_Builder *sb);

char* try_render_link(char *pr, String_Builder *sb)
{
    char *display_start = ++pr;
    char *display_end = find_close(pr, '[', ']');
    pr = display_end;
    if (*pr == '\0') return NULL;
    if (*++pr != '(') return NULL;
    char *link_start = ++pr;
    char *link_end = strchrnul(pr, ')');
    if (*link_end == '\0') return NULL;
    if (strchrnul(link_start, ' ') < link_end) return NULL;
    sb_appendf(sb, "<a href=\"%.*s\">", (int) (link_end-link_start), link_start);
    render_text_field(display_start, (size_t) (display_end-display_start), sb);
    sb_append_cstr(sb, "</a>");
    return link_end;
}

char *try_render_autolink(char *pr, String_Builder *sb)
{
    char *link_start = ++pr;
    char *link_end = strchrnul(pr, '>');
    if (*link_end == '\0') return NULL;
    sb_appendf(sb, "<a href=\"%.*s\">%.*s</a>", (int) (link_end-link_start), link_start, (int) (link_end-link_start), link_start);
    return link_end;
}

char* try_render_image(char *pr, String_Builder *sb)
{
    char *display_start = pr+=2;
    char *display_end = strchrnul(pr, ']');
    pr = display_end;
    if (*pr == '\0') return NULL;
    if (*++pr != '(') return NULL;
    char *link_start = ++pr;
    char *link_end = strchrnul(pr, ')');
    if (*link_end == '\0') return NULL;
    if (strchrnul(link_start, ' ') < link_end) return NULL;
    sb_appendf(sb, "<img src=\"%.*s\" alt=\"%.*s\">", (int) (link_end-link_start), link_start, (int) (display_end-display_start), display_start);
    return link_end;
}

void render_text_field(char *pr, size_t n, String_Builder *sb)
{
    bool styles[_Style_Count] = {0};
    char *start = pr;
    pr = skip_whitespace(pr);
    char *last = pr;
    while (true){
        if (*pr == '`'){
            sb_append_buf(sb, last, pr-last);
            sb_append_cstr(sb, styles[Style_Code]? "</code>" : "<code>");
            styles[Style_Code] = !styles[Style_Code];
            last = ++pr;
            continue;
        }
        if ((size_t) (pr-start) >= n){
            sb_append_buf(sb, last, pr-last);
            return;
        }
        if (!styles[Style_Code]){
            if (starts_with(pr, "**") || starts_with(pr, "__")){
                sb_append_buf(sb, last, pr-last);
                sb_append_cstr(sb, styles[Style_Bold]? "</strong>" : "<strong>");
                styles[Style_Bold] = !styles[Style_Bold];
                last = pr+=2;
                continue;
            }
            if (starts_with(pr, "![")){
                sb_append_buf(sb, last, pr-last);
                char *result = try_render_image(pr, sb);
                if (result == NULL){
                    sb_append_buf(sb, pr, 1);
                }else{
                    pr = result;
                }
                last = ++pr;
                continue;
            }
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
                case '[':{
                    sb_append_buf(sb, last, pr-last);
                    char *result = try_render_link(pr, sb);
                    if (result == NULL){
                        sb_append_buf(sb, pr, 1);
                    }else{
                        pr = result;
                    }
                    last = ++pr;
                } continue;
                case '<':{
                    sb_append_buf(sb, last, pr-last);
                    char *result = try_render_autolink(pr, sb);
                    if (result == NULL){
                        sb_append_cstr(sb, "&lt;");
                    }else{
                        pr = result;
                    }
                    last = ++pr;
                }continue;
                case '\\':{
                    // escaping
                    sb_append_buf(sb, last, pr-last);
                    sb_append_buf(sb, ++pr, 1);
                    last = ++pr;
                }continue;
            }
        }
        pr += 1;
    }
}

void render_text(char *pr, String_Builder *sb)
{
    render_text_field(pr, strlen(pr), sb);
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
    sb_append_cstr(sb, "<pre><code>");
    while (true){
        line_ptr = get_next_line(line_ptr, line_buffer, buffer_size);
        if (line_ptr == NULL || starts_with(line_buffer, "```")) break;
        render_html_escaped(line_buffer, sb);
    }
    sb_append_cstr(sb, "</code></pre>\n");
    return line_ptr;
}

char* render_blockquote(char *pr, char *line_buffer, size_t buffer_size, char *line_ptr, String_Builder *sb)
{
    size_t last_level = 0;
    size_t depth = 0;
    char *next_line = line_ptr;
    while (line_ptr != NULL){
        pr = skip_whitespace(line_buffer);
        size_t level = count_char(pr, '>');
        if (level == 0) break;
        line_ptr = next_line;
        if (level > last_level){
            sb_append_cstr(sb, "<blockquote>\n");
            depth += 1;
        }else if (level < last_level){
            sb_append_cstr(sb, "</blockquote>\n");
            depth -= 1;
        }
        last_level = level;
        render_text(pr+level, sb);
        sb_append_cstr(sb, "<br>\n");
        next_line = get_next_line(line_ptr, line_buffer, buffer_size);
    }
    for (size_t i=0; i<depth; ++i){
        sb_append_cstr(sb, "</blockquote>\n");
    }
    
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
        size_t header_level = count_char(pr, '#');
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
            case '-':{
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

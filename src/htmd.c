#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include <htmd.h>

#ifndef HTMD_CLI
    #define NOB_IMPLEMENTATION
#endif // HTMD_CLI
#define NOB_STRIP_PREFIX
#include <nob.h>

typedef enum{
    Fmt_Bold,
    Fmt_Italic,
    Fmt_Code,
    _Fmt_Count
} Format;

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

bool line_is_blank(char *line)
{
    char *pr = line;
    while (*pr != '\0'){
        if (!isspace(*pr)) return false;
        pr++;
    }
    return true;
}

char* try_parse_link(char *pr, String_Builder *sb)
{
    char *text_start = ++pr;
    char *result = text_start;
    char *text_end = strchrnul(pr, ']');
    pr = text_end;
    if (*pr == '\0') return_defer(pr);
    if (*++pr != '(') return_defer(strchr(pr, '\0'));
    char *link_start = ++pr;
    char *link_end = strchrnul(pr, ')');
    if (*link_end == '\0') return_defer(link_end);
    // TODO: link may not have any non-trailing spaces
    sb_appendf(sb, "<a href=\"%.*s\">%.*s</a>", (int) (link_end-link_start), link_start, (int) (text_end-text_start), text_start);
    return link_end;

defer:
    sb_append_buf(sb, text_start, result-text_start);
    return result;
}

char* try_parse_image(char *pr, String_Builder *sb)
{
    char *text_start = pr+=2;
    char *result = text_start;
    char *text_end = strchrnul(pr, ']');
    pr = text_end;
    if (*pr == '\0') return_defer(pr);
    if (*++pr != '(') return_defer(strchr(pr, '\0'));
    char *link_start = ++pr;
    char *link_end = strchrnul(pr, ')');
    if (*link_end == '\0') return_defer(link_end);
    // TODO: link may not have any non-trailing spaces
    sb_appendf(sb, "<img src=\"%.*s\" alt=\"%.*s\">", (int) (link_end-link_start), link_start, (int) (text_end-text_start), text_start);
    return link_end;

defer:
    sb_append_buf(sb, text_start, result-text_start);
    return result;
}


char* skip_spaces(char *pr)
{
    while (isspace(*pr)){++pr;}
    return pr;
}

char* parse_code(char *pr, String_Builder *sb)
{
    char *code_start = ++pr;
    char *code_end = strchrnul(code_start, '`');
    if (*code_end == '\0') return code_start;
    sb_appendf(sb, "<code>%.*s</code>", (int) (code_end-code_start), code_start);
    return code_end+1;
}

void parse_text(char *line, String_Builder *sb)
{
    Format fmts[_Fmt_Count] = {0};
    char *pr = skip_spaces(line);
    char *s = pr;
    while (true){
        switch (*pr){
            case '\\':{
                sb_append_buf(sb, s, pr-s);
                sb_append_buf(sb, ++pr, 1);
                s = pr+1;
            }break;
            case '*':{
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
            }break;
            case '_':{
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
            }break;
            case '[': {
                sb_append_buf(sb, s, pr-s);
                pr = try_parse_link(pr, sb);
                s = pr+1;
            }break;
            case '!':{
                sb_append_buf(sb, s, pr-s);
                pr = try_parse_image(pr, sb);
                s = pr+1;
            }break;
            case '`':{
                sb_append_buf(sb, s, pr-s);
                pr = parse_code(pr, sb);
                s = pr;
                continue;
            }
            case '\0':{
                sb_append_buf(sb, s, pr-s);
                return;
            }
        }
        pr += 1;
    }
    
}

void parse_paragraph(char *pr, String_Builder *sb)
{
    sb_appendf(sb, "<p>");
    parse_text(pr, sb);
    sb_appendf(sb, "</p>");
}

size_t get_header_level(char *pr)
{
    size_t count = 0;
    while (*pr++ == '#'){
        count++;
    }
    return count;
}

char* get_start(char *line, char c)
{
    for (size_t i=0; i<4; ++i, ++line){
        if (*line == c) return line+1;
        if (!isspace(*line)) return NULL;
    }
    return NULL;
}

char* get_olist_start(char *line)
{
    for (size_t i=0; i<4; ++i, ++line){
        if (isdigit(*line) && *(line+1) == '.') return line+2;
        if (!isspace(*line)) return NULL;
    }
    return NULL;
}

bool is_tripple_char(char *line, char c)
{
    char *start = get_start(line, c);
    if (start == NULL) return false;
    for (size_t i=0; i<2; ++i){
        if (*(start+i) != c) return false;
    }
    return true;
}

void escape_line(char *pr, String_Builder *sb)
{
    while (true){
        char c = *pr++;
        switch(c){
            case '<': sb_append_cstr(sb, "&lt;"); break;
            case '>': sb_append_cstr(sb, "&gt;"); break;
            case '\0': {
                sb_append_cstr(sb, "\n");
                return;
            } break;
            default: sb_append_buf(sb, &c, 1);
        }
    }
}

char* render_markdown(char *input)
{
    char line_buffer[4096];
    char *line = input;
    
    String_Builder temp_sb = {0};
    String_Builder output_sb = {0};

    bool last_line_blank = false;
    bool is_list = false;
    bool is_olist = false;
    bool is_bq = false;
    bool is_code = false;
    
    while (true){
        line = get_next_line(line, line_buffer, sizeof(line_buffer));
        if (line == NULL) break;
        // TODO: implement more markdown features
        if (is_tripple_char(line_buffer, '-')){
            last_line_blank = false;
            is_list = false;
            is_olist = false;
            is_bq = false;
            sb_append_cstr(&output_sb, "<hr>\n");
            continue;
        }
        // TOOD: enable nested lists
        char *list_start = get_start(line_buffer, '-');
        if (list_start != NULL){
            last_line_blank = false;
            if (!is_list){
                sb_append_cstr(&output_sb, "<ul>\n");
                is_list = true;
            }
            parse_text(list_start, &temp_sb);
            sb_appendf(&output_sb, "    <li>%.*s</li>\n", (int) temp_sb.count, temp_sb.items);
            temp_sb.count = 0;
            continue;
        }
        char *olist_start = get_olist_start(line_buffer);
        if (olist_start != NULL){
            last_line_blank = false;
            if (!is_olist){
                sb_append_cstr(&output_sb, "<ol>\n");
                is_olist = true;
            }
            parse_text(olist_start, &temp_sb);
            sb_appendf(&output_sb, "    <li>%.*s</li>\n", (int) temp_sb.count, temp_sb.items);
            temp_sb.count = 0;
            continue;
        }
        char *bq_start = get_start(line_buffer, '>');
        if (bq_start != NULL){
            last_line_blank = false;
            if (!is_bq){
                sb_append_cstr(&output_sb, "<blockquote>\n");
                is_bq = true;
            }
            if (line_is_blank(bq_start+1)) continue;
            parse_paragraph(bq_start, &temp_sb);
            sb_appendf(&output_sb, "    %.*s\n", (int) temp_sb.count, temp_sb.items);
            temp_sb.count = 0;
            continue;
        }
        if (is_tripple_char(line_buffer, '`')){
            last_line_blank = false;
            if (!is_code){
                sb_append_cstr(&output_sb, "<pre>\n<code>");
                is_code = true;
            }else{
                sb_append_cstr(&output_sb, "</code>\n</pre>\n");
                is_code = false;
            }
            continue;
        }
        if (is_code){
            escape_line(line_buffer, &output_sb);
            continue;
        }
        if (is_list){
            sb_append_cstr(&output_sb, "</ul>\n");
            is_list = false;
        }
        if (is_olist){
            sb_append_cstr(&output_sb, "</ol>\n");
            is_olist = false;
        }
        if (is_bq){
            sb_append_cstr(&output_sb, "</blockquote>\n");
            is_bq = false;
        }
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
            parse_text(line_buffer+header_level, &temp_sb);
            sb_appendf(&output_sb, "<h%zu>%.*s</h%zu>\n", header_level, (int) temp_sb.count, temp_sb.items, header_level);
            temp_sb.count = 0;
        }
        else {
            parse_paragraph(line_buffer, &temp_sb);
            sb_appendf(&output_sb, "%.*s\n", (int) temp_sb.count, temp_sb.items);
            temp_sb.count = 0;
        }
    }
    sb_free(temp_sb);
    sb_append_null(&output_sb);
    return output_sb.items;
}


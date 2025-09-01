#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>

#include <htmd.h>
#include <cwalk.h>

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include <nob.h>

static char exe_dir[FILENAME_MAX];
static char temp_path[FILENAME_MAX];

bool get_exe_path(char *buffer, size_t buffer_size)
{
#ifdef _WIN32
    GetModuleFileNameA(NULL, buffer, buffer_size);
#elif __APPLE__
    if (_NSGetExecutablePath(buffer, &buffer_size) != 0) return false;
#elif __linux__
    ssize_t len = readlink("/proc/self/exe", buffer, buffer_size-1);
    if (len == -1) return false;
    buffer[len] = '\0';
#else
    fprintf(stderr, "[ERROR] %s:%d:%s: Unsupported platform!\n", __LINE__, __FILE__, __func__);
    return false;
#endif 
    cwk_path_normalize(buffer, buffer, buffer_size);
    return true;
}

bool get_parent_dir(const char *path, char *buffer, size_t buffer_size)
{
    size_t parent_len;
    cwk_path_get_dirname(path, &parent_len);
    if (parent_len == 0 || parent_len >= buffer_size) return false;
    memmove(buffer, path, parent_len);
    buffer[parent_len] = '\0';
    cwk_path_normalize(buffer, buffer, buffer_size);
    return true;
}

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

void print_usage(const char *program_name)
{
    printf("Usage: %s [OPTIONS] <input_file>\n\n", program_name);

    printf("Options:\n");
    printf("  -f                   : create a full html\n");
    printf("  -s                   : add styling to output (only together with -f)\n");
    printf("  --file <output_file> : write the output to a file\n");
    printf("  -h / --help          : print this help message\n\n");
}

int main(int argc, char *argv[])
{
    if (!get_exe_path(exe_dir, sizeof(exe_dir))){
        eprintfn("Could not resolve executable path");
        return 1;
    }
    get_parent_dir(exe_dir, exe_dir, sizeof(exe_dir));
    const char *program_name = shift_args(&argc, &argv);
    
    bool full_html = false;
    bool do_styling = false;
    const char *output_file = NULL;
    const char *input_file = NULL;

    while (argc > 0){
        const char *arg = shift_args(&argc, &argv);
        if (strcmp(arg, "-f") == 0){
            full_html = true;
        }else if (strcmp(arg, "-s") == 0){
            do_styling = true;
        }else if (strcmp(arg, "--file") == 0){
            output_file = shift_args(&argc, &argv);
        }else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0){
            print_usage(program_name);
            return 0;
        }else{
            if (input_file == NULL){
                input_file = arg;
            }else{
                eprintfn("Invalid argument '%s'!", arg);
                print_usage(program_name);
                return 1;
            }
        }
    }
    if (input_file == NULL){
        eprintfn("No input file provided!\n");
        print_usage(program_name);
        return 1;
    }
    int result = 0;

    String_Builder sb = {0};
    
    char *content = read_file(input_file);
    if (full_html){
        sb_append_cstr(&sb, "<!DOCTYPE html>\n<html>\n<head>\n");
        if (do_styling){
            cwk_path_join(exe_dir, "src/head.html", temp_path, sizeof(temp_path));
            read_entire_file(temp_path, &sb);
            sb_append_cstr(&sb, "<style>\n");

            cwk_path_join(exe_dir, "src/style.css", temp_path, sizeof(temp_path));
            read_entire_file(temp_path, &sb);
            sb_append_cstr(&sb, "</style>\n");
        }
        sb_append_cstr(&sb, "</head>\n<body>\n");
    }
    char *output = render_markdown(content);
    if (output != NULL){
        sb_append_cstr(&sb, output);
    }
    if (full_html) sb_append_cstr(&sb, "</body>\n</html>");
    if (output_file != NULL){
        FILE *file = fopen(output_file, "w");
        if (file == NULL){
            eprintfn("Could not open output file '%s': %s", output_file, strerror(errno));
            return_defer(1);
        }
        fwrite(sb.items, 1, sb.count, file);
        fclose(file);
    }else if (sb.items != NULL) {
         puts(sb.items);
    }
  defer:
    free(content);
    sb_free(sb);
    return result;
}

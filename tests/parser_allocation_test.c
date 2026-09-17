/* White-box boundary probe: reject the huge allocation in the allocator hook,
 * so the file-buffer growth limit is verified without reserving gigabytes. */
#include "../vendor/tomlc17/tomlc17.c"

static size_t requested_size;

static void *bounded_realloc(void *pointer, size_t size)
{
    requested_size = size;
    return size > 2048 ? NULL : realloc(pointer, size);
}

int main(void)
{
    toml_option_t options = toml_default_option();
    options.mem_realloc = bounded_realloc;
    toml_set_option(options);
    char *cell = cell_realloc(NULL, 1);
    if (!cell) return 1;
    cell[0] = 'x';
    char *grown = cell_realloc(cell, INT_MAX);
    int ok = !grown && requested_size >= (size_t)INT_MAX && cell[0] == 'x';
    cell_free(grown ? grown : cell);
    toml_set_option(toml_default_option());
    if (!ok) {
        fprintf(stderr, "parser cell growth corrupted size or failed allocation ownership\n");
        return 1;
    }
    puts("parser_allocation_test: ok (INT_MAX growth rejected without overflow)");
    return 0;
}

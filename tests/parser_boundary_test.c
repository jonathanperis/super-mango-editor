#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tomlc17.h"

static size_t live_allocations;
static size_t allocation_calls;
static size_t fail_allocation = SIZE_MAX;

static void counted_free(void *pointer)
{
    if (pointer) live_allocations--;
    free(pointer);
}

static void *counted_realloc(void *pointer, size_t size)
{
    if (!size) { counted_free(pointer); return NULL; }
    if (allocation_calls++ == fail_allocation) return NULL;
    int fresh = pointer == NULL;
    void *result = realloc(pointer, size);
    if (result && fresh) live_allocations++;
    return result;
}

static int parser_cases(void)
{
    static const struct { const char *text; int valid; } cases[] = {
        {"a={\"\"=1,b=2}",1}, {"a={b=1,b=2}",0},
        {"a=2024-02-29",1}, {"a=2024-2-29",0}, {"a=2024-02-30",0},
        {"a=12:34:56",1}, {"a=30:34:56",0},
        {"a=2026-09-10T12:34:56Z",1}, {"a=2026-09-10T12:34:56+03:00",1},
        {"a=2026-9999999999999999999-01",0}, {"a=12:999999999999999:00",0},
        {"a=\"\\u",0}, {"a={\"\"=1,\"\"=2}",0}
    };
    for (size_t i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        toml_result_t parsed=toml_parse(cases[i].text,(int)strlen(cases[i].text));
        int ok=parsed.ok==cases[i].valid;
        toml_free(parsed);
        if (!ok) { fprintf(stderr,"parser boundary %zu failed\n",i); return 1; }
    }
    toml_result_t a=toml_parse("x={a=1}",7), b=toml_parse("x={a=2,b=3}",11);
    toml_result_t merged=toml_merge(&a,&b);
    int ok=merged.ok && toml_seek(merged.toptab,"x.a").u.int64==2 && toml_seek(merged.toptab,"x.b").u.int64==3;
    toml_free(merged); toml_free(a); toml_free(b);
    if (!ok) return 1;
    for (int depth=30;depth<=31;depth++) {
        char nested[96]; int used=0;
        nested[used++]='a'; nested[used++]='=';
        for (int i=0;i<depth;i++) nested[used++]='[';
        nested[used++]='0';
        for (int i=0;i<depth;i++) nested[used++]=']';
        nested[used]='\0';
        toml_result_t parsed=toml_parse(nested,used);
        ok=parsed.ok==(depth==30);
        toml_free(parsed);
        if (!ok) return 1;
    }

    /* Bounded deterministic mutation probes also run under ASan/UBSan. This
     * is not a replacement for coverage-guided fuzzing, but needs no runtime. */
    uint32_t random=0x12345678u;
    for (int run=0;run<10000;run++) {
        char text[128];
        const char *seed=cases[run%(sizeof(cases)/sizeof(cases[0]))].text;
        strcpy(text,seed);
        size_t length=strlen(text);
        for (int edit=0;edit<3;edit++) {
            random^=random<<13; random^=random>>17; random^=random<<5;
            text[random%(length+1)]=(char)(random>>24);
        }
        text[length+1]='\0';
        toml_result_t parsed=toml_parse(text,(int)strlen(text));
        toml_free(parsed);
    }
    return 0;
}

static int allocation_failure_case(int merge, size_t fail, size_t *calls)
{
    const char *left = "section={before=1,items=[{id=1}],name=\"café\"}";
    const char *right = "section={after=2,items=[{id=2}]}";
    toml_result_t a = {0}, b = {0};
    fail_allocation = SIZE_MAX;
    if (merge) {
        a = toml_parse(left, (int)strlen(left));
        b = toml_parse(right, (int)strlen(right));
        if (!a.ok || !b.ok) { toml_free(a); toml_free(b); return 1; }
    }
    allocation_calls = 0;
    fail_allocation = fail;
    toml_result_t result = merge ? toml_merge(&a, &b) : toml_parse(left, (int)strlen(left));
    *calls = allocation_calls;
    fail_allocation = SIZE_MAX;
    int bad = fail == SIZE_MAX && !result.ok;
    if (result.ok) {
        toml_datum_t before = toml_seek(result.toptab, "section.before");
        toml_datum_t items = toml_seek(result.toptab, "section.items");
        toml_datum_t name = toml_seek(result.toptab, "section.name");
        bad |= before.type != TOML_INT64 || before.u.int64 != 1 || items.type != TOML_ARRAY ||
               items.u.arr.size != (merge ? 2 : 1) || name.type != TOML_STRING || strcmp(name.u.str.ptr, "café");
        if (merge) {
            toml_datum_t after = toml_seek(result.toptab, "section.after");
            bad |= after.type != TOML_INT64 || after.u.int64 != 2;
        }
    }
    toml_free(result);
    toml_free(a);
    toml_free(b);
    if (bad || live_allocations) {
        fprintf(stderr, "parser allocation case merge=%d fail=%zu: bad=%d live=%zu\n",
                merge, fail, bad, live_allocations);
        return 1;
    }
    return 0;
}

static int allocation_failure_sweep(void)
{
    size_t total = 0;
    for (int merge = 0; merge < 2; merge++) {
        size_t calls;
        if (allocation_failure_case(merge, SIZE_MAX, &calls)) return 1;
        for (size_t fail = 0; fail < calls; fail++) {
            size_t attempted;
            if (allocation_failure_case(merge, fail, &attempted)) return 1;
            total++;
        }
    }
    printf("parser allocation sweep: %zu failure points checked\n", total);
    return 0;
}

int parser_boundary_test(void)
{
    toml_option_t options = toml_default_option();
    options.mem_realloc = counted_realloc;
    options.mem_free = counted_free;
    live_allocations = 0;
    toml_set_option(options);
    int result = parser_cases();
    if (!result) result = allocation_failure_sweep();
    toml_set_option(toml_default_option());
    if (live_allocations) {
        fprintf(stderr, "parser leaked %zu allocations\n", live_allocations);
        return 1;
    }
    if (!result) puts("parser_boundary_test: ok (10000 bounded mutations; allocations balanced)");
    return result;
}

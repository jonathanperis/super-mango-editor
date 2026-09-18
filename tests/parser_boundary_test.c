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

static int bounded_input_cases(void)
{
    static const struct { const char *text; int valid; } cases[] = {
        {"",1}, {"a=1",1}, {"a=2024-",0}, {"a=2024-02-29",1},
        {"a=\"\\u",0}, {"a={x=[{v=1}],x={leak=[2]}}",0}, {"a=[{x=[1,2]},",0},
        {"\xef",0}, {"\xef\xbb",0}, {"\xef\xbb\xbf",1},
        {"\xef\xbb\xbf" "a=1",1}, {"\xef\xbb\xbf\xef\xbb\xbf" "a=1",0},
        {"a=1\n\xef\xbb\xbf" "b=2",0}, {"a=\"\xef\xbb\xbf\"",1},
        {"a=12:34:56.123456789",1}, {"café=1",1}, {"a=1\rb=2",0}
    };
    for (size_t i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        size_t length=strlen(cases[i].text);
        /* Exactly-sized allocations catch reads beyond the new bounded-input
         * API under ASan; none of these inputs requires a NUL terminator. */
        char *text=malloc(length ? length : 1);
        if (!text) return 1;
        memcpy(text,cases[i].text,length);
        toml_result_t parsed=toml_parse(text,(int)length);
        free(text);
        int ok=parsed.ok==cases[i].valid && (parsed.ok || parsed.errmsg[0]);
        toml_free(parsed);
        if (!ok || live_allocations) {
            fprintf(stderr,"parser bounded input %zu failed (live=%zu)\n",i,live_allocations);
            return 1;
        }
    }
    return 0;
}

static int equivalence_and_merge_cases(void)
{
    static const struct { const char *left, *right; int equivalent; } cases[] = {
        {"\xef\xbb\xbf" "a=1\nb={x=2,y=3}", "b={y=3,x=2}\na=1", 1},
        {"a=[1,2]", "a=[2,1]", 0},
        {"a={x=1,y=2}", "a={y=2,x=3}", 0},
        {"a={x=1}", "a={y=1}", 0},
        {"a={x=1}", "a={x=1,y=2}", 0}
    };
    for (size_t i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        toml_result_t a=toml_parse(cases[i].left,(int)strlen(cases[i].left));
        toml_result_t b=toml_parse(cases[i].right,(int)strlen(cases[i].right));
        int ok=a.ok && b.ok && toml_equiv(&a,&b)==cases[i].equivalent &&
               toml_equiv(&b,&a)==cases[i].equivalent;
        toml_free(a); toml_free(b);
        if (!ok) { fprintf(stderr,"parser equivalence case %zu failed\n",i); return 1; }
    }
    const char *arrays[]={"items=[1,2]", "[[items]]\nid=3"};
    for (int direction=0;direction<2;direction++) {
        const char *left=arrays[direction], *right=arrays[1-direction];
        toml_result_t a=toml_parse(left,(int)strlen(left));
        toml_result_t b=toml_parse(right,(int)strlen(right));
        toml_result_t merged=toml_merge(&a,&b);
        int ok=a.ok && b.ok && merged.ok && toml_equiv(&merged,&b);
        toml_free(a); toml_free(b); toml_free(merged);
        if (!ok) { fprintf(stderr,"parser array merge direction %d failed\n",direction); return 1; }
    }
    return 0;
}

static int allocation_failure_case(const char *left, int merge, size_t fail, size_t *calls)
{
    const char *right = "section={after=2,items=[{id=2}]}";
    toml_result_t a = {0}, b = {0};
    fail_allocation = SIZE_MAX;
    if (merge) {
        a = toml_parse_named(left, (int)strlen(left), "left.toml");
        b = toml_parse_named(right, (int)strlen(right), "right.toml");
        if (!a.ok || !b.ok) { toml_free(a); toml_free(b); return 1; }
    }
    allocation_calls = 0;
    fail_allocation = fail;
    toml_result_t result = merge ? toml_merge(&a, &b) :
                          toml_parse_named(left, (int)strlen(left), "left.toml");
    *calls = allocation_calls;
    fail_allocation = SIZE_MAX;
    /* Merged keys, strings and source names must outlive both input pools. */
    toml_free(a);
    toml_free(b);
    int bad = fail == SIZE_MAX ? !result.ok : result.ok || !result.errmsg[0];
    if (result.ok) {
        toml_datum_t before = toml_seek(result.toptab, "section.before");
        toml_datum_t items = toml_seek(result.toptab, "section.items");
        toml_datum_t name = toml_seek(result.toptab, "section.name");
        toml_datum_t padding = toml_seek(result.toptab, "section.padding");
        toml_datum_t large = toml_seek(result.toptab, "section.large");
        bad |= before.type != TOML_INT64 || before.u.int64 != 1 || items.type != TOML_ARRAY ||
               items.u.arr.size != (merge ? 2 : 1) || name.type != TOML_STRING || strcmp(name.u.str.ptr, "café");
        bad |= !before.source || strcmp(before.source, "left.toml") || before.lineno != 1 ||
               !(items.flag & TOML_FLAG_INLINED) || padding.type != TOML_ARRAY || padding.u.arr.size != 12 ||
               large.type != TOML_STRING || large.u.str.len != 2048;
        if (padding.type == TOML_ARRAY) {
            for (int i=0;i<padding.u.arr.size;i++) {
                toml_datum_t item=padding.u.arr.elem[i];
                bad |= item.type != TOML_STRING || item.u.str.len != 600;
                if (item.type == TOML_STRING && item.u.str.len == 600)
                    for (int j=0;j<600;j++) bad |= item.u.str.ptr[j] != 'p';
            }
        }
        if (large.type == TOML_STRING && large.u.str.len == 2048)
            for (int i=0;i<2048;i++) bad |= large.u.str.ptr[i] != 'L';
        if (merge) {
            toml_datum_t after = toml_seek(result.toptab, "section.after");
            bad |= after.type != TOML_INT64 || after.u.int64 != 2 ||
                   !after.source || strcmp(after.source, "right.toml");
        }
    }
    toml_free(result);
    if (bad || live_allocations) {
        fprintf(stderr, "parser allocation case merge=%d fail=%zu: bad=%d live=%zu\n",
                merge, fail, bad, live_allocations);
        return 1;
    }
    return 0;
}

static int allocation_failure_sweep(void)
{
    /* Exercise both pooled small-page growth and dedicated large pages, plus
     * array/table cell growth and source-name ownership during merge. */
    char left[10000]="section={before=1,items=[{id=1}],name=\"café\",padding=[";
    size_t used=strlen(left);
    for (int i=0;i<12;i++) {
        left[used++]='"';
        memset(left+used,'p',600); used+=600;
        left[used++]='"'; left[used++]=',';
    }
    const char *suffix="],large=\"";
    memcpy(left+used,suffix,strlen(suffix)); used+=strlen(suffix);
    memset(left+used,'L',2048); used+=2048;
    memcpy(left+used,"\"}",3);
    size_t total = 0;
    for (int merge = 0; merge < 2; merge++) {
        size_t calls;
        if (allocation_failure_case(left, merge, SIZE_MAX, &calls)) return 1;
        for (size_t fail = 0; fail < calls; fail++) {
            size_t attempted;
            if (allocation_failure_case(left, merge, fail, &attempted)) return 1;
            total++;
        }
    }
    printf("parser allocation sweep: %zu failure points checked\n", total);
    return 0;
}

static int experiment_array_limit(void)
{
    /* EXPERIMENT_MAX_FRAMES in src/core/game_experiment.h; keep this parser
     * probe independent of the graphics headers pulled in by the runtime type. */
    const int max_frames=36000;
    size_t capacity=(size_t)(max_frames+1)*2+16;
    char *text=malloc(capacity);
    if (!text) return 1;
    for (int extra=0;extra<=1;extra++) {
        int count=max_frames+extra;
        memcpy(text,"frames=[",8);
        size_t used=8;
        for (int i=0;i<count;i++) {
            text[used++]='0'; text[used++]=',';
        }
        text[used++]=']'; text[used]='\0';
        toml_result_t result=toml_parse(text,(int)used);
        int ok=extra ? !result.ok && strstr(result.errmsg,"array too large") != NULL :
                      result.ok && toml_get(result.toptab,"frames").u.arr.size==count;
        toml_free(result);
        if (!ok || live_allocations) {
            fprintf(stderr,"parser experiment array limit failed at %d entries\n",count);
            free(text);
            return 1;
        }
    }
    free(text);
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
    if (!result) result = bounded_input_cases();
    if (!result) result = equivalence_and_merge_cases();
    if (!result) result = allocation_failure_sweep();
    if (!result) result = experiment_array_limit();
    toml_set_option(toml_default_option());
    if (live_allocations) {
        fprintf(stderr, "parser leaked %zu allocations\n", live_allocations);
        return 1;
    }
    if (!result) puts("parser_boundary_test: ok (10000 bounded mutations; allocations balanced)");
    return result;
}

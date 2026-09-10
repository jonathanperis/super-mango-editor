#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "tomlc17.h"

int parser_boundary_test(void)
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
    puts("parser_boundary_test: ok (10000 bounded mutations)");
    return 0;
}

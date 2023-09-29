#include <cstdlib>
#include <stdio.h>
#include <cctype>

int main() {
    unsigned int b = 0;
    unsigned int w = 0;
    unsigned int l = 0;
    char c;
    while((c = fgetc(stdin)) != EOF){
        b++;
        if (c == '\n') {
            l++;
        };
        if (isspace(c)) {
            w++;
        };
    };
    fprintf(stdout, "%d %d %d\n", l,w,b);
}
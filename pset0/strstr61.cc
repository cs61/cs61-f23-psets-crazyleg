#include <cstring>
#include <cassert>
#include <cstdio>

// char* mystrstr(const char* s1, const char* s2) {
//     // loop over `s1`
//     while (*s1) {
//         // loop over `s2` until `s2` ends or differs from `s1`
//         const char* s1try = s1;
//         const char* s2try = s2;
//         while (*s2try && *s2try == *s1try) {
//             ++s2try;
//             ++s1try;
//         }
//         // success if we got to the end of `s2`
//         if (!*s2try) {
//             return (char*) s1;
//         }
//         s1++;
//     }
//     // special case
//     if (!*s2) {
//         return (char*) s1;
//     }
//     return nullptr;
// }

char* mystrstr(const char* s1, const char* s2) {
    // Your code here
     // special case
    if (!s2[0]) {
        return (char*) s1;
    };
    unsigned int i = 0;
    unsigned int j = 0;
    while (s1[i]){
        // printf("$%c$%c$\n", s1[i], s2[j]);
        if (s1[i] == s2[j]){
            j++;
        }
        else{
            j = 0;
        };
        if (!s2[j]) {
            return (char*) &s1[i-j+1];
        }
        i++;
    };
    return nullptr;
}

int main(int argc, char* argv[]) {
    assert(argc == 3);
    printf("strstr(\"%s\", \"%s\") = %p\n",
           argv[1], argv[2], strstr(argv[1], argv[2]));
    printf("mystrstr(\"%s\", \"%s\") = %p\n",
           argv[1], argv[2], mystrstr(argv[1], argv[2]));
    assert(strstr(argv[1], argv[2]) == mystrstr(argv[1], argv[2]));
}
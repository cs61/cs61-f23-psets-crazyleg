#include <stdio.h>
#include <string.h>

int main(int argc, char* argv[]) {
    int i = 1;
    int j = 1;

   while (i < argc) {
      j = i;
      while (j > 1 && strcmp(argv[j-1], argv[j]) > 0) {
         char* temp = argv[j-1];
         argv[j-1] = argv[j];
         argv[j] = temp;
         j--;
      }
      i++;
   }

   i = 1;
    while (i < argc) {
        printf("%s\n", argv[i]);
        i++;
    }
}
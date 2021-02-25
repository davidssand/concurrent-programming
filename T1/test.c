#include <stdio.h>
#include <string.h>

void slice_str(const char * str, char * buffer, size_t start, size_t end)
{
  size_t j = 0;
  for ( size_t i = start; i <= end; ++i ) {
    buffer[j++] = str[i];
  }
  buffer[j] = 0;
}

int main(void) {
  const char str[] = "Polly";
  const size_t len = strlen(str);
  char buffer[len + 1];

  slice_str(str, buffer, 0, len);
  printf("%s\n", buffer);

  printf("%ld\n", sizeof(str)/sizeof(str[0]));
  printf("%ld\n", sizeof(buffer)/sizeof(buffer[0]));

  return 0;
}
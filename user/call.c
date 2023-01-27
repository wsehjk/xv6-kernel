#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int g(int x) {
  return x+3;
}

int f(int x) {
  return g(x);
}

void main(int argc, char* argv[]) {
  int a = 0;
  if (argc != 2)  {
    a = 8;
  }
  else {
    a = atoi(argv[1]);
  }
  unsigned int i = 0x00646c72;
	printf("H%x Wo%s\n", 57616, &i);
  printf("%d %d\n", f(a)+1, 13);
  printf("x=%d y=%d", 3);
  exit(0);
}

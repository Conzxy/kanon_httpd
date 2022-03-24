#include <assert.h>

#include "unix/process.h"

using namespace unix;

int main()
{
  Process process{};
  assert(process.Fork(
    []()
    {
      printf("parent process\n");
    },
    []()
    {
      printf("child process\n");
    }));
}
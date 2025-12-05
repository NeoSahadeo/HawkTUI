#include <unistd.h>
#include "include/hawktui.hpp"

int main() {
  UIContext* ctx = new UIContext();
  sleep(1);
  delete ctx;
  return 0;
}

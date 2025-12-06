#include <unistd.h>
#include <memory>
#include "include/hawktui.hpp"

int main() {
  UIContext* ctx = new UIContext();
  // auto box = std::make_shared<UIBox>(10, 5, 0, 0, true);
  // ctx->add_child(box);
  auto textibox =
      std::make_shared<UITextiBox>(15, 5, 0, 0, "Hello, world!", 1, 2);
  textibox->flags |= TypeFlags::Draggable;
  ctx->add_child(textibox);
  ctx->start();
  delete ctx;
  return 0;
}

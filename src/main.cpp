#include <unistd.h>
#include <memory>
#include <sstream>
#include <string>
#include "include/hawktui.hpp"
#define NDEBUG

int main() {
  UIContext* ctx = new UIContext();

  auto g_mouse_callback = [](Event::MouseData d) { d.ctx->stop(); };
  auto button = UIButton::create(&ctx->mouse_event, "Quit",
                                 ctx->get_width() - 6, 0, g_mouse_callback);

  ctx->screen_event.add(Event::Type::Resize, [&](Event::ScreenData d) {
    auto p = button->composition[1];
    auto text = std::static_pointer_cast<UIText>(button->composition[1]);
    text->set_pos(ctx->get_width() - 6, 0);
  });

  // auto origin = Coords{0, 0};
  // auto p2 = Coords{ctx->get_width(), ctx->get_height()};

  // auto b = UILine::create(origin, p2);

  // ctx->mouse_event.add(Event::Type::Mousemove, [&](Event::MouseData d) {
  // b->set_pos(origin, Coords{d.x, d.y});
  // });

  // ctx->mouse_event.add(Event::Type::Click, [&](Event::MouseData d) {
  //   auto p = Coords{d.x, d.y};
  //   origin = p;
  //   b->set_pos(origin, p);
  // });

  ctx->add_child(button);

  // auto box = UIBox::create();
  // auto text = UIText::create(20, 10, "Quit");
  // ctx->add_child(box);
  // ctx->add_child(text);

  for (int x{}; x < 1; x++) {
    std::string str{"node" + std::to_string(x)};
    auto node =
        UINode::create(&ctx->mouse_event, 0, x * 4, str, g_mouse_callback);
    // auto button =
    //     UIButton::create(&ctx->mouse_event, "eXit", 8, 8, g_mouse_callback);
    // node->callback = g_mouse_callback;

    // node->composition.emplace_back(button);
    ctx->add_child(node);
  }

  ctx->observer().sub(Event::Type::Mousemove, ctx->mouse_event);
  ctx->observer().sub(Event::Type::Mousedown, ctx->mouse_event);
  ctx->observer().sub(Event::Type::Mouseup, ctx->mouse_event);
  ctx->observer().sub(Event::Type::Click, ctx->mouse_event);
  ctx->observer().sub(Event::Type::Resize, ctx->screen_event);

  ctx->start();
  delete ctx;
  return 0;
}

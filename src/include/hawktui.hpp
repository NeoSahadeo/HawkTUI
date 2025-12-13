#include <ncurses.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#ifndef HAWKTUI_H
#define HAWKTUI_H

enum class TypeId { None, Box, Text, TextiBox, Button, Label };

enum class TypeFlags : uint8_t {
  None = 0,
  Draggable = 1,
};

constexpr TypeFlags operator|=(TypeFlags& x, TypeFlags y) noexcept {
  x = static_cast<TypeFlags>(static_cast<uint8_t>(x) | static_cast<uint8_t>(y));
  return x;
}

constexpr bool operator&(TypeFlags x, TypeFlags y) noexcept {
  return static_cast<bool>(static_cast<uint8_t>(x) & static_cast<uint8_t>(y));
}

template <typename Derived, typename Base>
std::unique_ptr<Derived> static_unique_ptr_cast(
    std::unique_ptr<Base>&& ptr) noexcept {
  return std::unique_ptr<Derived>(static_cast<Derived*>(ptr.release()));
}

class EventManager {
 public:
  struct Event {};

  std::unordered_map<std::string, std::vector<std::function<void(Event&)>>>
      handlers;

  /**
   * Subscribe a callback to an event
   * */
  template <typename EventT, typename F>
  void subscribe(const std::string& event, F&& callback) {
    handlers[event].emplace_back(
        [cb = std::forward<F>(callback)](const auto& e) mutable {
          cb(static_cast<const EventT&>(e));
        });
  }

  /**
   * Dispatch an event with a lvalue
   * */
  void dispatch(const std::string& event, Event& data) {
    if (auto callbacks = handlers.find(event); callbacks != handlers.end()) {
      for (auto& f : callbacks->second) {
        f(data);
      }
    }
  }

  /**
   * Dispatch an event with an rvalue
   * */
  void dispatch(const std::string& event, Event&& data = Event{}) {
    if (auto callbacks = handlers.find(event); callbacks != handlers.end()) {
      for (auto& f : callbacks->second) {
        f(data);
      }
    }
  }
};

class _AbstractUIElement {
 public:
  ~_AbstractUIElement() = default;
  _AbstractUIElement() = default;

  std::vector<std::shared_ptr<_AbstractUIElement>> composition{};
  TypeFlags flags{TypeFlags::None};
  WINDOW* window{};

  virtual void render() = 0;
  virtual TypeId type() = 0;
};

template <TypeId T>
class IUIElement : public _AbstractUIElement {
 public:
  TypeId type() { return T; };
};

/**
 * Creates a UIBox element - empty box with a border
 * */
class UIBox : public IUIElement<TypeId::Box> {
 public:
  size_t width;
  size_t height;
  int x;
  int y;

  UIBox() {
    width = 10;
    height = 5;
    x = 0;
    y = 0;
    window = newwin(height, width, this->y, this->x);
  }

  void render() override {
    box(window, 0, 0);
    touchwin(window);
    wnoutrefresh(window);
  }
};

/**
 * Creates a text element
 * */
class UIText : public IUIElement<TypeId::Text> {
 public:
  std::string label;
  size_t width;
  size_t height;
  int x;
  int y;

  UIText(std::string label) : label(label) {
    width = 10;
    height = 5;
    x = 0;
    y = 0;
    window = newwin(height, width, this->y, this->x);
  }

  void render() override {
    mvwprintw(window, 0, 0, "%s", label.c_str());
    touchwin(window);
    wnoutrefresh(window);
  }
};

/**
 * Create a button
 * */
class UIButton : public IUIElement<TypeId::Button> {
 public:
  UIButton() {
    composition.emplace_back(std::make_shared<UIBox>());
    composition.emplace_back(std::make_shared<UIText>("This is a button"));
  }
  void render() {};
};

/**
 * UI Context class that sets up the main window and pointer events
 * */
class UIContext {
 public:
  WINDOW* window;
  mmask_t oldmask;
  int screen_width;
  int screen_height;
  EventManager events;
  bool running;
  std::vector<std::shared_ptr<_AbstractUIElement>> children;

  struct MouseEvent : EventManager::Event {
    int x;
    int y;
    std::shared_ptr<_AbstractUIElement> element;
  } m_e;

  UIContext() {
    window = initscr();
    // start_color();
    getmaxyx(window, screen_height, screen_width);
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    mmask_t mask, oldmask;
    mask = ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION;
    curs_set(0);
    mouseinterval(0);
    mousemask(mask, &oldmask);
    oldmask = oldmask;
    printf("\033[?1003h\n");
    running = true;
  }

  ~UIContext() {
    printf("\033[?1003l\n");
    curs_set(1);
    mousemask(oldmask, NULL);
    delwin(window);
    endwin();
    children.clear();
  }

  void start() {
    render(children);
    int c;
    MEVENT event;
    struct Coords {
      int x, y;
    };
    Coords click_offset{0};

    std::shared_ptr<_AbstractUIElement> current_element;
    c = wgetch(window);
    while (running) {
      c = wgetch(window);
      if (c == 'q') {
        running = false;
        break;
      }
      touchwin(stdscr);
      wnoutrefresh(window);
      if (c == KEY_RESIZE) {
        getmaxyx(window, screen_height, screen_width);
        events.dispatch("resize");
      }

      if (c == KEY_MOUSE) {
        while (getmouse(&event) == OK) {
          m_e.x = event.x;
          m_e.y = event.y;
          events.dispatch("mousemove", m_e);
          // if (current_element &&
          //     current_element->flags & TypeFlags::Draggable) {
          //   switch (current_element->type()) {
          //     case TypeId::Box: {
          //       auto box = std::static_pointer_cast<UIBox>(
          //           current_element);  // issue :(
          //       mvwin(box->window, event.y - click_offset.y,
          //             event.x - click_offset.x);
          //       break;
          //     }
          //     default:
          //       break;
          //   }
          // }
          //
          if (event.bstate & BUTTON1_PRESSED) {
            events.dispatch("mousedown", m_e);
            for (auto& child : children) {
              switch (child->type()) {
                case TypeId::TextiBox:
                case TypeId::Button:
                case TypeId::Box: {
                  auto box = std::static_pointer_cast<UIBox>(child);
                  int start_y, start_x;
                  getbegyx(child->window, start_y, start_x);
                  if (event.x >= start_x && event.x <= start_x + box->width &&
                      event.y >= start_y && event.y <= start_y + box->height) {
                    click_offset.x = event.x - start_x;
                    click_offset.y = event.y - start_y;
                    current_element = child;
                    m_e.element = child;
                  }
                  break;
                }
                default:
                  break;
              };
            }
          } else if (event.bstate & BUTTON1_RELEASED) {
            events.dispatch("mouseup", m_e);
            events.dispatch("click", m_e);
            current_element.reset();
          }
        }
      }
      render(children);
    }
  }

  void add_child(std::shared_ptr<_AbstractUIElement> child) {
    children.emplace_back(child);
  }

  void render(std::vector<std::shared_ptr<_AbstractUIElement>> __children) {
    wnoutrefresh(window);
    for (auto& child : __children) {
      if (child->composition.size() > 0) {
        render(child->composition);
      }
      child->render();
    }
    doupdate();
  }
};

// /**
//  * Create a UIButton that auto binds a 'click' event to itself.
//  * */
// class UIButton : public UIText, public UIBox {
//  public:
//   UIButton() = default;
//
//   template <typename F>
//   UIButton(int x, int y, EventManager* events, std::string text, F&&
//   callback)
//       : UIText(text, x, y) {
//     events->subscribe<UIContext::MouseEvent>(
//         "click", [&](UIContext::MouseEvent e) {
//           if (e.element && e.element->window == UIBox::window) {
//             callback(e);
//           }
//         });
//   }
//
//   static std::shared_ptr<UIButton> create(
//       EventManager* events,
//       std::string text,
//       int x,
//       int y,
//       std::function<void(UIContext::MouseEvent)> callback = {}) {
//     return std::make_shared<UIButton>(x, y, events, text, callback);
//   }
//
//   void render() override {
//     // mvwprintw(window, 0, 0, "%s", text.c_str());
//     // touchwin(window);
//     // wnoutrefresh(window);
//     // UIText::render();
//     // UIBox::render();
//   }
//
//   TypeId type() const override { return TypeId::Button; };
// };

#endif

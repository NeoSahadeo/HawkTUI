#include <ncurses.h>
#include <unistd.h>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "log.hpp"
#ifndef HAWKTUI_H
#define HAWKTUI_H

#define M_PI_2x 6.283185307
#define M_PI_3rd 4.71238898

typedef struct Coords {
  int x, y;
} Coords;

class UIContext;
class AbstractUIElement;

enum class TypeId { None, Box, Text, Button, Line, Curve, Node };

enum class TypeFlags : uint8_t {
  None,
  Draggable,
  Editable,
};

namespace Event {
enum class Type {
  Click,
  Mousemove,
  Mouseup,
  Mousedown,
  Keydown,
  Keyup,
  Keypress,
  Resize,
};

class EventListener {
 public:
  uintptr_t id;
  EventListener();
  ~EventListener() = default;
  virtual void update(Type event) = 0;
};

EventListener::EventListener() {
  id = reinterpret_cast<uintptr_t>(this);
}

class Observer {
 private:
  std::unordered_map<Type, std::vector<EventListener*>> _handlers{};

 public:
  void sub(Type event, EventListener& listener);
  void unsub(Type event, EventListener& listener);
  void notify(Type event);
};

void Observer::sub(Type event, EventListener& listener) {
  _handlers[event].emplace_back(&listener);
}

void Observer::notify(Type event) {
  for (auto&& vec : _handlers[event]) {
    vec->update(event);
  }
}

void Observer::unsub(Type event, EventListener& listener) {
  if (_handlers[event].empty())
    return;

  std::erase_if(_handlers[event],
                [&](EventListener* e) { return e->id == listener.id; });
}

class MouseEvent : public EventListener {
 public:
  struct Data {
    int x{0};
    int y{0};
    int offset_x{0};
    int offset_y{0};
    std::shared_ptr<AbstractUIElement> element;
    UIContext* ctx;
  };
  Data data{};

 private:
  struct Meta {
    std::function<void(Data)> func;
    std::uintptr_t id;
    Type type;
  };
  std::vector<Meta> _calls;

 public:
  template <typename F>
  auto add(Type event_type, F&& callback);

  template <typename F>
  void remove(F&& callback);

  void update(Type event) override;
};

template <typename F>
auto MouseEvent::add(Event::Type event_type, F&& callback) {
  auto ptr = reinterpret_cast<std::uintptr_t>(static_cast<void*>(&callback));
  _calls.emplace_back(Meta{std::forward<F>(callback), ptr, event_type});
  return ptr;
}

template <typename F>
void MouseEvent::remove(F&& callback) {
  auto ptr = reinterpret_cast<std::uintptr_t>(static_cast<void*>(&callback));
  std::erase_if(_calls, [&ptr](Meta m) { return m.id == ptr; });
}

void MouseEvent::update(Type event) {
  for (auto&& x : _calls) {
    if (x.type == event) {
      x.func(data);
    }
  }
}

};  // namespace Event

/** @brief Abstract base for all UI elements with nested composition support.
 *
 * Subclasses must implement render() and type().
 * Supports hierarchical UI trees
 * via composition member.
 * Defaults:
 *  empty composition
 *  TypeFlags::None
 *  null window
 * */
class AbstractUIElement {
 public:
  AbstractUIElement() = default;
  virtual ~AbstractUIElement() = default;

  std::vector<std::shared_ptr<AbstractUIElement>> composition{};
  TypeFlags flags{TypeFlags::None};
  WINDOW* window{nullptr};

  /** @brief Calls ncurses functions to draw UI element to its parent
   * ScreenContext window.
   * @note Automatically invoked by UIContext::render() during batch rendering.
   */
  virtual void render() = 0;

  /**@brief Returns this UI element's TypeId.
   * @warning TypeId must be registered with TypeId::register() before use.
   * @note Automatically invoked by UIContext::handle_click() during mouse
   * events.
   * */
  virtual TypeId type() = 0;
};

/** @brief Templated UI element base with shared window support and default
 * type().
 * */
template <TypeId T>
class IUIElement : public AbstractUIElement {
 public:
  IUIElement() = default;
  IUIElement(WINDOW* window) { this->window = window; }
  TypeId type() { return T; }
};

/**
 * Draws a line from point a to point b
 * */
class UILine : public IUIElement<TypeId::Line> {
  typedef struct Coords {
    int x{};
    int y{};
  } Coords;

  int eq_line(int x) { return gradient * (x - p1.x) + p1.y; }

 public:
  int x_delta;
  int y_delta;
  int width;
  int height;
  double gradient;
  Coords p1, p2;
  UILine(int x1, int y1, int x2, int y2) {
    p1 = {.x = x1, .y = y1};
    p2 = {.x = x2, .y = y2};
    x_delta = x2 - x1;
    y_delta = y2 - y1;
    if (x2 - x1 != 0) {
      gradient = (double)(y2 - y1) / (double)(x2 - x1);
    } else {
      gradient = 0;
    }
    width = std::abs(x_delta) + 1;
    height = std::abs(y_delta) + 1;
    window = newwin(width, height, 0, 0);
    // wbkgd(window, COLOR_PAIR(1));
  }

  void render() override {
    if (y_delta == 0) {
      mvwhline(window, p1.y, p1.x, '-', width);
      wnoutrefresh(window);
      return;
    }
    if (x_delta == 0) {
      mvwvline(window, p1.y, p1.x, '|', height);
      wnoutrefresh(window);
      return;
    }

    /*  Quadrant layout
     *  0,2 = '/'
     *  1,3 = '\'
     *
     *       |
     *    1  |  0
     *       |
     *  -----|------
     *       |
     *    2  |  3
     *       |
     * */

    Coords norm_p2 = {.x = p2.x, .y = p2.y};
    if (p1.x != 0) {
      norm_p2.x = p2.x - p1.x;
    }
    if (p1.y != 0) {
      norm_p2.y = p2.y - p1.y;
    }

    char x_dir = norm_p2.x > 0 ? 1 : -1;
    char y_dir = norm_p2.y > 0 ? 1 : -1;
    // logToFile("---> " + std::to_string(norm_p2.x) + ", " +
    //           std::to_string(norm_p2.y));

    char quadrant;
    if (x_dir == 1 && y_dir == 1) {
      quadrant = '\\';
    } else if (x_dir == -1 && y_dir == 1) {
      quadrant = '/';
    } else if (x_dir == -1 && y_dir == -1) {
      quadrant = '\\';
    } else if (x_dir == 1 && y_dir == -1) {
      quadrant = '/';
    }

    // logToFile("Quad: " + std::to_string(quadrant));
    for (int i{p1.x}; i != p2.x; i += x_dir) {
      // logToFile(std::to_string(i) + ", " + std::to_string(eq_line(i)));
      mvwprintw(window, eq_line(i), i, "%c", quadrant);
    }
    wnoutrefresh(window);
  }
};

/** @brief Primitive box element.
 * default:
 *  width 10 chars
 *  height 10 chars
 * */
class UIBox : public IUIElement<TypeId::Box> {
 private:
  size_t width;
  size_t height;
  int x;
  int y;

 public:
  /** @brief Overrides the default window with a user provided window.
   * @note Allows nested window hierarchies.
   * */
  UIBox(WINDOW* window);
  UIBox(WINDOW* window, int w, int h, int xpos, int ypos);

  UIBox();
  UIBox(int w, int h, int xpos, int ypos);

  /** @brief Returns height of the current window. */
  int get_height() const { return height; }

  /** @brief Returns width of the current window. */
  int get_width() const { return width; }

  /** @brief Returns position of the current window.
   * @return position Coords (x,y).
   * */
  Coords get_pos() const { return Coords{x, y}; }

  /** @brief Sets the dimensions of the window and tells the window to resize.
   * @param w Width in characters.
   * @param h Height in characters.
   */
  void set_dimensions(int w, int h);

  /** @brief Sets the position of the window and tells the window to move.
   * @param x Horizontal position
   * @param y Vectical position
   * */
  void set_pos(int x, int y);

  /**@brief Creates a UI box element and returns it.
   * @param x Horizontal position
   * @param y Vertical position
   * @param height Height of the box
   * @param width Width of the box
   * @note All parameters are in characters.
   * */
  static std::unique_ptr<UIBox> create(int x,
                                       int y,
                                       int height,
                                       int width,
                                       WINDOW* window);

  /** @brief Draws a box using ncurses. */
  void render() override;
};

UIBox::UIBox(WINDOW* window) : IUIElement(window) {};

UIBox::UIBox(WINDOW* window, int w, int h, int xpos, int ypos)
    : IUIElement(window), width(w), height(h), x(xpos), y(ypos) {}

UIBox::UIBox() : UIBox(10, 5, 0, 0) {};

UIBox::UIBox(int w, int h, int xpos, int ypos)
    : width(w), height(h), x(xpos), y(ypos) {
  window = newwin(height, width, y, x);
}

void UIBox::set_dimensions(int width, int height) {
  this->width = width;
  this->height = height;
  wresize(window, height, width);
}

void UIBox::set_pos(int x, int y) {
  this->x = x;
  this->y = y;
  mvwin(window, y, x);
}

std::unique_ptr<UIBox> UIBox::create(int x = 0,
                                     int y = 0,
                                     int width = 0,
                                     int height = 0,
                                     WINDOW* window = nullptr) {
  if (window) {
    return std::make_unique<UIBox>(window, width, height, x, y);
  }
  return std::make_unique<UIBox>(width, height, x, y);
}

void UIBox::render() {
  box(window, 0, 0);
  wnoutrefresh(window);
}

/**
 * Creates a text element
 * */
class UIText : public IUIElement<TypeId::Text> {
 private:
  std::string label;
  size_t width;
  size_t height;
  int text_x;
  int text_y;
  int win_x;
  int win_y;

 public:
  UIText(int text_x,
         int text_y,
         int win_x,
         int win_y,
         int width,
         int height,
         std::string label,
         WINDOW* window);

  Coords get_text_pos() const { return {win_x, win_y}; };

  Coords get_window_pos() const { return {win_x, win_y}; };

  int get_height() const { return height; };

  int get_width() const { return width; };

  static std::unique_ptr<UIText> create(int x,
                                        int y,
                                        std::string label,
                                        WINDOW* window);

  static std::unique_ptr<UIText> create(int x,
                                        int y,
                                        int width,
                                        int height,
                                        std::string label,
                                        WINDOW* window);

  static std::unique_ptr<UIText> create(int text_x,
                                        int text_y,
                                        int win_x,
                                        int win_y,
                                        int width,
                                        int height,
                                        std::string label,
                                        WINDOW* window);

  void render() override;
};

UIText::UIText(int text_x,
               int text_y,
               int win_x,
               int win_y,
               int width,
               int height,
               std::string label,
               WINDOW* window)
    : text_x(text_x),
      text_y(text_y),
      win_x(win_x),
      win_y(win_y),
      width(width),
      height(height),
      label(label) {
  if (window) {
    this->window = window;
    wresize(this->window, height, width);
    mvwin(this->window, win_y, win_x);
  } else {
    this->window = newwin(height, width, win_y, win_y);
  }
}

void UIText::render() {
  mvwprintw(window, text_y, text_x, "%s", label.c_str());
  wnoutrefresh(window);
}

std::unique_ptr<UIText> _create_uitext_primitive(int text_x,
                                                 int text_y,
                                                 int win_x,
                                                 int win_y,
                                                 std::string label,
                                                 int width = -1,
                                                 int height = -1,
                                                 WINDOW* window = nullptr) {
  // center the text if no width & height is specified
  if (width == -1) {
    width = label.length() + 2;
    text_x = 1;
  }
  if (height == -1) {
    height = 3;
    text_y = 1;
  }
  return std::make_unique<UIText>(text_x, text_y, win_x, win_y, width, height,
                                  label, window);
}

std::unique_ptr<UIText> UIText::create(int x,
                                       int y,
                                       std::string label = "",
                                       WINDOW* window = nullptr) {
  return _create_uitext_primitive(0, 0, x, y, label, -1, -1, window);
}

std::unique_ptr<UIText> UIText::create(int x,
                                       int y,
                                       int width,
                                       int height,
                                       std::string label = "",
                                       WINDOW* window = nullptr) {
  return _create_uitext_primitive(0, 0, x, y, label, width, height, window);
}

std::unique_ptr<UIText> create(int text_x,
                               int text_y,
                               int win_x,
                               int win_y,
                               int width,
                               int height,
                               std::string label,
                               WINDOW* window) {
  return _create_uitext_primitive(text_x, text_y, win_x, win_y, label, width,
                                  height, window);
};

/** @brief RAII wrapper for ncurses screen context with UI element hierarchy
 * and event management
 *
 * Initializes ncurses with mouse support, manages screen dimensions, and owns
 * a tree of UI elements.
 *
 * Handles lifecycle (initscr/endwin), resize events, and event dispatching
 * via Event::Observer.
 *
 * @note Non-copyable/moveable to ensure exclusive ownership of ncurses state.
 * */
class ScreenContext {
 private:
  WINDOW* _window;
  mmask_t _oldmask;
  int _screen_width;
  int _screen_height;
  bool _running;
  Event::Observer _observer;
  std::vector<std::unique_ptr<AbstractUIElement>> _children;

  void configure_ncurses();
  void cleanup_ncurses();

 public:
  ScreenContext();
  ~ScreenContext();

  /** Disallows move/copy semantics */
  ScreenContext(const ScreenContext&) = delete;
  ScreenContext& operator=(const ScreenContext&) = delete;
  ScreenContext(ScreenContext&&) = delete;
  ScreenContext& operator=(ScreenContext&&) = delete;

  /** @brief Returns the current screen width in characters.
   * @return Cached width from last ncurses query.
   */
  int get_width() const { return _screen_width; }

  /** @brief Returns the current screen height in characters.
   * @return Cached height from last ncurses query.
   */
  int get_height() const { return _screen_height; }

  /**
   * @brief Returns the raw ncurses window handle.
   * @return Pointer to underlying WINDOW (do not delete).
   * @warning Direct ncurses usage bypasses RAII safety.
   */
  WINDOW* get_window() const { return _window; }

  /** @brief Returns const reference to child UI elements.
   * @return Read-only view of the element hierarchy.
   * @note Do not modify or store the returned reference.
   */
  const std::vector<std::unique_ptr<AbstractUIElement>>& get_children() const {
    return _children;
  }

  /** @brief Updates the cached screen dimensions from the ncurses
   * window.
   * @note Called automatically by the resize event */
  void update_dimensions();

  /** @brief Sets the running state of the screen context to false. */
  void stop() { _running = false; }

  /** @brief Queries the running state of the screen context. */
  bool is_running() const { return _running; }

  /** @brief Adds child UI element to this context's hierarchy.
   * @param child Unique ownership of element to add (moved into storage).
   * @warning Child ownership transfers to ScreenContext. Do not reference
   * after calling.
   * */
  void add_child(std::unique_ptr<AbstractUIElement> child);

  /** @brief Deletes child UI element from this context's hierarchy.
   * @param child Raw UI Element pointer (from any_child.get()).
   * @note Safe if not found. Compares object addresses only.
   * */
  void del_child(AbstractUIElement* child);

  /** @brief Clears the children UI hierarchy from this context.
   * @note Safe to call at any point. Destroys all owned children.
   * */
  void clear_children();

  /** @brief Returns reference to the event manager for this context.
   * @return Mutable reference to EventManager for event subscription/dispatch
   * */
  Event::Observer& observer() { return _observer; }
};

void ScreenContext::add_child(std::unique_ptr<AbstractUIElement> child) {
  if (!child)
    return;
  _children.emplace_back(std::move(child));
}

void ScreenContext::del_child(AbstractUIElement* child) {
  if (!child)
    return;
  auto it =
      std::find_if(_children.begin(), _children.end(),
                   [&child](const auto& ptr) { return ptr.get() == child; });
  if (it != _children.end()) {
    _children.erase(it);
  }
}

ScreenContext::ScreenContext()
    : _window(nullptr),
      _oldmask(0),
      _screen_width(0),
      _screen_height(0),
      _running(false) {
  configure_ncurses();
}

ScreenContext::~ScreenContext() {
  cleanup_ncurses();
}

void ScreenContext::clear_children() {
  _children.clear();
}

void ScreenContext::configure_ncurses() {
  _window = initscr();
  if (!_window) {
    throw std::runtime_error("Failed to initialize ncurses window");
  }

  getmaxyx(_window, _screen_height, _screen_width);

  cbreak();
  noecho();
  keypad(_window, TRUE);
  mmask_t mask;
  mask = ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION;
  curs_set(0);
  mouseinterval(0);
  mousemask(mask, &_oldmask);
  printf("\033[?1003h\n");
  fflush(stdout);
  _running = true;
}

void ScreenContext::cleanup_ncurses() {
  printf("\033[?1003l\n");
  fflush(stdout);
  curs_set(1);
  mousemask(_oldmask, nullptr);
  if (_window) {
    delwin(_window);
    _window = nullptr;
  }
  endwin();
}

void ScreenContext::update_dimensions() {
  if (!_window) {
    return;
  }
  getmaxyx(_window, _screen_height, _screen_width);
}

/** @brief Internal renderer supporting shared_ptr and unique ptr hierarchies.
 * @note Private implementation. Use UIContext::start() instead.
 * */
class Renderer {
 public:
  /** @brief Recursively renders shared_ptr UI element hierarchy.
   * @param children Shared ownership hierarchy to render.
   * @note Internal. Use UIContext::start() instead to avoid flickering and
   * perf overhead.
   * */
  void render(std::vector<std::shared_ptr<AbstractUIElement>> children) {
    render_impl(children);
  };

  /** @brief Recursively renders unique_ptr UI element hierarchy.
   * @param children Unique hierarchy to render.
   * @note Internal. Use UIContext::start() instead to avoid flickering and
   * perf overhead.
   * */
  void render(const std::vector<std::unique_ptr<AbstractUIElement>>& children) {
    render_impl(children);
  };

 private:
  template <class C>
  void render_impl(const C& children) {
    for (auto& child : children) {
      if (child->composition.size() > 0) {
        render(child->composition);
      }
      child->render();
    }
  }
};

/** @brief RAII UI context that renders child hierarchy and dispatches ncurses
 * events.
 *
 * Extends ScreenContext with automatic child rendering and MEVENT dispatching
 * to EventManager. Inherits RegisteredEvents for event subscription.
 * */
class UIContext : public ScreenContext, public Renderer {
 public:
  Event::MouseEvent mouse_event{Event::MouseEvent()};
  UIContext() = default;
  ~UIContext() = default;

  /** @brief Starts the main ncurses event loop with child rendering and event
   * dispatch.
   * @note Blocks until stop() is called or window is destroyed.
   * */
  void start();

  /** @brief Detects mouse interaction on elements and dispatches mousedown,
   * mouseup, and click events.
   * @param children Shared ownership hierarchy to test for click hits.
   * @note Internal. Automatically called from unique_ptr-based
   * handle_click().
   * */
  void handle_click(std::vector<std::shared_ptr<AbstractUIElement>> children);

  /** @brief Checks if child has a shared_ptr UI hierarchy and forwards
   * handling to the shared_ptr handle_click().
   * @param children Unique ownership hierarchy to test for shared_ptr
   * hierarchy.
   * @note Internal. Automatically called from start().
   * */
  void handle_click(
      const std::vector<std::unique_ptr<AbstractUIElement>>& children);

  /** @brief Wraps render() with a single wnoutrefresh() + doupdate() for
   * efficiency and to avoid flickering.
   * @note Internal method. Use start() instead to insure children exist.
   * */
  void batch_render();
};

void UIContext::start() {
  batch_render();

  WINDOW* win = get_window();
  MEVENT event;
  int c{};
  mouse_event.data.ctx = this;
  while (is_running()) {
    c = wgetch(win);
    if (c == 'q') {
      stop();
      break;
    }
    touchwin(stdscr);
    wnoutrefresh(win);
    if (c == KEY_RESIZE) {
      update_dimensions();
      observer().notify(Event::Type::Resize);
    }

    if (c == KEY_MOUSE) {
      while (getmouse(&event) == OK) {
        mouse_event.data.x = event.x;
        mouse_event.data.y = event.y;
        observer().notify(Event::Type::Mousemove);
        if (event.bstate & BUTTON1_PRESSED) {
          handle_click(get_children());
          observer().notify(Event::Type::Mousedown);
        } else if (event.bstate & BUTTON1_RELEASED) {
          observer().notify(Event::Type::Mouseup);
          observer().notify(Event::Type::Click);
          mouse_event.data.element.reset();
        }
      }
    }
    batch_render();
  }
}

void UIContext::batch_render() {
  wnoutrefresh(get_window());
  render(get_children());
  doupdate();
}

void UIContext::handle_click(
    const std::vector<std::unique_ptr<AbstractUIElement>>& _children) {
  for (auto& child : _children) {
    if (child->composition.size() > 0) {
      UIContext::handle_click(child->composition);
    };
  }
}

void UIContext::handle_click(
    std::vector<std::shared_ptr<AbstractUIElement>> _children) {
  for (auto& child : _children) {
    if (child->composition.size() > 0) {
      handle_click(child->composition);
    };
    switch (child->type()) {
      case TypeId::Box: {
        auto box = std::static_pointer_cast<UIBox>(child);
        int start_y, start_x;
        getbegyx(child->window, start_y, start_x);
        if (mouse_event.data.x >= start_x &&
            mouse_event.data.x <= start_x + box->get_width() &&
            mouse_event.data.y >= start_y &&
            mouse_event.data.y <= start_y + box->get_height()) {
          mouse_event.data.offset_x = mouse_event.data.x - start_x;
          mouse_event.data.offset_y = mouse_event.data.y - start_y;
          mouse_event.data.element = child;
        }
        break;
      }
      default:
        break;
    };
  }
}

/**
 * Create a button
 * */
class UIButton : public IUIElement<TypeId::Button> {
 public:
  template <typename F>
  UIButton(Event::MouseEvent* event,
           std::string label,
           int x,
           int y,
           F&& callback);

  static std::unique_ptr<UIButton> create(
      Event::MouseEvent* event,
      std::string label,
      int x,
      int y,
      std::function<void(Event::MouseEvent::Data)> callback);

  void render() {};
};

template <typename F>
UIButton::UIButton(Event::MouseEvent* event,
                   std::string label,
                   int x,
                   int y,
                   F&& callback) {
  auto box = UIBox::create();
  auto text = UIText::create(x, y, label, box->window);
  box->set_dimensions(text->get_width(), text->get_height());
  this->window = box->window;

  composition.emplace_back(std::move(box));
  composition.emplace_back(std::move(text));

  event->add(Event::Type::Click, [&](Event::MouseEvent::Data d) {
    if (d.element && d.element->window == this->window) {
      callback(d);
    }
  });
}

std::unique_ptr<UIButton> UIButton::create(
    Event::MouseEvent* event,
    std::string label,
    int x,
    int y,
    std::function<void(Event::MouseEvent::Data)> callback = {}) {
  return std::make_unique<UIButton>(event, label, x, y, callback);
}

#endif

#include <ncurses.h>
#include <unistd.h>
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

class UIContext;
class AbstractUIElement;

typedef struct Coords {
  int x, y;
} Coords;

namespace Type {
enum class Id { None, Box, Text, Button, Line, Curve, Node };

enum class Flags : uint8_t {
  None,
  Draggable,
  Editable,
};
};  // namespace Type

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

template <typename C>
class GenericEvent : public EventListener {
 public:
  C data;

 private:
  struct Meta {
    std::function<void(C)> func;
    std::uintptr_t id;
    Type type;
  };
  std::vector<Meta> _calls;

 public:
  template <typename F>
  auto add(Type event_type, F&& arg);

  template <typename F>
  void remove(F&& arg);
  void update(Type event) override;
};

template <typename C>
template <typename F>
auto GenericEvent<C>::add(Event::Type event_type, F&& arg) {
  auto ptr = reinterpret_cast<std::uintptr_t>(static_cast<void*>(&arg));
  _calls.emplace_back(
      Meta{.func = std::forward<F>(arg), .id = ptr, .type = event_type});
  return ptr;
}

template <typename C>
template <typename F>
void GenericEvent<C>::remove(F&& arg) {
  auto ptr = reinterpret_cast<std::uintptr_t>(static_cast<void*>(&arg));
  std::erase_if(_calls, [&ptr](Meta m) { return m.id == ptr; });
}

template <typename C>
void GenericEvent<C>::update(Type event) {
  for (Meta& x : _calls) {
    if (x.type == event) {
      x.func(this->data);
    }
  }
}

struct MouseData {
  int x{0};
  int y{0};
  int offset_x{0};
  int offset_y{0};
  std::shared_ptr<AbstractUIElement> element;
  UIContext* ctx;
};

class MouseEvent : public GenericEvent<MouseData> {};

struct ScreenData {
  int width{};
  int height{};
  UIContext* ctx;
};

class ScreenEvent : public GenericEvent<ScreenData> {};

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
  Type::Flags flags{Type::Flags::None};
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
  virtual Type::Id type() = 0;
};

/** @brief Templated UI element base with shared window support and default
 * type().
 * */
template <Type::Id T>
class IUIElement : public AbstractUIElement {
 public:
  IUIElement() = default;
  IUIElement(WINDOW* window) { this->window = window; }
  Type::Id type() { return T; }
};

/** @bried UI Line element. */
class UILine : public IUIElement<Type::Id::Line> {
 private:
  Coords pos1{}, pos2{};
  double gradient{};
  int x_delta{};
  int y_delta{};
  int width{};
  int height{};
  char x_dir{};
  char y_dir{};
  char quadrant{};
  int calcx_line(int x);
  void calc_char();
  void _calculate_line_data();

 public:
  UILine(const Coords& pos1, const Coords& pos2, WINDOW* window);

  static std::shared_ptr<UILine> create(Coords pos1,
                                        Coords pos2,
                                        WINDOW* window);

  void set_pos(Coords pos1, Coords pos2);
  void render() override;
};

void UILine::_calculate_line_data() {
  x_delta = pos2.x - pos1.x;
  y_delta = pos2.y - pos1.y;

  if (x_delta == 0) {
    gradient = 0;
  } else {
    gradient = (double)(y_delta) / (double)(x_delta);
  }

  width = std::abs(x_delta) + 1;
  height = std::abs(y_delta) + 1;
}

UILine::UILine(const Coords& pos1, const Coords& pos2, WINDOW* window)
    : pos1(pos1), pos2(pos2) {
  _calculate_line_data();
  if (window) {
    this->window = window;
    wresize(this->window, height, width);
  } else {
    this->window = stdscr;
  }
}

std::shared_ptr<UILine> UILine::create(Coords pos1,
                                       Coords pos2,
                                       WINDOW* window = nullptr) {
  return std::make_shared<UILine>(pos1, pos2, window);
};

void UILine::set_pos(Coords pos1, Coords pos2) {
  werase(window);
  this->pos1 = pos1;
  this->pos2 = pos2;
  _calculate_line_data();
};

int UILine::calcx_line(int x) {
  return gradient * (x - pos1.x) + pos1.y;
}

void UILine::calc_char() {
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

  Coords norm_p2 = {.x = pos2.x, .y = pos2.y};
  if (pos1.x != 0) {
    norm_p2.x = pos2.x - pos1.x;
  }
  if (pos1.y != 0) {
    norm_p2.y = pos2.y - pos1.y;
  }

  x_dir = norm_p2.x > 0 ? 1 : -1;
  y_dir = norm_p2.y > 0 ? 1 : -1;

  if (x_dir == 1 && y_dir == 1 || x_dir == -1 && y_dir == -1) {
    quadrant = '\\';
  } else if (x_dir == -1 && y_dir == 1 || x_dir == 1 && y_dir == -1) {
    quadrant = '/';
  }
}

void UILine::render() {
  if (y_delta == 0) {
    int fp = 0;
    if (pos1.x > pos2.x)
      fp = pos1.x - pos2.x;
    mvwhline(window, pos1.y, pos1.x - fp, '-', width);
    wnoutrefresh(window);
    return;
  }
  if (x_delta == 0) {
    int fp = 0;
    if (pos1.y > pos2.y)
      fp = pos1.y - pos2.y;
    mvwvline(window, pos1.y - fp, pos1.x, '|', height);
    wnoutrefresh(window);
    return;
  }

  calc_char();

  for (int i{pos1.x}; i != pos2.x; i += x_dir) {
    mvwprintw(window, calcx_line(i), i, "%c", quadrant);
  }
  wnoutrefresh(window);
}

/** @brief Primitive box element.
 * default:
 *  width 10 chars
 *  height 10 chars
 * */
class UIBox : public IUIElement<Type::Id::Box> {
 private:
  size_t width{};
  size_t height{};
  int x{};
  int y{};

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
  static std::shared_ptr<UIBox> create(int x,
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

std::shared_ptr<UIBox> UIBox::create(int x = 0,
                                     int y = 0,
                                     int width = 0,
                                     int height = 0,
                                     WINDOW* window = nullptr) {
  if (window) {
    return std::make_shared<UIBox>(window, width, height, x, y);
  }
  return std::make_shared<UIBox>(width, height, x, y);
}

void UIBox::render() {
  box(window, 0, 0);
  wnoutrefresh(window);
}

/** @brief UI Text element.
 * @note Text elements have a width, height (both are values for the window)
 * A text position, a window position, and label content.
 * @note Use UIText::create over directly intializing the class.
 * @note UIText::create will automatically generate the appropriate text UI
 * element for a given label.
 * */
class UIText : public IUIElement<Type::Id::Text> {
 private:
  std::string label;
  size_t width;
  size_t height;
  int text_x;
  int text_y;
  int win_x;
  int win_y;

 public:
  /**@brief Default constructor*/
  UIText(int text_x,
         int text_y,
         int win_x,
         int win_y,
         int width,
         int height,
         std::string label,
         WINDOW* window);
  /**@brief Returns the x and y position of the text inside the current window.
   */
  Coords get_text_pos() const { return {win_x, win_y}; };

  /**@brief Returns the x and y position of the current window.*/
  Coords get_window_pos() const { return {win_x, win_y}; };

  /**@brief Returns the height of the window.*/
  int get_height() const { return height; };

  /**@brief Returns the width of the window.*/
  int get_width() const { return width; };

  void set_pos(int x, int y);

  void set_label(std::string label);

  void set_dimensions(int width, int height);

  /**@brief Creates an UI text element.
   * @param x Horizontal position of window.
   * @param y Vectical position of window.
   * @param label String that is rendered.
   * @param window Optional window override.
   * */
  static std::shared_ptr<UIText> create(int x,
                                        int y,
                                        std::string label,
                                        WINDOW* window);

  /**@brief Creates an UI text element.
   * @param x Horizontal position of window.
   * @param y Vectical position of window.
   * @param width Width of the window in characters.
   * @param height Height of the window in characters.
   * @param label String that is rendered.
   * @param window Optional window override.
   * */
  static std::shared_ptr<UIText> create(int x,
                                        int y,
                                        int width,
                                        int height,
                                        std::string label,
                                        WINDOW* window);

  /**@brief Creates an UI text element.
   * @param text_x Horizontal position of the text relative to this window.
   * @param text_y Vectical position of the text relative to this window.
   * @param win_x Horizontal position of window
   * @param win_y Vectical position of window.
   * @param width Width of the window in characters.
   * @param height Height of the window in characters.
   * @param label String that is rendered.
   * @param window Optional window override.
   * */
  static std::shared_ptr<UIText> create(int text_x,
                                        int text_y,
                                        int win_x,
                                        int win_y,
                                        int width,
                                        int height,
                                        std::string label,
                                        WINDOW* window);

  /**@brief Render text in this current window at text_x, text_y.*/
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

void UIText::set_pos(int x, int y) {
  this->win_x = x;
  this->win_y = y;
  mvwin(window, y, x);
};

void UIText::set_label(std::string label) {
  this->label = label;
};

void UIText::set_dimensions(int width, int height) {
  this->width = width;
  this->height = height;
  wresize(window, height, width);
};

std::shared_ptr<UIText> _create_uitext_primitive(int text_x,
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
  return std::make_shared<UIText>(text_x, text_y, win_x, win_y, width, height,
                                  label, window);
}

std::shared_ptr<UIText> UIText::create(int x,
                                       int y,
                                       std::string label = "",
                                       WINDOW* window = nullptr) {
  return _create_uitext_primitive(0, 0, x, y, label, -1, -1, window);
}

std::shared_ptr<UIText> UIText::create(int x,
                                       int y,
                                       int width,
                                       int height,
                                       std::string label = "",
                                       WINDOW* window = nullptr) {
  return _create_uitext_primitive(0, 0, x, y, label, width, height, window);
}

std::shared_ptr<UIText> create(int text_x,
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

/**@brief UI Button element class.
 * @note Callback methods are very flexible and are allowed to have capture
 * groups.
 * @important Callback methods MUST have a parameter of type
 * Event::MouseEvent::Data.
 * */
class UIButton : public IUIElement<Type::Id::Button> {
 public:
  template <typename F>

  /**@brief Default contructor. */
  UIButton(Event::MouseEvent* event,
           std::string label,
           int x,
           int y,
           F&& callback);

  /**@brief Creates a UI button element.
   * @param event Event handler context for the button to bind to.
   * @param label The buttons label.
   * @param x Horizontal position of the button.
   * @param y Vectical position of the button.
   * @param callback Optional callback method.
   * @important Callback methods MUST have a parameter of type
   * Event::MouseEvent::Data.
   * */
  static std::shared_ptr<UIButton> create(
      Event::MouseEvent* event,
      std::string label,
      int x,
      int y,
      std::function<void(Event::MouseData)> callback);

  void render() {};
};

class UINode : public IUIElement<Type::Id::Node> {
 private:
  std::vector<std::shared_ptr<UILine>> connections;
  int x;
  int y;
  std::shared_ptr<UIButton> clickable{0};
  std::shared_ptr<UILine> current_line{0};
  Coords line_origin{};

 public:
  UINode(Event::MouseEvent* e, int x, int y, std::string label);

  static std::shared_ptr<UINode> create(Event::MouseEvent* e,
                                        int x,
                                        int y,
                                        std::string label);

  void render() {};
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
  std::vector<std::shared_ptr<AbstractUIElement>> _children;

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
  const std::vector<std::shared_ptr<AbstractUIElement>>& get_children() const {
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
  void add_child(std::shared_ptr<AbstractUIElement> child);

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

void ScreenContext::add_child(std::shared_ptr<AbstractUIElement> child) {
  if (!child)
    return;
  _children.emplace_back(child);
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
  Event::ScreenEvent screen_event{Event::ScreenEvent()};

  UIContext();
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

UIContext::UIContext() {}

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
      screen_event.data.ctx = this;
      screen_event.data.height = ScreenContext::get_height();
      screen_event.data.width = ScreenContext::get_width();
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
    if (mouse_event.data.element)
      goto element_found;
    if (child->composition.size() > 0) {
      handle_click(child->composition);
    };
    switch (child->type()) {
      case Type::Id::Button: {
        auto button = std::static_pointer_cast<UINode>(child);
        std::shared_ptr<UIBox> box = nullptr;
        for (std::shared_ptr<AbstractUIElement>& nested_child :
             button->composition) {
          if (nested_child->type() == Type::Id::Box) {
            box = std::static_pointer_cast<UIBox>(nested_child);
            break;
          }
        }
        if (!box)
          break;

        int start_y, start_x;
        getbegyx(box->window, start_y, start_x);
        if (mouse_event.data.x >= start_x &&
            mouse_event.data.x <= start_x + box->get_width() &&
            mouse_event.data.y >= start_y &&
            mouse_event.data.y <= start_y + box->get_height()) {
          mouse_event.data.offset_x = mouse_event.data.x - start_x;
          mouse_event.data.offset_y = mouse_event.data.y - start_y;
          mouse_event.data.element = button;
        }
        break;
      }
      case Type::Id::Node: {
        auto node = std::static_pointer_cast<UINode>(child);
        std::shared_ptr<UIBox> box = nullptr;
        for (std::shared_ptr<AbstractUIElement>& nested_child :
             node->composition) {
          if (nested_child->type() == Type::Id::Box) {
            box = std::static_pointer_cast<UIBox>(nested_child);
            break;
          }
        }
        if (!box)
          return;

        int start_y, start_x;
        getbegyx(box->window, start_y, start_x);
        if (mouse_event.data.x >= start_x &&
            mouse_event.data.x <= start_x + box->get_width() &&
            mouse_event.data.y >= start_y &&
            mouse_event.data.y <= start_y + box->get_height()) {
          mouse_event.data.offset_x = mouse_event.data.x - start_x;
          mouse_event.data.offset_y = mouse_event.data.y - start_y;
          mouse_event.data.element = node;
        }
        break;
      }
      default:
        break;
    };
  }
element_found:
  return;
}

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

  composition.emplace_back(box);
  composition.emplace_back(text);

  event->add(Event::Type::Click, [&](Event::MouseData d) {
    if (d.element && d.element->window == this->window) {
      callback(d);
    }
  });
}

std::shared_ptr<UIButton> UIButton::create(
    Event::MouseEvent* event,
    std::string label,
    int x,
    int y,
    std::function<void(Event::MouseData)> callback = {}) {
  return std::make_shared<UIButton>(event, label, x, y, callback);
}

UINode::UINode(Event::MouseEvent* e, int x, int y, std::string label) {
  auto box = UIBox::create();
  auto text = UIText::create(x, y, label, box->window);
  box->set_dimensions(text->get_width(), text->get_height());
  this->x = x;
  this->y = y;
  this->window = box->window;

  clickable = UIButton::create(e, "x", 0, 0, [&](Event::MouseData d) {
    if (!current_line && d.element && d.element->window == clickable->window) {
      logToFile("Clicked!");
      line_origin = Coords{d.x, d.y};
      current_line = UILine::create(line_origin, line_origin);
      composition.emplace_back(current_line);
    }
  });

  composition.emplace_back(box);
  composition.emplace_back(text);
  composition.emplace_back(clickable);

  e->add(Event::Type::Mousedown, [&](Event::MouseData d) {
    if (!current_line && d.element && d.element->window == clickable->window) {
      line_origin = Coords{d.x, d.y};
      current_line = UILine::create(line_origin, line_origin);
      composition.emplace_back(current_line);
    }
    if (current_line && !d.element) {
      line_origin = {0, 0};
      composition.pop_back();
      current_line.reset();
    }
  });

  e->add(Event::Type::Mouseup, [&](Event::MouseData d) {
    // if (current_line && d.element && d.element->type() == Type::Id::Node &&
    //     d.element->window != this->window &&
    //     d.element->window != clickable->window) {
    //   connections.push_back(current_line);
    //   composition.pop_back();
    //   composition.emplace_back(connections.back());
    //   current_line.reset();
    //   line_origin = {0, 0};
    // }
  });

  e->add(Event::Type::Mousemove, [&](Event::MouseData d) {
    if (d.element && d.element->window == this->window) {
      this->x = d.x - d.offset_x;
      this->y = d.y - d.offset_y;
      auto box =
          std::static_pointer_cast<UIBox>(this->clickable->composition.front());
      box->set_pos(this->x, this->y);
      mvwin(this->window, this->y, this->x);
    }

    if (!d.element && current_line) {
      current_line->set_pos(line_origin, Coords{d.x, d.y});
    }
  });
}

std::shared_ptr<UINode> UINode::create(Event::MouseEvent* e,
                                       int x,
                                       int y,
                                       std::string label) {
  return std::make_shared<UINode>(e, x, y, label);
}

#endif

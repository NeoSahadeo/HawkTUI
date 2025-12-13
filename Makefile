NAME        := main

LIBS				:= ncurses

SRC_DIR			:= src
SRCS				:= $(shell find $(SRC_DIR) -name "*.cpp")

BUILD_DIR   := .build
OBJS        := $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
DEPS        := $(OBJS:.o=.d)

CC          := clang++
CFLAGS      := -O0 -g -std=c++23
CPPFLAGS    := -MMD -MP -I include
LDLIBS      := $(addprefix -l,$(LIBS))

RM          := rm -rf
MAKEFLAGS   += --no-print-directory
DIR_DUP     = mkdir -p $(@D)

all: $(NAME)

dev: $(NAME)
	./$(NAME)

$(NAME): $(OBJS)
	$(CC) $(OBJS) $(LDLIBS) -o $(NAME)
	$(info CREATED $(NAME))

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(DIR_DUP)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
	$(info CREATED $@)

-include $(DEPS)

clean:
	$(RM) $(OBJS) $(DEPS)
	$(info CLEANED)

fclean: clean
	$(RM) $(NAME)

re:
	$(MAKE) fclean
	$(MAKE) all

.PHONY: clean fclean re dev

.SILENT:

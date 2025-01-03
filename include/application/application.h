#pragma once

#include "pipewire/filter.h"

#include <pipewire/context.h>
#include <pipewire/main-loop.h>

struct port {};

namespace application {

class Application {
public:
  Application(int argc, char *argv[]);

  static void on_process(void *user_data, struct spa_io_position *position);

  static void do_quit(void *user_data, int signal_number);

  void shutdown();

  int run();

private:
  struct pw_main_loop *loop;
  struct pw_context *context;
  struct pw_core *core;
  struct pw_registry *registry;
  struct pw_filter *filter;
  struct spa_hook registry_listener;
  struct port *osc_messages_port;
};

} // namespace application

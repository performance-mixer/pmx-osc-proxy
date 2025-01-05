#include "application/application.h"

#include <cstring>

#include <iostream>
#include <oscpp/print.hpp>
#include <oscpp/server.hpp>

#include <pipewire/loop.h>
#include <pipewire/pipewire.h>

#include <spa/control/control.h>
#include <spa/debug/pod.h>
#include <spa/pod/builder.h>
#include <spa/pod/iter.h>
#include <spa/pod/parser.h>
#include <spa/pod/pod.h>
#include <spa/utils/dict.h>

void application::Application::on_process(void *user_data,
                                          struct spa_io_position *position) {

  auto this_pointer = static_cast<Application *>(user_data);

  struct pw_buffer *pw_buffer;
  if ((pw_buffer = pw_filter_dequeue_buffer(this_pointer->osc_messages_port)) ==
      nullptr) {
    return;
  }

  for (unsigned int i = 0; i < pw_buffer->buffer->n_datas; i++) {
    auto pod = static_cast<struct spa_pod *>(spa_pod_from_data(
        pw_buffer->buffer->datas[i].data, pw_buffer->buffer->datas[i].maxsize,
        pw_buffer->buffer->datas[i].chunk->offset,
        pw_buffer->buffer->datas[i].chunk->size));

    if (!pod)
      continue;

    if (!spa_pod_is_sequence(pod)) {
      return;
    }

    auto sequence = reinterpret_cast<struct spa_pod_sequence *>(pod);

    struct spa_pod_control *pod_control;
    SPA_POD_SEQUENCE_FOREACH(sequence, pod_control) {

      if (pod_control->type != SPA_CONTROL_OSC) {
        return;
      }

      uint8_t *data = nullptr;
      uint32_t length;
      spa_pod_get_bytes(&pod_control->value, (const void **)&data, &length);

      OSCPP::Server::Packet outer_packet(data, length);
      OSCPP::Server::Bundle bundle(outer_packet);
      OSCPP::Server::PacketStream packets(bundle.packets());
      while (!packets.atEnd()) {
        OSCPP::Server::Message msg(packets.next());
        OSCPP::Server::ArgStream args(msg.args());
        auto value = args.float32();
        std::cout << msg << ": " << value << std::endl;
      }
    }
  }

  pw_filter_queue_buffer(this_pointer->osc_messages_port, pw_buffer);
}

static const pw_filter_events filter_events = {
    .version = PW_VERSION_FILTER_EVENTS,
    .process = application::Application::on_process,
};

application::Application::Application(int argc, char *argv[]) {
  pw_init(&argc, &argv);

  loop = pw_main_loop_new(NULL);

  pw_loop_add_signal(pw_main_loop_get_loop(loop), SIGINT, do_quit, this);
  pw_loop_add_signal(pw_main_loop_get_loop(loop), SIGTERM, do_quit, this);

  context = pw_context_new(pw_main_loop_get_loop(loop), nullptr, 0);
  core = pw_context_connect(context, nullptr, 0);
  registry = pw_core_get_registry(core, PW_VERSION_REGISTRY, 0);

  filter = pw_filter_new_simple(
      pw_main_loop_get_loop(loop), "pmx-osc-proxy",
      pw_properties_new(PW_KEY_MEDIA_TYPE, "Osc", PW_KEY_MEDIA_CATEGORY,
                        "Filter", PW_KEY_MEDIA_CLASS, "Osc/Sink",
                        PW_KEY_MEDIA_ROLE, "DSP", NULL),
      &filter_events, this);

  osc_messages_port = static_cast<port *>(pw_filter_add_port(
      filter, PW_DIRECTION_INPUT, PW_FILTER_PORT_FLAG_MAP_BUFFERS, sizeof(port),
      pw_properties_new(PW_KEY_FORMAT_DSP, "8 bit raw midi", PW_KEY_PORT_NAME,
                        "input", NULL),
      NULL, 0));
}

int application::Application::run() {
  if (pw_filter_connect(filter, PW_FILTER_FLAG_RT_PROCESS, NULL, 0) < 0) {
    fprintf(stderr, "can't connect\n");
    return -1;
  }

  pw_main_loop_run(loop);
  pw_proxy_destroy((struct pw_proxy *)registry);
  pw_core_disconnect(core);
  pw_filter_destroy(filter);
  pw_filter_destroy(filter);
  pw_main_loop_destroy(loop);
  pw_deinit();

  return 0;
}

void application::Application::shutdown() { pw_main_loop_quit(loop); }

void application::Application::do_quit(void *user_data, int signal_number) {
  auto this_pointer = static_cast<Application *>(user_data);
  this_pointer->shutdown();
}

#include "application/application.h"

int main(int argc, char *argv[]) {
  application::Application application(argc, argv);
  application.run();
}

#include "minitest.h"

int main(int argc, char** argv) { return minitest::runAll(argc > 1 ? argv[1] : nullptr); }

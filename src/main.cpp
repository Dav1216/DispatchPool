// Entry point (initializes and runs Dispatcher)
#include "dealer.h"
#include "worker_process.h"

int main() {
    Dealer dealer;
    dealer.run();
    return 0;
}

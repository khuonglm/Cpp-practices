// Interactive REPL to play with the book by hand.
//   L B|S <id> <qty> <px>   limit order      (e.g.  L B 1 10 100)
//   M B|S <id> <qty>        market order      (e.g.  M S 2 5)
//   C <id>                  cancel
//   P                       print top-of-book
//   Q                       quit
#include "ob/matching_engine.hpp"
#include <iostream>
#include <string>

using namespace ob;

static void print_trades(const std::vector<Trade>& ts) {
    if (ts.empty()) std::cout << "  (no trade)\n";
    for (const auto& t : ts) std::cout << "  " << t << "\n";
}

int main() {
    MatchingEngine e;
    std::cout << "commands:  L B|S id qty px  |  M B|S id qty  |  C id  |  P  |  Q\n";
    std::string cmd;
    while (std::cout << "> " && std::cin >> cmd) {
        if (cmd == "L") {
            std::string bs; OrderId id; Qty qty; Price px; std::cin >> bs >> id >> qty >> px;
            print_trades(e.add_limit(id, bs == "B" ? Side::Buy : Side::Sell, px, qty));
        } else if (cmd == "M") {
            std::string bs; OrderId id; Qty qty; std::cin >> bs >> id >> qty;
            print_trades(e.add_market(id, bs == "B" ? Side::Buy : Side::Sell, qty));
        } else if (cmd == "C") {
            OrderId id; std::cin >> id;
            std::cout << (e.cancel(id) ? "  cancelled\n" : "  not found\n");
        } else if (cmd == "P") {
            auto bb = e.best_bid(); auto ba = e.best_ask();
            std::cout << "  BID " << (bb ? std::to_string(*bb) : "-")
                      << "   ASK " << (ba ? std::to_string(*ba) : "-") << "\n";
        } else if (cmd == "Q") {
            break;
        } else {
            std::cout << "  ? unknown command\n";
        }
    }
    return 0;
}

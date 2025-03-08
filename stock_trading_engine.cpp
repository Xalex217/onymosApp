#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <random>
#include <chrono>

#define MAX_STOCKS 1024

struct Order {
    char type;
    int ticket;
    int quantity;
    int price;
    std::unique_ptr<Order> next;

    Order(char t, int tk, int q, int p) : type(t), ticket(tk), quantity(q), price(p), next(nullptr) {}
};

class OrderList {
private:
    std::mutex mtx;
    std::unique_ptr<Order> head;

public:
    void addOrder(std::unique_ptr<Order> order) {
        std::lock_guard<std::mutex> lock(mtx);
        order->next = std::move(head);
        head = std::move(order);
    }

    Order* getBestOrder(char type) {
        std::lock_guard<std::mutex> lock(mtx);
        Order* bestOrder = nullptr;
        Order* prev = nullptr;
        Order* current = head.get();

        std::cout << "Scanning orders for best " << (type == 'B' ? "BUY" : "SELL") << " order:\n";

        while (current) {
            std::cout << "  Checking order: " << current->quantity << " @ " << current->price << "\n";

            if (!bestOrder || 
                (current->type == 'B' && current->price > bestOrder->price) || 
                (current->type == 'S' && current->price < bestOrder->price)) {
                bestOrder = current;
            }
            current = current->next.get();
        }

        if (bestOrder) {
            std::cout << "  Best order chosen: " << bestOrder->quantity << " @ " << bestOrder->price << "\n";
        } else {
            std::cout << "  No valid order found.\n";
        }

        return bestOrder;
    }

    void removeOrder(Order* order) {
        std::lock_guard<std::mutex> lock(mtx);
        Order* prev = nullptr;
        Order* current = head.get();

        while (current) {
            if (current == order) {
                std::cout << "Removing order: " << current->quantity << " @ " << current->price << "\n";
                if (prev) {
                    prev->next = std::move(current->next);
                } else {
                    head = std::move(current->next);
                }
                return;
            }
            prev = current;
            current = current->next.get();
        }
    }
};

OrderList sellOrder[MAX_STOCKS];
OrderList buyOrder[MAX_STOCKS];
std::mutex cout_mtx;


void logMessage(const std::string& msg) {
    std::lock_guard<std::mutex> lock(cout_mtx);
    std::cout << msg << std::endl;
}

void addOrder(char type, int ticket, int quantity, int price) {
    auto order = std::make_unique<Order>(type, ticket, quantity, price);
    logMessage("Adding order: " + std::string(1, type) + " " + std::to_string(quantity) + 
               " @ " + std::to_string(price) + " for stock " + std::to_string(ticket));

    if (type == 'B') {
        buyOrder[ticket].addOrder(std::move(order));
    } else {
        sellOrder[ticket].addOrder(std::move(order));
    }
}

void matchOrder(int ticket) {
    while (true) {
        Order* buy = buyOrder[ticket].getBestOrder('B');
        Order* sell = sellOrder[ticket].getBestOrder('S');

        if (!buy || !sell) {
            logMessage("No matching orders for stock " + std::to_string(ticket));
            break;
        }

        if (buy->price < sell->price) {
            logMessage("Buy price (" + std::to_string(buy->price) + 
                       ") is lower than sell price (" + std::to_string(sell->price) + 
                       "), no match.");
            break;
        }

        int tradeQuantity = std::min(buy->quantity, sell->quantity);
        logMessage("Executing trade: " + std::to_string(ticket) + " " + 
                   std::to_string(tradeQuantity) + " @ " + std::to_string(buy->price));

        buy->quantity -= tradeQuantity;
        sell->quantity -= tradeQuantity;

        if (buy->quantity == 0) {
            buyOrder[ticket].removeOrder(buy);
        }
        if (sell->quantity == 0) {
            sellOrder[ticket].removeOrder(sell);
        }
    }
}


void simulateOrders() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> ticketDist(0, MAX_STOCKS - 1);
    std::uniform_int_distribution<int> quantityDist(1, 100);
    std::uniform_int_distribution<int> priceDist(1, 500);
    std::uniform_int_distribution<int> typeDist(0, 1);

    for (int i = 0; i < 1000; i++) {
        int ticket = ticketDist(gen);
        int quantity = quantityDist(gen);
        int price = priceDist(gen);
        char type = typeDist(gen) ? 'B' : 'S';

        addOrder(type, ticket, quantity, price);
        matchOrder(ticket);
    }
}

int main() {
    std::cout << "Stock Trading Engine Running...\n";
    std::thread t1(simulateOrders);
    std::thread t2(simulateOrders);

    t1.join();
    t2.join();

    return 0;
}

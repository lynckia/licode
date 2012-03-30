#include "Observer.h"

#include <cstdlib>            // For atoi()

//using namespace std;

int main(int argc, char *argv[]) {
    SDPReceiver* receiver = new SDPReceiver();
    Observer *subscriber = new Observer("subscriber", receiver);
    new Observer("publisher", receiver);
    subscriber->wait();
    return 0;
}


SDPReceiver::SDPReceiver() {
}
    
std::string SDPReceiver::callSubscriber(std::string sdp) {
    printf("********************* Received SDP in Subscriber\n");
    return sdp;
}

std::string SDPReceiver::callPublisher(std::string sdp) {
    printf("********************* Received SDP in Publisher\n");
    return sdp;
}


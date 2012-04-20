#include "PCSocket.h"

#include <pthread.h>
#include <cstdlib>
#include <stdio.h>// For atoi()

#include "SDPReceiver.h"

class Observer : PCClientObserver {
public:
    Observer(std::string name, SDPReceiver *receiver);
    ~Observer();
    void OnSignedIn();  // Called when we're logged on.
    void OnDisconnected();
    void OnPeerConnected(int id, const std::string& name);
    void OnPeerDisconnected(int peer_id);
    void OnMessageFromPeer(int peer_id, const std::string& message);
    void OnMessageSent(int err);
    void wait();


    static void Replace(std::string& text, const std::string& pattern, const std::string& replace);
    static std::string Match(const std::string& text, const std::string& pattern);

private:
    static void *init(void* obj);
    void start();


    PC *pc_;
    pthread_t thread;
    std::string name_;
    SDPReceiver *receiver_;
};


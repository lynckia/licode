class OneToManyProcessor;
class SDPReceiver {
public:
    SDPReceiver();
    virtual ~SDPReceiver(){};
    void createPublisher(int peer_id);
    void createSubscriber(int peer_id);
    void setRemoteSDP(int peer_id, const std::string &sdp);
    std::string getLocalSDP(int peer_id);
private:
    OneToManyProcessor* muxer;
};

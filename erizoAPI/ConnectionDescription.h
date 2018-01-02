
#ifndef ERIZOAPI_CONNECTIONDESCRIPTION_H_
#define ERIZOAPI_CONNECTIONDESCRIPTION_H_

#include <nan.h>
#include <SdpInfo.h>

/*
 * Wrapper class of erizo::SdpInfo
 *
 */
class ConnectionDescription : public Nan::ObjectWrap {
 public:
    static NAN_MODULE_INIT(Init);

    std::shared_ptr<erizo::SdpInfo> me;

 private:
    ConnectionDescription();
    ~ConnectionDescription();

    /*
     * Constructor.
     * Constructs an empty SdpInfo without any information.
     */
    static NAN_METHOD(New);
    /*
     * Closes the SdpInfo.
     * The object cannot be used after this call.
     */
    static NAN_METHOD(close);

    static NAN_METHOD(setProfile);
    static NAN_METHOD(setBundle);
    static NAN_METHOD(addBundleTag);
    static NAN_METHOD(setRtcpMux);
    static NAN_METHOD(setAudioAndVideo);

    static NAN_METHOD(setAudioSsrc);
    static NAN_METHOD(setVideoSsrcList);

    static NAN_METHOD(setDirection);

    static NAN_METHOD(setFingerprint);
    static NAN_METHOD(setDtlsRole);

    static NAN_METHOD(setVideoBandwidth);

    static NAN_METHOD(addCandidate);
    static NAN_METHOD(addCryptoInfo);
    static NAN_METHOD(setICECredentials);

    static NAN_METHOD(addRid);
    static NAN_METHOD(addPt);
    static NAN_METHOD(addExtension);
    static NAN_METHOD(addFeedback);
    static NAN_METHOD(addParameter);

    static NAN_METHOD(postProcessInfo);

    static Nan::Persistent<v8::Function> constructor;
};

#endif  // ERIZOAPI_CONNECTIONDESCRIPTION_H_

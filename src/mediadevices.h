#ifndef __MEDIADEVICES_H__
#define __MEDIADEVICES_H__

#include <string>

#include <node.h>
#include <v8.h>
#include <node_object_wrap.h>
#include <uv.h>

#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/api/peerconnectioninterface.h"
#include "webrtc/api/mediastreaminterface.h"

#include "common.h"
#include "nan.h"

using namespace node;
using namespace v8;

namespace node_webrtc {

class MediaDevices
: public Nan::ObjectWrap
{

public:  
  // Class implementation
  MediaDevices();
  ~MediaDevices();

  // NodeJS Wrapping
  // NodeJS Wrapping
  static void Init(rtc::Thread* signalingThread, rtc::Thread* workerThread, v8::Handle<v8::Object> exports);
  static Nan::Persistent<Function> constructor;
  static NAN_METHOD(New);

  static NAN_METHOD(GetUserMedia);

private:
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> _peer_connection_factory;

  static rtc::Thread* _signalingThread;
  static rtc::Thread* _workerThread;};

}  // namespace node_webrtc

#endif

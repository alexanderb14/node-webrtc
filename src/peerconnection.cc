/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "peerconnection.h"

#include "webrtc/api/test/fakeconstraints.h"
#include "webrtc/modules/audio_device/dummy/file_audio_device_factory.h"

#include "common.h"
#include "mediastream.h"
#include "datachannel.h"
#include "create-offer-observer.h"
#include "create-answer-observer.h"
#include "set-local-description-observer.h"
#include "set-remote-description-observer.h"

#include <iostream>

using node_webrtc::PeerConnection;
using v8::External;
using v8::Function;
using v8::FunctionTemplate;
using v8::Handle;
using v8::Integer;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Uint32;
using v8::Value;

Nan::Persistent<Function> PeerConnection::constructor;
rtc::Thread* PeerConnection::_signalingThread;
rtc::Thread* PeerConnection::_workerThread;


// PeerConnection constructor/destructor
PeerConnection::PeerConnection()
  : loop(uv_default_loop()) {
  _createOfferObserver = new rtc::RefCountedObject<CreateOfferObserver>(this);
  _createAnswerObserver = new rtc::RefCountedObject<CreateAnswerObserver>(this);
  _setLocalDescriptionObserver = new rtc::RefCountedObject<SetLocalDescriptionObserver>(this);
  _setRemoteDescriptionObserver = new rtc::RefCountedObject<SetRemoteDescriptionObserver>(this);

  webrtc::FileAudioDeviceFactory::SetFilenamesToUse("/devel/speech.pcm", "/deve/out.pcm");

  webrtc::PeerConnectionInterface::RTCConfiguration config;
  webrtc::PeerConnectionInterface::IceServer server;
  server.uri = "stun:stun.l.google.com:19302";
  config.servers.push_back(server);

  webrtc::FakeConstraints constraints;
  constraints.AddOptional(webrtc::MediaConstraintsInterface::kEnableDtlsSrtp, "true");

  _peer_connection_factory  = webrtc::CreatePeerConnectionFactory(
    _workerThread, 
    _signalingThread, 
    nullptr, 
    nullptr, 
    nullptr);
  ASSERT(_peer_connection_factory.get() != NULL);

  _peer_connection = _peer_connection_factory->CreatePeerConnection(
    config, 
    &constraints, 
    NULL, 
    NULL, 
    this);
  ASSERT(_peer_connection.get() != NULL);

  uv_mutex_init(&lock);
  uv_async_init(loop, &async, reinterpret_cast<uv_async_cb>(Run));

  async.data = this;
}

PeerConnection::~PeerConnection() {
  TRACE_CALL;
  _peer_connection = nullptr;
  _peer_connection_factory = nullptr;
  TRACE_END;
}

void PeerConnection::QueueEvent(AsyncEventType type, void* data) {
  TRACE_CALL;
  AsyncEvent evt;
  evt.type = type;
  evt.data = data;
  uv_mutex_lock(&lock);
  _events.push(evt);
  uv_mutex_unlock(&lock);

  uv_async_send(&async);
  TRACE_END;
}

void PeerConnection::Run(uv_async_t* handle, int status) {
  TRACE_CALL;

  Nan::HandleScope scope;

  PeerConnection* self = static_cast<PeerConnection*>(handle->data);
  TRACE_CALL_P((uintptr_t)self);
  Local<Object> pc = self->handle();
  bool do_shutdown = false;

 while (true) {
    uv_mutex_lock(&self->lock);
    bool empty = self->_events.empty();
    if (empty) {
      uv_mutex_unlock(&self->lock);
      break;
    }
    AsyncEvent evt = self->_events.front();
    self->_events.pop();
    uv_mutex_unlock(&self->lock);

    TRACE_U("evt.type", evt.type);
    if (PeerConnection::ERROR_EVENT & evt.type) {
      PeerConnection::ErrorEvent* data = static_cast<PeerConnection::ErrorEvent*>(evt.data);
      Local<Function> callback = Local<Function>::Cast(pc->Get(Nan::New("onerror").ToLocalChecked()));
      Local<Value> argv[1];
      argv[0] = Nan::Error(data->msg.c_str());
      Nan::MakeCallback(pc, callback, 1, argv);
    } else if (PeerConnection::SDP_EVENT & evt.type) {
      PeerConnection::SdpEvent* data = static_cast<PeerConnection::SdpEvent*>(evt.data);
      Local<Function> callback = Local<Function>::Cast(pc->Get(Nan::New("onsuccess").ToLocalChecked()));
      Local<Value> argv[1];
      argv[0] = Nan::New(data->desc.c_str()).ToLocalChecked();
      Nan::MakeCallback(pc, callback, 1, argv);
    } else if (PeerConnection::VOID_EVENT & evt.type) {
      Local<Function> callback = Local<Function>::Cast(pc->Get(Nan::New("onsuccess").ToLocalChecked()));
      Local<Value> argv[1];
      Nan::MakeCallback(pc, callback, 0, argv);
    } else if (PeerConnection::SIGNALING_STATE_CHANGE & evt.type) {
      PeerConnection::StateEvent* data = static_cast<PeerConnection::StateEvent*>(evt.data);
      Local<Function> callback = Local<Function>::Cast(pc->Get(Nan::New("onsignalingstatechange").ToLocalChecked()));
      if (!callback.IsEmpty()) {
        Local<Value> argv[1];
        argv[0] = Nan::New<Uint32>(data->state);
        Nan::MakeCallback(pc, callback, 1, argv);
      }
      if (webrtc::PeerConnectionInterface::kClosed == data->state) {
        do_shutdown = true;
      }
    } else if (PeerConnection::ICE_CONNECTION_STATE_CHANGE & evt.type) {
      PeerConnection::StateEvent* data = static_cast<PeerConnection::StateEvent*>(evt.data);
      Local<Function> callback = Local<Function>::Cast(pc->Get(Nan::New("oniceconnectionstatechange").ToLocalChecked()));
      if (!callback.IsEmpty()) {
        Local<Value> argv[1];
        argv[0] = Nan::New<Uint32>(data->state);
        Nan::MakeCallback(pc, callback, 1, argv);
      }
    } else if (PeerConnection::ICE_GATHERING_STATE_CHANGE & evt.type) {
      PeerConnection::StateEvent* data = static_cast<PeerConnection::StateEvent*>(evt.data);
      Local<Function> callback = Local<Function>::Cast(pc->Get(Nan::New("onicegatheringstatechange").ToLocalChecked()));
      if (!callback.IsEmpty()) {
        Local<Value> argv[1];
        argv[0] = Nan::New<Uint32>(data->state);
        Nan::MakeCallback(pc, callback, 1, argv);
      }
    } else if (PeerConnection::ICE_CANDIDATE & evt.type) {
      PeerConnection::IceEvent* data = static_cast<PeerConnection::IceEvent*>(evt.data);
      Local<Function> callback = Local<Function>::Cast(pc->Get(Nan::New("onicecandidate").ToLocalChecked()));
      if (!callback.IsEmpty()) {
        Local<Value> argv[3];
        argv[0] = Nan::New(data->candidate.c_str()).ToLocalChecked();
        argv[1] = Nan::New(data->sdpMid.c_str()).ToLocalChecked();
        argv[2] = Nan::New<Integer>(data->sdpMLineIndex);
        Nan::MakeCallback(pc, callback, 3, argv);
      }
    }
  }

  if (do_shutdown) {
    uv_close(reinterpret_cast<uv_handle_t*>(&self->async), nullptr);
  }

  TRACE_END;
}

// PeerConnectionObserver implementation
void PeerConnection::OnSignalingChange(
    webrtc::PeerConnectionInterface::SignalingState new_state) {
  TRACE_CALL;
  StateEvent* data = new StateEvent(static_cast<uint32_t>(new_state));
  QueueEvent(PeerConnection::SIGNALING_STATE_CHANGE, static_cast<void*>(data));
  TRACE_END;
}

void PeerConnection::OnAddStream(
    webrtc::PeerConnectionInterface::SignalingState new_state) {
  TRACE_CALL;
  TRACE_END;
}

void PeerConnection::OnRemoveStream(
    webrtc::PeerConnectionInterface::SignalingState new_state) {
  TRACE_CALL;
  TRACE_END;
}

void PeerConnection::OnRenegotiationNeeded() {
  TRACE_CALL;
  TRACE_END;
}

void PeerConnection::OnIceConnectionChange(
    webrtc::PeerConnectionInterface::IceConnectionState new_state) {
  TRACE_CALL;
  StateEvent* data = new StateEvent(static_cast<uint32_t>(new_state));
  QueueEvent(PeerConnection::ICE_CONNECTION_STATE_CHANGE, static_cast<void*>(data));
  TRACE_END;
}

void PeerConnection::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {
  TRACE_CALL;
  StateEvent* data = new StateEvent(static_cast<uint32_t>(new_state));
  QueueEvent(PeerConnection::ICE_GATHERING_STATE_CHANGE, static_cast<void*>(data));
  TRACE_END;
}

void PeerConnection::OnIceCandidate(const webrtc::IceCandidateInterface* candidate) {
  TRACE_CALL;
  PeerConnection::IceEvent* data = new PeerConnection::IceEvent(candidate);
  QueueEvent(PeerConnection::ICE_CANDIDATE, static_cast<void*>(data));
  TRACE_END;
}

void PeerConnection::OnIceConnectionReceivingChange(bool receiving) {
  TRACE_CALL;
  TRACE_END;
}

// NodeJS Wrapping
NAN_METHOD(PeerConnection::New) {
  TRACE_CALL;

  if (!info.IsConstructCall()) {
    return Nan::ThrowTypeError("Use the new operator to construct the PeerConnection.");
  }

  PeerConnection* obj = new PeerConnection();
  obj->Wrap(info.This());

  TRACE_END;
  info.GetReturnValue().Set(info.This());
}

NAN_METHOD(PeerConnection::CreateOffer) {
  TRACE_CALL;

  PeerConnection* self = Nan::ObjectWrap::Unwrap<PeerConnection>(info.This());

  self->_peer_connection->CreateOffer(self->_createOfferObserver, nullptr);

  TRACE_END;
  info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(PeerConnection::CreateAnswer) {
  TRACE_CALL;

  PeerConnection* self = Nan::ObjectWrap::Unwrap<PeerConnection>(info.This());

  self->_peer_connection->CreateAnswer(self->_createAnswerObserver, nullptr);

  TRACE_END;
  info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(PeerConnection::SetLocalDescription) {
  TRACE_CALL;

  PeerConnection* self = Nan::ObjectWrap::Unwrap<PeerConnection>(info.This());
  Local<Object> desc = Local<Object>::Cast(info[0]);
  String::Utf8Value _type(desc->Get(Nan::New("type").ToLocalChecked())->ToString());
  String::Utf8Value _sdp(desc->Get(Nan::New("sdp").ToLocalChecked())->ToString());

  std::string type = *_type;
  std::string sdp = *_sdp;
  std::cout << "local^^^^^^^^^^^^^ " << "type " << type << std::endl;
  std::cout << "local^^^^^^^^^^^^^ " << "sdp " << sdp << std::endl;


  webrtc::SdpParseError error;
  webrtc::SessionDescriptionInterface* sdi = webrtc::CreateSessionDescription(type, sdp, &error);

  self->_peer_connection->SetLocalDescription(self->_setLocalDescriptionObserver, sdi);

  TRACE_END;
  info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(PeerConnection::SetRemoteDescription) {
  TRACE_CALL;

  PeerConnection* self = Nan::ObjectWrap::Unwrap<PeerConnection>(info.This());
  Local<Object> desc = Local<Object>::Cast(info[0]);
  String::Utf8Value _type(desc->Get(Nan::New("type").ToLocalChecked())->ToString());
  String::Utf8Value _sdp(desc->Get(Nan::New("sdp").ToLocalChecked())->ToString());

  std::string type = *_type;
  std::string sdp = *_sdp;
  std::cout << "remote^^^^^^^^^^^^^ " << "type " << type << std::endl;
  std::cout << "remote^^^^^^^^^^^^^ " << "sdp " << sdp << std::endl;

  webrtc::SdpParseError error;
  webrtc::SessionDescriptionInterface* sdi = webrtc::CreateSessionDescription(type, sdp, &error);

  self->_peer_connection->SetRemoteDescription(self->_setRemoteDescriptionObserver, sdi);

  TRACE_END;
  info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(PeerConnection::UpdateIce) {
  TRACE_CALL;
  TRACE_END;
  info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(PeerConnection::AddIceCandidate) {
  TRACE_CALL;

  PeerConnection* self = Nan::ObjectWrap::Unwrap<PeerConnection>(info.This());
  Handle<Object> sdp = Handle<Object>::Cast(info[0]);

  String::Utf8Value _candidate(sdp->Get(Nan::New("candidate").ToLocalChecked())->ToString());
  std::string candidate = *_candidate;

  String::Utf8Value _sipMid(sdp->Get(Nan::New("sdpMid").ToLocalChecked())->ToString());
  std::string sdp_mid = *_sipMid;

  uint32_t sdp_mline_index = sdp->Get(Nan::New("sdpMLineIndex").ToLocalChecked())->Uint32Value();

  webrtc::SdpParseError sdpParseError;
  webrtc::IceCandidateInterface* ci = webrtc::CreateIceCandidate(sdp_mid, sdp_mline_index, candidate, &sdpParseError);

  if (self->_peer_connection->AddIceCandidate(ci)) {
    self->QueueEvent(PeerConnection::ADD_ICE_CANDIDATE_SUCCESS, static_cast<void*>(nullptr));
  } else {
    PeerConnection::ErrorEvent* data = new PeerConnection::ErrorEvent(std::string("Failed to set ICE candidate."));
    self->QueueEvent(PeerConnection::ADD_ICE_CANDIDATE_ERROR, static_cast<void*>(data));
  }

  TRACE_END;
  info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(PeerConnection::CreateDataChannel) {
  TRACE_CALL;

  PeerConnection* self = Nan::ObjectWrap::Unwrap<PeerConnection>(info.This());
  String::Utf8Value label(info[0]->ToString());
  Handle<Object> dataChannelDict = Handle<Object>::Cast(info[1]);

  webrtc::DataChannelInit dataChannelInit;
  if (dataChannelDict->Has(Nan::New("id").ToLocalChecked())) {
    Local<Value> value = dataChannelDict->Get(Nan::New("id").ToLocalChecked());
    if (value->IsInt32()) {
      dataChannelInit.id = value->Int32Value();
    }
  }
  if (dataChannelDict->Has(Nan::New("maxRetransmitTime").ToLocalChecked())) {
    Local<Value> value = dataChannelDict->Get(Nan::New("maxRetransmitTime").ToLocalChecked());
    if (value->IsInt32()) {
      dataChannelInit.maxRetransmitTime = value->Int32Value();
    }
  }
  if (dataChannelDict->Has(Nan::New("maxRetransmits").ToLocalChecked())) {
    Local<Value> value = dataChannelDict->Get(Nan::New("maxRetransmits").ToLocalChecked());
    if (value->IsInt32()) {
      dataChannelInit.maxRetransmits = value->Int32Value();
    }
  }
  if (dataChannelDict->Has(Nan::New("negotiated").ToLocalChecked())) {
    Local<Value> value = dataChannelDict->Get(Nan::New("negotiated").ToLocalChecked());
    if (value->IsBoolean()) {
      dataChannelInit.negotiated = value->BooleanValue();
    }
  }
  if (dataChannelDict->Has(Nan::New("ordered").ToLocalChecked())) {
    Local<Value> value = dataChannelDict->Get(Nan::New("ordered").ToLocalChecked());
    if (value->IsBoolean()) {
      dataChannelInit.ordered = value->BooleanValue();
    }
  }
  if (dataChannelDict->Has(Nan::New("protocol").ToLocalChecked())) {
    Local<Value> value = dataChannelDict->Get(Nan::New("protocol").ToLocalChecked());
    if (value->IsString()) {
      dataChannelInit.protocol = *String::Utf8Value(value->ToString());
    }
  }

  rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel_interface = self->_peer_connection->CreateDataChannel(*label, &dataChannelInit);
  DataChannelObserver* observer = new DataChannelObserver(data_channel_interface);

  Local<Value> cargv[1];
  cargv[0] = Nan::New<External>(static_cast<void*>(observer));
  Local<Value> dc = Nan::New(DataChannel::constructor)->NewInstance(1, cargv);

  TRACE_END;
  info.GetReturnValue().Set(dc);
}

NAN_METHOD(PeerConnection::AddStream) {
  TRACE_CALL;

  PeerConnection* self = Nan::ObjectWrap::Unwrap<PeerConnection>(info.This());
  node_webrtc::MediaStream* ms = Nan::ObjectWrap::Unwrap<MediaStream>( info[0]->ToObject() );

  self->_peer_connection->AddStream(ms->GetInterface());

  TRACE_END;
  info.GetReturnValue().Set(Nan::Undefined());

  self->QueueEvent(PeerConnection::VOID_EVENT, static_cast<void*>(nullptr));
}

NAN_METHOD(PeerConnection::Close) {
  TRACE_CALL;

  PeerConnection* self = Nan::ObjectWrap::Unwrap<PeerConnection>(info.This());
  self->_peer_connection->Close();

  TRACE_END;
  info.GetReturnValue().Set(Nan::Undefined());
}

NAN_GETTER(PeerConnection::GetLocalDescription) {
  TRACE_CALL;

  PeerConnection* self = Nan::ObjectWrap::Unwrap<PeerConnection>(info.Holder());
  const webrtc::SessionDescriptionInterface* sdi = self->_peer_connection->local_description();

  Handle<Value> value;
  if (nullptr == sdi) {
    value = Nan::Null();
  } else {
    std::string sdp;
    sdi->ToString(&sdp);
    value = Nan::New(sdp.c_str()).ToLocalChecked();
  }

  TRACE_END;
#if NODE_MAJOR_VERSION == 0
  info.GetReturnValue().Set(Nan::New(value));
#else
  info.GetReturnValue().Set(value);
#endif
}

NAN_SETTER(PeerConnection::ReadOnly) {
  INFO("PeerConnection::ReadOnly");
}

void PeerConnection::Init(rtc::Thread* signalingThread, rtc::Thread* workerThread, Handle<Object> exports) {
  _signalingThread = signalingThread;
  _workerThread = workerThread;

  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("PeerConnection").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  Nan::SetPrototypeMethod(tpl, "createOffer", CreateOffer);
  Nan::SetPrototypeMethod(tpl, "createAnswer", CreateAnswer);
  Nan::SetPrototypeMethod(tpl, "setLocalDescription", SetLocalDescription);
  Nan::SetPrototypeMethod(tpl, "setRemoteDescription", SetRemoteDescription);
  Nan::SetPrototypeMethod(tpl, "updateIce", UpdateIce);
  Nan::SetPrototypeMethod(tpl, "addIceCandidate", AddIceCandidate);

  Nan::SetPrototypeMethod(tpl, "addStream", AddStream);

  Nan::SetAccessor(tpl->InstanceTemplate(), Nan::New("localDescription").ToLocalChecked(), GetLocalDescription, ReadOnly);

  constructor.Reset(tpl->GetFunction());
  exports->Set(Nan::New("PeerConnection").ToLocalChecked(), tpl->GetFunction());
}

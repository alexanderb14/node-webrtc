#include <node_buffer.h>

#include <stdint.h>
#include <iostream>
#include <string>

#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/api/jsep.h"

#include "common.h"
#include "mediastream.h"
#include "mediastreamtrack.h"

using node_webrtc::MediaStream;
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

Nan::Persistent<Function> MediaStream::constructor;

MediaStream::MediaStream(webrtc::MediaStreamInterface* msi)
  : loop(uv_default_loop()),
    _internalMediaStream(msi) {
  uv_mutex_init(&lock);
  uv_async_init(loop, &async, reinterpret_cast<uv_async_cb>(Run));

  async.data = this;
}

MediaStream::~MediaStream()
{
    
}

NAN_METHOD(MediaStream::New) {
  TRACE_CALL;

  if (!info.IsConstructCall()) {
    return Nan::ThrowTypeError("Use the new operator to construct the PeerConnection.");
  }

  v8::Local<v8::External> _msi = v8::Local<v8::External>::Cast(info[0]);
  webrtc::MediaStreamInterface* msi = static_cast<webrtc::MediaStreamInterface*>(_msi->Value());

  MediaStream* obj = new MediaStream(msi);
  msi->RegisterObserver(obj);
  obj->Wrap( info.This() );

  TRACE_END;
  info.GetReturnValue().Set( info.This() );
}

void MediaStream::QueueEvent(AsyncEventType type, void* data)
{
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

void MediaStream::Run(uv_async_t* handle, int status)
{
  TRACE_CALL;

  Nan::HandleScope scope;

  MediaStream* self = static_cast<MediaStream*>(handle->data);
  TRACE_CALL_P((uintptr_t)self);
  //Local<Object> ms = self->handle();
  bool do_shutdown = false;

  while(true)
  {
    uv_mutex_lock(&self->lock);
    bool empty = self->_events.empty();
    if(empty)
    {
      uv_mutex_unlock(&self->lock);
      break;
    }
    AsyncEvent evt = self->_events.front();
    self->_events.pop();
    uv_mutex_unlock(&self->lock);

    TRACE_U("evt.type", evt.type);
    if(MediaStream::CHANGE & evt.type)
    {

    }
    
    if(MediaStream::ACTIVE & evt.type) 
    {

    } else if(MediaStream::INACTIVE & evt.type) 
    {

    }
    if(MediaStream::ADDTRACK & evt.type) 
    {

    }
    if(MediaStream::REMOVETRACK & evt.type) 
    {

    }
  }

  if (do_shutdown) {
    uv_close(reinterpret_cast<uv_handle_t*>(&self->async), nullptr);
  }

  TRACE_END;
}

void MediaStream::OnChanged()
{
  TRACE_CALL;
  QueueEvent(MediaStream::CHANGE, static_cast<void*>(NULL));
  TRACE_END;
}

NAN_GETTER(MediaStream::GetId) {
  TRACE_CALL;

  MediaStream* self = ObjectWrap::Unwrap<MediaStream>( info.Holder() );

  std::string label = self->_internalMediaStream->label();

  TRACE_END;
  info.GetReturnValue().Set(Nan::New(label.c_str()).ToLocalChecked());
}

NAN_SETTER(MediaStream::ReadOnly) {
  INFO("MediaStream::ReadOnly");
}

webrtc::MediaStreamInterface* MediaStream::GetInterface() {
    return _internalMediaStream;
}

void MediaStream::Init( Handle<Object> exports ) {
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("MediaStream").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  Nan::SetAccessor(tpl->InstanceTemplate(), Nan::New("id").ToLocalChecked(), GetId, ReadOnly);

  constructor.Reset(tpl->GetFunction());
  exports->Set(Nan::New("MediaStream").ToLocalChecked(), tpl->GetFunction());
}

// Hardware integration probe: emits a quiet test tone or measures only that
// frequency in WASAPI loopback. Never saves or prints captured system audio.
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>
using Microsoft::WRL::ComPtr;
static void check(HRESULT hr){if(FAILED(hr))throw std::runtime_error("WASAPI HRESULT "+std::to_string(uint32_t(hr)));}
int main(int argc,char** argv){
 try{
  check(CoInitializeEx(nullptr,COINIT_MULTITHREADED));
  ComPtr<IMMDeviceEnumerator> enumerator;ComPtr<IMMDevice> device;ComPtr<IAudioClient> client;
  check(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator)));
  check(enumerator->GetDefaultAudioEndpoint(eRender,eMultimedia,&device));
  check(device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(client.GetAddressOf())));
  WAVEFORMATEX* format=nullptr;check(client->GetMixFormat(&format));
  const bool floating=format->wFormatTag==WAVE_FORMAT_IEEE_FLOAT||(format->wFormatTag==WAVE_FORMAT_EXTENSIBLE&&reinterpret_cast<WAVEFORMATEXTENSIBLE*>(format)->SubFormat==KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
  if(!floating||format->wBitsPerSample!=32)throw std::runtime_error("Probe requires a float32 device mix format");
  const auto rate=format->nSamplesPerSec;const auto channels=format->nChannels;
  const bool tone=argc>1&&std::string(argv[1])=="--tone";
  check(client->Initialize(AUDCLNT_SHAREMODE_SHARED,tone?0:AUDCLNT_STREAMFLAGS_LOOPBACK,1000000,0,format,nullptr));
  CoTaskMemFree(format);
  const double step=2*3.141592653589793*997/rate;
  uint64_t index=0;double sine=0,cosine=0;
  if(tone){
   ComPtr<IAudioRenderClient> render;check(client->GetService(IID_PPV_ARGS(&render)));
   UINT32 capacity;check(client->GetBufferSize(&capacity));check(client->Start());
   std::cout<<"ready"<<std::endl;
   const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(90);
   while(std::chrono::steady_clock::now()<end){
    UINT32 padding;check(client->GetCurrentPadding(&padding));const auto frames=capacity-padding;
    if(frames){BYTE* data;check(render->GetBuffer(frames,&data));auto* out=reinterpret_cast<float*>(data);
     for(unsigned i=0;i<frames;++i,++index)for(unsigned ch=0;ch<channels;++ch)out[i*channels+ch]=float(0.02*std::sin(index*step));
     check(render->ReleaseBuffer(frames,0));}
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
   }
  }else{
   ComPtr<IAudioCaptureClient> capture;check(client->GetService(IID_PPV_ARGS(&capture)));check(client->Start());
   const auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(700);
   while(std::chrono::steady_clock::now()<end){
    UINT32 packet;check(capture->GetNextPacketSize(&packet));
    while(packet){BYTE* data;UINT32 frames;DWORD flags;check(capture->GetBuffer(&data,&frames,&flags,nullptr,nullptr));
     auto* samples=reinterpret_cast<float*>(data);
     for(unsigned i=0;i<frames;++i,++index){const double sample=(flags&AUDCLNT_BUFFERFLAGS_SILENT)?0:samples[i*channels];sine+=sample*std::sin(index*step);cosine+=sample*std::cos(index*step);}
     check(capture->ReleaseBuffer(frames));check(capture->GetNextPacketSize(&packet));}
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
   }
   if(!index)throw std::runtime_error("No WASAPI loopback frames");
   std::cout<<nlohmann::json{{"toneAmplitude",2*std::hypot(sine,cosine)/index},{"frames",index},{"sampleRate",rate}}.dump()<<std::endl;
  }
  check(client->Stop());return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
}

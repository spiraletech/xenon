#include "xenon/sovereign_runtime.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <mmsystem.h>
#pragma comment(lib,"winhttp.lib")
#pragma comment(lib,"winmm.lib")
#endif

namespace xenon {
namespace {
std::vector<std::byte> bytes(std::string_view s){std::vector<std::byte> out(s.size());for(std::size_t i=0;i<s.size();++i)out[i]=static_cast<std::byte>(s[i]);return out;}
std::string text(const std::vector<std::byte>& b){std::string s; s.resize(b.size()); for(std::size_t i=0;i<b.size();++i)s[i]=static_cast<char>(b[i]); return s;}
std::string json_escape(std::string_view s){std::string o;for(char c:s){if(c=='"'||c=='\\')o.push_back('\\');if(c=='\n'){o+="\\n";continue;}o.push_back(c);}return o;}
std::string field(std::string_view name,std::string_view value,std::string_view boundary){std::ostringstream o;o<<"--"<<boundary<<"\r\nContent-Disposition: form-data; name=\""<<name<<"\"\r\n\r\n"<<value<<"\r\n";return o.str();}
std::string find_json_string(const std::string& s,const std::string& key){const auto k='"'+key+'"';auto p=s.find(k);if(p==std::string::npos)return{};p=s.find(':',p+k.size());if(p==std::string::npos)return{};p=s.find('"',p);if(p==std::string::npos)return{};auto e=s.find('"',p+1);if(e==std::string::npos)return{};return s.substr(p+1,e-p-1);}
}

HttpResponse NativeHttpTransport::send(const HttpRequest& r){
#ifdef _WIN32
    URL_COMPONENTSA uc{};uc.dwStructSize=sizeof(uc);char host[256]{};char path[2048]{};uc.lpszHostName=host;uc.dwHostNameLength=255;uc.lpszUrlPath=path;uc.dwUrlPathLength=2047;
    if(!WinHttpCrackUrl(r.url.c_str(),0,0,reinterpret_cast<LPURL_COMPONENTS>(&uc))) throw std::runtime_error("L41 WinHTTP invalid URL");
    std::wstring whost(host,host+uc.dwHostNameLength),wpath(path,path+uc.dwUrlPathLength),wmethod(r.method.begin(),r.method.end());
    HINTERNET ses=WinHttpOpen(L"XENON/1.1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);if(!ses)throw std::runtime_error("L41 WinHTTP session failed");
    HINTERNET con=WinHttpConnect(ses,whost.c_str(),uc.nPort,0);if(!con){WinHttpCloseHandle(ses);throw std::runtime_error("L41 WinHTTP connect failed");}
    const DWORD flags=(uc.nScheme==INTERNET_SCHEME_HTTPS)?WINHTTP_FLAG_SECURE:0;HINTERNET req=WinHttpOpenRequest(con,wmethod.c_str(),wpath.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,flags);if(!req){WinHttpCloseHandle(con);WinHttpCloseHandle(ses);throw std::runtime_error("L41 WinHTTP request failed");}
    for(const auto& h:r.headers){std::wstring line(h.first.begin(),h.first.end());line+=L": ";line.append(h.second.begin(),h.second.end());WinHttpAddRequestHeaders(req,line.c_str(),static_cast<DWORD>(-1),WINHTTP_ADDREQ_FLAG_ADD|WINHTTP_ADDREQ_FLAG_REPLACE);}
    if(!r.content_type.empty()){std::wstring line=L"Content-Type: ";line.append(r.content_type.begin(),r.content_type.end());WinHttpAddRequestHeaders(req,line.c_str(),static_cast<DWORD>(-1),WINHTTP_ADDREQ_FLAG_ADD|WINHTTP_ADDREQ_FLAG_REPLACE);}
    auto* ptr=r.body.empty()?WINHTTP_NO_REQUEST_DATA:reinterpret_cast<LPVOID>(const_cast<std::byte*>(r.body.data()));DWORD len=static_cast<DWORD>(r.body.size());
    if(!WinHttpSendRequest(req,WINHTTP_NO_ADDITIONAL_HEADERS,0,ptr,len,len,0)||!WinHttpReceiveResponse(req,nullptr)){WinHttpCloseHandle(req);WinHttpCloseHandle(con);WinHttpCloseHandle(ses);throw std::runtime_error("L41 WinHTTP send failed");}
    DWORD status=0,sz=sizeof(status);WinHttpQueryHeaders(req,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&sz,WINHTTP_NO_HEADER_INDEX);
    HttpResponse out;out.status=static_cast<int>(status);for(;;){DWORD avail=0;if(!WinHttpQueryDataAvailable(req,&avail)||avail==0)break;auto old=out.body.size();out.body.resize(old+avail);DWORD read=0;if(!WinHttpReadData(req,out.body.data()+old,avail,&read))break;out.body.resize(old+read);}
    WinHttpCloseHandle(req);WinHttpCloseHandle(con);WinHttpCloseHandle(ses);return out;
#else
    (void)r;throw std::runtime_error("L41 NativeHttpTransport currently requires Windows WinHTTP");
#endif
}

AceStepBackend::AceStepBackend(std::shared_ptr<IHttpTransport> h,std::string u,std::string c):http_(std::move(h)),base_url_(std::move(u)),checkpoint_path_(std::move(c)){}
std::string_view AceStepBackend::name()const noexcept{return "ACE-Step";} ProviderCapabilities AceStepBackend::capabilities()const noexcept{return ProviderCapability::TextToInstrumental|ProviderCapability::QualityRole|ProviderCapability::LocalRuntime;} RuntimeType AceStepBackend::runtime_type()const noexcept{return RuntimeType::Local;}
BackendHealth AceStepBackend::health(){BackendHealth h{"ACE-Step",!checkpoint_path_.empty(),false,{}};try{auto r=http_->send({"GET",base_url_+"/health",{}, {},{}});h.reachable=r.status==200;h.detail=h.reachable?"/health OK":"HTTP "+std::to_string(r.status);}catch(const std::exception&e){h.detail=e.what();}return h;}
GenerationArtifact AceStepBackend::generate(const GenerationRequest&r,const std::filesystem::path& out){if(checkpoint_path_.empty())throw std::runtime_error("L41 ACE-Step checkpoint path not configured");std::filesystem::create_directories(out);auto target=out/("acestep_"+std::to_string(r.seed)+".wav");std::ostringstream j;j<<"{\"checkpoint_path\":\""<<json_escape(checkpoint_path_)<<"\",\"audio_duration\":"<<r.duration_seconds<<",\"prompt\":\""<<json_escape(r.prompt)<<"\",\"lyrics\":\"\",\"infer_step\":60,\"guidance_scale\":15.0,\"scheduler_type\":\"euler\",\"cfg_type\":\"apg\",\"omega_scale\":10.0,\"actual_seeds\":["<<r.seed<<"],\"guidance_interval\":0.5,\"guidance_interval_decay\":0.0,\"min_guidance_scale\":3.0,\"use_erg_tag\":true,\"use_erg_lyric\":false,\"use_erg_diffusion\":true,\"oss_steps\":[],\"output_path\":\""<<json_escape(target.string())<<"\"}";HttpRequest q{"POST",base_url_+"/generate",{{"Accept","application/json"}},bytes(j.str()),"application/json"};auto resp=http_->send(q);if(resp.status!=200)throw std::runtime_error("L41 ACE-Step generation HTTP "+std::to_string(resp.status));auto body=text(resp.body);auto p=find_json_string(body,"output_path");if(p.empty())p=target.string();GenerationArtifact a;a.audio_path=p;a.backend_name="ACE-Step";a.resolved_seed=r.seed;if(!std::filesystem::exists(a.audio_path))throw std::runtime_error("L41 ACE-Step reported success but output file is missing");return a;}

StableAudioBackend::StableAudioBackend(std::shared_ptr<IHttpTransport>h,std::string key,std::string model):http_(std::move(h)),api_key_(std::move(key)),model_(std::move(model)){}
std::string_view StableAudioBackend::name()const noexcept{return "Stable Audio";} ProviderCapabilities StableAudioBackend::capabilities()const noexcept{return ProviderCapability::TextToInstrumental|ProviderCapability::QualityRole;} RuntimeType StableAudioBackend::runtime_type()const noexcept{return RuntimeType::RemoteApi;} BackendHealth StableAudioBackend::health()const{return {"Stable Audio",!api_key_.empty(),false,api_key_.empty()?"STABILITY_API_KEY missing":"configured; reachability checked on request"};}
GenerationArtifact StableAudioBackend::generate(const GenerationRequest&r,const std::filesystem::path& out){if(api_key_.empty())throw std::runtime_error("L41 STABILITY_API_KEY is not configured");std::filesystem::create_directories(out);const std::string boundary="xenon-l41-boundary";std::string body;body+=field("none","",boundary);body+=field("prompt",r.prompt,boundary);body+=field("output_format","wav",boundary);body+=field("duration",std::to_string(r.duration_seconds),boundary);body+=field("model",model_,boundary);body+=field("seed",std::to_string(r.seed),boundary);body+="--"+boundary+"--\r\n";HttpRequest q{"POST","https://api.stability.ai/v2beta/audio/stable-audio-2/text-to-audio",{{"Authorization","Bearer "+api_key_},{"Accept","audio/*"},{"stability-client-id","xenon-music-os"}},bytes(body),"multipart/form-data; boundary="+boundary};auto resp=http_->send(q);if(resp.status!=200)throw std::runtime_error("L41 Stable Audio HTTP "+std::to_string(resp.status)+": "+text(resp.body));auto p=out/("stable_audio_"+std::to_string(r.seed)+".wav");std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(resp.body.data()),static_cast<std::streamsize>(resp.body.size()));if(!f)throw std::runtime_error("L41 failed to write Stable Audio result");GenerationArtifact a;a.audio_path=p;a.backend_name="Stable Audio";a.resolved_seed=r.seed;return a;}

std::vector<MidiEndpoint> NativeMidiRuntime::enumerate() const{std::vector<MidiEndpoint> out;
#ifdef _WIN32
 for(UINT i=0;i<midiInGetNumDevs();++i){MIDIINCAPSA c{};if(midiInGetDevCapsA(i,&c,sizeof(c))==MMSYSERR_NOERROR)out.push_back({i,c.szPname,true,false});}
 for(UINT i=0;i<midiOutGetNumDevs();++i){MIDIOUTCAPSA c{};if(midiOutGetDevCapsA(i,&c,sizeof(c))==MMSYSERR_NOERROR)out.push_back({i,c.szPname,false,true});}
#endif
 return out;}
Vst3SdkStatus vst3_sdk_status() noexcept{
#ifdef XENON_VST3_SDK_ENABLED
 return {true,true,"Steinberg VST3 SDK integration enabled"};
#else
 return {false,false,"Steinberg VST3 SDK not vendored/enabled; existing L33 host remains abstraction + factory-symbol validation only"};
#endif
}
SovereignRuntime::SovereignRuntime(std::shared_ptr<IHttpTransport>h,std::string u,std::string c,std::string k):http_(std::move(h)),ace_url_(std::move(u)),ace_checkpoint_(std::move(c)),stability_key_(std::move(k)){}
SovereignRuntimeDiagnostics SovereignRuntime::diagnose(){AceStepBackend a(http_,ace_url_,ace_checkpoint_);StableAudioBackend s(http_,stability_key_);SovereignRuntimeDiagnostics d;d.ace_step=a.health();d.stable_audio=s.health();d.midi=NativeMidiRuntime{}.enumerate();d.vst3=vst3_sdk_status();return d;}
std::unique_ptr<IModelBackend> SovereignRuntime::make_ace_step()const{return std::make_unique<AceStepBackend>(http_,ace_url_,ace_checkpoint_);} std::unique_ptr<IModelBackend> SovereignRuntime::make_stable_audio()const{return std::make_unique<StableAudioBackend>(http_,stability_key_);}

} // namespace xenon

#include "xenon/etherhue_dsp.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

static void require(bool c, const char* m){ if(!c) throw std::runtime_error(m); }

int main(){
 try {
  xenon::EtherHueDSP dsp;
  dsp.reset(44100);
  xenon::SynesthesiaState calm;
  calm.warmth=.75; calm.brightness=.25; calm.texture=.2; calm.tension=.2; calm.motion=.2; calm.psychedelic_index=.15; calm.density=.35; calm.luminance=.4;
  auto a=dsp.update(calm);
  xenon::SynesthesiaState intense=calm;
  intense.warmth=.25; intense.brightness=.85; intense.texture=.9; intense.tension=.9; intense.motion=.85; intense.psychedelic_index=.9; intense.density=.8; intense.luminance=.8;
  auto target=dsp.map(intense); auto b=dsp.update(intense);
  require(target.saturation<=dsp.policy().max_saturation,"saturation out of bounds");
  require(target.stereo_width>=dsp.policy().min_stereo_width && target.stereo_width<=dsp.policy().max_stereo_width,"width out of bounds");
  require(std::abs(b.saturation-a.saturation) < std::abs(target.saturation-a.saturation)+1e-9,"smoothing failed");
  std::vector<float> audio(4410*2);
  for(size_t i=0;i<audio.size()/2;++i){ float s=.3f*std::sin(2.0*3.141592653589793*220.0*i/44100.0); audio[i*2]=s; audio[i*2+1]=s*.8f; }
  auto before=audio; dsp.process_interleaved_stereo(audio);
  bool changed=false; for(size_t i=0;i<audio.size();++i){ require(std::isfinite(audio[i]),"non-finite output"); require(audio[i]>=-1.f&&audio[i]<=1.f,"output unclamped"); if(std::abs(audio[i]-before[i])>1e-6f) changed=true; }
  require(changed,"DSP did not alter signal");
  bool odd=false; try { std::vector<float> x(3); dsp.process_interleaved_stereo(x); } catch(const std::invalid_argument&){ odd=true; } require(odd,"odd stereo buffer accepted");
  bool bad=false; try { xenon::EtherHuePolicy p; p.smoothing=0; dsp.set_policy(p); } catch(const std::invalid_argument&){ bad=true; } require(bad,"invalid policy accepted");
  std::cout<<"L29 ETHERHUE DSP smoke passed\n"; return 0;
 } catch(const std::exception& e){ std::cerr<<"L29 smoke failed: "<<e.what()<<'\n'; return 1; }
}

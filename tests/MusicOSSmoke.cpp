#include "xenon/backends/native_preview_backend.hpp"
#include "xenon/music_os.hpp"
#include <filesystem>
#include <iostream>
#include <memory>
int main(){namespace fs=std::filesystem; xenon::ModelRouter router;router.add_provider(std::make_unique<xenon::NativePreviewBackend>(),100);xenon::MusicOS os(std::move(router));
 auto before=os.state();if(before.project_open||before.model_provider_count!=1)return 1;os.open_project("l40-music-os");os.memory().remember_preference("dark spacious drums",.9);os.devices().connect(xenon::TrinityDeviceKind::SigilGuitar);os.devices().connect(xenon::TrinityDeviceKind::GlyphPad);os.devices().connect(xenon::TrinityDeviceKind::ThreadDeck);
 xenon::GenerationRequest r;r.prompt="L40 unified music os instrumental";r.duration_seconds=1.0;r.bpm=100;r.key="C minor";r.seed=4040;auto out=fs::temp_directory_path()/"xenon_l40_music_os";fs::remove_all(out);auto g=os.create(r,out);if(!fs::exists(g.artifact.audio_path)||g.dna.fingerprint.empty())return 2;auto a=os.analyze_audio(g.artifact.audio_path);if(a.frames.empty())return 3;
 auto ref=os.session().add_reference("generated",g.artifact.audio_path.string());auto prompt=os.session().add_prompt(ref,r.prompt);auto dna=os.session().add_ether_dna(prompt,g.dna.fingerprint);auto gen=os.session().add_generation(dna,g.artifact.audio_path.string(),g.dna.fingerprint);if(gen.empty())return 4;
 auto state=os.state();if(!state.project_open||state.project_id!="l40-music-os"||state.capabilities.size()<10)return 5;auto manifest=os.manifest();if(manifest.find("XENON_MUSIC_OS|1")==std::string::npos||manifest.find("trinity")==std::string::npos||manifest.find("mix-master")==std::string::npos)return 6;
 xenon::DeviceGesture gesture;gesture.device="Sigil Guitar";gesture.x=.5;gesture.y=.5;gesture.pressure=.8;gesture.gate=true;auto ev=os.devices().ingest(xenon::TrinityDeviceKind::SigilGuitar,gesture);if(ev.midi.empty())return 7;
 bool guarded=false;try{xenon::AutonomousProducerGoal goal;goal.project_id="l40";goal.goal="test";(void)os.produce(goal,out);}catch(const std::runtime_error&){guarded=true;}if(!guarded)return 8;
 fs::remove_all(out);std::cout<<"XENON L40 Music OS unified surface verified\n";return 0;}

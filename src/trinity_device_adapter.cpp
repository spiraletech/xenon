#include "xenon/trinity_device_adapter.hpp"

#include <algorithm>
#include <stdexcept>

namespace xenon {

MidiTrinityAdapter::MidiTrinityAdapter(TrinityDeviceKind kind,unsigned int index)
    : kind_(kind),input_index_(index) {}
MidiTrinityAdapter::~MidiTrinityAdapter(){stop();}
TrinityDeviceKind MidiTrinityAdapter::kind() const noexcept{return kind_;}
void MidiTrinityAdapter::start(){input_.open(input_index_);}
void MidiTrinityAdapter::stop() noexcept{input_.close();}
bool MidiTrinityAdapter::active() const noexcept{return input_.is_open();}

DeviceGesture MidiTrinityAdapter::map_message(TrinityDeviceKind kind,const NativeMidiMessage& m) {
    DeviceGesture g;
    switch(kind){
        case TrinityDeviceKind::SigilGuitar:g.device="Sigil Guitar";break;
        case TrinityDeviceKind::GlyphPad:g.device="GlyphPad";break;
        case TrinityDeviceKind::ThreadDeck:g.device="ThreadDeck";break;
    }
    const auto type=static_cast<unsigned char>(m.status&0xF0u);
    if(type==0x90u||type==0x80u){
        g.x=std::clamp(static_cast<double>(m.data1)/127.0,0.0,1.0);
        g.pressure=std::clamp(static_cast<double>(m.data2)/127.0,0.0,1.0);
        g.y=0.5;
        g.gate=(type==0x90u&&m.data2>0);
    }else if(type==0xB0u){
        g.x=std::clamp(static_cast<double>(m.data1)/127.0,0.0,1.0);
        g.y=std::clamp(static_cast<double>(m.data2)/127.0,0.0,1.0);
        g.pressure=g.y;
        g.gate=true;
    }else if(type==0xE0u){
        const int bend=(static_cast<int>(m.data2)<<7)|m.data1;
        g.x=0.5;
        g.y=std::clamp(static_cast<double>(bend)/16383.0,0.0,1.0);
        g.pressure=0.5;
        g.gate=true;
    }else{
        throw std::invalid_argument("L41 unsupported physical Trinity MIDI message");
    }
    return g;
}

std::size_t MidiTrinityAdapter::pump(TrinityDeviceBus& bus){
    if(!active())throw std::runtime_error("L41 physical Trinity adapter is not active");
    if(!bus.connected(kind_))bus.connect(kind_);
    std::size_t count=0;
    for(const auto&m:input_.drain()){
        try{auto g=map_message(kind_,m);(void)bus.ingest(kind_,g);++count;}catch(const std::invalid_argument&){/* ignore non-performance MIDI */}
    }
    return count;
}

} // namespace xenon

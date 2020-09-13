#include "MidiDevice.h"

#include "ops/values.h"
#include "ops/strings.h"

#include "types/Color.h"
#include "types/Vector.h"

#include "Context.h"

//status bytes
const unsigned char NOTE_OFF = 0x80;
const unsigned char NOTE_ON = 0x90;
const unsigned char KEY_PRESSURE = 0xA0;
const unsigned char CONTROLLER_CHANGE = 0xB0;
const unsigned char PROGRAM_CHANGE = 0xC0;
const unsigned char CHANNEL_PRESSURE = 0xD0;
const unsigned char PITCH_BEND = 0xE0;
const unsigned char SYSTEM_EXCLUSIVE = 0xF0;
const unsigned char SONG_POSITION = 0xF2;
const unsigned char SONG_SELECT = 0xF3;
const unsigned char TUNE_REQUEST = 0xF6;
const unsigned char END_OF_SYSEX = 0xF7;
const unsigned char TIMING_TICK = 0xF8;
const unsigned char START_SONG = 0xFA;
const unsigned char CONTINUE_SONG = 0xFB;
const unsigned char STOP_SONG = 0xFC;
const unsigned char ACTIVE_SENSING = 0xFE;
const unsigned char SYSTEM_RESET = 0xFF;

MidiDevice::MidiDevice(void* _ctx, const std::string& _name, size_t _midiPort) {
    type = DEVICE_MIDI;
    ctx = _ctx;
    name = _name;
    midiPort = _midiPort;

    try {
        midiIn = new RtMidiIn();
    } catch(RtMidiError &error) {
        error.printMessage();
    }

    midiIn->openPort(_midiPort, "MidiGyver");
    midiIn->setCallback(onMidi, this);
    midiIn->ignoreTypes(false, false, true);

    // midiName = midiIn->getPortName(_midiPort);
    // stringReplace(midiName, '_');

    try {
        midiOut = new RtMidiOut();
        midiOut->openPort(_midiPort, "MidiGyver");
    }
    catch(RtMidiError &error) {
        error.printMessage();
    }
}

MidiDevice::~MidiDevice() {
    delete this->midiIn;
}

void MidiDevice::send_CC(size_t _key, size_t _value) {
    std::vector<unsigned char> msg;
    msg.push_back( CONTROLLER_CHANGE );
    msg.push_back( _key );
    msg.push_back( _value );
    midiOut->sendMessage( &msg );   
}

void extractHeader(std::vector<unsigned char>* _message, std::string& _type, int& _bytes, unsigned char& _channel) {
    unsigned char status = 0;
    int j = 0;

    if ((_message->at(0) & 0xf0) != 0xf0) {
        _channel = _message->at(0) & 0x0f;
        status = _message->at(0) & 0xf0;
    }
    else {
        _channel = 0;
        status = _message->at(0);
    }

    switch(status) {
        case NOTE_OFF:
            _type = "note_off";
            _bytes = 2;
            break;

        case NOTE_ON:
            _type = "note_on";
            _bytes = 2;
            break;

        case KEY_PRESSURE:
            _type = "key_pressure";
            _bytes = 2;
            break;

        case CONTROLLER_CHANGE:
            _type = "controller_change";
            _bytes = 2;
            break;

        case PROGRAM_CHANGE:
            _type = "program_change";
            _bytes = 2;
            break;

        case CHANNEL_PRESSURE:
            _type = "channel_pressure";
            _bytes = 2;
            break;

        case PITCH_BEND:
            _type = "pitch_bend";
            _bytes = 2;
            break;

        case SYSTEM_EXCLUSIVE:
            if(_message->size() == 6) {
                unsigned int type = _message->at(4);
                if (type == 1)
                    _type = "mmc_stop";
                else if(type == 2)
                    _type = "mmc_play";
                else if(type == 4)
                    _type = "mmc_fast_forward";
                else if(type == 5)
                    _type = "mmc_rewind";
                else if(type == 6)
                    _type = "mmc_record";
                else if(type == 9)
                    _type = "mmc_pause";
            }
            _bytes = 0;
            break;

        case SONG_POSITION:
            _type = "song_position";
            _bytes = 2;
            break;

        case SONG_SELECT:
            _type = "song_select";
            _bytes = 2;
            break;

        case TUNE_REQUEST:
            _type = "tune_request";
            _bytes = 2;
            break;

        case TIMING_TICK:
            _type = "timing_tick";
            _bytes = 0;
            break;

        case START_SONG:
            _type = "start_song";
            _bytes = 0;
            break;

        case CONTINUE_SONG:
            _type = "continue_song";
            _bytes = 0;
            break;

        case STOP_SONG:
            _type = "stop_song";
            _bytes = 0;
            break;

        default:
            _type = "";
            _bytes = 0;
            break;
    }

    if (status == NOTE_ON && _message->at(2) == 0) {
        _type = "note_off";
    }
}

std::vector<std::string> MidiDevice::getInputPorts() {
    std::vector<std::string> devices;

    RtMidiIn* midiIn = new RtMidiIn();
    unsigned int nPorts = midiIn->getPortCount();

    for(unsigned int i = 0; i < nPorts; i++) {
        std::string name = midiIn->getPortName(i);
        stringReplace(name, '_');
        devices.push_back( name );
    }

    delete midiIn;

    return devices;
}

void MidiDevice::onMidi(double _deltatime, std::vector<unsigned char>* _message, void* _userData) {
    unsigned int nBytes = 0;
    try {
        nBytes = _message->size();
    } 
    catch(RtMidiError &error) {
        error.printMessage();
        exit(EXIT_FAILURE);
    }

    MidiDevice *device = static_cast<MidiDevice*>(_userData);
    Context *context = static_cast<Context*>(device->ctx);

    std::string type;
    int bytes;
    unsigned char channel;
    extractHeader(_message, type, bytes, channel);

    size_t key = _message->at(1);
    float value = (float)_message->at(2);

    context->configMutex.lock();
    // std::cout << device->name << " Channel: " << channel << " Key: " << key << " Value:" << value << std::endl;
    if (context->doKeyExist(device->name, key)) {
        YAML::Node node = context->getKeyNode(device->name, key);
        if (context->shapeKeyValue(node, device->name, type, key, &value)) {
            context->mapKeyValue(node, device->name, key, value);
        }
    }
    context->configMutex.unlock();

}



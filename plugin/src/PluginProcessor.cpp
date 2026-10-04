#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "MidiFileWriter.h"

#include <cmath>
#include <map>

MidiSpaceAudioProcessor::MidiSpaceAudioProcessor()
    : juce::AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)) {
    ensureServerRunning();
}

MidiSpaceAudioProcessor::~MidiSpaceAudioProcessor() {
    stopServer();
}

// Find the bundled server executable, if present.
static juce::File findServerExe() {
    const juce::File candidates[] = {
        juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
            .getChildFile("MidiSpace").getChildFile("server").getChildFile("midispace_server.exe"),
        juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("MidiSpace").getChildFile("server").getChildFile("midispace_server.exe"),
        juce::File::getSpecialLocation(juce::File::currentApplicationFile).getSiblingFile("midispace_server.exe"),
    };
    for (const auto& f : candidates)
        if (f.existsAsFile())
            return f;
    return {};
}

void MidiSpaceAudioProcessor::ensureServerRunning() {
    if (serverChecked_)
        return;
    serverChecked_ = true;

    // If the server is already reachable (e.g. a dev instance), do nothing.
    {
        juce::URL url("http://127.0.0.1:8765/health");
        auto stream = url.createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(500));
        if (stream != nullptr)
            return;
    }

    const auto exe = findServerExe();
    if (exe == juce::File())
        return;   // not bundled; user runs the server manually

    auto* cp = new juce::ChildProcess();
    // Don't capture stdout/stderr: it runs for the whole session.
    if (cp->start(exe.getFullPathName(), 0)) {
        serverProcess_.reset(cp);
    } else {
        delete cp;
    }
}

void MidiSpaceAudioProcessor::stopServer() {
    // PyInstaller exe spawns a child that holds the actual server; kill the
    // whole process tree so no orphan is left behind.
    juce::ChildProcess killer;
    killer.start("taskkill /IM midispace_server.exe /T /F",
                 juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr);
    killer.waitForProcessToFinish(3000);

    if (serverProcess_) {
        serverProcess_->kill();
        serverProcess_ = nullptr;
    }
}

void MidiSpaceAudioProcessor::prepareToPlay(double, int) {}

void MidiSpaceAudioProcessor::releaseResources() {}

bool MidiSpaceAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto main = layouts.getMainOutputChannelSet();
    return main == juce::AudioChannelSet::mono() || main == juce::AudioChannelSet::stereo();
}

double MidiSpaceAudioProcessor::getBpm() const {
    if (auto* ph = getPlayHead()) {
        if (auto pos = ph->getPosition()) {
            if (auto bpm = pos->getBpm())
                return *bpm;
        }
    }
    return 120.0;
}

void MidiSpaceAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    for (int i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear(i, 0, numSamples);

    const double sr = getSampleRate();
    const double blockDur = sr > 0.0 ? numSamples / sr : 0.0;

    // Follow the host transport.  FL sends an all-notes-off / all-sound-off
    // (CC 120/123) to instruments when its transport stops; treat that as a
    // request to stop our loop too.  This is far more reliable than reading
    // the playhead's isPlaying flag, which FL leaves ambiguous after a stop.
    bool hostRequestedStop = false;
    for (const auto meta : midi) {
        const auto m = meta.getMessage();
        if (m.isAllNotesOff() || m.isAllSoundOff()) {
            hostRequestedStop = true;
            break;
        }
    }
    if (hostRequestedStop)
        stop();

    // Capture incoming MIDI (FL piano roll).
    if (capturing_ && sr > 0.0) {
        juce::ScopedLock cl(captureLock_);
        // Re-read the host tempo each block: FL only reports it reliably while
        // the transport is running, so a snapshot taken when Capture was first
        // clicked would be stale (default 120).  Refresh both the mapping BPM
        // and the 2-bar window length.
        captureBpm_ = getBpm();
        captureMaxSeconds_ = 8.0 * 60.0 / captureBpm_;

        for (const auto meta : midi) {
            const auto m = meta.getMessage();
            if (m.isNoteOn() || m.isNoteOff()) {
                const double t = captureTime_ + meta.samplePosition / sr;
                captureEvents_.push_back({ t, m.isNoteOn(), m.getNoteNumber() });
                if (m.isNoteOn() && firstNoteTime_ < 0.0)
                    firstNoteTime_ = t;
            }
        }
        captureTime_ += blockDur;
        // Auto-stop exactly 2 bars after the first note.
        if (firstNoteTime_ >= 0.0 && captureTime_ >= firstNoteTime_ + captureMaxSeconds_)
            capturing_ = false;
    }

    // Output MIDI = pass-through incoming MIDI (FL piano roll) + generated loop.
    juce::MidiBuffer outMidi;

    // Thru: forward incoming MIDI so the piano roll can be auditioned through
    // the instrument routed after MidiSpace (e.g. Analog V).
    outMidi.addEvents(midi, 0, numSamples, 0);

    {
        juce::ScopedLock sl(lock_);

        if (sendAllNotesOff_.exchange(false)) {
            outMidi.addEvent(juce::MidiMessage::allNotesOff(1), 0);
            activeNotes_.clear();
        }

        if (playing_ && loopLenSec_ > 0.0 && sr > 0.0 && midiSeq_.getNumEvents() > 0) {
            double switchAtSec = -1.0;

            // Emit the current loop. While a switch is pending, do NOT wrap
            // around, so the currently-sounding note can actually finish and
            // free up activeNotes_ (a seamless arpeggio would otherwise keep
            // re-triggering note-ons at the loop boundary forever).
            for (int i = 0; i < midiSeq_.getNumEvents(); ++i) {
                auto* e = midiSeq_.getEventPointer(i);
                double lt = e->message.getTimeStamp() - posSec_;

                if (hasPending_) {
                    if (lt < 0.0)
                        continue;   // past event; skip while switching
                } else if (lt < 0.0) {
                    lt += loopLenSec_;
                }

                if (lt >= 0.0 && lt < blockDur) {
                    outMidi.addEvent(e->message, static_cast<int>(lt * sr));
                    if (e->message.isNoteOn()) {
                        activeNotes_.insert(e->message.getNoteNumber());
                    } else if (e->message.isNoteOff()) {
                        activeNotes_.erase(e->message.getNoteNumber());
                        if (hasPending_ && activeNotes_.empty()) {
                            switchAtSec = lt;
                            break;   // stop the old loop here
                        }
                    }
                }
            }

            if (switchAtSec >= 0.0) {
                // Swap to the pending melody and continue from the switch point.
                midiSeq_ = pendingSeq_;
                pendingSeq_.clear();
                loopLenSec_ = pendingLoopLenSec_;
                posSec_ = 0.0;
                hasPending_ = false;
                playing_ = true;

                const double remainingSec = blockDur - switchAtSec;
                for (int i = 0; i < midiSeq_.getNumEvents(); ++i) {
                    auto* e = midiSeq_.getEventPointer(i);
                    const double t = e->message.getTimeStamp();
                    if (t >= 0.0 && t < remainingSec) {
                        outMidi.addEvent(e->message, static_cast<int>((switchAtSec + t) * sr));
                        if (e->message.isNoteOn())
                            activeNotes_.insert(e->message.getNoteNumber());
                        else if (e->message.isNoteOff())
                            activeNotes_.erase(e->message.getNoteNumber());
                    }
                }
                posSec_ = remainingSec;
            } else {
                posSec_ += blockDur;
                if (posSec_ >= loopLenSec_)
                    posSec_ -= loopLenSec_;
            }
        }
    }

    midi.clear();
    midi.addEvents(outMidi, 0, numSamples, 0);
}

juce::AudioProcessorEditor* MidiSpaceAudioProcessor::createEditor() {
    return new MidiSpaceAudioProcessorEditor(*this);
}

void MidiSpaceAudioProcessor::setCurrentProgram(int) {}
const juce::String MidiSpaceAudioProcessor::getProgramName(int) { return {}; }
void MidiSpaceAudioProcessor::changeProgramName(int, const juce::String&) {}
void MidiSpaceAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    juce::MemoryOutputStream mo(destData, true);
    mo.writeString(nodeStateXml_);
}

void MidiSpaceAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (data == nullptr || sizeInBytes <= 0)
        return;
    juce::MemoryInputStream mi(data, static_cast<size_t>(sizeInBytes), false);
    nodeStateXml_ = mi.readString();
}

void MidiSpaceAudioProcessor::setResult(const std::vector<midispace::NoteEvent>& notes, double bpm) {
    juce::ScopedLock sl(lock_);

    // Build the new loop into pendingSeq_.
    juce::MidiMessageSequence seq;
    const double spq = 60.0 / bpm;
    for (const auto& n : notes) {
        seq.addEvent(juce::MidiMessage::noteOn(1, n.pitch, static_cast<uint8_t>(80)),
                     n.startQuarter * spq);
        seq.addEvent(juce::MidiMessage::noteOff(1, n.pitch), n.endQuarter * spq);
    }
    seq.updateMatchedPairs();

    bpm_ = bpm;
    pendingSeq_ = std::move(seq);
    pendingLoopLenSec_ = 8.0 * spq;

    if (activeNotes_.empty()) {
        // Nothing sounding now: start the new loop immediately.
        midiSeq_ = pendingSeq_;
        pendingSeq_.clear();
        loopLenSec_ = pendingLoopLenSec_;
        posSec_ = 0.0;
        hasPending_ = false;
        playing_ = true;
    } else {
        // A note is sounding: wait for it to end before swapping.
        hasPending_ = true;
    }
}

void MidiSpaceAudioProcessor::play() {
    playing_ = true;
}

void MidiSpaceAudioProcessor::stop() {
    juce::ScopedLock sl(lock_);
    playing_ = false;
    hasPending_ = false;
    pendingSeq_.clear();
    activeNotes_.clear();
    sendAllNotesOff_ = true;
}

bool MidiSpaceAudioProcessor::isPlaying() const {
    return playing_;
}

void MidiSpaceAudioProcessor::startCapture() {
    juce::ScopedLock cl(captureLock_);
    captureEvents_.clear();
    captureTime_ = 0.0;
    firstNoteTime_ = -1.0;
    captureBpm_ = getBpm();
    captureMaxSeconds_ = 8.0 * 60.0 / captureBpm_;   // 2 bars at host tempo
    capturing_ = true;
}

std::vector<midispace::NoteEvent> MidiSpaceAudioProcessor::stopCapture() {
    std::vector<midispace::NoteEvent> out;
    juce::ScopedLock cl(captureLock_);
    capturing_ = false;

    // Use the BPM observed during capture (refresh happened in processBlock),
    // not a fresh getBpm() which may be stale now that playback has stopped.
    const double spq = 60.0 / captureBpm_;
    const double t0 = firstNoteTime_ >= 0.0 ? firstNoteTime_ : 0.0;   // align to first note
    std::map<int, double> onTimes;
    for (const auto& e : captureEvents_) {
        const double t = e.timeSec - t0;
        if (t < 0.0)
            continue;
        if (e.noteOn) {
            onTimes[e.pitch] = t;
        } else {
            const auto it = onTimes.find(e.pitch);
            if (it != onTimes.end()) {
                const float sq = static_cast<float>(it->second / spq);
                const float eq = static_cast<float>(t / spq);
                if (eq > sq && sq < 8.0f)
                    out.push_back({ e.pitch, sq, std::min(eq, 8.0f) });
                onTimes.erase(it);
            }
        }
    }
    captureEvents_.clear();
    firstNoteTime_ = -1.0;
    return out;
}

bool MidiSpaceAudioProcessor::saveResultToFile(const juce::File& file) {
    juce::ScopedLock sl(lock_);
    if (midiSeq_.getNumEvents() == 0)
        return false;

    std::vector<midispace::NoteEvent> notes;
    const double spq = 60.0 / bpm_;
    for (int i = 0; i < midiSeq_.getNumEvents(); ++i) {
        const auto& m = midiSeq_.getEventPointer(i)->message;
        if (m.isNoteOn()) {
            const double startQ = m.getTimeStamp() / spq;
            double endQ = startQ + 1.0;
            for (int j = i + 1; j < midiSeq_.getNumEvents(); ++j) {
                const auto& m2 = midiSeq_.getEventPointer(j)->message;
                if (m2.isNoteOff() && m2.getNoteNumber() == m.getNoteNumber()) {
                    endQ = m2.getTimeStamp() / spq;
                    break;
                }
            }
            notes.push_back({ m.getNoteNumber(), static_cast<float>(startQ), static_cast<float>(endQ) });
        }
    }
    return midispace::writeMidiFile(notes, file, bpm_);
}

// Plugin entry point — the host (standalone / DAW) calls this to create the processor.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new MidiSpaceAudioProcessor();
}

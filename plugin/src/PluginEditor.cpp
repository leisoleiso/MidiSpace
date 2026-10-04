#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "MidiFileWriter.h"
#include "Arpeggio.h"

namespace {

// motif  -> "pitch,start,end; pitch,start,end; ..."
static juce::String motifToString(const std::vector<midispace::NoteEvent>& motif) {
    juce::StringArray parts;
    for (const auto& e : motif)
        parts.add(juce::String(e.pitch) + "," + juce::String(e.startQuarter) + "," + juce::String(e.endQuarter));
    return parts.joinIntoString(";");
}

static std::vector<midispace::NoteEvent> stringToMotif(const juce::String& s) {
    std::vector<midispace::NoteEvent> out;
    if (s.isEmpty())
        return out;
    for (const auto& tok : juce::StringArray::fromTokens(s, ";", "")) {
        const auto f = juce::StringArray::fromTokens(tok, ",", "");
        if (f.size() >= 3) {
            midispace::NoteEvent e;
            e.pitch = f[0].getIntValue();
            e.startQuarter = f[1].getFloatValue();
            e.endQuarter = f[2].getFloatValue();
            out.push_back(e);
        }
    }
    return out;
}

// latent -> comma-separated floats
static juce::String latentToString(const midispace::LatentVector& latent) {
    juce::StringArray parts;
    for (float v : latent)
        parts.add(juce::String(v));
    return parts.joinIntoString(",");
}

static midispace::LatentVector stringToLatent(const juce::String& s) {
    midispace::LatentVector out;
    if (s.isEmpty())
        return out;
    for (const auto& tok : juce::StringArray::fromTokens(s, ",", ""))
        out.push_back(tok.getFloatValue());
    return out;
}

static juce::String nodesToXml(const std::vector<midispace::MidiNode>& nodes,
                               juce::Point<float> ballPos) {
    juce::ValueTree root("MidiSpace");
    root.setProperty("ballX", ballPos.x, nullptr);
    root.setProperty("ballY", ballPos.y, nullptr);
    for (const auto& n : nodes) {
        juce::ValueTree node("Node");
        node.setProperty("name", n.name, nullptr);
        node.setProperty("x", n.position.x, nullptr);
        node.setProperty("y", n.position.y, nullptr);
        node.setProperty("colour", n.colour.toString(), nullptr);
        node.setProperty("motif", motifToString(n.motif), nullptr);
        node.setProperty("latent", latentToString(n.latent), nullptr);
        root.addChild(node, -1, nullptr);
    }
    if (auto xml = root.createXml())
        return xml->toString();
    return {};
}

static std::vector<midispace::MidiNode> xmlToNodes(const juce::String& xmlStr,
                                                   juce::Point<float>& ballPosOut) {
    std::vector<midispace::MidiNode> nodes;
    if (xmlStr.isEmpty())
        return nodes;
    auto xml = juce::XmlDocument::parse(xmlStr);
    if (xml == nullptr)
        return nodes;
    auto root = juce::ValueTree::fromXml(*xml);
    if (!root.isValid())
        return nodes;

    if (root.hasProperty("ballX") && root.hasProperty("ballY"))
        ballPosOut = juce::Point<float>(
            static_cast<float>(static_cast<double>(root.getProperty("ballX"))),
            static_cast<float>(static_cast<double>(root.getProperty("ballY"))));

    for (const auto& node : root) {
        midispace::MidiNode n;
        n.name = node.getProperty("name").toString();
        n.id = n.name;
        n.position = juce::Point<float>(
            static_cast<float>(static_cast<double>(node.getProperty("x"))),
            static_cast<float>(static_cast<double>(node.getProperty("y"))));
        const auto colourProp = node.getProperty("colour");
        if (colourProp.isString() && colourProp.toString().isNotEmpty())
            n.colour = juce::Colour::fromString(colourProp.toString());
        n.motif = stringToMotif(node.getProperty("motif").toString());
        n.latent = stringToLatent(node.getProperty("latent").toString());
        nodes.push_back(n);
    }
    return nodes;
}

} // namespace

MidiSpaceAudioProcessorEditor::MidiSpaceAudioProcessorEditor(MidiSpaceAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processor_(p),
      model_("http://127.0.0.1:8765"),
      gen_(model_) {
    setSize(900, 700);
    setResizable(true, true);
    setResizeLimits(700, 560, 2000, 1400);
    addAndMakeVisible(canvas_);

    statusLabel_.setFont(14.0f);
    statusLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    statusLabel_.setText("Loading latents...", juce::dontSendNotification);
    addAndMakeVisible(statusLabel_);

    resultLabel_.setFont(13.0f);
    resultLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    addAndMakeVisible(resultLabel_);

    canvas_.onBallMoved = [this] { gen_.onBallMoved(); };
    canvas_.onBallReleased = [this] { handleBallReleased(); pushNodeState(); };
    canvas_.onNodesChanged = [this] { pushNodeState(); };

    gen_.onGenerating = [this] {
        statusLabel_.setText("Generating...", juce::dontSendNotification);
    };
    gen_.onResult = [this](const std::vector<midispace::NoteEvent>& raw) {
        statusLabel_.setText("Ready", juce::dontSendNotification);
        lastRawResult_ = raw;
        applyAndPlay();
    };
    gen_.onFailed = [this](const juce::String& msg) {
        statusLabel_.setText("Generation failed", juce::dontSendNotification);
        resultLabel_.setText(msg, juce::dontSendNotification);
    };

    playButton_.onClick = [this] { processor_.play(); };
    stopButton_.onClick = [this] { processor_.stop(); };
    regenButton_.onClick = [this] { regenerate(); };
    saveButton_.onClick = [this] { saveResult(); };
    newButton_.onClick = [this] { addNewNode(); };
    deleteButton_.onClick = [this] { deleteSelectedNode(); };
    addResultButton_.onClick = [this] { addResultAsNode(); };
    captureButton_.onClick = [this] { startCapture(); };
    arpButton_.onClick = [this] { applyArpeggio(); };
    for (auto* b : { &playButton_, &stopButton_, &regenButton_, &saveButton_,
                     &newButton_, &deleteButton_, &addResultButton_, &captureButton_, &arpButton_ })
        addAndMakeVisible(b);

    for (auto* l : { &rootCaption_, &modeCaption_, &typeCaption_ }) {
        l->setFont(11.0f);
        l->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible(l);
    }
    rootCaption_.setText("Root", juce::dontSendNotification);
    modeCaption_.setText("Mode", juce::dontSendNotification);
    typeCaption_.setText("Type", juce::dontSendNotification);

    for (int i = 0; i < 12; ++i)
        keyBox_.addItem(juce::MidiMessage::getMidiNoteName(60 + i, true, false, 4), i + 1);
    keyBox_.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(keyBox_);

    modeBox_.addItem("Chord", 1);
    modeBox_.addItem("Scale", 2);
    modeBox_.setSelectedId(1, juce::dontSendNotification);
    modeBox_.onChange = [this] { populateTypeBox(); };
    addAndMakeVisible(modeBox_);

    addAndMakeVisible(typeBox_);
    populateTypeBox();

    makeSlider(transposeSlider_, transposeLabel_, "Key", -12.0, 12.0, 1.0, 0.0, [this] {
        controls_.transposeSemitones = static_cast<int>(transposeSlider_.getValue());
        applyAndPlay();
    });
    makeSlider(densitySlider_, densityLabel_, "Density", 0.1, 1.0, 0.1, 1.0, [this] {
        controls_.density = static_cast<float>(densitySlider_.getValue());
        applyAndPlay();
    });
    makeSlider(minPitchSlider_, minPitchLabel_, "Min", 36.0, 84.0, 1.0, 36.0, [this] {
        controls_.minPitch = static_cast<int>(minPitchSlider_.getValue());
        applyAndPlay();
    });
    makeSlider(maxPitchSlider_, maxPitchLabel_, "Max", 48.0, 96.0, 1.0, 96.0, [this] {
        controls_.maxPitch = static_cast<int>(maxPitchSlider_.getValue());
        applyAndPlay();
    });

    // Restore node state from the processor's in-memory state.  This survives
    // editor close/reopen (the processor outlives the editor) but resets to
    // defaults when the plugin is deleted and reloaded (fresh processor).
    const auto defaults = midispace::allMotifs();
    juce::Point<float> ballPos = canvas_.ballPosition();
    std::vector<midispace::MidiNode> loaded;
    const auto stateXml = processor_.getNodeStateXml();
    if (stateXml.isNotEmpty())
        loaded = xmlToNodes(stateXml, ballPos);

    if (!loaded.empty()) {
        for (size_t i = 0; i < loaded.size() && i < defaults.size(); ++i)
            if (loaded[i].motif.empty())
                loaded[i].motif = defaults[i];
        for (size_t i = 0; i < loaded.size(); ++i)
            if (loaded[i].colour == juce::Colour())
                loaded[i].colour = midispace::paletteColour(static_cast<int>(i));
        canvas_.setNodes(loaded);
        canvas_.setBallPosition(ballPos);
    } else {
        for (size_t i = 0; i < defaults.size() && i < static_cast<size_t>(canvas_.getNodeCount()); ++i)
            canvas_.setNodeMotif(static_cast<int>(i), defaults[i]);
    }

    startLatentLoad();
    startTimer(150);
}

MidiSpaceAudioProcessorEditor::~MidiSpaceAudioProcessorEditor() {
    stopTimer();
    *alive_ = false;
    loadPool_.removeAllJobs(true, 2000);
}

void MidiSpaceAudioProcessorEditor::timerCallback() {
    if (captureNodeIndex_ >= 0 && !processor_.isCapturing())
        finishCapture();
}

void MidiSpaceAudioProcessorEditor::makeSlider(juce::Slider& s, juce::Label& l,
                                               const juce::String& text,
                                               double minV, double maxV, double step,
                                               double init, std::function<void()> onChange) {
    l.setText(text, juce::dontSendNotification);
    l.setFont(12.0f);
    l.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(l);

    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 44, 18);
    s.setRange(minV, maxV, step);
    s.setValue(init, juce::dontSendNotification);
    s.onValueChange = [onChange] { onChange(); };
    addAndMakeVisible(s);
}

void MidiSpaceAudioProcessorEditor::startLatentLoad() {
    // Only encode nodes whose latent is missing (cached latents skip this).
    const auto& nodes = canvas_.getNodes();
    std::vector<int> toEncodeIdx;
    for (int i = 0; i < static_cast<int>(nodes.size()); ++i)
        if (nodes[i].latent.empty() && !nodes[i].motif.empty())
            toEncodeIdx.push_back(i);

    if (toEncodeIdx.empty()) {
        statusLabel_.setText("Ready - drag the ball, click a node to select",
                             juce::dontSendNotification);
        return;
    }

    std::vector<std::vector<midispace::NoteEvent>> toEncode;
    for (int i : toEncodeIdx)
        toEncode.push_back(nodes[i].motif);

    auto alive = alive_;
    loadPool_.addJob([this, alive, toEncodeIdx, toEncode]() {
        try {
            std::vector<midispace::LatentVector> lats;
            for (const auto& m : toEncode)
                lats.push_back(model_.encode(m));
            juce::MessageManager::callAsync([this, alive, toEncodeIdx, lats]() {
                if (!*alive)
                    return;
                for (size_t i = 0; i < toEncodeIdx.size() && i < lats.size(); ++i)
                    canvas_.setNodeLatent(toEncodeIdx[i], lats[i]);
                pushNodeState();   // persist the freshly-encoded latents
                statusLabel_.setText("Ready - drag the ball, click a node to select",
                                     juce::dontSendNotification);
            });
        } catch (const std::exception& e) {
            const juce::String msg = e.what();
            juce::MessageManager::callAsync([this, alive, msg]() {
                if (!*alive)
                    return;
                statusLabel_.setText("Latent load failed", juce::dontSendNotification);
                resultLabel_.setText(msg, juce::dontSendNotification);
            });
        }
    });
}

void MidiSpaceAudioProcessorEditor::handleBallReleased() {
    const auto& nodes = canvas_.getNodes();
    if (nodes.empty())
        return;

    const auto w = canvas_.getWeights();

    for (size_t i = 0; i < w.size() && i < nodes.size(); ++i) {
        if (w[i] >= 0.999f && !nodes[i].motif.empty()) {
            statusLabel_.setText("At anchor " + nodes[i].name, juce::dontSendNotification);
            lastRawResult_ = nodes[i].motif;
            applyAndPlay();
            return;
        }
    }

    if (nodes[0].latent.empty()) {
        statusLabel_.setText("Latents not ready", juce::dontSendNotification);
        return;
    }

    lastZ_ = interpolate(w);
    gen_.onBallReleased(lastZ_);
}

void MidiSpaceAudioProcessorEditor::regenerate() {
    if (lastZ_.empty())
        return;
    gen_.onBallReleased(lastZ_);
}

midispace::LatentVector MidiSpaceAudioProcessorEditor::interpolate(
    const std::vector<float>& weights) const {
    const auto& nodes = canvas_.getNodes();
    const size_t dim = nodes.empty() ? 0 : nodes[0].latent.size();
    midispace::LatentVector z(dim, 0.0f);
    for (size_t i = 0; i < nodes.size() && i < weights.size(); ++i)
        for (size_t d = 0; d < dim; ++d)
            z[d] += weights[i] * nodes[i].latent[d];
    return z;
}

void MidiSpaceAudioProcessorEditor::applyAndPlay() {
    if (lastRawResult_.empty())
        return;
    currentResult_ = controls_.apply(lastRawResult_);
    resultLabel_.setText(notesToString(currentResult_), juce::dontSendNotification);
    processor_.setResult(currentResult_, processor_.getBpm());   // follow host tempo
}

void MidiSpaceAudioProcessorEditor::addResultAsNode() {
    if (currentResult_.empty())
        return;

    const auto result = currentResult_;
    const juce::String name = nextNodeName();
    auto alive = alive_;

    statusLabel_.setText("Encoding node " + name + "...", juce::dontSendNotification);
    loadPool_.addJob([this, alive, result, name]() {
        try {
            auto latent = model_.encode(result);
            juce::MessageManager::callAsync([this, alive, result, latent, name]() {
                if (!*alive)
                    return;
                auto pos = canvas_.ballPosition() + juce::Point<float>(40.0f, -50.0f);
                canvas_.addNode(name, pos, result, latent);
                statusLabel_.setText("Added node " + name, juce::dontSendNotification);
            });
        } catch (const std::exception& e) {
            const juce::String msg = e.what();
            juce::MessageManager::callAsync([this, alive, msg]() {
                if (!*alive)
                    return;
                statusLabel_.setText("Add node failed", juce::dontSendNotification);
                resultLabel_.setText(msg, juce::dontSendNotification);
            });
        }
    });
}

void MidiSpaceAudioProcessorEditor::addNewNode() {
    const std::vector<midispace::NoteEvent> blank = { { 60, 0.0f, 8.0f } };
    const juce::String name = nextNodeName();
    auto alive = alive_;

    statusLabel_.setText("Adding node " + name + "...", juce::dontSendNotification);
    loadPool_.addJob([this, alive, blank, name]() {
        try {
            auto latent = model_.encode(blank);
            juce::MessageManager::callAsync([this, alive, blank, latent, name]() {
                if (!*alive)
                    return;
                auto pos = canvas_.ballPosition() + juce::Point<float>(30.0f, -30.0f);
                canvas_.addNode(name, pos, blank, latent);
                statusLabel_.setText("Added node " + name + " - select it, then Capture to load MIDI",
                                     juce::dontSendNotification);
            });
        } catch (const std::exception& e) {
            const juce::String msg = e.what();
            juce::MessageManager::callAsync([this, alive, msg]() {
                if (!*alive)
                    return;
                statusLabel_.setText("Add node failed", juce::dontSendNotification);
            });
        }
    });
}

void MidiSpaceAudioProcessorEditor::deleteSelectedNode() {
    const int idx = canvas_.getSelectedNodeIndex();
    if (idx >= 0) {
        canvas_.removeNode(idx);
        statusLabel_.setText("Deleted node", juce::dontSendNotification);
    } else {
        statusLabel_.setText("Click a node to select it first", juce::dontSendNotification);
    }
}

void MidiSpaceAudioProcessorEditor::startCapture() {
    const auto& nodes = canvas_.getNodes();
    const int idx = canvas_.getSelectedNodeIndex();
    if (idx < 0 || idx >= static_cast<int>(nodes.size())) {
        statusLabel_.setText("Click a node to select it first", juce::dontSendNotification);
        return;
    }

    captureNodeIndex_ = idx;
    processor_.startCapture();
    statusLabel_.setText(
        "Capturing " + nodes[idx].name +
        " - write in FL piano roll (F7), press Space for ~2 bars",
        juce::dontSendNotification);
}

void MidiSpaceAudioProcessorEditor::finishCapture() {
    if (captureNodeIndex_ < 0)
        return;

    const int idx = captureNodeIndex_;
    captureNodeIndex_ = -1;
    const auto notes = processor_.stopCapture();

    if (notes.empty()) {
        statusLabel_.setText("No MIDI received - press Space in FL first", juce::dontSendNotification);
        return;
    }

    canvas_.setNodeMotif(idx, notes);
    const auto& ns = canvas_.getNodes();
    const juce::String nodeName = (idx >= 0 && idx < static_cast<int>(ns.size())) ? ns[idx].name : juce::String();
    statusLabel_.setText("Captured into node " + nodeName + " - re-encoding...",
                         juce::dontSendNotification);

    auto alive = alive_;
    loadPool_.addJob([this, alive, idx, notes]() {
        try {
            auto latent = model_.encode(notes);
            juce::MessageManager::callAsync([this, alive, idx, latent]() {
                if (!*alive)
                    return;
                canvas_.setNodeLatent(idx, latent);
                pushNodeState();   // persist latent with the node
                statusLabel_.setText("Ready", juce::dontSendNotification);
            });
        } catch (const std::exception& e) {
            const juce::String msg = e.what();
            juce::MessageManager::callAsync([this, alive, msg]() {
                if (!*alive)
                    return;
                statusLabel_.setText("Re-encode failed", juce::dontSendNotification);
            });
        }
    });
}

void MidiSpaceAudioProcessorEditor::pushNodeState() {
    processor_.setNodeStateXml(nodesToXml(canvas_.getNodes(), canvas_.ballPosition()));
}

void MidiSpaceAudioProcessorEditor::populateTypeBox() {
    typeBox_.clear(juce::dontSendNotification);
    if (modeBox_.getSelectedId() == 1) {   // Chord
        for (int i = 0; i < midispace::chordTypeCount(); ++i)
            typeBox_.addItem(midispace::chordTypeName(i), i + 1);
    } else {                               // Scale
        for (int i = 0; i < midispace::scaleTypeCount(); ++i)
            typeBox_.addItem(midispace::scaleTypeName(i), i + 1);
    }
    typeBox_.setSelectedId(1, juce::dontSendNotification);
}

void MidiSpaceAudioProcessorEditor::applyArpeggio() {
    const int idx = canvas_.getSelectedNodeIndex();
    const auto& nodes = canvas_.getNodes();
    if (idx < 0 || idx >= static_cast<int>(nodes.size())) {
        statusLabel_.setText("Click a node to select it first", juce::dontSendNotification);
        return;
    }

    const int root = 60 + (keyBox_.getSelectedId() - 1);
    const int typeIdx = typeBox_.getSelectedId() - 1;
    std::vector<midispace::NoteEvent> notes;
    if (modeBox_.getSelectedId() == 1)   // Chord
        notes = midispace::makeArpeggio(root, midispace::chordOffsets(typeIdx));
    else                                 // Scale
        notes = midispace::makeScaleRun(root, midispace::scaleOffsets(typeIdx));
    canvas_.setNodeMotif(idx, notes);
    pushNodeState();
    statusLabel_.setText("Applied - re-encoding...", juce::dontSendNotification);

    auto alive = alive_;
    loadPool_.addJob([this, alive, idx, notes]() {
        try {
            auto latent = model_.encode(notes);
            juce::MessageManager::callAsync([this, alive, idx, latent]() {
                if (!*alive)
                    return;
                canvas_.setNodeLatent(idx, latent);
                pushNodeState();   // persist latent with the node
                statusLabel_.setText("Ready", juce::dontSendNotification);
            });
        } catch (const std::exception& e) {
            const juce::String msg = e.what();
            juce::MessageManager::callAsync([this, alive, msg]() {
                if (!*alive)
                    return;
                statusLabel_.setText("Arpeggio re-encode failed", juce::dontSendNotification);
            });
        }
    });
}

void MidiSpaceAudioProcessorEditor::saveResult() {
    const int idx = canvas_.getSelectedNodeIndex();
    const auto& nodes = canvas_.getNodes();
    std::vector<midispace::NoteEvent> notes;
    if (idx >= 0 && idx < static_cast<int>(nodes.size()))
        notes = nodes[idx].motif;
    else
        notes = currentResult_;

    if (notes.empty())
        return;

    auto alive = alive_;
    auto* fc = new juce::FileChooser(
        "Save MIDI as...",
        juce::File::getSpecialLocation(juce::File::userMusicDirectory),
        "*.mid");

    fc->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
        [this, alive, fc, notes](const juce::FileChooser& chooser) {
            const auto chosen = chooser.getResult();
            delete fc;
            if (chosen == juce::File() || !*alive)
                return;

            const auto f = chosen.withFileExtension(".mid");
            if (midispace::writeMidiFile(notes, f, 120.0))
                statusLabel_.setText("Saved: " + f.getFileName(), juce::dontSendNotification);
            else
                statusLabel_.setText("Save failed", juce::dontSendNotification);
        });
}

juce::String MidiSpaceAudioProcessorEditor::nextNodeName() const {
    const auto& nodes = canvas_.getNodes();
    for (int i = 1; i <= 999; ++i) {
        const juce::String num(i);
        bool used = false;
        for (const auto& n : nodes) {
            if (n.name == num) {
                used = true;
                break;
            }
        }
        if (!used)
            return num;
    }
    return juce::String(nodes.size() + 1);
}

juce::String MidiSpaceAudioProcessorEditor::notesToString(
    const std::vector<midispace::NoteEvent>& notes) {
    juce::String s;
    for (const auto& n : notes) {
        if (s.isNotEmpty())
            s += "  ";
        // octaveForMiddleC = 5 matches FL Studio's piano-roll labeling
        // (FL calls MIDI 60 "C5", not "C4").
        s += juce::MidiMessage::getMidiNoteName(n.pitch, true, true, 5);
    }
    return s;
}

void MidiSpaceAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black);
}

void MidiSpaceAudioProcessorEditor::resized() {
    auto a = getLocalBounds();
    const int bottomH = 210;
    canvas_.setBounds(a.removeFromTop(getHeight() - bottomH));

    statusLabel_.setBounds(a.removeFromTop(20).reduced(10, 1));
    resultLabel_.setBounds(a.removeFromTop(18).reduced(10, 0));

    auto btnRow1 = a.removeFromTop(34);
    playButton_.setBounds(btnRow1.removeFromLeft(56).reduced(4, 4));
    stopButton_.setBounds(btnRow1.removeFromLeft(56).reduced(4, 4));
    regenButton_.setBounds(btnRow1.removeFromLeft(64).reduced(4, 4));
    saveButton_.setBounds(btnRow1.removeFromLeft(82).reduced(4, 4));

    auto btnRow2 = a.removeFromTop(34);
    newButton_.setBounds(btnRow2.removeFromLeft(70).reduced(4, 4));
    deleteButton_.setBounds(btnRow2.removeFromLeft(70).reduced(4, 4));
    addResultButton_.setBounds(btnRow2.removeFromLeft(90).reduced(4, 4));
    captureButton_.setBounds(btnRow2.removeFromLeft(90).reduced(4, 4));

    auto arpRow = a.removeFromTop(34);
    rootCaption_.setBounds(arpRow.removeFromLeft(34).reduced(4, 9));
    keyBox_.setBounds(arpRow.removeFromLeft(58).reduced(3, 4));
    modeCaption_.setBounds(arpRow.removeFromLeft(40).reduced(4, 9));
    modeBox_.setBounds(arpRow.removeFromLeft(68).reduced(3, 4));
    typeCaption_.setBounds(arpRow.removeFromLeft(34).reduced(4, 9));
    typeBox_.setBounds(arpRow.removeFromLeft(150).reduced(3, 4));
    arpButton_.setBounds(arpRow.removeFromLeft(70).reduced(4, 4));

    auto row1 = a.removeFromTop(34);
    transposeLabel_.setBounds(row1.removeFromLeft(64).reduced(4, 8));
    transposeSlider_.setBounds(row1.removeFromLeft(150).reduced(4, 6));
    densityLabel_.setBounds(row1.removeFromLeft(56).reduced(4, 8));
    densitySlider_.setBounds(row1.reduced(4, 6));

    auto row2 = a.removeFromTop(34);
    minPitchLabel_.setBounds(row2.removeFromLeft(64).reduced(4, 8));
    minPitchSlider_.setBounds(row2.removeFromLeft(150).reduced(4, 6));
    maxPitchLabel_.setBounds(row2.removeFromLeft(56).reduced(4, 8));
    maxPitchSlider_.setBounds(row2.reduced(4, 6));
}

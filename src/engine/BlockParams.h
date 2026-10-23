// BlockParams.h — Top End's canonical parameter registry.
//
// Everything settable in the rig is described here once: pedal blocks, the amp,
// the cab, the tracked synth, the looper and the in/out stages.
// The UI is generated from this table, presets are `path -> value` maps against
// it, and MIDI learn binds to the same paths (see RigModel.h §path space).
//
// RELATIONSHIP TO THE SANCTUARY FX RACK. Top End reuses the twelve FX units
// already built in `FxDsp.h` (chorus, flanger, phaser, tremolo, EQ, both delays,
// both reverbs) rather than writing bass-flavoured copies of them. A block whose
// `reuse` field is set takes its parameter list VERBATIM from `jm::fxSpec()`, so
// the same preset values mean the same thing in both products and a fix to a
// shared unit lands in both. Blocks without `reuse` are Top End's own DSP
// (drives, dynamics, filter/pitch, gate) and live in PedalDsp.h.
#pragma once
#include <optional>
#include <string>
#include <vector>
#include "FxParams.h"

namespace topend {

using ParamKind = jm::ParamKind;
using ParamSpec = jm::ParamSpec;
using ParamValues = jm::ParamValues;

//==============================================================================
// Block types. INTEGER IDS ARE SAVED IN PRESETS — append only, never reorder.
enum class BlockType : int
{
    none = 0,
    noiseGate,
    // drive
    clearDrive, tubeOd, fuzz, boost,
    // dynamics
    optoComp, fetComp, graphicEq, paraEq,
    // filter / pitch
    envFilter, octaver,
    // modulation (reused)
    chorus, flanger, phaser, tremolo,
    // time (reused)
    analogDelay, digitalDelay, roomVerb, ambientVerb,
    // added for guitar (kept at the end so the earlier indices never move)
    wah,
    Count
};
constexpr int kNumBlockTypes = (int) BlockType::Count;

struct BlockSpec
{
    BlockType type = BlockType::none;
    const char* id = "none";          // JSON id, e.g. "clearDrive"
    const char* name = "";            // display name on the pedal
    const char* category = "";        // UI grouping: Drive / Dynamics / Filter / Mod / Time / Utility
    std::optional<jm::FxType> reuse;  // set => parameters and DSP come from the shared FX rack
    std::vector<ParamSpec> params;
};

/// The chain limits. PRE is the pedalboard in front of the amp; POST sits after
/// the cab where modulation and ambience belong.
constexpr int kMaxPreBlocks = 8;
constexpr int kMaxPostBlocks = 4;

//==============================================================================
inline const std::vector<BlockSpec>& blockRegistry()
{
    // Reused blocks are declared with just a name and an FxType; their parameter
    // list is filled in from the shared registry the first time through.
    static std::vector<BlockSpec> reg = []
    {
        std::vector<BlockSpec> r {
            { BlockType::none, "none", "Empty", "Utility", {}, {} },

            //---------------------------------------------------------- utility
            { BlockType::noiseGate, "noiseGate", "Noise Gate", "Utility", {}, {
                { "threshold", "Threshold", ParamKind::Float, -90, 0, -70, {}, "dB" },
                { "release", "Release", ParamKind::Float, 10, 1000, 120, {}, "ms", false, true },
                { "depth", "Depth", ParamKind::Float, 0, 100, 100 } } },

            //------------------------------------------------------------ drive
            // A transparent drive: the clean signal is blended back under the
            // driven one, so the guitar keeps its pick attack and its own voice
            // and the pedal adds grit and push rather than replacing the tone.
            { BlockType::clearDrive, "clearDrive", "Clear Drive", "Drive", {}, {
                { "drive", "Drive", ParamKind::Float, 0, 100, 35 },
                { "tone", "Tone", ParamKind::Float, 0, 100, 55 },
                { "blend", "Mix", ParamKind::Float, 0, 100, 100 },
                { "bass", "Bass", ParamKind::Float, -12, 12, 0, {}, "dB" },
                { "treble", "Treble", ParamKind::Float, -12, 12, 0, {}, "dB" },
                { "level", "Level", ParamKind::Float, 0, 100, 50 } } },

            { BlockType::tubeOd, "tubeOd", "Tube Drive", "Drive", {}, {
                { "drive", "Drive", ParamKind::Float, 0, 100, 40 },
                { "tone", "Tone", ParamKind::Float, 0, 100, 55 },
                { "level", "Level", ParamKind::Float, 0, 100, 50 },
                { "tube", "Tube", ParamKind::Enum, 0, 2, 0, { "12AX7", "12AU7", "6L6" } },
                { "bias", "Bias", ParamKind::Float, 0, 100, 50 } } },

            { BlockType::fuzz, "fuzz", "Fuzz", "Drive", {}, {
                { "fuzz", "Fuzz", ParamKind::Float, 0, 100, 60 },
                { "tone", "Tone", ParamKind::Float, 0, 100, 50 },
                { "level", "Level", ParamKind::Float, 0, 100, 40 },
                { "gate", "Gate", ParamKind::Float, 0, 100, 20 },
                { "blend", "Mix", ParamKind::Float, 0, 100, 100 } } },

            { BlockType::boost, "boost", "Treble Boost", "Drive", {}, {
                { "gain", "Gain", ParamKind::Float, 0, 24, 6, {}, "dB" },
                { "presence", "Presence", ParamKind::Float, 0, 100, 50 },
                { "low", "Low", ParamKind::Float, -12, 12, 0, {}, "dB" } } },

            //--------------------------------------------------------- dynamics
            { BlockType::optoComp, "optoComp", "Studio Comp", "Dynamics", {}, {
                { "sustain", "Sustain", ParamKind::Float, 0, 100, 50 },
                { "tone", "Tone", ParamKind::Float, 0, 100, 50 },
                { "level", "Level", ParamKind::Float, 0, 100, 50 },
                { "mix", "Mix", ParamKind::Float, 0, 100, 100 } } },

            { BlockType::fetComp, "fetComp", "FET Squeeze", "Dynamics", {}, {
                { "threshold", "Threshold", ParamKind::Float, -40, 0, -16, {}, "dB" },
                { "ratio", "Ratio", ParamKind::Enum, 0, 4, 1, { "4:1", "8:1", "12:1", "20:1", "All" } },
                { "attack", "Attack", ParamKind::Float, 0.1f, 30, 4, {}, "ms", false, true },
                { "release", "Release", ParamKind::Float, 30, 1200, 200, {}, "ms", false, true },
                { "makeup", "Makeup", ParamKind::Float, 0, 24, 6, {}, "dB" },
                { "mix", "Mix", ParamKind::Float, 0, 100, 100 } } },

            { BlockType::graphicEq, "graphicEq", "Guitar EQ", "Dynamics", {}, {
                { "g100", "100", ParamKind::Float, -15, 15, 0, {}, "dB" },
                { "g250", "250", ParamKind::Float, -15, 15, 0, {}, "dB" },
                { "g800", "800", ParamKind::Float, -15, 15, 0, {}, "dB" },
                { "g2k", "2k", ParamKind::Float, -15, 15, 0, {}, "dB" },
                { "g5k", "5k", ParamKind::Float, -15, 15, 0, {}, "dB" },
                { "level", "Level", ParamKind::Float, -12, 12, 0, {}, "dB" } } },

            //---------------------------------------------------- filter, pitch
            { BlockType::envFilter, "envFilter", "Auto Wah", "Filter", {}, {
                { "sens", "Sensitivity", ParamKind::Float, 0, 100, 55 },
                { "range", "Range", ParamKind::Float, 0, 100, 60 },
                { "q", "Q", ParamKind::Float, 0, 100, 55 },
                { "direction", "Sweep", ParamKind::Enum, 0, 1, 0, { "Up", "Down" } },
                { "decay", "Decay", ParamKind::Float, 20, 800, 180, {}, "ms", false, true },
                { "mix", "Mix", ParamKind::Float, 0, 100, 100 } } },

            { BlockType::octaver, "octaver", "Octave Down", "Filter", {}, {
                { "sub1", "Sub 1", ParamKind::Float, 0, 100, 60 },
                { "sub2", "Sub 2", ParamKind::Float, 0, 100, 0 },
                { "dry", "Dry", ParamKind::Float, 0, 100, 100 },
                { "tone", "Tone", ParamKind::Float, 0, 100, 50 },
                { "growl", "Growl", ParamKind::Float, 0, 100, 25 } } },

            //------------------------------------------------- reused FX blocks
            { BlockType::chorus, "chorus", "Chorus", "Modulation", jm::FxType::jmChorus, {} },
            { BlockType::flanger, "flanger", "Flanger", "Modulation", jm::FxType::flanger, {} },
            { BlockType::phaser, "phaser", "Phaser", "Modulation", jm::FxType::phaser, {} },
            { BlockType::tremolo, "tremolo", "Tremolo", "Modulation", jm::FxType::tremoloAutoPan, {} },
            { BlockType::paraEq, "paraEq", "Studio EQ", "Dynamics", jm::FxType::vintageEq, {} },
            { BlockType::analogDelay, "analogDelay", "Tape Echo", "Time", jm::FxType::tapeDelay, {} },
            { BlockType::digitalDelay, "digitalDelay", "Digital Delay", "Time", jm::FxType::digitalDelay, {} },
            { BlockType::roomVerb, "roomVerb", "Room", "Time", jm::FxType::plateReverb, {} },
            { BlockType::ambientVerb, "ambientVerb", "Hall", "Time", jm::FxType::hallShimmer, {} },

            // A wah is a resonant band-pass the foot sweeps. Position is a plain
            // parameter, so an expression pedal reaches it through MIDI Learn
            // like any other control.
            { BlockType::wah, "wah", "Wah", "Filter", {}, {
                { "position", "Position", ParamKind::Float, 0, 100, 50 },
                { "range", "Range", ParamKind::Enum, 0, 1, 0, { "Vintage", "Wide" } },
                { "q", "Q", ParamKind::Float, 0, 100, 60 },
                { "mix", "Mix", ParamKind::Float, 0, 100, 100 } } },
        };

        for (auto& b : r)
            if (b.reuse.has_value())
                b.params = jm::fxSpec (*b.reuse).params;

        // Sorted by the enum so `blockSpec(t)` is an index, not a search. The
        // declaration order above is grouped for readability; this makes the two
        // orders independent so a new block can be declared next to its family.
        std::vector<BlockSpec> byType ((size_t) kNumBlockTypes);
        for (auto& b : r) byType[(size_t) b.type] = b;
        return byType;
    }();
    return reg;
}

inline const BlockSpec& blockSpec (BlockType t)
{
    const auto& r = blockRegistry();
    const auto i = (size_t) t;
    return i < r.size() ? r[i] : r[0];
}

inline std::optional<BlockType> blockTypeFromId (const std::string& id)
{
    for (auto& b : blockRegistry())
        if (id == b.id) return b.type;
    return std::nullopt;
}

inline const ParamSpec* findBlockParam (const BlockSpec& s, const std::string& id)
{
    for (auto& p : s.params)
        if (id == p.id) return &p;
    return nullptr;
}

inline ParamValues defaultBlockParams (BlockType t)
{
    ParamValues v;
    for (auto& p : blockSpec (t).params) v[p.id] = p.def;
    return v;
}

//==============================================================================
// AMP. One parameter list covers all six models; a model uses the subset it
// physically has (see `ampVisibleParams`), and the DSP ignores the rest. Keeping
// one list means switching model preserves the shared knobs — turn Bass down on
// the Plexi, switch to the Black Panel, Bass is still down, which is what a
// player expects from a modelling amp.
enum class AmpModel : int { plexi = 0, blackPanel, tweed, topBoost, hiGain, solidClean, Count };
constexpr int kNumAmpModels = (int) AmpModel::Count;

inline const std::vector<const char*>& ampModelNames()
{
    static const std::vector<const char*> n { "Plexi 68", "Black Panel", "Tweed 57", "Top Boost 30", "Modern Hi-Gain", "Solid Clean" };
    return n;
}
inline const std::vector<const char*>& ampModelIds()
{
    static const std::vector<const char*> n { "plexi", "blackPanel", "tweed", "topBoost", "hiGain", "solidClean" };
    return n;
}
/// Clean / Crunch / Lead: the same model with less or more preamp.
enum class AmpChannel : int { clean = 0, crunch, lead, Count };

inline const std::vector<ParamSpec>& ampParams()
{
    static const std::vector<ParamSpec> p {
        { "model", "Model", ParamKind::Enum, 0, kNumAmpModels - 1, 0,
          { "Plexi 68", "Black Panel", "Tweed 57", "Top Boost 30", "Modern Hi-Gain", "Solid Clean" } },
        { "channel", "Channel", ParamKind::Enum, 0, 2, 1, { "Clean", "Crunch", "Lead" } },
        { "gain", "Gain", ParamKind::Float, 0, 100, 50 },
        { "bass", "Bass", ParamKind::Float, 0, 100, 50 },
        { "mid", "Mid", ParamKind::Float, 0, 100, 55 },
        { "treble", "Treble", ParamKind::Float, 0, 100, 55 },
        { "presence", "Presence", ParamKind::Float, 0, 100, 45 },
        { "bright", "Bright", ParamKind::Bool, 0, 1, 0 },
        { "tight", "Tight", ParamKind::Bool, 0, 1, 0 },
        { "sag", "Sag", ParamKind::Float, 0, 100, 30 },
        { "master", "Master", ParamKind::Float, 0, 100, 55 } };
    return p;
}

/// Which knobs the front panel shows for a model. Order is panel order.
inline const std::vector<const char*>& ampVisibleParams (AmpModel m)
{
    // Every model shows the same six knobs, in the same place, so a player's
    // hand finds Gain and Master without looking; the switches differ.
    static const std::vector<const char*> brit   { "channel", "gain", "bass", "mid", "treble", "presence", "master", "tight", "sag" };
    static const std::vector<const char*> us     { "channel", "gain", "bass", "mid", "treble", "presence", "master", "bright", "sag" };
    static const std::vector<const char*> modern { "channel", "gain", "bass", "mid", "treble", "presence", "master", "tight" };
    static const std::vector<const char*> solid  { "channel", "gain", "bass", "mid", "treble", "presence", "master", "bright" };
    switch (m)
    {
        case AmpModel::blackPanel:
        case AmpModel::tweed:
        case AmpModel::topBoost:   return us;
        case AmpModel::hiGain:     return modern;
        case AmpModel::solidClean: return solid;
        default:                   return brit;
    }
}

//==============================================================================
// CAB
enum class CabModel : int { c112open = 0, c212, c412, userIr, Count };
inline const std::vector<ParamSpec>& cabParams()
{
    static const std::vector<ParamSpec> p {
        { "model", "Cab", ParamKind::Enum, 0, (int) CabModel::Count - 1, 1,
          { "1x12 Open", "2x12 Alnico", "4x12 V30", "Your IR" } },
        { "mic", "Mic", ParamKind::Enum, 0, 2, 0, { "Dynamic", "Condenser", "Ribbon" } },
        { "distance", "Distance", ParamKind::Float, 0, 100, 25 },
        // 0 = the mic on the dust cap (brightest), 100 = at the cone's edge.
        { "axis", "Position", ParamKind::Float, 0, 100, 36 },
        { "lowCut", "Low Cut", ParamKind::Float, 20, 300, 80, {}, "Hz", false, true },
        { "highCut", "High Cut", ParamKind::Float, 2000, 16000, 9000, {}, "Hz", false, true },
        { "ir", "IR", ParamKind::Float, -1, 255, -1 },   // index into the scanned IR folder; -1 = none
        { "level", "Level", ParamKind::Float, -24, 12, 0, {}, "dB" } };
    return p;
}

//==============================================================================
// INPUT / OUTPUT
inline const std::vector<ParamSpec>& inputParams()
{
    static const std::vector<ParamSpec> p {
        { "trim", "Input Trim", ParamKind::Float, -24, 24, 0, {}, "dB" },
        { "gateOn", "Gate", ParamKind::Bool, 0, 1, 1 },
        { "gateThreshold", "Gate Threshold", ParamKind::Float, -90, 0, -70, {}, "dB" },
        { "gateRelease", "Gate Release", ParamKind::Float, 10, 1000, 120, {}, "ms", false, true },
        { "diOn", "DI Blend", ParamKind::Bool, 0, 1, 0 },
        { "diLevel", "DI Level", ParamKind::Float, -60, 6, -12, {}, "dB" },
        { "diLowPass", "DI Low Pass", ParamKind::Float, 60, 20000, 20000, {}, "Hz", false, true } };
    return p;
}

inline const std::vector<ParamSpec>& outputParams()
{
    static const std::vector<ParamSpec> p {
        { "master", "Master", ParamKind::Float, -60, 12, 0, {}, "dB" },
        // The ceiling is NOT a control: `jm::MasterLimiter` is a fixed -0.3 dBFS
        // safety wall shared with the other Amanorsac products. A knob that
        // implied otherwise would be a lie, so there isn't one.
        { "limiter", "Limiter", ParamKind::Bool, 0, 1, 1 } };
    return p;
}

//==============================================================================
// TRACKED SYNTH (BassSynth.h) — a mono voice that follows the note the bass is playing.
enum class SynthRoute : int { preAmp = 0, postCab, Count };
inline const std::vector<ParamSpec>& synthParams()
{
    static const std::vector<ParamSpec> p {
        { "on", "Synth", ParamKind::Bool, 0, 1, 0 },
        { "wave", "Wave", ParamKind::Enum, 0, 3, 0, { "Saw", "Square", "Triangle", "Pulse" } },
        { "detune", "Detune", ParamKind::Float, 0, 50, 8, {}, "cents" },
        { "sub", "Sub", ParamKind::Float, 0, 100, 40 },
        { "cutoff", "Cutoff", ParamKind::Float, 60, 8000, 900, {}, "Hz", false, true },
        { "reso", "Resonance", ParamKind::Float, 0, 100, 30 },
        { "envAmt", "Env Amount", ParamKind::Float, -100, 100, 45 },
        { "attack", "Attack", ParamKind::Float, 0.5f, 500, 4, {}, "ms", false, true },
        { "decay", "Decay", ParamKind::Float, 5, 2000, 220, {}, "ms", false, true },
        { "sustain", "Sustain", ParamKind::Float, 0, 100, 60 },
        { "release", "Release", ParamKind::Float, 5, 3000, 160, {}, "ms", false, true },
        { "glide", "Glide", ParamKind::Float, 0, 500, 0, {}, "ms" },
        { "octave", "Octave", ParamKind::Float, -2, 2, 0 },
        { "level", "Level", ParamKind::Float, -60, 6, -8, {}, "dB" },
        { "route", "Route", ParamKind::Enum, 0, 1, 1, { "Pre Amp", "Post Cab" } } };
    return p;
}

//==============================================================================
// LOOPER (Looper.h)
constexpr int kMaxLoopLayers = 4;
inline const std::vector<ParamSpec>& looperParams()
{
    static const std::vector<ParamSpec> p {
        { "level", "Loop Level", ParamKind::Float, -60, 6, 0, {}, "dB" },
        { "decay", "Overdub Decay", ParamKind::Float, 0, 100, 0, {}, "%" },
        { "quantize", "Quantize", ParamKind::Enum, 0, 2, 1, { "Free", "Bar", "Beat" } },
        { "speed", "Speed", ParamKind::Enum, 0, 1, 0, { "Normal", "Half" } },
        { "reverse", "Reverse", ParamKind::Bool, 0, 1, 0 },
        { "fadeMs", "Seam Fade", ParamKind::Float, 1, 50, 6, {}, "ms" },
        { "playThrough", "Play Through", ParamKind::Bool, 0, 1, 1 },
        { "autoStart", "Start On Note", ParamKind::Bool, 0, 1, 1 } };     // REC waits for the first note, on the bar   // hear the live guitar while a loop runs
    return p;
}

//==============================================================================
/// Lookup used by the path space: given a section name, its parameter list.
inline const std::vector<ParamSpec>* sectionParams (const std::string& section)
{
    if (section == "in")    return &inputParams();
    if (section == "amp")   return &ampParams();
    if (section == "cab")   return &cabParams();
    if (section == "synth") return &synthParams();
    if (section == "loop")  return &looperParams();
    if (section == "out")   return &outputParams();
    return nullptr;
}

inline const ParamSpec* findParamIn (const std::vector<ParamSpec>& list, const std::string& id)
{
    for (auto& p : list)
        if (id == p.id) return &p;
    return nullptr;
}

inline ParamValues defaultsFor (const std::vector<ParamSpec>& list)
{
    ParamValues v;
    for (auto& p : list) v[p.id] = p.def;
    return v;
}

} // namespace topend

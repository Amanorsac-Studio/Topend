// GateTest.cpp — a test build with no valid key must be SILENT, checked against the real
// processor, the real public key and a real block of audio.
//
//   TopEndGateTest            no key, and a key that has expired: both must be silent
//   TOPEND_TEST_KEY=<key> TopEndGateTest
//                             additionally checks that a good key lets audio through.
//                             Supply a live key at run time; none is kept in the source.
//
// Compiled with TOPEND_TESTER_BUILD=1 (see CMakeLists.txt), which the ordinary soak
// and unit-test targets are not. The expired key below is signed by the real Top End
// signing key but dated 1 January 2001 and lapsed on 31 January 2001, so it can
// never open anything and is safe to keep here.
#include "plugin/TopEndProcessor.h"
#include <cmath>
#include <cstdio>

using namespace topend;

static const char* kExpiredKey = "eyJhIjoidG9wZW5kLXRlc3RlciIsImkiOjk3ODMwNzIwMDAwMCwiZSI6OTgwODk5MjAwMDAwLCJuIjoiNWY1MGUxZTcifQ.Ij-p3Ytsyx8TNyUFvsATrgx1lq2kvj5f7k01D-aiMjcFAKGpeUdOMsokU-NsWK7_kCh9kKJcMlIG6QoB5rcNsWwsXAT08IDRIn8aKdkFPZ-NLciPHk0hGTFJZkihknRYQiZiIEVaoUokFmh-AMSaMmaDbtiGpohqBBP6dC5bP-fOY5UceHrDyOhTC0sdrW9CFi0V0tSNw6OYZutyHaPsJaEb6I97JwksDXO8U1yQLtFZeKrCGb89kPt5LWJvJ7eOx7e0rzfoHj7Bwl7rWpFDb1ssJEM4sljlcLKq5evgKawhIn2e6xjpoiC60dKSIRHkowe_EgEIxmCBcIWPAyWBcA";

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    if (! ok) { ++failures; std::printf ("  FAIL  %s\n", what.toRawUTF8()); }
    else std::printf ("  ok    %s\n", what.toRawUTF8());
}

/// Runs 40 blocks of a loud bass-ish sine through a processor and returns the
/// largest output sample seen.
static float loudestOutput (TopEndProcessor& p)
{
    constexpr int block = 256;
    p.setPlayConfigDetails (2, 2, 48000.0, block);
    p.prepareToPlay (48000.0, block);
    juce::AudioBuffer<float> b (2, block);
    juce::MidiBuffer midi;
    float loudest = 0.0f;
    double phase = 0.0;
    for (int n = 0; n < 40; ++n)
    {
        for (int i = 0; i < block; ++i)
        {
            const float s = 0.5f * (float) std::sin (phase);
            phase += 2.0 * juce::MathConstants<double>::pi * 55.0 / 48000.0;
            b.setSample (0, i, s);
            b.setSample (1, i, s);
        }
        p.processBlock (b, midi);
        loudest = juce::jmax (loudest, b.getMagnitude (0, block));
    }
    return loudest;
}

static void setEnv (const char* k, const juce::String& v)
{
   #if JUCE_WINDOWS
    _putenv_s (k, v.toRawUTF8());
   #else
    setenv (k, v.toRawUTF8(), 1);
   #endif
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("TopEndGate-" + juce::Uuid().toDashedString());
    setEnv ("TOPEND_DATA_DIR", root.getChildFile ("data").getFullPathName());
    const auto state = root.getChildFile ("state");
    setEnv ("TOPEND_STATE_DIR", state.getFullPathName());
    state.createDirectory();

    std::printf ("Test-build gate, in the real processor\n");
    {
        TopEndProcessor p;
        check (p.testerGated(), "this is a test build");
        check (p.testerStatus().status == tester::Status::none, "a new machine has no key");
        check (loudestOutput (p) == 0.0f, "with no key the output is silent");
    }
    state.getChildFile ("tester-key.txt").replaceWithText (kExpiredKey);
    {
        TopEndProcessor p;
        check (p.testerStatus().status == tester::Status::expired, "a stored key that has expired is recognised as expired");
        check (loudestOutput (p) == 0.0f, "with an expired key the output is silent");
        check (p.submitTesterKey ("nonsense").status == tester::Status::malformed, "a bad key is refused");
        check (loudestOutput (p) == 0.0f, "and the output stays silent");
    }
    const auto live = juce::SystemStats::getEnvironmentVariable ("TOPEND_TEST_KEY", {});
    if (live.isNotEmpty())
    {
        state.getChildFile ("tester-key.txt").deleteFile();
        TopEndProcessor p;
        check (loudestOutput (p) == 0.0f, "before the key is entered the output is silent");
        const auto c = p.submitTesterKey (live);
        check (c.status == tester::Status::ok, "the supplied key is accepted (" + juce::String (c.daysLeft) + " days left)");
        check (loudestOutput (p) > 0.001f, "and once it is, audio passes");
        TopEndProcessor again;      // a second instance, as in a DAW project with two of them
        check (loudestOutput (again) > 0.001f, "a second instance finds the stored key and plays at once");
    }
    else
        std::printf ("  (TOPEND_TEST_KEY is not set, so the open-gate check was skipped)\n");

    root.deleteRecursively();
    std::printf ("\n%s\n", failures == 0 ? "OK" : "FAILED");
    return failures == 0 ? 0 : 1;
}

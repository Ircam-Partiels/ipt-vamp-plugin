#pragma once

#include <IvePluginAdapter.hpp>
#include <array>
#include <memory>
#include <optional>
#include <set>
#include <torch/script.h>

namespace Ipt
{
    class Plugin
    : public Vamp::Plugin
    , public Ive::PluginExtension
    {
    public:
        Plugin(float inputSampleRate);
        ~Plugin() override = default;

        // Vamp::Plugin
        bool initialise(size_t channels, size_t stepSize, size_t blockSize) override;

        InputDomain getInputDomain() const override;

        std::string getIdentifier() const override;
        std::string getName() const override;
        std::string getDescription() const override;
        std::string getMaker() const override;
        int getPluginVersion() const override;
        std::string getCopyright() const override;

        size_t getPreferredBlockSize() const override;
        size_t getPreferredStepSize() const override;
        OutputList getOutputDescriptors() const override;

        void reset() override;
        FeatureSet process(float const* const* inputBuffers, Vamp::RealTime timestamp) override;
        FeatureSet getRemainingFeatures() override;

        ParameterList getParameterDescriptors() const override;
        float getParameter(std::string paramid) const override;
        void setParameter(std::string paramid, float newval) override;

        // Ive::PluginExtension
        OutputExtraList getOutputExtraDescriptors(size_t outputDescriptorIndex) const override;

    private:
        void runModel(FeatureList& list);

        static auto constexpr kSampleRateMethod = "get_sr";
        static auto constexpr kSegmentLengthMethod = "get_seglen";
        static auto constexpr kClassNamesMethod = "get_classnames";
        static auto constexpr kForwardMethod = "forward";
        static auto constexpr kSegmentHopSizeMs = 20.0;
        static auto constexpr kMinimumEnergyThreshold = -80.0; // in dB
        static auto constexpr kBatchSize = 512;
#ifdef __APPLE__
        static auto constexpr kDeviceType = torch::DeviceType::MPS;
#endif

        class Resampler
        {
        public:
            Resampler(double sourceSampleRate, double targetSampleRate);
            ~Resampler() = default;

            std::tuple<size_t, size_t> process(size_t numInputSamples, float const* inputBuffer, size_t numOutputSamples, float* outputBuffer);
            void reset();

            void setTargetSampleRate(double sampleRate) noexcept;
            double getRatio() const noexcept;

        private:
            double const mSourceSampleRate;
            double mTargetSampleRate;
            std::array<float, 5> mLastInputSamples;
            double mSubSamplePos{1.0};
            size_t mIndexBuffer{0};
        };

        size_t mBlockSize{0};
        size_t mModelIndex{0};
        std::optional<std::pair<size_t, Feature>> mLastFeature{};

        Resampler mResampler;
        std::vector<float> mBuffer;
        size_t mBufferWritePosition{0};
        size_t mBufferReadPosition{0};
        size_t mNumFrameProcessed{0};
        std::vector<float> mIntermediary;
        std::vector<float> mFlatInputs;
        std::vector<size_t> mFlatIndices;

        torch::jit::script::Module mModel;
        double mModelSampleRate = 0.0;
        size_t mModelSegmentLength = 0;
        std::vector<std::string> mModelClassNames;
    };
} // namespace Ipt

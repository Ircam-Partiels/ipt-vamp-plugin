#include "ipt_vamp_plugin.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <numeric>
#include <vamp-sdk/PluginAdapter.h>

#if defined(_MSC_VER)
#define forcedinline __forceinline
#else
#define forcedinline inline __attribute__((always_inline))
#endif

#ifndef NDEBUG
#define IptDbg(message) std::cout << "[Ipt] " << message << "\n"
#define IptErr(message) std::cerr << "[Ipt] " << message << "\n";
#else
#define IptDbg(message)
#define IptErr(message)
#endif

namespace Ipt
{
    static std::filesystem::path const executableDir = std::filesystem::current_path();

    static std::vector<std::filesystem::path> getModelPaths()
    {
        static auto const resourcesDir = executableDir / "Contents" / "Resources" / "models";
        std::vector<std::filesystem::path> modelPaths;
        if(!std::filesystem::exists(resourcesDir))
        {
            return modelPaths;
        }

        for(auto const& entry : std::filesystem::directory_iterator(resourcesDir))
        {
            if(entry.is_regular_file() && entry.path().extension() == ".ts")
            {
                modelPaths.push_back(entry.path());
            }
        }
        std::sort(modelPaths.begin(), modelPaths.end());
        return modelPaths;
    }
} // namespace Ipt

namespace ResamplerUtils
{
    template <int k>
    struct LagrangeResampleHelper
    {
        static forcedinline void calc(float& a, float b) noexcept
        {
            a *= b * (1.0f / k);
        }
    };

    template <>
    struct LagrangeResampleHelper<0>
    {
        static forcedinline void calc(float&, float) noexcept {}
    };

    template <int k>
    static float calcCoefficient(float input, float offset) noexcept
    {
        LagrangeResampleHelper<0 - k>::calc(input, -2.0f - offset);
        LagrangeResampleHelper<1 - k>::calc(input, -1.0f - offset);
        LagrangeResampleHelper<2 - k>::calc(input, 0.0f - offset);
        LagrangeResampleHelper<3 - k>::calc(input, 1.0f - offset);
        LagrangeResampleHelper<4 - k>::calc(input, 2.0f - offset);
        return input;
    }

    static float valueAtOffset(const float* inputs, float offset, int index) noexcept
    {
        auto result = 0.0f;
        result += calcCoefficient<0>(inputs[index], offset);
        index = (++index % 5);
        result += calcCoefficient<1>(inputs[index], offset);
        index = (++index % 5);
        result += calcCoefficient<2>(inputs[index], offset);
        index = (++index % 5);
        result += calcCoefficient<3>(inputs[index], offset);
        index = (++index % 5);
        result += calcCoefficient<4>(inputs[index], offset);
        return result;
    }
} // namespace ResamplerUtils

Ipt::Plugin::Resampler::Resampler(double sourceSampleRate, double targetSampleRate)
: mSourceSampleRate(sourceSampleRate)
, mTargetSampleRate(targetSampleRate)
{
    reset();
}

std::tuple<size_t, size_t> Ipt::Plugin::Resampler::process(size_t numInputSamples, float const* inputBuffer, size_t numOutputSamples, float* outputBuffer)
{
    double const speedRatio = getRatio();
    size_t numGeneratedSamples = 0;
    size_t numUsedSamples = 0;
    auto subSamplePos = mSubSamplePos;
    while(numUsedSamples < numInputSamples && numGeneratedSamples < numOutputSamples)
    {
        while(subSamplePos >= 1.0 && numUsedSamples < numInputSamples)
        {
            mLastInputSamples[mIndexBuffer] = inputBuffer[numUsedSamples++];
            if(++mIndexBuffer == mLastInputSamples.size())
            {
                mIndexBuffer = 0;
            }
            subSamplePos -= 1.0;
        }
        if(subSamplePos < 1.0)
        {
            outputBuffer[numGeneratedSamples++] = ResamplerUtils::valueAtOffset(mLastInputSamples.data(), static_cast<float>(subSamplePos), static_cast<int>(mIndexBuffer));
            subSamplePos += speedRatio;
        }
    }
    while(subSamplePos >= 1.0 && numUsedSamples < numInputSamples)
    {
        mLastInputSamples[mIndexBuffer] = inputBuffer[numUsedSamples++];
        if(++mIndexBuffer == mLastInputSamples.size())
        {
            mIndexBuffer = 0;
        }
        subSamplePos -= 1.0;
    }
    mSubSamplePos = subSamplePos;
    return std::make_tuple(numUsedSamples, numGeneratedSamples);
}

void Ipt::Plugin::Resampler::reset()
{
    mIndexBuffer = 0;
    mSubSamplePos = 1.0;
    std::fill(mLastInputSamples.begin(), mLastInputSamples.end(), 0.0f);
}

void Ipt::Plugin::Resampler::setTargetSampleRate(double sampleRate) noexcept
{
    mTargetSampleRate = sampleRate;
}

double Ipt::Plugin::Resampler::getRatio() const noexcept
{
    return mSourceSampleRate / mTargetSampleRate;
}

Ipt::Plugin::Plugin(float inputSampleRate)
: Vamp::Plugin(inputSampleRate)
, mResampler(inputSampleRate, inputSampleRate)
{
}

void Ipt::Plugin::reset()
{
    mLastFeature.reset();
    mFlatInputs.clear();
    mFlatIndices.clear();
    mResampler.reset();
    mBufferWritePosition = 0;
    mBufferReadPosition = 0;
    std::fill(mBuffer.begin(), mBuffer.end(), 0.0f);
    std::fill(mIntermediary.begin(), mIntermediary.end(), 0.0f);
}

bool Ipt::Plugin::initialise(size_t channels, size_t stepSize, size_t blockSize)
{
    if(channels != static_cast<size_t>(1) || stepSize != blockSize)
    {
        return false;
    }
    reset();
    auto const models = getModelPaths();
    if(mModelIndex >= models.size())
    {
        IptErr("failed to retrieve the model!");
        return false;
    }

    auto const model = getModelPaths().at(mModelIndex);
    if(model.empty())
    {
        IptErr("failed to retrieve the model!");
        return false;
    }

    mBlockSize = blockSize;
    mModel = torch::jit::load(model.string());
    mModel.eval();
#ifdef __APPLE__
    mModel.to(kDeviceType);
#endif

    mModelSampleRate = static_cast<double>(mModel.get_method(kSampleRateMethod)(std::vector<c10::IValue>()).to<int>());
    mModelSegmentLength = static_cast<size_t>(mModel.get_method(kSegmentLengthMethod)(std::vector<c10::IValue>()).to<int>());
    mBuffer.resize(mModelSegmentLength * 2);
    mIntermediary.resize(mModelSegmentLength);
    mFlatInputs.reserve(mModelSegmentLength * kBatchSize);
    mFlatIndices.reserve(kBatchSize);
    mModelClassNames.clear();
    for(const auto& classname : mModel.get_method(kClassNamesMethod)(std::vector<c10::IValue>()).toList())
    {
        mModelClassNames.emplace_back(classname.get().to<std::string>());
    }
    mResampler.setTargetSampleRate(mModelSampleRate);
    return mModelSampleRate > 0.0 && mModelSegmentLength > 0 && !mModelClassNames.empty();
}

std::string Ipt::Plugin::getIdentifier() const
{
    return "ipt";
}

std::string Ipt::Plugin::getName() const
{
    return "IPT";
}

std::string Ipt::Plugin::getDescription() const
{
    return "Audio classification, a toolkit for the real-time recognition of instrumental playing techniques using deep learning CNN models, developed at Ircam and Tokyo University of the Arts, and supported by the European Research Council (ERC) as part of the Raising Co-creativity in Cyber-Human Musicianship (REACH) Project directed by Gérard Assayag, under the European Union's Horizon 2020 research and innovation program (GA #883313).";
}

std::string Ipt::Plugin::getMaker() const
{
    return "Ircam";
}

int Ipt::Plugin::getPluginVersion() const
{
    return IPT_VAMP_PLUGIN_PLUGIN_VERSION;
}

std::string Ipt::Plugin::getCopyright() const
{
    return "IPT by Nicolas Brochec, Joakim Borg, and Marco Fiorini. IPT Vamp plug-in by Pierre Guillot at Ircam.";
}

Ipt::Plugin::InputDomain Ipt::Plugin::getInputDomain() const
{
    return TimeDomain;
}

size_t Ipt::Plugin::getPreferredBlockSize() const
{
    return static_cast<size_t>(256);
}

size_t Ipt::Plugin::getPreferredStepSize() const
{
    return static_cast<size_t>(0);
}

Ipt::Plugin::OutputList Ipt::Plugin::getOutputDescriptors() const
{
    OutputDescriptor d;
    d.identifier = "marker";
    d.name = "Category";
    d.description = "The category of the audio content";
    d.unit = "";
    d.hasFixedBinCount = true;
    d.binCount = 0;
    d.hasKnownExtents = false;
    d.minValue = 0.0f;
    d.maxValue = 0.0f;
    d.isQuantized = false;
    d.sampleType = OutputDescriptor::SampleType::VariableSampleRate;
    d.hasDuration = false;
    return {d};
}

Ipt::Plugin::ParameterList Ipt::Plugin::getParameterDescriptors() const
{
    ParameterList list;
    auto const models = getModelPaths();
    if(!models.empty())
    {
        ParameterDescriptor param;
        param.identifier = "model";
        param.name = "Model";
        param.description = "The model used to generate the tokens";
        param.unit = "";
        for(auto const& model : models)
        {
            param.valueNames.push_back(model.filename().replace_extension().string());
        }
        param.minValue = 0.0f;
        param.maxValue = static_cast<float>(models.size());
        param.defaultValue = 0.0f;
        param.isQuantized = true;
        param.quantizeStep = 1.0f;
        list.push_back(std::move(param));
    }
    return list;
}

void Ipt::Plugin::setParameter(std::string paramid, float newval)
{
    if(paramid == "model")
    {
        auto const max = static_cast<float>(getModelPaths().size());
        mModelIndex = static_cast<size_t>(std::floor(std::clamp(newval, 0.0f, max)));
    }
    else
    {
        std::cerr << "Invalid parameter : " << paramid << "\n";
    }
}

float Ipt::Plugin::getParameter(std::string paramid) const
{
    if(paramid == "model")
    {
        return static_cast<float>(mModelIndex);
    }
    std::cerr << "Invalid parameter : " << paramid << "\n";
    return 0.0f;
}

Ipt::Plugin::OutputExtraList Ipt::Plugin::getOutputExtraDescriptors(size_t outputDescriptorIndex) const
{

    if(outputDescriptorIndex != 0)
    {
        return {};
    }
    OutputExtraList list;
    {
        OutputExtraDescriptor d;
        d.identifier = "score";
        d.name = "Score";
        d.description = "The score of the detected category";
        d.unit = "";
        d.hasKnownExtents = true;
        d.minValue = 0.0f;
        d.maxValue = 1.0f;
        d.isQuantized = false;
        d.quantizeStep = 0.0f;
        list.push_back(std::move(d));
    }
    {
        OutputExtraDescriptor d;
        d.identifier = "occurences";
        d.name = "Occurences";
        d.description = "The number of consecutive occurrences of the category";
        d.unit = "";
        d.hasKnownExtents = false;
        d.minValue = 1.0f;
        d.maxValue = 100.0f;
        d.isQuantized = true;
        d.quantizeStep = 1.0f;
        list.push_back(std::move(d));
    }
    return list;
}

void Ipt::Plugin::runModel(FeatureList& list)
{
    if(mFlatIndices.empty())
    {
        return;
    }
    if(mFlatIndices.size() * mModelSegmentLength != mFlatInputs.size())
    {
        assert(false && "Invalid input size");
        return;
    }
    auto const hopSizeSamples = static_cast<size_t>(static_cast<double>(mModelSampleRate) * kSegmentHopSizeMs / 1000.0);

#ifdef __APPLE__
    auto tensorIn = torch::from_blob(mFlatInputs.data(), {static_cast<long long>(mFlatIndices.size()), 1, static_cast<long long>(mModelSegmentLength)}, torch::kFloat32).to(kDeviceType);
#else
    auto tensorIn = torch::from_blob(mFlatInputs.data(), {static_cast<long long>(mFlatIndices.size()), 1, static_cast<long long>(mModelSegmentLength)}, torch::kFloat32);
#endif
    auto tensorOut = mModel.get_method(kForwardMethod)(std::vector<torch::jit::IValue>{tensorIn}).toTensor();
    tensorOut = torch::softmax(tensorOut, -1).to(torch::kCPU).contiguous();

    auto const numClasses = tensorOut.size(-1);
    if(numClasses != mModelClassNames.size())
    {
        mFlatInputs.clear();
        mFlatIndices.clear();
        assert(false && "Invalid input size");
        return;
    }
    auto const* frameStart = tensorOut.contiguous().data_ptr<float>();
    for(size_t index = 0; index < mFlatIndices.size(); ++index)
    {
        auto const* frameEnd = std::next(frameStart, numClasses);
        auto const* classIt = std::max_element(frameStart, frameEnd);
        if(classIt != frameEnd)
        {
            auto const classPosition = std::distance(frameStart, classIt);
            auto const classScore = *classIt;

            auto const framePosition = mFlatIndices.at(index);
            if(mLastFeature.has_value() && mLastFeature.value().first != classPosition)
            {
                list.push_back(std::move(mLastFeature.value().second));
                mLastFeature.reset();
            }

            // There is already a feature so we just upate the score
            if(mLastFeature.has_value())
            {
                auto& values = mLastFeature.value().second.values;
                values[0] = std::max(values[0], classScore);
                values[1] = values[1] + 1.0f;
            }
            else
            {
                Feature feature;
                feature.hasTimestamp = true;
                feature.timestamp = Vamp::RealTime::frame2RealTime(framePosition * hopSizeSamples, mModelSampleRate);
                feature.hasDuration = false;
                feature.label = mModelClassNames.at(classPosition);
                feature.values.push_back(classScore);
                feature.values.push_back(1.0f);
                mLastFeature = std::make_pair(classPosition, feature);
            }
        }
        frameStart = frameEnd;
    }
    mFlatInputs.clear();
    mFlatIndices.clear();
}

Ipt::Plugin::FeatureSet Ipt::Plugin::process(float const* const* inputBuffers, [[maybe_unused]] Vamp::RealTime timestamp)
{
    // Copy the input samples into the buffer (with resampling if necessary)
    auto const blockSize = mBlockSize;
    auto const* inputBuffer = inputBuffers[0];
    auto const sampleRateRatio = static_cast<double>(mModelSampleRate) / static_cast<double>(getInputSampleRate());
    if(std::abs(sampleRateRatio) < 0.0001)
    {
        if((mBuffer.size() - mBufferWritePosition) < blockSize)
        {
            mBuffer.resize(mBufferWritePosition + blockSize);
        }
        std::copy(inputBuffer, inputBuffer + blockSize, std::next(mBuffer.begin(), mBufferWritePosition));
        mBufferWritePosition += blockSize;
    }
    else
    {
        auto const expectedSize = static_cast<size_t>(std::ceil(static_cast<double>(blockSize) * sampleRateRatio));
        if((mBuffer.size() - mBufferWritePosition) < expectedSize)
        {
            mBuffer.resize(mBufferWritePosition + expectedSize, 0.0f);
        }
        auto const result = mResampler.process(blockSize, inputBuffer, expectedSize, mBuffer.data() + mBufferWritePosition);
        if(std::get<0>(result) != blockSize)
        {
            IptErr("Resampler failed to consume all input samples! Consumed " << std::get<0>(result) << " out of " << blockSize);
        }
        mBufferWritePosition += std::get<1>(result);
    }

    FeatureList fl;
    auto const hopSizeSamples = static_cast<size_t>(static_cast<double>(mModelSampleRate) * kSegmentHopSizeMs / 1000.0);
    // Perform the available frames
    while(mBufferReadPosition + hopSizeSamples < mBufferWritePosition)
    {
        auto const copyPositionEnd = mBufferReadPosition + hopSizeSamples;
        auto const copyPositionStart = copyPositionEnd - std::min(mModelSegmentLength, copyPositionEnd);
        auto const zeroPaddingSize = copyPositionEnd < mModelSegmentLength ? mModelSegmentLength - copyPositionEnd : 0;
        std::fill_n(mIntermediary.begin(), zeroPaddingSize, 0.0f);
        std::copy_n(std::next(mBuffer.cbegin(), static_cast<long>(copyPositionStart)), copyPositionEnd - copyPositionStart, std::next(mIntermediary.begin(), static_cast<long>(zeroPaddingSize)));

        // Check if the segment is above the minimum threshold in dB
        // using sub part of size kSegmentHopSizeMs
        auto const isAboveThreshold = [&, this]()
        {
            size_t i = 0;
            while(i < mIntermediary.size())
            {
                auto const numSamples = std::min(mIntermediary.size() - i, hopSizeSamples);
                auto const startIt = std::next(mIntermediary.begin(), static_cast<long>(i));
                auto const endIt = std::next(startIt, static_cast<long>(numSamples));
                if(std::sqrt(std::accumulate(startIt, endIt, 0.0f, [](auto const sum, const auto amp)
                                             {
                                                 return sum + amp * amp;
                                             }) /
                             static_cast<float>(numSamples)) >= kMinimumEnergyThreshold)
                {
                    return true;
                }
                i += numSamples;
            }
            return false;
        }();

        if(isAboveThreshold)
        {
            mFlatInputs.insert(mFlatInputs.end(), mIntermediary.cbegin(), mIntermediary.cend());
            mFlatIndices.push_back(mNumFrameProcessed);
            if(mFlatIndices.size() >= kBatchSize)
            {
                runModel(fl);
            }
        }

        mBufferReadPosition += hopSizeSamples;
        ++mNumFrameProcessed;
    }
    auto const oldFrames = mBufferReadPosition > mModelSegmentLength ? mBufferReadPosition - mModelSegmentLength : 0;
    auto const numFrames = mBufferWritePosition - oldFrames;
    std::copy_n(std::next(mBuffer.cbegin(), static_cast<long>(oldFrames)), numFrames, mBuffer.begin());
    mBufferWritePosition -= oldFrames;
    mBufferReadPosition -= oldFrames;

    return {{0, fl}};
}

Ipt::Plugin::FeatureSet Ipt::Plugin::getRemainingFeatures()
{
    FeatureList fl;
    runModel(fl);
    if(mLastFeature.has_value())
    {
        fl.push_back(std::move(mLastFeature.value().second));
        mLastFeature.reset();
    }
    return {{0, fl}};
}

#ifdef __cplusplus
extern "C"
{
#endif
    VampPluginDescriptor const* vampGetPluginDescriptor(unsigned int version, unsigned int index)
    {
        if(version < 1)
        {
            return nullptr;
        }
        switch(index)
        {
            case 0:
            {
                static Vamp::PluginAdapter<Ipt::Plugin> adaptater;
                return adaptater.getDescriptor();
            }
            default:
            {
                return nullptr;
            }
        }
    }

    IVE_EXTERN IvePluginDescriptor const* iveGetPluginDescriptor(unsigned int version, unsigned int index)
    {
        if(version < 1)
        {
            return nullptr;
        }
        switch(index)
        {
            default:
            {
                return Ive::PluginAdapter::getDescriptor<Ipt::Plugin>();
            }
        }
    }
#ifdef __cplusplus
}
#endif

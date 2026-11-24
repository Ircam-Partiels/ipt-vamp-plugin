# IPT Vamp Plugin
The IPT Vamp plugin is an implementation of IPT, a toolkit for the real-time recognition of instrumental playing techniques using deep learning CNN models, developed at Ircam and Tokyo University of the Arts, and supported by the European Research Council (ERC) as part of the Raising Co-creativity in Cyber-Human Musicianship (REACH) Project directed by Gérard Assayag, under the European Union's Horizon 2020 research and innovation program (GA #883313).

## Installation

Download the IPT Vamp plugin installation package for your operating system from the [Releases](https://github.com/Ircam-Partiels/ipt-vamp-plugin/releases) section and run the installer.

## Compilation

The compilation system is based on [CMake](https://cmake.org/), for example:
```
cmake . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Credits
- **[IPT Vamp plugin](https://www.ircam.fr/)** by Pierre Guillot, Gaston Fontagneu at [IRCAM IMR department](https://www.ircam.fr/).  
- **[IPT](http://repmus.ircam.fr/ipt/home)** by Nicolas Brochec, Joakim Borg, and Marco Fiorini at IRCAM.
- **[Vamp SDK](https://github.com/vamp-plugins/vamp-plugin-sdk)** by Chris Cannam, copyright (c) 2005-2024 Chris Cannam and Centre for Digital Music, Queen Mary, University of London.
- **[Ircam Vamp Extension](https://github.com/Ircam-Partiels/ircam-vamp-extension)** by Pierre Guillot at [IRCAM IMR department](https://www.ircam.fr/).  

## License

The IPT Vamp plugin is distributed under the [MIT License](LICENSE). It includes third-party software and models that remain under their own licenses:

| Component | License |
| --- | --- |
| [LibTorch](https://github.com/pytorch/pytorch) 2.4.1 (and the third-party libraries bundled with PyTorch) | [BSD 3-Clause License](resources/licenses/libtorch.txt) (see also the [notice](resources/licenses/libtorch-NOTICE.txt)) |
| [AsmJit](https://github.com/asmjit/asmjit) (included in LibTorch on Windows) | [zlib License](resources/licenses/asmjit.txt) |
| [libgomp](https://gcc.gnu.org/onlinedocs/libgomp/) (included in LibTorch on Linux) | [GPLv3](resources/licenses/libgomp-COPYING3.txt) with the [GCC Runtime Library Exception 3.1](resources/licenses/libgomp-COPYING.RUNTIME.txt) |
| [Intel oneAPI Math Kernel Library](https://www.intel.com/content/www/us/en/developer/tools/oneapi/onemkl.html) and Intel OpenMP Runtime Library (used by LibTorch on Windows) | Intel Simplified Software License |
| [libuv](https://github.com/libuv/libuv) (used by LibTorch on Windows) | MIT License |
| [Vamp Plugin SDK](https://github.com/vamp-plugins/vamp-plugin-sdk) | MIT-style license (and BSD 3-Clause for KissFFT) |
| [Ircam Vamp Extension](https://github.com/Ircam-Partiels/ircam-vamp-extension) | [MIT License](ircam-vamp-extension/LICENSE) |
| Lagrange resampler derived from [JUCE](https://github.com/juce-framework/JUCE) 7 (juce_audio_basics module) | ISC License |
| [IPT models](https://huggingface.co/nbrochec/ipt_models) (electric guitar, flute, trumpet and trumpet with harmon mute) by Nicolas Brochec, Joakim Borg and Marco Fiorini | [CC BY-NC 4.0](resources/licenses/libipt.txt), the license of the [libipt](https://github.com/nbrochec/libipt) project |

The IPT models are distributed under the Creative Commons Attribution-NonCommercial 4.0 International license: commercial use of the models is not permitted. The source code of the plugin remains under the MIT License, but the plugin bundle and the installers embed the models, hence they can only be used for non-commercial purposes. The [third-party notices](resources/ThirdPartyNotices.txt) and the full texts of the licenses are copied into the `Resources/licenses` folder of the plugin bundle, hence they are distributed with the plugin and the installers.

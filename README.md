# IPT Vamp Plugin
The IPT Vamp plugin is an implementation of IPT, a toolkit for the real-time recognition of instrumental playing techniques using deep learning CNN models, developed at Ircam and Tokyo University of the Arts, and supported by the European Research Council (ERC) as part of the Raising Co-creativity in Cyber-Human Musicianship (REACH) Project directed by Gérard Assayag, under the European Union's Horizon 2020 research and innovation program (GA #883313).

## Installation

Download the Whisper Vamp plugin installation package for your operating system from the [Releases](https://github.com/Ircam-Partiels/ipt-vamp-plugin/releases) section and run the installer. 

## Compilation

The compilation system is based on [CMake](https://cmake.org/), for example:
```
cmake . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Credits
- **[IPT Vamp plugin](https://www.ircam.fr/)** by Pierre Guillot at [IRCAM IMR department](https://www.ircam.fr/).  
- **[IPT](http://repmus.ircam.fr/ipt/home)** by Nicolas Brochec, Joakim Borg, and Marco Fiorini at IRCAM.
- **[Vamp SDK](https://github.com/vamp-plugins/vamp-plugin-sdk)** by Chris Cannam, copyright (c) 2005-2024 Chris Cannam and Centre for Digital Music, Queen Mary, University of London.
- **[Ircam Vamp Extension](https://github.com/Ircam-Partiels/ircam-vamp-extension)** by Pierre Guillot at [IRCAM IMR department](https://www.ircam.fr/).  

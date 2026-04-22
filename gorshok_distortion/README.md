# Gorshok Distortion

A professional-grade audio plugin (VST3-style) for high-gain guitar processing, written in C and x86-64 Assembly (NASM) with SSE/AVX optimizations.

## Features

- **High-Gain Distortion**: Hard clipping algorithm inspired by classic Marshall amps
- **Low-Pass Filter**: Adjustable tone control (2000Hz - 8000Hz cutoff)
- **Assembly Optimization**: SSE and AVX instructions for ultra-low latency
- **Parameters**: Gain (0-10), Tone (0-1), Output Level (0-1)

## Project Structure

```
gorshok_distortion/
├── src/
│   ├── dsp_core.asm    # Assembly DSP kernel (SSE/AVX)
│   └── main.c          # Plugin logic and API
├── CMakeLists.txt      # Build system
├── .github/
│   └── workflows/
│       └── build.yml   # CI/CD automation
└── README.md
```

## Building

### Prerequisites

- **Windows**: Visual Studio or MinGW-w64 + NASM
- **Linux**: GCC/Clang + NASM + CMake

### Build Commands

```bash
# Create build directory
mkdir build && cd build

# Configure
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_AVX=ON

# Build
cmake --build . --config Release
```

### Output

- **Windows**: `build/lib/GorshokDistortion.dll`
- **Linux**: `build/lib/libGorshokDistortion.so`

## Parameters

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Gain | 0.0 - 10.0 | 5.0 | Pre-distortion gain |
| Tone | 0.0 - 1.0 | 0.5 | Low-pass filter cutoff |
| Output | 0.0 - 1.0 | 0.7 | Output volume level |

## DSP Algorithm

### Distortion Chain

1. **Pre-gain**: Amplify input signal
2. **Soft clipping**: Polynomial approximation of tanh
3. **Hard clipping**: Clamp to [-1, 1] range
4. **Low-pass filter**: Single-pole IIR filter
5. **Output scaling**: Apply output level

### Assembly Optimization

- **SSE**: Process 4 samples at a time (stereo)
- **AVX**: Process 8 samples at a time (faster)
- Target latency: < 5ms at 48kHz

## CI/CD

GitHub Actions automatically:
1. Installs NASM and CMake on Ubuntu
2. Compiles C and Assembly code
3. Uploads binary as `Gorshok_Distortion_Plugin` artifact

## License

MIT License - "KOROL I SHUT PRESET"
// filepath: gorshok_distortion\src\main.c
/**
 * Gorshok Distortion - Audio Plugin Main Entry Point
 * High-gain guitar distortion plugin with assembly-optimized DSP
 * VST3-style plugin structure for professional audio applications
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

// =============================================================================
// Plugin Configuration
// =============================================================================

#define PLUGIN_NAME "Gorshok Distortion"
#define PLUGIN_VENDOR "Korol i Shut"
#define PLUGIN_VERSION "1.0.0"
#define MAX_BUFFER_SIZE 4096
#define SAMPLE_RATE 48000.0

// =============================================================================
// Plugin Parameters
// =============================================================================

typedef struct {
    float gain;          // 0.0 - 10.0 (default: 5.0)
    float tone;          // 0.0 - 1.0 (default: 0.5)
    float output_level;  // 0.0 - 1.0 (default: 0.7)
} PluginParameters;

// =============================================================================
// External Assembly Functions
// =============================================================================

// SSE-optimized processing (compatible with older CPUs)
extern void process_audio_sse(
    float* input_buffer,
    float* output_buffer,
    uint32_t sample_count,
    float gain,
    float tone,
    float output_level
);

// AVX-optimized processing (faster on modern CPUs)
extern void process_audio_avx(
    float* input_buffer,
    float* output_buffer,
    uint32_t sample_count,
    float gain,
    float tone,
    float output_level
);

// Filter initialization
extern void init_filter_coefficients(float tone);
extern void update_filter_tone(float tone);

// =============================================================================
// Plugin State
// =============================================================================

typedef struct {
    PluginParameters params;
    float* input_buffer;
    float* output_buffer;
    uint32_t buffer_size;
    uint32_t sample_rate;
    int use_avx;
    int initialized;
} GorshokPlugin;

// =============================================================================
// Plugin API Functions
// =============================================================================

/**
 * Create a new plugin instance
 */
GorshokPlugin* plugin_create(uint32_t sample_rate, uint32_t buffer_size) {
    GorshokPlugin* plugin = (GorshokPlugin*)calloc(1, sizeof(GorshokPlugin));
    if (!plugin) {
        return NULL;
    }
    
    plugin->sample_rate = sample_rate > 0 ? sample_rate : (uint32_t)SAMPLE_RATE;
    plugin->buffer_size = buffer_size > 0 ? buffer_size : MAX_BUFFER_SIZE;
    
    // Allocate audio buffers
    plugin->input_buffer = (float*)calloc(plugin->buffer_size * 2, sizeof(float));  // Stereo
    plugin->output_buffer = (float*)calloc(plugin->buffer_size * 2, sizeof(float));
    
    if (!plugin->input_buffer || !plugin->output_buffer) {
        plugin_destroy(plugin);
        return NULL;
    }
    
    // Set default parameters
    plugin->params.gain = 5.0f;
    plugin->params.tone = 0.5f;
    plugin->params.output_level = 0.7f;
    
    // Detect CPU capabilities (simplified - assume AVX available)
    plugin->use_avx = 1;  // Can be extended with CPUID detection
    
    plugin->initialized = 1;
    
    printf("[Gorshok] Plugin created: %s v%s\n", PLUGIN_NAME, PLUGIN_VERSION);
    printf("[Gorshok] Sample rate: %u Hz, Buffer size: %u samples\n", 
           plugin->sample_rate, plugin->buffer_size);
    printf("[Gorshok] Using %s optimization\n", plugin->use_avx ? "AVX" : "SSE");
    
    return plugin;
}

/**
 * Destroy plugin instance
 */
void plugin_destroy(GorshokPlugin* plugin) {
    if (!plugin) return;
    
    if (plugin->input_buffer) free(plugin->input_buffer);
    if (plugin->output_buffer) free(plugin->output_buffer);
    free(plugin);
    
    printf("[Gorshok] Plugin destroyed\n");
}

/**
 * Set plugin parameter
 */
int plugin_set_parameter(GorshokPlugin* plugin, const char* name, float value) {
    if (!plugin || !name) return -1;
    
    if (strcmp(name, "gain") == 0) {
        plugin->params.gain = fmaxf(0.0f, fminf(10.0f, value));
        printf("[Gorshok] Gain set to: %.2f\n", plugin->params.gain);
    } else if (strcmp(name, "tone") == 0) {
        plugin->params.tone = fmaxf(0.0f, fminf(1.0f, value));
        // Update filter coefficients
        init_filter_coefficients(plugin->params.tone);
        printf("[Gorshok] Tone set to: %.2f\n", plugin->params.tone);
    } else if (strcmp(name, "output") == 0) {
        plugin->params.output_level = fmaxf(0.0f, fminf(1.0f, value));
        printf("[Gorshok] Output level set to: %.2f\n", plugin->params.output_level);
    } else {
        return -1;
    }
    
    return 0;
}

/**
 * Get plugin parameter
 */
float plugin_get_parameter(GorshokPlugin* plugin, const char* name) {
    if (!plugin || !name) return 0.0f;
    
    if (strcmp(name, "gain") == 0) return plugin->params.gain;
    if (strcmp(name, "tone") == 0) return plugin->params.tone;
    if (strcmp(name, "output") == 0) return plugin->params.output_level;
    
    return 0.0f;
}

/**
 * Process audio buffer
 * Input/Output: Interleaved stereo float buffers
 */
void plugin_process(
    GorshokPlugin* plugin,
    float* input,
    float* output,
    uint32_t frame_count
) {
    if (!plugin || !input || !output || frame_count == 0) {
        return;
    }
    
    // Ensure we don't exceed buffer size
    uint32_t samples = frame_count;
    if (samples > plugin->buffer_size) {
        samples = plugin->buffer_size;
    }
    
    // Call the appropriate assembly kernel
    if (plugin->use_avx) {
        process_audio_avx(
            input,
            output,
            samples,
            plugin->params.gain,
            plugin->params.tone,
            plugin->params.output_level
        );
    } else {
        process_audio_sse(
            input,
            output,
            samples,
            plugin->params.gain,
            plugin->params.tone,
            plugin->params.output_level
        );
    }
}

/**
 * Reset plugin state (called when transport stops)
 */
void plugin_reset(GorshokPlugin* plugin) {
    if (!plugin) return;
    
    // Clear filter states by reinitializing
    init_filter_coefficients(plugin->params.tone);
    
    printf("[Gorshok] Plugin state reset\n");
}

// =============================================================================
// VST3-Style Plugin Descriptor (Simplified)
// =============================================================================

typedef struct {
    char name[64];
    char vendor[64];
    char version[16];
    uint32_t num_params;
    uint32_t num_inputs;
    uint32_t num_outputs;
} PluginDescriptor;

void plugin_get_descriptor(PluginDescriptor* desc) {
    if (!desc) return;
    
    strncpy(desc->name, PLUGIN_NAME, sizeof(desc->name) - 1);
    strncpy(desc->vendor, PLUGIN_VENDOR, sizeof(desc->vendor) - 1);
    strncpy(desc->version, PLUGIN_VERSION, sizeof(desc->version) - 1);
    desc->num_params = 3;        // Gain, Tone, Output
    desc->num_inputs = 2;        // Stereo input
    desc->num_outputs = 2;       // Stereo output
}

// =============================================================================
// Main - Standalone Test / DLL Entry Point
// =============================================================================

#ifdef _WIN32
__declspec(dllexport)
#endif
void* plugin_entry_point(void) {
    // This would be the VST3 plugin entry in a real implementation
    return NULL;
}

// Test main function
#ifdef GORSHOK_TEST
int main(int argc, char* argv[]) {
    printf("=== Gorshok Distortion Plugin Test ===\n\n");
    
    // Create plugin
    GorshokPlugin* plugin = plugin_create(48000, 512);
    if (!plugin) {
        fprintf(stderr, "Failed to create plugin\n");
        return 1;
    }
    
    // Test parameters
    printf("\n--- Testing Parameters ---\n");
    plugin_set_parameter(plugin, "gain", 7.5f);
    plugin_set_parameter(plugin, "tone", 0.3f);
    plugin_set_parameter(plugin, "output", 0.8f);
    
    printf("\nGain: %.2f\n", plugin_get_parameter(plugin, "gain"));
    printf("Tone: %.2f\n", plugin_get_parameter(plugin, "tone"));
    printf("Output: %.2f\n", plugin_get_parameter(plugin, "output"));
    
    // Generate test signal (simple sine wave)
    printf("\n--- Processing Test Signal ---\n");
    uint32_t test_samples = 256;
    float* test_input = (float*)calloc(test_samples * 2, sizeof(float));
    float* test_output = (float*)calloc(test_samples * 2, sizeof(float));
    
    // Generate 440 Hz test tone
    for (uint32_t i = 0; i < test_samples; i++) {
        float sample = sinf(2.0f * 3.14159f * 440.0f * (float)i / 48000.0f) * 0.5f;
        test_input[i * 2] = sample;     // Left
        test_input[i * 2 + 1] = sample; // Right
    }
    
    // Process
    plugin_process(plugin, test_input, test_output, test_samples);
    
    // Show some output samples
    printf("First 8 output samples (L/R):\n");
    for (uint32_t i = 0; i < 8; i++) {
        printf("  [%u] L: %.4f  R: %.4f\n", i, test_output[i*2], test_output[i*2+1]);
    }
    
    // Cleanup
    free(test_input);
    free(test_output);
    plugin_destroy(plugin);
    
    printf("\n=== Test Complete ===\n");
    return 0;
}
#endif
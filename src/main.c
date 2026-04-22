// filepath: gorshok_distortion\src\main.c
/**
 * Gorshok Distortion - Audio Plugin (Simplified)
 * High-gain guitar distortion plugin
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// =============================================================================
// Plugin Configuration
// =============================================================================

#define PLUGIN_NAME "Gorshok Distortion"
#define PLUGIN_VERSION "1.0.0"
#define MAX_BUFFER_SIZE 4096
#define SAMPLE_RATE 48000

// =============================================================================
// Plugin Parameters
// =============================================================================

typedef struct {
    float gain;
    float tone;
    float output_level;
} PluginParameters;

// =============================================================================
// Plugin State
// =============================================================================

typedef struct {
    PluginParameters params;
    float* input_buffer;
    float* output_buffer;
    uint32_t buffer_size;
    uint32_t sample_rate;
    int initialized;
} GorshokPlugin;

// =============================================================================
// External Assembly Functions - C linkage
// =============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// Assembly DSP functions - simple names matching .asm
extern void gorshok_process_asm(
    float* input_buffer,
    float* output_buffer,
    uint32_t sample_count,
    float gain,
    float tone,
    float output_level
);

extern void gorshok_init_filter(float tone);

#ifdef __cplusplus
}
#endif

// =============================================================================
// Plugin API Functions
// =============================================================================

GorshokPlugin* plugin_create(uint32_t sample_rate, uint32_t buffer_size) {
    GorshokPlugin* plugin = (GorshokPlugin*)calloc(1, sizeof(GorshokPlugin));
    if (!plugin) {
        return NULL;
    }
    
    plugin->sample_rate = (sample_rate > 0) ? sample_rate : SAMPLE_RATE;
    plugin->buffer_size = (buffer_size > 0) ? buffer_size : MAX_BUFFER_SIZE;
    
    plugin->input_buffer = (float*)calloc(plugin->buffer_size * 2, sizeof(float));
    plugin->output_buffer = (float*)calloc(plugin->buffer_size * 2, sizeof(float));
    
    if (!plugin->input_buffer || !plugin->output_buffer) {
        if (plugin->input_buffer) free(plugin->input_buffer);
        if (plugin->output_buffer) free(plugin->output_buffer);
        free(plugin);
        return NULL;
    }
    
    plugin->params.gain = 5.0f;
    plugin->params.tone = 0.5f;
    plugin->params.output_level = 0.7f;
    plugin->initialized = 1;
    
    printf("[Gorshok] Plugin created: %s v%s\n", PLUGIN_NAME, PLUGIN_VERSION);
    
    return plugin;
}

void plugin_destroy(GorshokPlugin* plugin) {
    if (!plugin) return;
    
    if (plugin->input_buffer) free(plugin->input_buffer);
    if (plugin->output_buffer) free(plugin->output_buffer);
    free(plugin);
    
    printf("[Gorshok] Plugin destroyed\n");
}

int plugin_set_parameter(GorshokPlugin* plugin, const char* name, float value) {
    if (!plugin || !name) return -1;
    
    if (strcmp(name, "gain") == 0) {
        plugin->params.gain = (value < 0.0f) ? 0.0f : (value > 10.0f) ? 10.0f : value;
    } else if (strcmp(name, "tone") == 0) {
        plugin->params.tone = (value < 0.0f) ? 0.0f : (value > 1.0f) ? 1.0f : value;
        gorshok_init_filter(plugin->params.tone);
    } else if (strcmp(name, "output") == 0) {
        plugin->params.output_level = (value < 0.0f) ? 0.0f : (value > 1.0f) ? 1.0f : value;
    } else {
        return -1;
    }
    
    return 0;
}

float plugin_get_parameter(GorshokPlugin* plugin, const char* name) {
    if (!plugin || !name) return 0.0f;
    
    if (strcmp(name, "gain") == 0) return plugin->params.gain;
    if (strcmp(name, "tone") == 0) return plugin->params.tone;
    if (strcmp(name, "output") == 0) return plugin->params.output_level;
    
    return 0.0f;
}

void plugin_process(GorshokPlugin* plugin, float* input, float* output, uint32_t frame_count) {
    if (!plugin || !input || !output || frame_count == 0) {
        return;
    }
    
    uint32_t samples = (frame_count > plugin->buffer_size) ? plugin->buffer_size : frame_count;
    
    // Call assembly kernel
    gorshok_process_asm(
        input,
        output,
        samples,
        plugin->params.gain,
        plugin->params.tone,
        plugin->params.output_level
    );
}

void plugin_reset(GorshokPlugin* plugin) {
    if (!plugin) return;
    gorshok_init_filter(plugin->params.tone);
}

// =============================================================================
// Main - Test
// =============================================================================

#ifdef GORSHOK_TEST

int main(int argc, char* argv[]) {
    printf("=== Gorshok Distortion Plugin Test ===\n\n");
    
    GorshokPlugin* plugin = plugin_create(48000, 512);
    if (!plugin) {
        fprintf(stderr, "Failed to create plugin\n");
        return 1;
    }
    
    printf("\n--- Testing Parameters ---\n");
    plugin_set_parameter(plugin, "gain", 7.5f);
    plugin_set_parameter(plugin, "tone", 0.3f);
    plugin_set_parameter(plugin, "output", 0.8f);
    
    printf("\nGain: %.2f\n", plugin_get_parameter(plugin, "gain"));
    printf("Tone: %.2f\n", plugin_get_parameter(plugin, "tone"));
    printf("Output: %.2f\n", plugin_get_parameter(plugin, "output"));
    
    printf("\n--- Processing Test Signal ---\n");
    uint32_t test_samples = 256;
    float* test_input = (float*)calloc(test_samples * 2, sizeof(float));
    float* test_output = (float*)calloc(test_samples * 2, sizeof(float));
    
    // Simple test signal - no math functions needed
    for (uint32_t i = 0; i < test_samples; i++) {
        float sample = (float)((i % 128) - 64) / 64.0f;  // Simple triangle wave
        test_input[i * 2] = sample;
        test_input[i * 2 + 1] = sample;
    }
    
    plugin_process(plugin, test_input, test_output, test_samples);
    
    printf("First 8 output samples (L/R):\n");
    for (uint32_t i = 0; i < 8; i++) {
        printf("  [%u] L: %.4f  R: %.4f\n", i, test_output[i*2], test_output[i*2+1]);
    }
    
    free(test_input);
    free(test_output);
    plugin_destroy(plugin);
    
    printf("\n=== Test Complete ===\n");
    return 0;
}

#endif
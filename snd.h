/**
 * Sound library abstraction
 * implemented in OpenAL 1.1
 */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
 

/* This macro turns on extra redundant checking */
#ifndef NDEBUG
#define SND_DEBUG
#endif

/* This macro turns off all checking, use carefully! You should also turn off SND_DEBUG since that's not directly coupled */
#if 0
#define SND_NO_CHECKS
#endif
 

typedef struct float32_vec3_t {
    float x, y, z;
} float32_vec3_t;

typedef enum snd_result_t {
    SND_OK = 0,
    SND_ERROR_AL_NOT_PRESENT = -1,
    SND_ERROR_OUT_OF_MEMORY = -2,
    SND_ERROR_UNKNOWN = -3,
    SND_ERROR_INVALID_PARAM = -4,
    SND_ERROR_BUFFER_STILL_IN_USE = -5,
    SND_ERROR_RECORDING_DEVICE_INVALID = -6,
    SND_ERROR_RECORDING_NOT_AVAILABLE = -7,
    SND_ERROR_DEVICE_OUT_OF_LISTENING_CONTEXTS = -8,
    SND_ERROR_LISTENING_CONTEXT_OUT_OF_SOURCES = -9,
    SND_ERROR_SOURCE_STATICALLY_BOUND = -10,
    SND_ERROR_SOURCE_NO_BUFFERS_QUEUED = -11,
    SND_ERROR_SOURCE_NOT_ENOUGH_QUEUED_BUFFERS_FINISHED = -12,
    SND_RESULT_MAX_ENUM = 0x7fffffff
} snd_result_t;
typedef enum snd_format_t {
    SND_FORMAT_PCM_UINT8_MONO = 0,
    SND_FORMAT_PCM_INT16_MONO = 1,
    SND_FORMAT_PCM_UINT8_STEREO_INTERLEAVED_LR = 2,
    SND_FORMAT_PCM_INT16_STEREO_INTERLEAVED_LR = 3,
    SND_FORMAT_MAX_ENUM = 0x7fffffff
} snd_format_t;
typedef enum snd_distance_model_type_t {
    SND_DISTANCE_MODEL_TYPE_INVERSE_DISTANCE = 0,
    SND_DISTANCE_MODEL_TYPE_INVERSE_DISTANCE_CLAMPED = 1,
    SND_DISTANCE_MODEL_TYPE_LINEAR_DISTANCE = 2,
    SND_DISTANCE_MODEL_TYPE_LINEAR_DISTANCE_CLAMPED = 3,
    SND_DISTANCE_MODEL_TYPE_EXPONENT_DISTANCE = 4,
    SND_DISTANCE_MODEL_TYPE_EXPONENT_DISTANCE_CLAMPED = 5,
    SND_DISTANCE_MODEL_TYPE_NONE = 6,
    SND_DISTANCE_MODEL_TYPE_MAX_ENUM = 0x7fffffff,
} snd_distance_model_type_t;
typedef enum snd_source_position_format_t {
    SND_SOURCE_POSITION_FORMAT_SECONDS = 0,
    SND_SOURCE_POSITION_FORMAT_SAMPLES = 1,
    SND_SOURCE_POSITION_FORMAT_BYTES = 2,
    SND_SOURCE_POSITION_FORMAT_MAX_ENUM = 0x7fffffff
} snd_source_position_format_t;

typedef struct snd_device_list_t {
    int                              nr_playback_devices;
    int                              playback_devices_default_id;
    const char**                     playback_devices;
    int                              nr_recording_devices;
    int                              recording_devices_default_id;
    const char**                     recording_devices;
} snd_device_list_t;
typedef struct snd_recording_device_t {
    void* handle;
} snd_recording_device_t;
typedef struct snd_listener_context_t {
    void* handle;
} snd_listener_context_t;
typedef struct snd_listener_context_params_t {
    snd_distance_model_type_t distance_model;
    float doppler_factor, speed_of_sound, gain_multiplier;
    float32_vec3_t position, velocity;
    struct { float32_vec3_t forward, up; } orientation;
} snd_listener_context_params_t __attribute__((packed));
typedef struct snd_buffer_t {
    uint32_t id;
} snd_buffer_t;
typedef struct snd_source_t {
    uint32_t id;
} snd_source_t;
typedef struct snd_source_params_t {
    struct {
        float multiplier, min, max, outer_angle_secondary_multiplier;
    } gain;
    struct {
        float inner_angle, outer_angle;
    } cone;
    struct {
        float reference, max, rolloff_factor;
    } distance;
    float pitch_shift_multiplier;
    float32_vec3_t position, velocity, direction;
    bool position_relative_to_listener, looping;
} snd_source_params_t __attribute__((packed));

snd_result_t snd_init(snd_device_list_t* out_device_list);
snd_result_t snd_exit(void);

snd_result_t snd_recording_device_open(uint32_t recording_device_id, snd_format_t format, uint32_t frequency_hz, size_t internal_buffer_size, snd_recording_device_t* device);
snd_result_t snd_recording_device_close(snd_recording_device_t device);
snd_result_t snd_recording_start(snd_recording_device_t device);
snd_result_t snd_recording_stop(snd_recording_device_t device);
snd_result_t snd_recording_get_nr_samples(snd_recording_device_t device, size_t* nr_samples);
snd_result_t snd_recording_retrieve_samples_nonblocking(snd_recording_device_t device, void* buffer, size_t nr_samples);

snd_result_t snd_listener_context_create(uint32_t playback_device_id, uint32_t mixing_frequency_Hz, uint32_t refresh_interval_Hz, bool synchronous, uint32_t requested_min_nr_mono_sources, uint32_t requested_min_nr_stereo_sources, snd_listener_context_t* context);
snd_result_t snd_listener_context_params_get(snd_listener_context_t context, snd_listener_context_params_t* params);
snd_result_t snd_listener_context_params_set(snd_listener_context_t context, const snd_listener_context_params_t params);
snd_result_t snd_listener_context_process(snd_listener_context_t context);
snd_result_t snd_listener_context_suspend(snd_listener_context_t context);
snd_result_t snd_listener_context_destroy(snd_listener_context_t context);

snd_result_t snd_buffer_alloc(snd_listener_context_t context, snd_format_t format, uint32_t frequency_hz, void* data, size_t size, snd_buffer_t* buffer);
snd_result_t snd_buffer_free(snd_listener_context_t context, snd_buffer_t buffer);

snd_result_t snd_source_create(snd_listener_context_t context, snd_source_t* source);
snd_result_t snd_source_delete(snd_listener_context_t context, snd_source_t source);
snd_result_t snd_source_params_get(snd_listener_context_t context, snd_source_t source, snd_source_params_t* params);
snd_result_t snd_source_params_set(snd_listener_context_t context, snd_source_t source, const snd_source_params_t params);

snd_result_t snd_source_play_position_set(snd_listener_context_t context, snd_source_t source, snd_source_position_format_t format, float value);
snd_result_t snd_source_play_position_get(snd_listener_context_t context, snd_source_t source, snd_source_position_format_t format, float* out_value);
snd_result_t snd_source_queue_position_get(snd_listener_context_t context, snd_source_t source, uint32_t* nr_buffers_in_queue, uint32_t* currently_playing_index);

snd_result_t snd_source_static_buffer(snd_listener_context_t context, snd_source_t source, snd_buffer_t buffer);
snd_result_t snd_source_queue_buffers(snd_listener_context_t context, snd_source_t source, uint32_t nr_buffers, snd_buffer_t* buffers);
snd_result_t snd_source_unqueue_buffers(snd_listener_context_t context, snd_source_t source, uint32_t nr_buffers, snd_buffer_t* out_buffers);
snd_result_t snd_source_reset_buffer_state(snd_listener_context_t context, snd_source_t source);

snd_result_t snd_sources_play  (snd_listener_context_t context, uint32_t nr_sources, snd_source_t* sources);
snd_result_t snd_sources_pause (snd_listener_context_t context, uint32_t nr_sources, snd_source_t* sources);
snd_result_t snd_sources_stop  (snd_listener_context_t context, uint32_t nr_sources, snd_source_t* sources);
snd_result_t snd_sources_rewind(snd_listener_context_t context, uint32_t nr_sources, snd_source_t* sources);




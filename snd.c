#include "snd.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <stdio.h>

#define AL_NO_PROTOTYPES
#define ALC_NO_PROTOTYPES
#include <AL/al.h>
#include <AL/alc.h>

/* Function loading facilities from alad: modelled after GLFW 3.3, see win32_module.c and posix_module.c specifically */
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32) || defined(__MINGW32__)
#define NOMINMAX
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else /* Unix defaults otherwise */
#include <dlfcn.h>
#endif /* _WIN32 */




//TODO: check all alGetError vs alcGetError!
//TODO: add constraints for the getters/setters according to property info in spec! (no NaN etc.)




#define SND_INITIAL_ARRAY_CAP 1024


static struct g_al {
    void*                            module;
    snd_device_list_t                devices;
    ALCdevice**                      playback_device_handles;
    struct {
        LPALCCREATECONTEXT               CreateContext;
        LPALCMAKECONTEXTCURRENT          MakeContextCurrent;
        LPALCPROCESSCONTEXT              ProcessContext;
        LPALCSUSPENDCONTEXT              SuspendContext;
        LPALCDESTROYCONTEXT              DestroyContext;
        LPALCGETCURRENTCONTEXT           GetCurrentContext;
        LPALCGETCONTEXTSDEVICE           GetContextsDevice;
        LPALCOPENDEVICE                  OpenDevice;
        LPALCCLOSEDEVICE                 CloseDevice;
        LPALCGETERROR                    GetError;
        LPALCGETSTRING                   GetString;
        LPALCGETINTEGERV                 GetIntegerv;
        LPALCCAPTUREOPENDEVICE           CaptureOpenDevice;
        LPALCCAPTURECLOSEDEVICE          CaptureCloseDevice;
        LPALCCAPTURESTART                CaptureStart;
        LPALCCAPTURESTOP                 CaptureStop;
        LPALCCAPTURESAMPLES              CaptureSamples;
    } c;
    LPALGETSTRING                    GetString;
    LPALGETINTEGER                   GetInteger;
    LPALGETFLOAT                     GetFloat;
    LPALGETERROR                     GetError;
    LPALDOPPLERFACTOR                DopplerFactor;
    LPALSPEEDOFSOUND                 SpeedOfSound;
    LPALDISTANCEMODEL                DistanceModel;
    LPALLISTENERF                    Listenerf;
    LPALLISTENERFV                   Listenerfv;
    LPALGETLISTENERF                 GetListenerf;
    LPALGETLISTENERFV                GetListenerfv;
    LPALGENSOURCES                   GenSources;
    LPALDELETESOURCES                DeleteSources;
    LPALISSOURCE                     IsSource;
    LPALSOURCEF                      Sourcef;
    LPALSOURCEFV                     Sourcefv;
    LPALSOURCEI                      Sourcei;
    LPALGETSOURCEF                   GetSourcef;
    LPALGETSOURCEFV                  GetSourcefv;
    LPALGETSOURCEI                   GetSourcei;
    LPALSOURCEPLAYV                  SourcePlayv;
    LPALSOURCESTOPV                  SourceStopv;
    LPALSOURCEREWINDV                SourceRewindv;
    LPALSOURCEPAUSEV                 SourcePausev;
    LPALSOURCEQUEUEBUFFERS           SourceQueueBuffers;
    LPALSOURCEUNQUEUEBUFFERS         SourceUnqueueBuffers;
    LPALGENBUFFERS                   GenBuffers;
    LPALDELETEBUFFERS                DeleteBuffers;
    LPALISBUFFER                     IsBuffer;
    LPALBUFFERDATA                   BufferData;
    LPALGETBUFFERI                   GetBufferi;
} g_al;

/* minimal version of alad for just core AL/ALC since we don't use more: */
typedef void (*snd_func) (void);
typedef snd_func (*snd_loader) (void* module, const char *name);
static void snd_load_al(snd_loader loader) {
    assert(g_al.module != NULL);
    g_al.c.CreateContext      = (LPALCCREATECONTEXT)       (loader(g_al.module, "alcCreateContext"));
    g_al.c.MakeContextCurrent = (LPALCMAKECONTEXTCURRENT)  (loader(g_al.module, "alcMakeContextCurrent"));
    g_al.c.ProcessContext     = (LPALCPROCESSCONTEXT)      (loader(g_al.module, "alcProcessContext"));
    g_al.c.SuspendContext     = (LPALCSUSPENDCONTEXT)      (loader(g_al.module, "alcSuspendContext"));
    g_al.c.DestroyContext     = (LPALCDESTROYCONTEXT)      (loader(g_al.module, "alcDestroyContext"));
    g_al.c.GetCurrentContext  = (LPALCGETCURRENTCONTEXT)   (loader(g_al.module, "alcGetCurrentContext"));
    g_al.c.GetContextsDevice  = (LPALCGETCONTEXTSDEVICE)   (loader(g_al.module, "alcGetContextsDevice"));
    g_al.c.OpenDevice         = (LPALCOPENDEVICE)          (loader(g_al.module, "alcOpenDevice"));
    g_al.c.CloseDevice        = (LPALCCLOSEDEVICE)         (loader(g_al.module, "alcCloseDevice"));
    g_al.c.GetError           = (LPALCGETERROR)            (loader(g_al.module, "alcGetError"));
    g_al.c.GetString          = (LPALCGETSTRING)           (loader(g_al.module, "alcGetString"));
    g_al.c.GetIntegerv        = (LPALCGETINTEGERV)         (loader(g_al.module, "alcGetIntegerv"));
    g_al.c.CaptureOpenDevice  = (LPALCCAPTUREOPENDEVICE)   (loader(g_al.module, "alcCaptureOpenDevice"));
    g_al.c.CaptureCloseDevice = (LPALCCAPTURECLOSEDEVICE)  (loader(g_al.module, "alcCaptureCloseDevice"));
    g_al.c.CaptureStart       = (LPALCCAPTURESTART)        (loader(g_al.module, "alcCaptureStart"));
    g_al.c.CaptureStop        = (LPALCCAPTURESTOP)         (loader(g_al.module, "alcCaptureStop"));
    g_al.c.CaptureSamples     = (LPALCCAPTURESAMPLES)      (loader(g_al.module, "alcCaptureSamples"));
    g_al.GetString            = (LPALGETSTRING)            (loader(g_al.module, "alGetString"));
    g_al.GetInteger           = (LPALGETINTEGER)           (loader(g_al.module, "alGetInteger"));
    g_al.GetFloat             = (LPALGETFLOAT)             (loader(g_al.module, "alGetFloat"));
    g_al.GetError             = (LPALGETERROR)             (loader(g_al.module, "alGetError"));
    g_al.DopplerFactor        = (LPALDOPPLERFACTOR)        (loader(g_al.module, "alDopplerFactor"));
    g_al.SpeedOfSound         = (LPALSPEEDOFSOUND)         (loader(g_al.module, "alSpeedOfSound"));
    g_al.DistanceModel        = (LPALDISTANCEMODEL)        (loader(g_al.module, "alDistanceModel"));
    g_al.Listenerf            = (LPALLISTENERF)            (loader(g_al.module, "alListenerf"));
    g_al.Listenerfv           = (LPALLISTENERFV)           (loader(g_al.module, "alListenerfv"));
    g_al.GetListenerf         = (LPALGETLISTENERF)         (loader(g_al.module, "alGetListenerf"));
    g_al.GetListenerfv        = (LPALGETLISTENERFV)        (loader(g_al.module, "alGetListenerfv"));
    g_al.GenSources           = (LPALGENSOURCES)           (loader(g_al.module, "alGenSources"));
    g_al.DeleteSources        = (LPALDELETESOURCES)        (loader(g_al.module, "alDeleteSources"));
    g_al.IsSource             = (LPALISSOURCE)             (loader(g_al.module, "alIsSource"));
    g_al.Sourcef              = (LPALSOURCEF)              (loader(g_al.module, "alSourcef"));
    g_al.Sourcefv             = (LPALSOURCEFV)             (loader(g_al.module, "alSourcefv"));
    g_al.Sourcei              = (LPALSOURCEI)              (loader(g_al.module, "alSourcei"));
    g_al.GetSourcef           = (LPALGETSOURCEF)           (loader(g_al.module, "alGetSourcef"));
    g_al.GetSourcefv          = (LPALGETSOURCEFV)          (loader(g_al.module, "alGetSourcefv"));
    g_al.GetSourcei           = (LPALGETSOURCEI)           (loader(g_al.module, "alGetSourcei"));
    g_al.SourcePlayv          = (LPALSOURCEPLAYV)          (loader(g_al.module, "alSourcePlayv"));
    g_al.SourceStopv          = (LPALSOURCESTOPV)          (loader(g_al.module, "alSourceStopv"));
    g_al.SourceRewindv        = (LPALSOURCEREWINDV)        (loader(g_al.module, "alSourceRewindv"));
    g_al.SourcePausev         = (LPALSOURCEPAUSEV)         (loader(g_al.module, "alSourcePausev"));
    g_al.SourceQueueBuffers   = (LPALSOURCEQUEUEBUFFERS)   (loader(g_al.module, "alSourceQueueBuffers"));
    g_al.SourceUnqueueBuffers = (LPALSOURCEUNQUEUEBUFFERS) (loader(g_al.module, "alSourceUnqueueBuffers"));
    g_al.GenBuffers           = (LPALGENBUFFERS)           (loader(g_al.module, "alGenBuffers"));
    g_al.DeleteBuffers        = (LPALDELETEBUFFERS)        (loader(g_al.module, "alDeleteBuffers"));
    g_al.IsBuffer             = (LPALISBUFFER)             (loader(g_al.module, "alIsBuffer"));
    g_al.BufferData           = (LPALBUFFERDATA)           (loader(g_al.module, "alBufferData"));
    g_al.GetBufferi           = (LPALGETBUFFERI)           (loader(g_al.module, "alGetBufferi"));
}
static snd_loader snd_load_al_dll(void) {
/* since we only use core functions, it's enough to load all function pointers once at init from the DLL.
 * This would be different for direct functions, but since they're newer we avoid them here.
 */
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32) || defined(__MINGW32__)
    g_al.module = LoadLibraryA ("OpenAL32.dll");
    if (g_al.module == NULL) {
        g_al.module = LoadLibraryA ("soft_oal.dll");
    }
    if(g_al.module == NULL) {
        return NULL;
    }
    return (snd_loader) GetProcAddress;
#else /* Unix defaults otherwise */
#if defined(__APPLE__)
    /* not tested myself; the only references I could find are https://github.com/ToweOPrO/sadsad and https://pastebin.com/MEmh3ZFr, which is at least tenuous */
    g_al.module = dlopen ("libopenal.1.dylib", RTLD_LAZY | RTLD_LOCAL);
    if (g_al.module == NULL) {
        g_al.module = dlopen ("libopenal.dylib", RTLD_LAZY | RTLD_LOCAL);
    }
    if(g_al.module == NULL) {
        return NULL;
    }
#else /* Linux and BSD */
    g_al.module = dlopen ("libopenal.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (g_al.module == NULL) {
        g_al.module = dlopen ("libopenal.so", RTLD_LAZY | RTLD_LOCAL);
    }
    if(g_al.module == NULL) {
        return NULL;
    }
#endif
    /* there are also libopenal.so.1.[X].[Y] and libopenal.1.[X].[Y].dylib respectively, but it would be difficult to look all of those up */
    return (snd_loader) dlsym;
#endif /* _WIN32 */
}
static void snd_unload_al_dll(void) {
    assert(g_al.module != NULL);
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32) || defined(__MINGW32__)
    FreeLibrary(g_al.module);
#else /* Unix defaults otherwise */
    dlclose(g_al.module);
#endif
}

/* NOT replaceable with normal split functions! It has to deal with strain NULs as seperators and the strange double NUL as terminator */
static snd_result_t split_device_string(const char* str, int* out_num_strings, const char*** out_strings) {
    int num = 0, last_nul = INT_MAX, copied = 0, len; char** str_array;
#ifndef SND_NO_CHECKS
    if(out_num_strings == NULL || out_strings == NULL) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    /* First scan for the number of strings: */
    for(int i = 0;; i++) {
        if(str[i] == '\0') {
            if(last_nul == i-1) {
                break;
            }
            num++;
            last_nul = i;
        }
    }
    str_array = calloc(num, sizeof(char*));
    /* Second scan to copy the strings out: */
    last_nul = -1;
    for(int i = 0;; i++) {
        if(str[i] == '\0') {
            if(last_nul == i-1) {
                break;
            }
            len = i - last_nul; /* includes the NUL in its size */
            str_array[copied] = calloc(len, sizeof(char));
            memmove(str_array[copied], &str[last_nul+1], len-1);
            copied++;
            last_nul = i;
        }
    }
#ifndef SND_NO_CHECKS
    if(copied != num) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    out_num_strings[0] = num;
    out_strings[0] = (const char**) str_array;
    return SND_OK;
}

snd_result_t snd_init(snd_device_list_t* out_device_list) {
    snd_result_t r; const ALCchar* str;
    
#ifndef SND_NO_CHECKS
    if(out_device_list == NULL) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif
    
    snd_loader loader = snd_load_al_dll();
    if(loader == NULL) {
        return SND_ERROR_AL_NOT_PRESENT;
    }
    snd_load_al(loader);
    
    str = g_al.c.GetString(NULL, ALC_DEVICE_SPECIFIER);
#ifndef SND_NO_CHECKS
    if(g_al.c.GetError(NULL) != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    if(str == NULL) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    r = split_device_string(str, &(g_al.devices.nr_playback_devices), &(g_al.devices.playback_devices));
#ifndef SND_NO_CHECKS
    if(r != SND_OK) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    
    str = g_al.c.GetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);
#ifndef SND_NO_CHECKS
    if(g_al.c.GetError(NULL) != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    if(str == NULL) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    g_al.devices.playback_devices_default_id = -1;
    for(int i = 0; i < g_al.devices.nr_playback_devices; i++) {
        if(strcmp(g_al.devices.playback_devices[i], str) == 0) {
            g_al.devices.playback_devices_default_id = i;
            break;
        }
    }
#ifndef SND_NO_CHECKS
    if(g_al.devices.playback_devices_default_id < 0) {
        return SND_ERROR_UNKNOWN;
    }
#endif

    str = g_al.c.GetString(NULL, ALC_CAPTURE_DEVICE_SPECIFIER);
#ifndef SND_NO_CHECKS
    if(g_al.c.GetError(NULL) != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    if(str == NULL) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    r = split_device_string(str, &(g_al.devices.nr_recording_devices), &(g_al.devices.recording_devices));
#ifndef SND_NO_CHECKS
    if(r != SND_OK) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    
    str = g_al.c.GetString(NULL, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER);
#ifndef SND_NO_CHECKS
    if(g_al.c.GetError(NULL) != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    if(str == NULL) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    g_al.devices.recording_devices_default_id = -1;
    for(int i = 0; i < g_al.devices.nr_recording_devices; i++) {
        if(strcmp(g_al.devices.recording_devices[i], str) == 0) {
            g_al.devices.recording_devices_default_id = i;
            break;
        }
    }
#ifndef SND_NO_CHECKS
    if(g_al.devices.recording_devices_default_id < 0) {
        return SND_ERROR_UNKNOWN;
    }
#endif

    g_al.playback_device_handles = calloc(g_al.devices.nr_playback_devices, sizeof(ALCdevice*));

    out_device_list[0] = g_al.devices;
    
    return SND_OK;
}
snd_result_t snd_exit(void) {
    snd_unload_al_dll();
    
    for(int i = 0; i < g_al.devices.nr_playback_devices; i++) {
        if(g_al.playback_device_handles[i] != NULL) {
            g_al.c.CloseDevice(g_al.playback_device_handles[i]);
#ifndef SND_NO_CHECKS
            if(g_al.GetError() != AL_NO_ERROR) {
                return SND_ERROR_UNKNOWN;
            }
#endif
        }
    }
    if(g_al.devices.playback_devices != NULL) {
        free(g_al.devices.playback_devices);
    }
    if(g_al.devices.recording_devices != NULL) {
        free(g_al.devices.recording_devices);
    }
    return SND_OK;
}

snd_result_t snd_recording_device_open(uint32_t recording_device_id, snd_format_t format, uint32_t frequency_hz, size_t internal_buffer_size, snd_recording_device_t* device) {
    ALenum al_format; const ALCchar* dev_name; const ALchar* str; ALCdevice* handle;
    
#ifndef SND_NO_CHECKS
    if(device == NULL || recording_device_id < 0 || recording_device_id >= g_al.devices.nr_recording_devices) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif
    switch(format) {
    case SND_FORMAT_PCM_UINT8_MONO:
        al_format = AL_FORMAT_MONO8;
        break;
    case SND_FORMAT_PCM_INT16_MONO:
        al_format = AL_FORMAT_MONO16;
        break;
    case SND_FORMAT_PCM_UINT8_STEREO_INTERLEAVED_LR:
        al_format = AL_FORMAT_STEREO8;
        break;
    case SND_FORMAT_PCM_INT16_STEREO_INTERLEAVED_LR:
        al_format = AL_FORMAT_STEREO16;
        break;
    default:
        return SND_ERROR_INVALID_PARAM;
    }
    dev_name = (const ALchar*) g_al.devices.recording_devices[recording_device_id];
    
    handle = g_al.c.CaptureOpenDevice(dev_name, frequency_hz, al_format, internal_buffer_size);
#ifndef SND_NO_CHECKS
    switch(g_al.GetError()) {
    case AL_NO_ERROR:
        break;
    case AL_OUT_OF_MEMORY:
        return SND_ERROR_RECORDING_DEVICE_INVALID;
    default:
        return SND_ERROR_UNKNOWN;
    }
    if(handle == NULL) {
        return SND_ERROR_UNKNOWN;
    }
#endif
#ifdef SND_DEBUG
    str = g_al.c.GetString(handle, ALC_CAPTURE_DEVICE_SPECIFIER);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    if(str == NULL || strcmp(str, dev_name) != 0) {
        return SND_ERROR_UNKNOWN;
    }
#endif

    device[0].handle = handle;
    return SND_OK;
}
snd_result_t snd_recording_device_close(snd_recording_device_t device) {
#ifndef SND_NO_CHECKS
    if(device.handle == NULL) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif
    g_al.c.CaptureCloseDevice((ALCdevice*)device.handle);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    return SND_OK;
}
snd_result_t snd_recording_start(snd_recording_device_t device) {
#ifndef SND_NO_CHECKS
    if(device.handle == NULL) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif
    g_al.c.CaptureStart((ALCdevice*)device.handle);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    return SND_OK;
}
snd_result_t snd_recording_stop(snd_recording_device_t device) {
#ifndef SND_NO_CHECKS
    if(device.handle == NULL) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif
    g_al.c.CaptureStop((ALCdevice*)device.handle);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    return SND_OK;
}
snd_result_t snd_recording_retrieve_samples_nonblocking(snd_recording_device_t device, void* buffer, size_t nr_samples) {
#ifndef SND_NO_CHECKS
    if(device.handle == NULL || buffer == NULL) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif
    g_al.c.CaptureSamples((ALCdevice*)device.handle, buffer, nr_samples);
#ifndef SND_NO_CHECKS
    switch(g_al.GetError()) {
    case AL_NO_ERROR:
        break;
    case ALC_INVALID_VALUE:
        return SND_ERROR_RECORDING_NOT_AVAILABLE;
    default:
        return SND_ERROR_UNKNOWN;
    }
#endif
    return SND_OK;
}
snd_result_t snd_recording_get_nr_samples(snd_recording_device_t device, size_t* nr_samples) {
    ALCint i;
#ifndef SND_NO_CHECKS
    if(device.handle == NULL || nr_samples == NULL) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif
    g_al.c.GetIntegerv((ALCdevice*)device.handle, ALC_CAPTURE_SAMPLES, 1, &i);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    nr_samples[0] = i;
    return SND_OK;
}


static snd_result_t snd_context_set(ALCcontext* new, ALCcontext** old) {
    ALCcontext* old_con; ALCboolean b;
    
#ifndef SND_NO_CHECKS
    if(new == NULL) {
        return SND_ERROR_UNKNOWN;
    }
#endif

    if(old != NULL) {
        old_con = g_al.c.GetCurrentContext();
#ifndef SND_NO_CHECKS
        if(g_al.GetError() != AL_NO_ERROR) {
            return SND_ERROR_UNKNOWN;
        }
        if(old_con == NULL) {
            return SND_ERROR_UNKNOWN;
        }
#endif
    }
    
    if(old == NULL || new != old_con) {
        b = g_al.c.MakeContextCurrent(new);
#ifndef SND_NO_CHECKS
        if(g_al.GetError() != AL_NO_ERROR) {
            return SND_ERROR_UNKNOWN;
        }
        if(b != AL_TRUE) {
            return SND_ERROR_UNKNOWN;
        }
#endif
    }
    
    if(old != NULL)
        old[0] = old_con;
        
    return SND_OK;
}

snd_result_t snd_listener_context_create(uint32_t playback_device_id, uint32_t mixing_frequency_Hz, uint32_t refresh_interval_Hz, bool synchronous, uint32_t requested_min_nr_mono_sources, uint32_t requested_min_nr_stereo_sources, snd_listener_context_t* context) {
    const ALCchar* str; ALCint attrlist[11]; ALCboolean b; ALCint iv[11];
    ALCcontext* handle; ALCcontext* old_con; ALCdevice* dev;
    
#ifndef SND_NO_CHECKS
    if(context == NULL || playback_device_id < 0 || playback_device_id >= g_al.devices.nr_playback_devices) {
        return SND_ERROR_INVALID_PARAM;
    }
    /* we convert from uint32 to int32 so check there is no issue */
    if(mixing_frequency_Hz > INT32_MAX || refresh_interval_Hz > INT32_MAX || requested_min_nr_mono_sources > INT32_MAX || requested_min_nr_stereo_sources > INT32_MAX) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif

    if(g_al.playback_device_handles[playback_device_id] == NULL) {
        g_al.playback_device_handles[playback_device_id] = g_al.c.OpenDevice(g_al.devices.playback_devices[playback_device_id]);
#ifndef SND_NO_CHECKS
        if(g_al.GetError() != AL_NO_ERROR) {
            return SND_ERROR_UNKNOWN;
        }
#endif
#ifdef SND_DEBUG
        str = g_al.c.GetString(g_al.playback_device_handles[playback_device_id], ALC_DEVICE_SPECIFIER);
        if(g_al.GetError() != AL_NO_ERROR) {
            return SND_ERROR_UNKNOWN;
        }
        if(str == NULL || strcmp(str, g_al.devices.playback_devices[playback_device_id]) != 0) {
            return SND_ERROR_UNKNOWN;
        }
#endif
    }
    dev = g_al.playback_device_handles[playback_device_id];

    attrlist[0] = ALC_FREQUENCY;
    attrlist[1] = mixing_frequency_Hz;
    attrlist[2] = ALC_REFRESH;
    attrlist[3] = refresh_interval_Hz;
    attrlist[4] = ALC_SYNC;
    attrlist[5] = synchronous;
    attrlist[6] = ALC_MONO_SOURCES;
    attrlist[7] = requested_min_nr_mono_sources;
    attrlist[8] = ALC_STEREO_SOURCES;
    attrlist[9] = requested_min_nr_stereo_sources;
    attrlist[10] = 0;

    handle = g_al.c.CreateContext(dev, attrlist);
#ifndef SND_NO_CHECKS
    switch(g_al.GetError()) {
    case AL_NO_ERROR:
        break;
    case ALC_INVALID_VALUE:
        return SND_ERROR_DEVICE_OUT_OF_LISTENING_CONTEXTS;
    default:
        return SND_ERROR_UNKNOWN;
    }
    if(handle == NULL) {
        return SND_ERROR_UNKNOWN;
    }
#endif
#ifdef SND_DEBUG
    if(g_al.c.GetContextsDevice(handle) != dev) {
        return SND_ERROR_UNKNOWN;
    }

    old_con = g_al.c.GetCurrentContext();
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    if(old_con == NULL) {
        return SND_ERROR_UNKNOWN;
    }
    b = g_al.c.MakeContextCurrent(handle);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    if(b != AL_TRUE) {
        return SND_ERROR_UNKNOWN;
    }
    
    g_al.c.GetIntegerv(dev, ALC_ATTRIBUTES_SIZE, 1, iv);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    if(iv[0] != 11) {
        return SND_ERROR_UNKNOWN;
    }
    
    g_al.c.GetIntegerv(dev, ALC_ALL_ATTRIBUTES, 11, iv);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    if(memcmp(iv, attrlist, sizeof(ALCint)*11) != 0) {
        return SND_ERROR_UNKNOWN;
    }
    
    b = g_al.c.MakeContextCurrent(old_con);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    if(b != AL_TRUE) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    
    context[0].handle = handle;
    return SND_OK;
}
snd_result_t snd_listener_context_params_get(snd_listener_context_t context, snd_listener_context_params_t* out_params) {
    snd_result_t r; ALCcontext* old_con; snd_listener_context_params_t params;
    ALfloat fv[6]; ALint i;
#ifndef SND_NO_CHECKS
    if(context.handle == NULL || out_params == NULL) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif

    r = snd_context_set(context.handle, &old_con);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

    g_al.GetListenerfv(AL_GAIN, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    params.gain_multiplier = fv[0];

    g_al.GetListenerfv(AL_POSITION, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    params.position.x = fv[0];
    params.position.y = fv[1];
    params.position.z = fv[2];

    g_al.GetListenerfv(AL_VELOCITY, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    params.velocity.x = fv[0];
    params.velocity.y = fv[1];
    params.velocity.z = fv[2];
    
    g_al.GetListenerfv(AL_ORIENTATION, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    params.orientation.forward.x = fv[0];
    params.orientation.forward.y = fv[1];
    params.orientation.forward.z = fv[2];
    params.orientation.up.x = fv[3];
    params.orientation.up.y = fv[4];
    params.orientation.up.z = fv[5];
    
    i = g_al.GetInteger(AL_DISTANCE_MODEL);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    switch(i) {
    case AL_INVERSE_DISTANCE:
        params.distance_model = SND_DISTANCE_MODEL_TYPE_INVERSE_DISTANCE;
        break;
    case AL_INVERSE_DISTANCE_CLAMPED:
        params.distance_model = SND_DISTANCE_MODEL_TYPE_INVERSE_DISTANCE_CLAMPED;
        break;
    case AL_LINEAR_DISTANCE:
        params.distance_model = SND_DISTANCE_MODEL_TYPE_LINEAR_DISTANCE;
        break;
    case AL_LINEAR_DISTANCE_CLAMPED:
        params.distance_model = SND_DISTANCE_MODEL_TYPE_LINEAR_DISTANCE_CLAMPED;
        break;
    case AL_EXPONENT_DISTANCE:
        params.distance_model = SND_DISTANCE_MODEL_TYPE_EXPONENT_DISTANCE;
        break;
    case AL_EXPONENT_DISTANCE_CLAMPED:
        params.distance_model = SND_DISTANCE_MODEL_TYPE_EXPONENT_DISTANCE_CLAMPED;
        break;
    case AL_NONE:
        params.distance_model = SND_DISTANCE_MODEL_TYPE_NONE;
        break;
    default:
        return SND_ERROR_UNKNOWN;
    }
    
    fv[0] = g_al.GetFloat(AL_DOPPLER_FACTOR);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    params.doppler_factor = fv[0];
    
    fv[0] = g_al.GetFloat(AL_SPEED_OF_SOUND);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    params.speed_of_sound = fv[0];

    r = snd_context_set(old_con, NULL);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

    out_params[0] = params;
    return SND_OK;
}
snd_result_t snd_listener_context_params_set(snd_listener_context_t context, const snd_listener_context_params_t params) {
    snd_result_t r; ALCcontext* old_con; snd_listener_context_params_t test_params; ALenum e; ALfloat fv[6];
#ifndef SND_NO_CHECKS
    if(context.handle == NULL) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif

    r = snd_context_set(context.handle, &old_con);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

    switch(params.distance_model) {
    case SND_DISTANCE_MODEL_TYPE_INVERSE_DISTANCE:
        e = AL_INVERSE_DISTANCE;
        break;
    case SND_DISTANCE_MODEL_TYPE_INVERSE_DISTANCE_CLAMPED:
        e = AL_INVERSE_DISTANCE_CLAMPED;
        break;
    case SND_DISTANCE_MODEL_TYPE_LINEAR_DISTANCE:
        e = AL_LINEAR_DISTANCE;
        break;
    case SND_DISTANCE_MODEL_TYPE_LINEAR_DISTANCE_CLAMPED:
        e = AL_LINEAR_DISTANCE_CLAMPED;
        break;
    case SND_DISTANCE_MODEL_TYPE_EXPONENT_DISTANCE:
        e = AL_EXPONENT_DISTANCE;
        break;
    case SND_DISTANCE_MODEL_TYPE_EXPONENT_DISTANCE_CLAMPED:
        e = AL_EXPONENT_DISTANCE_CLAMPED;
        break;
    case SND_DISTANCE_MODEL_TYPE_NONE:
        e = AL_NONE;
        break;
    default:
        return SND_ERROR_INVALID_PARAM;
    }
    g_al.DistanceModel(e);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif

    g_al.DopplerFactor(params.doppler_factor);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif

    g_al.SpeedOfSound(params.speed_of_sound);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif

    fv[0] = params.gain_multiplier;
    g_al.Listenerfv(AL_GAIN, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif

    fv[0] = params.position.x;
    fv[1] = params.position.y;
    fv[2] = params.position.z;
    g_al.Listenerfv(AL_POSITION, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif

    fv[0] = params.velocity.x;
    fv[1] = params.velocity.y;
    fv[2] = params.velocity.z;
    g_al.Listenerfv(AL_VELOCITY, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif

    fv[0] = params.orientation.forward.x;
    fv[1] = params.orientation.forward.y;
    fv[2] = params.orientation.forward.z;
    fv[3] = params.orientation.up.x;
    fv[4] = params.orientation.up.y;
    fv[5] = params.orientation.up.z;
    g_al.Listenerfv(AL_ORIENTATION, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif


    r = snd_context_set(old_con, NULL);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif


#ifdef SND_DEBUG
    r = snd_listener_context_params_get(context, &test_params);
    if(r != SND_OK) { return r; }

    if(memcmp(&test_params, &params, sizeof(snd_listener_context_params_t)) != 0) {
        return SND_ERROR_UNKNOWN;
    }
#endif

    return SND_OK;
}
snd_result_t snd_listener_context_process(snd_listener_context_t context) {
#ifndef SND_NO_CHECKS
    ALCdevice* dev;
    if(context.handle == NULL) {
        return SND_ERROR_INVALID_PARAM;
    }
    
    dev = g_al.c.GetContextsDevice(context.handle);
#endif

    g_al.c.ProcessContext(context.handle);
#ifndef SND_NO_CHECKS
    if(g_al.c.GetError(dev) != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif

    return SND_OK;
}
snd_result_t snd_listener_context_suspend(snd_listener_context_t context) {
#ifndef SND_NO_CHECKS
    ALCdevice* dev;
    if(context.handle == NULL) {
        return SND_ERROR_INVALID_PARAM;
    }
    
    dev = g_al.c.GetContextsDevice(context.handle);
#endif

    g_al.c.SuspendContext(context.handle);
#ifndef SND_NO_CHECKS
    if(g_al.c.GetError(dev) != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif

    return SND_OK;
}
snd_result_t snd_listener_context_destroy(snd_listener_context_t context) {
    ALCboolean b;
#ifndef SND_NO_CHECKS
    ALCdevice* dev;
    if(context.handle == NULL) {
        return SND_ERROR_INVALID_PARAM;
    }
    
    dev = g_al.c.GetContextsDevice(context.handle);
#endif

    if(g_al.c.GetCurrentContext() == context.handle) {
        b = g_al.c.MakeContextCurrent(NULL);
#ifndef SND_NO_CHECKS
        if(g_al.c.GetError(dev) != AL_NO_ERROR) {
            return SND_ERROR_UNKNOWN;
        }
        if(b != AL_TRUE) {
            return SND_ERROR_UNKNOWN;
        }
#endif
    }

    g_al.c.DestroyContext(context.handle);
#ifndef SND_NO_CHECKS
    if(g_al.c.GetError(dev) != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif

    return SND_OK;
}

snd_result_t snd_buffer_alloc(snd_listener_context_t context, snd_format_t format, uint32_t frequency_hz, void* data, size_t size, snd_buffer_t* buffer) {
    ALuint id;
    ALenum al_format;
    ALint bits, channels, i;
    snd_result_t r; ALCcontext* old_con;

#ifndef SND_NO_CHECKS
    if(context.handle == NULL || buffer == NULL || data == NULL) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif
    switch(format) {
    case SND_FORMAT_PCM_UINT8_MONO:
        al_format = AL_FORMAT_MONO8;
        bits = 8; channels = 1;
        break;
    case SND_FORMAT_PCM_INT16_MONO:
        al_format = AL_FORMAT_MONO16;
        if(size%2 != 0) {
            return SND_ERROR_INVALID_PARAM;
        }
        bits = 16; channels = 1;
        break;
    case SND_FORMAT_PCM_UINT8_STEREO_INTERLEAVED_LR:
        al_format = AL_FORMAT_STEREO8;
        if(size%2 != 0) {
            return SND_ERROR_INVALID_PARAM;
        }
        bits = 8; channels = 2;
        break;
    case SND_FORMAT_PCM_INT16_STEREO_INTERLEAVED_LR:
        al_format = AL_FORMAT_STEREO16;
        if(size%4 != 0) {
            return SND_ERROR_INVALID_PARAM;
        }
        bits = 16; channels = 2;
        break;
    default:
        return SND_ERROR_INVALID_PARAM;
    }
    
    r = snd_context_set(context.handle, &old_con);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

    g_al.GenBuffers(1, &id);
#ifndef SND_NO_CHECKS
    switch(g_al.GetError()) {
    case AL_NO_ERROR:
        break;
    case AL_OUT_OF_MEMORY:
        return SND_ERROR_OUT_OF_MEMORY;
    default:
        return SND_ERROR_UNKNOWN;
    }
    if(g_al.IsBuffer(id) != AL_TRUE) {
        return SND_ERROR_UNKNOWN;
    }
#endif

    g_al.BufferData(id, al_format, data, size, frequency_hz);
#ifndef SND_NO_CHECKS
    switch(g_al.GetError()) {
    case AL_NO_ERROR:
        break;
    case AL_OUT_OF_MEMORY:
        return SND_ERROR_OUT_OF_MEMORY;
    default:
        return SND_ERROR_UNKNOWN;
    }
#endif

#ifdef SND_DEBUG
    g_al.GetBufferi(id, AL_FREQUENCY, &i);
    if(i != frequency_hz) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.GetBufferi(id, AL_BITS, &i);
    if(i != bits) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.GetBufferi(id, AL_CHANNELS, &i);
    if(i != channels) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.GetBufferi(id, AL_SIZE, &i);
    if(i != size) {
        return SND_ERROR_UNKNOWN;
    }
#endif

    r = snd_context_set(old_con, NULL);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

    buffer[0].id = id;
    return SND_OK;
}
snd_result_t snd_buffer_free(snd_listener_context_t context, snd_buffer_t buffer) {
    snd_result_t r; ALCcontext* old_con;
    
#ifndef SND_NO_CHECKS
    if(context.handle == NULL) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif
    
    r = snd_context_set(context.handle, &old_con);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

#ifndef SND_NO_CHECKS
    if(g_al.IsBuffer(buffer.id) != AL_TRUE) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif

    g_al.DeleteBuffers(1, &(buffer.id));
#ifndef SND_NO_CHECKS
    switch(g_al.GetError()) {
    case AL_NO_ERROR:
        break;
    case AL_INVALID_OPERATION:
        return SND_ERROR_BUFFER_STILL_IN_USE;
    default:
        return SND_ERROR_UNKNOWN;
    }
#endif

    r = snd_context_set(old_con, NULL);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

    return SND_OK;
}

snd_result_t snd_source_create(snd_listener_context_t context, snd_source_t* source) {
    snd_result_t r; ALCcontext* old_con; ALuint id;
    
#ifndef SND_NO_CHECKS
    if(context.handle == NULL || source == NULL) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif
    
    r = snd_context_set(context.handle, &old_con);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

    g_al.GenSources(1, &id);
#ifndef SND_NO_CHECKS
    switch(g_al.GetError()) {
    case AL_NO_ERROR:
        break;
    case AL_OUT_OF_MEMORY:
        return SND_ERROR_OUT_OF_MEMORY;
    case AL_INVALID_VALUE:
        return SND_ERROR_LISTENING_CONTEXT_OUT_OF_SOURCES;
    default:
        return SND_ERROR_UNKNOWN;
    }
    if(g_al.IsSource(id) != AL_TRUE) {
        return SND_ERROR_UNKNOWN;
    }
#endif

    r = snd_context_set(old_con, NULL);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

    source[0].id = id;

    return SND_OK;
}
snd_result_t snd_source_delete(snd_listener_context_t context, snd_source_t source) {
    snd_result_t r; ALCcontext* old_con;
    
#ifndef SND_NO_CHECKS
    if(context.handle == NULL) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif
    
    r = snd_context_set(context.handle, &old_con);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

#ifndef SND_NO_CHECKS
    if(g_al.IsSource(source.id) != AL_TRUE) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif

    g_al.DeleteSources(1, &(source.id));
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif

    r = snd_context_set(old_con, NULL);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

    return SND_OK;
}
snd_result_t snd_source_params_get(snd_listener_context_t context, snd_source_t source, snd_source_params_t* out_params) {
    snd_result_t r; ALCcontext* old_con; snd_source_params_t params;
    ALint i; ALfloat fv[3];
    
#ifndef SND_NO_CHECKS
    if(context.handle == NULL || out_params == NULL) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif
    
    r = snd_context_set(context.handle, &old_con);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

#ifndef SND_NO_CHECKS
    if(g_al.IsSource(source.id) != AL_TRUE) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif

    g_al.GetSourcei(source.id, AL_SOURCE_RELATIVE, &i);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    params.position_relative_to_listener = i;
    
    g_al.GetSourcei(source.id, AL_LOOPING, &i);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    params.looping = i;
    
    g_al.GetSourcefv(source.id, AL_GAIN, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    params.gain.multiplier = fv[0];
    
    g_al.GetSourcefv(source.id, AL_MIN_GAIN, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    params.gain.min = fv[0];
    
    g_al.GetSourcefv(source.id, AL_MAX_GAIN, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    params.gain.max = fv[0];
    
    g_al.GetSourcefv(source.id, AL_CONE_OUTER_GAIN, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    params.gain.outer_angle_secondary_multiplier = fv[0];
    
    g_al.GetSourcefv(source.id, AL_CONE_INNER_ANGLE, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    params.cone.inner_angle = fv[0];
    
    g_al.GetSourcefv(source.id, AL_CONE_OUTER_ANGLE, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    params.cone.outer_angle = fv[0];
    
    g_al.GetSourcefv(source.id, AL_REFERENCE_DISTANCE, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    params.distance.reference = fv[0];
    
    g_al.GetSourcefv(source.id, AL_MAX_DISTANCE, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    params.distance.max = fv[0];
    
    g_al.GetSourcefv(source.id, AL_ROLLOFF_FACTOR, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    params.distance.rolloff_factor = fv[0];
    
    g_al.GetSourcefv(source.id, AL_PITCH, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    params.pitch_shift_multiplier = fv[0];
    
    g_al.GetSourcefv(source.id, AL_POSITION, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    params.position.x = fv[0];
    params.position.y = fv[1];
    params.position.z = fv[2];
    
    g_al.GetSourcefv(source.id, AL_VELOCITY, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    params.velocity.x = fv[0];
    params.velocity.y = fv[1];
    params.velocity.z = fv[2];
    
    g_al.GetSourcefv(source.id, AL_DIRECTION, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    params.direction.x = fv[0];
    params.direction.y = fv[1];
    params.direction.z = fv[2];

    r = snd_context_set(old_con, NULL);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

    out_params[0] = params;

    return SND_OK;
}
snd_result_t snd_source_params_set(snd_listener_context_t context, snd_source_t source, const snd_source_params_t params) {
    snd_result_t r; ALCcontext* old_con; ALfloat fv[3];
    snd_source_params_t test_params;
    
#ifndef SND_NO_CHECKS
    if(context.handle == NULL) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif
    
    r = snd_context_set(context.handle, &old_con);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

#ifndef SND_NO_CHECKS
    if(g_al.IsSource(source.id) != AL_TRUE) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif

    g_al.Sourcei(source.id, AL_SOURCE_RELATIVE, params.position_relative_to_listener);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    
    g_al.Sourcei(source.id, AL_LOOPING, params.looping);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    
    fv[0] = params.gain.multiplier;
    g_al.Sourcefv(source.id, AL_GAIN, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    
    fv[0] = params.gain.min;
    g_al.Sourcefv(source.id, AL_MIN_GAIN, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    
    fv[0] = params.gain.max;
    g_al.Sourcefv(source.id, AL_MAX_GAIN, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    
    fv[0] = params.gain.outer_angle_secondary_multiplier;
    g_al.Sourcefv(source.id, AL_CONE_OUTER_GAIN, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    
    fv[0] = params.cone.inner_angle;
    g_al.Sourcefv(source.id, AL_CONE_INNER_ANGLE, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    
    fv[0] = params.cone.outer_angle;
    g_al.Sourcefv(source.id, AL_CONE_OUTER_ANGLE, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    
    fv[0] = params.distance.reference;
    g_al.Sourcefv(source.id, AL_REFERENCE_DISTANCE, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    
    fv[0] = params.distance.max;
    g_al.Sourcefv(source.id, AL_MAX_DISTANCE, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    
    fv[0] = params.distance.rolloff_factor;
    g_al.Sourcefv(source.id, AL_ROLLOFF_FACTOR, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    
    fv[0] = params.pitch_shift_multiplier;
    g_al.Sourcefv(source.id, AL_PITCH, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    
    fv[0] = params.position.x;
    fv[1] = params.position.y;
    fv[2] = params.position.z;
    g_al.Sourcefv(source.id, AL_POSITION, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    
    fv[0] = params.velocity.x;
    fv[1] = params.velocity.y;
    fv[2] = params.velocity.z;
    g_al.Sourcefv(source.id, AL_VELOCITY, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
    
    fv[0] = params.direction.x;
    fv[1] = params.direction.y;
    fv[2] = params.direction.z;
    g_al.Sourcefv(source.id, AL_DIRECTION, fv);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif

    r = snd_context_set(old_con, NULL);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif


#ifdef SND_DEBUG
    r = snd_source_params_get(context, source, &test_params);
    if(r != SND_OK) { return r; }

    if(memcmp(&test_params,&params,sizeof(snd_source_params_t)) != 0) {
        return SND_ERROR_UNKNOWN;
    }
#endif

    return SND_OK;
}

snd_result_t snd_source_play_position_set(snd_listener_context_t context, snd_source_t source, snd_source_position_format_t format, float value) {
    snd_result_t r; ALCcontext* old_con; ALenum al_param_name;
    ALfloat f;
    
#ifndef SND_NO_CHECKS
    if(context.handle == NULL) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif

    switch(format) {
    case SND_SOURCE_POSITION_FORMAT_SECONDS:
        al_param_name = AL_SEC_OFFSET;
        break;
    case SND_SOURCE_POSITION_FORMAT_SAMPLES:
        al_param_name = AL_SAMPLE_OFFSET;
        break;
    case SND_SOURCE_POSITION_FORMAT_BYTES:
        al_param_name = AL_BYTE_OFFSET;
        break;
    default:
        return SND_ERROR_INVALID_PARAM;
    }
    
    f = value;
    
    r = snd_context_set(context.handle, &old_con);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

#ifndef SND_NO_CHECKS
    if(g_al.IsSource(source.id) != AL_TRUE) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif

    g_al.Sourcefv(source.id, al_param_name, &f);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
#ifdef SND_DEBUG
    g_al.GetSourcefv(source.id, al_param_name, &f);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    if(f != value) {
        return SND_ERROR_UNKNOWN;
    }
#endif

    r = snd_context_set(old_con, NULL);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

    return SND_OK;
}
snd_result_t snd_source_play_position_get(snd_listener_context_t context, snd_source_t source, snd_source_position_format_t format, float* out_value) {
    snd_result_t r; ALCcontext* old_con; ALenum al_param_name;
    ALfloat f;
    
#ifndef SND_NO_CHECKS
    if(context.handle == NULL || out_value == NULL) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif

    switch(format) {
    case SND_SOURCE_POSITION_FORMAT_SECONDS:
        al_param_name = AL_SEC_OFFSET;
        break;
    case SND_SOURCE_POSITION_FORMAT_SAMPLES:
        al_param_name = AL_SAMPLE_OFFSET;
        break;
    case SND_SOURCE_POSITION_FORMAT_BYTES:
        al_param_name = AL_BYTE_OFFSET;
        break;
    default:
        return SND_ERROR_INVALID_PARAM;
    }
    
    r = snd_context_set(context.handle, &old_con);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

#ifndef SND_NO_CHECKS
    if(g_al.IsSource(source.id) != AL_TRUE) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif

    g_al.GetSourcefv(source.id, al_param_name, &f);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif

    r = snd_context_set(old_con, NULL);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

    out_value[0] = f;

    return SND_OK;
}
snd_result_t snd_source_queue_position_get(snd_listener_context_t context, snd_source_t source, uint32_t* nr_buffers_in_queue, uint32_t* currently_playing_index) {
    snd_result_t r; ALCcontext* old_con;
    ALint i1, i2;
    
#ifndef SND_NO_CHECKS
    if(context.handle == NULL || nr_buffers_in_queue == NULL || currently_playing_index == NULL) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif
    
    r = snd_context_set(context.handle, &old_con);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

#ifndef SND_NO_CHECKS
    if(g_al.IsSource(source.id) != AL_TRUE) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif

    g_al.GetSourcei(source.id, AL_BUFFERS_QUEUED, &i1);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif

    g_al.GetSourcei(source.id, AL_BUFFERS_PROCESSED, &i2);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif

    r = snd_context_set(old_con, NULL);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

    nr_buffers_in_queue[0] = i1;
    currently_playing_index[0] = i2;

    return SND_OK;
}

snd_result_t snd_source_static_buffer(snd_listener_context_t context, snd_source_t source, snd_buffer_t buffer) {
    snd_result_t r; ALCcontext* old_con; ALint id, i;
    
#ifndef SND_NO_CHECKS
    if(context.handle == NULL) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif

    id = buffer.id;
    
    r = snd_context_set(context.handle, &old_con);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

#ifndef SND_NO_CHECKS
    if(g_al.IsSource(source.id) != AL_TRUE) {
        return SND_ERROR_INVALID_PARAM;
    }
    if(g_al.IsBuffer(buffer.id) != AL_TRUE) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif

    g_al.Sourcei(source.id, AL_BUFFER, id);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
#ifdef SND_DEBUG
    g_al.GetSourcei(source.id, AL_BUFFER, &i);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    if(i != id) {
        return SND_ERROR_UNKNOWN;
    }
#endif

#ifdef SND_DEBUG
    g_al.GetSourcei(source.id, AL_SOURCE_TYPE, &i);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    if(i != AL_STATIC) {
        return SND_ERROR_UNKNOWN;
    }
#endif

    r = snd_context_set(old_con, NULL);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

    return SND_OK;
}
snd_result_t snd_source_queue_buffers(snd_listener_context_t context, snd_source_t source, uint32_t nr_buffers, snd_buffer_t* buffers) {
    snd_result_t r; ALCcontext* old_con; ALint i;
    
#ifndef SND_NO_CHECKS
    if(context.handle == NULL || nr_buffers <= 0 || buffers == NULL) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif
    
    r = snd_context_set(context.handle, &old_con);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

#ifndef SND_NO_CHECKS
    if(g_al.IsSource(source.id) != AL_TRUE) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif

#ifndef SND_NO_CHECKS
    g_al.GetSourcei(source.id, AL_SOURCE_TYPE, &i);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    if(i != AL_STREAMING || i != AL_UNDETERMINED) {
        return SND_ERROR_SOURCE_STATICALLY_BOUND;
    }
#endif

    /* Since the only field of snd_buffer_t is the id, the arrays will line up.
     * NOTE: if anything is added to the snd_buffer_t struct, rewrite this! */
    g_al.SourceQueueBuffers(source.id, nr_buffers, (ALuint*) buffers);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif

#ifdef SND_DEBUG
    g_al.GetSourcei(source.id, AL_SOURCE_TYPE, &i);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    if(i != AL_STREAMING) {
        return SND_ERROR_UNKNOWN;
    }
#endif

    r = snd_context_set(old_con, NULL);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

    return SND_OK;
}
snd_result_t snd_source_unqueue_buffers(snd_listener_context_t context, snd_source_t source, uint32_t nr_buffers, snd_buffer_t* out_buffers) {
    snd_result_t r; ALCcontext* old_con; ALint i;
    
#ifndef SND_NO_CHECKS
    if(context.handle == NULL || nr_buffers <= 0 || out_buffers == NULL) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif
    
    r = snd_context_set(context.handle, &old_con);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

#ifndef SND_NO_CHECKS
    if(g_al.IsSource(source.id) != AL_TRUE) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif

#ifndef SND_NO_CHECKS
    g_al.GetSourcei(source.id, AL_SOURCE_TYPE, &i);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    if(i != AL_STREAMING) {
        return SND_ERROR_SOURCE_NO_BUFFERS_QUEUED;
    }
#endif

    /* Since the only field of snd_buffer_t is the id, the arrays will line up.
     * NOTE: if anything is added to the snd_buffer_t struct, rewrite this! */
    g_al.SourceUnqueueBuffers(source.id, nr_buffers, (ALuint*) out_buffers);
#ifndef SND_NO_CHECKS
    switch(g_al.GetError()) {
    case AL_NO_ERROR:
        break;
    case AL_INVALID_VALUE:
        return SND_ERROR_SOURCE_NOT_ENOUGH_QUEUED_BUFFERS_FINISHED;
    default:
        return SND_ERROR_UNKNOWN;
    }
#endif

#ifdef SND_DEBUG
    g_al.GetSourcei(source.id, AL_SOURCE_TYPE, &i);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    if(i != AL_STREAMING) {
        return SND_ERROR_UNKNOWN;
    }
#endif

    r = snd_context_set(old_con, NULL);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

    return SND_OK;
}
snd_result_t snd_source_reset_buffer_state(snd_listener_context_t context, snd_source_t source) {
    snd_result_t r; ALCcontext* old_con; ALint i;
    
#ifndef SND_NO_CHECKS
    if(context.handle == NULL) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif
    
    r = snd_context_set(context.handle, &old_con);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

#ifndef SND_NO_CHECKS
    if(g_al.IsSource(source.id) != AL_TRUE) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif

    g_al.Sourcei(source.id, AL_BUFFER, 0);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
#ifdef SND_DEBUG
    g_al.GetSourcei(source.id, AL_SOURCE_TYPE, &i);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    if(i != AL_UNDETERMINED) {
        return SND_ERROR_UNKNOWN;
    }
#endif

    r = snd_context_set(old_con, NULL);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

    return SND_OK;
}

snd_result_t snd_sources_play  (snd_listener_context_t context, uint32_t nr_sources, snd_source_t* sources) {
    snd_result_t r; ALCcontext* old_con; ALint i;
    
#ifndef SND_NO_CHECKS
    if(context.handle == NULL || sources == NULL || nr_sources <= 0) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif
    
    r = snd_context_set(context.handle, &old_con);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

#ifndef SND_NO_CHECKS
    for(int i = 0; i < nr_sources; i++) {
        if(g_al.IsSource(sources[i].id) != AL_TRUE) {
            return SND_ERROR_INVALID_PARAM;
        }
    }
#endif

    /* Since the only field of snd_source_t is the id, the arrays will line up.
     * NOTE: if anything is added to the snd_source_t struct, rewrite this! */
    g_al.SourcePlayv(nr_sources, (const ALuint *) sources);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
#ifdef SND_DEBUG
    for(int k = 0; k < nr_sources; k++) {
        g_al.GetSourcei(sources[k].id, AL_SOURCE_STATE, &i);
        if(g_al.GetError() != AL_NO_ERROR) {
            return SND_ERROR_UNKNOWN;
        }
        if(i != AL_PLAYING) {
            return SND_ERROR_UNKNOWN;
        }
    }
#endif

    r = snd_context_set(old_con, NULL);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

    return SND_OK;
}
snd_result_t snd_sources_pause (snd_listener_context_t context, uint32_t nr_sources, snd_source_t* sources) {
    snd_result_t r; ALCcontext* old_con;
#ifdef SND_DEBUG
    ALint* source_states = calloc(sizeof(ALint), nr_sources);
#endif
    
#ifndef SND_NO_CHECKS
    if(context.handle == NULL || sources == NULL || nr_sources <= 0) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif
    
    r = snd_context_set(context.handle, &old_con);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

#ifndef SND_NO_CHECKS
    for(int i = 0; i < nr_sources; i++) {
        if(g_al.IsSource(sources[i].id) != AL_TRUE) {
            return SND_ERROR_INVALID_PARAM;
        }
#ifdef SND_DEBUG
        g_al.GetSourcei(sources[i].id, AL_SOURCE_STATE, &source_states[i]);
        if(g_al.GetError() != AL_NO_ERROR) {
            return SND_ERROR_UNKNOWN;
        }
#endif
    }
#endif


    /* Since the only field of snd_source_t is the id, the arrays will line up.
     * NOTE: if anything is added to the snd_source_t struct, rewrite this! */
    g_al.SourcePausev(nr_sources, (const ALuint*) sources);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
#ifdef SND_DEBUG
    /* Only for AL_PLAYING sources does the spec guarentee state change */
    for(int i = 0; i < nr_sources; i++) {
        if(source_states[i] == AL_PLAYING) {
            g_al.GetSourcei(sources[i].id, AL_SOURCE_STATE, &source_states[i]);
            if(g_al.GetError() != AL_NO_ERROR) {
                return SND_ERROR_UNKNOWN;
            }
            if(source_states[i] != AL_PAUSED) {
                return SND_ERROR_UNKNOWN;
            }
        }
    }
#endif

    r = snd_context_set(old_con, NULL);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

    return SND_OK;
}
snd_result_t snd_sources_stop  (snd_listener_context_t context, uint32_t nr_sources, snd_source_t* sources) {
    snd_result_t r; ALCcontext* old_con;
#ifdef SND_DEBUG
    ALint* source_states = calloc(sizeof(ALint), nr_sources);
#endif
    
#ifndef SND_NO_CHECKS
    if(context.handle == NULL || sources == NULL || nr_sources <= 0) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif
    
    r = snd_context_set(context.handle, &old_con);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

#ifndef SND_NO_CHECKS
    for(int i = 0; i < nr_sources; i++) {
        if(g_al.IsSource(sources[i].id) != AL_TRUE) {
            return SND_ERROR_INVALID_PARAM;
        }
#ifdef SND_DEBUG
        g_al.GetSourcei(sources[i].id, AL_SOURCE_STATE, &source_states[i]);
        if(g_al.GetError() != AL_NO_ERROR) {
            return SND_ERROR_UNKNOWN;
        }
#endif
    }
#endif


    /* Since the only field of snd_source_t is the id, the arrays will line up.
     * NOTE: if anything is added to the snd_source_t struct, rewrite this! */
    g_al.SourceStopv(nr_sources, (const ALuint*) sources);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
#ifdef SND_DEBUG
    /* Only for AL_PLAYING and AL_PAUSED sources does the spec guarentee state change */
    for(int i = 0; i < nr_sources; i++) {
        if(source_states[i] == AL_PLAYING || source_states[i] == AL_PAUSED) {
            g_al.GetSourcei(sources[i].id, AL_SOURCE_STATE, &source_states[i]);
            if(g_al.GetError() != AL_NO_ERROR) {
                return SND_ERROR_UNKNOWN;
            }
            if(source_states[i] != AL_PAUSED) {
                return SND_ERROR_UNKNOWN;
            }
        }
    }
#endif

    r = snd_context_set(old_con, NULL);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

    return SND_OK;
}
snd_result_t snd_sources_rewind(snd_listener_context_t context, uint32_t nr_sources, snd_source_t* sources) {
    snd_result_t r; ALCcontext* old_con; ALint i;
    
#ifndef SND_NO_CHECKS
    if(context.handle == NULL || sources == NULL || nr_sources <= 0) {
        return SND_ERROR_INVALID_PARAM;
    }
#endif
    
    r = snd_context_set(context.handle, &old_con);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

#ifndef SND_NO_CHECKS
    for(int i = 0; i < nr_sources; i++) {
        if(g_al.IsSource(sources[i].id) != AL_TRUE) {
            return SND_ERROR_INVALID_PARAM;
        }
    }
#endif

    /* Since the only field of snd_source_t is the id, the arrays will line up.
     * NOTE: if anything is added to the snd_source_t struct, rewrite this! */
    g_al.SourceRewindv(nr_sources, (const ALuint*) sources);
#ifndef SND_NO_CHECKS
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
#endif
#ifdef SND_DEBUG
    for(int k = 0; k < nr_sources; k++) {
        g_al.GetSourcei(sources[k].id, AL_SOURCE_STATE, &i);
        if(g_al.GetError() != AL_NO_ERROR) {
            return SND_ERROR_UNKNOWN;
        }
        if(i != AL_INITIAL) {
            return SND_ERROR_UNKNOWN;
        }
    }
#endif

    r = snd_context_set(old_con, NULL);
#ifndef SND_NO_CHECKS
    if(r != SND_OK) { return r; }
#endif

    return SND_OK;
}




/*

List of AL stuff to use / implement:

funcions loaded above that may be unnecessary:
alGetString, alListenerf, alGetListenerf, alSourcef, alGetSourcef




unnecessary:
            we dont use extensions, so none of these are necessary
alGetProcAddress, alcGetProcAddress, alIsExtensionPresent, alcIsExtensionPresent, alGetEnumValue, alcGetEnumValue
ALC_EXTENSIONS  string, global
AL_EXTENSIONS       1i
            standard AL has no capabilities
alEnable, alDisable, alIsEnabled
            there are no query destinations for that
alGetBooleanv, alGetIntegerv, alGetFloatv, alGetDoublev, alGetBoolean, alGetDouble
            similar
al[Get]Listeneri, alBufferf, alBuffer3f, alBufferfv, alBufferi, alBuffer3i, alBufferiv, alGetBufferf, alGetBuffer3f, alGetBufferfv, alGetBuffer3i, alSource3i, alGetSource3i

            are unnecessary:
al[Get]Listener[3f|3i|iv], alGetBufferiv, alSourcef, alSource3f, alSourceiv, alGetSourcef, alGetSource3f, alGetSourceiv

            are the same as synchronized v versions with n == 1
alSourcePlay, alSourceStop, alSourceRewind, alSourcePause

        these are useless bc we can't use this to extrapolate anything
alcGetIntegerv
    ALC_MAJOR_VERSION           1i
    ALC_MINOR_VERSION           1i
alGetString
    AL_VENDOR
    AL_VERSION
    AL_RENDERER
    
deprecated:
alDopplerVelocity - no use at all, see https://github.com/kcat/openal-soft/pull/1122






    
    
    
    
    
    
    
    
    
    
    
*/

/*   OLD sources functions; maybe extract constraints?
 * 
 * context check? or adding in context mark in snd_source_t ? 
snd_result_t snd_source_create(snd_source_state_t state, snd_source_t* source) {
    ALuint id;

    if(source == NULL) {
        return SND_ERROR_INVALID_PARAM;
    }

    g_al.GenSources(&id);
    switch(g_al.GetError()) {
    case AL_NO_ERROR:
        break;
    case AL_OUT_OF_MEMORY:
        return SND_ERROR_OUT_OF_MEMORY;
    default:
        return SND_ERROR_UNKNOWN;
    }
    if(g_al.IsSource(id) != AL_TRUE) {
        return SND_ERROR_UNKNOWN;
    }

    snd_source_t new_id = -1;
    if(g_al.nr_sources == g_al.cap_sources) {
        g_al.cap_sources *= 2;
        g_al.source_used = recalloc(g_al.source_used, g_al.cap_sources*sizeof(bool));
        g_al.source_ids = recalloc(g_al.source_used, g_al.cap_sources*sizeof(ALuint));
        g_al.source_states = recalloc(g_al.source_used, g_al.cap_sources*sizeof(snd_source_state_t));
        new_id = g_al.nr_sources;
        g_al.nr_sources++;
    } else {
        for(int i = 0; i < g_al.nr_sources; i++) {
            if(!g_al.source_used[i]) {
                new_id = i;
                goto after_loop_else;
            }
        }

        if(new_id == -1) {
            new_id = g_al.nr_sources;
        }
    }

    assert(g_al.source_used[new_id] == false);
    g_al.source_used[new_id] = true;
    g_al.source_ids[new_id] = id;
    snd_result_t r = snd_source_state_get(new_id, NULL);
    if(r != SND_OK) {  return r; }

    source[0] = new_id;
    return SND_OK;
}

snd_result_t snd_source_state_get(snd_source_t source, const snd_source_state_t** source_state) {
    snd_source_state_t* state = &g_al.source_states[source];
    int32_t i;


    g_al.GetSourcefv(source, AL_POSITION, &state[0].position);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.GetSourcefv(source, AL_DIRECTION, &state[0].direction);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.GetSourcefv(source, AL_VELOCITY, &state[0].velocity);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }

    g_al.GetSourcef(source, AL_PITCH, &state[0].pitch_multiplier);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.GetSourcef(source, AL_GAIN, &state[0].gain.multiplier);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.GetSourcef(source, AL_MIN_GAIN, &state[0].gain.min);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.GetSourcef(source, AL_MAX_GAIN,  &state[0].gain.max);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.GetSourcef(source, AL_CONE_OUTER_GAIN, &state[0].gain.outer_angle_multiplier);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.GetSourcef(source, AL_REFERENCE_DISTANCE, &state[0].distance.reference);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.GetSourcef(source, AL_MAX_DISTANCE, &state[0].distance.max);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.GetSourcef(source, AL_ROLLOFF_FACTOR, &state[0].distance.rolloff_factor);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.GetSourcef(source, AL_CONE_INNER_ANGLE, &state[0].cone.inner_angle);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    g_al.GetSourcef(source, AL_CONE_OUTER_ANGLE, &state[0].cone.outer_angle);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }


    g_al.GetSourcei(source, AL_LOOPING, &i);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    switch(i) {
    case AL_TRUE:
        state.position_relative_to_listener = true;
        break;
    case AL_FALSE:
        state.position_relative_to_listener = false;
        break;
    default:
            return SND_ERROR_UNKNOWN;
    }
    g_al.GetSourcei(source, AL_SOURCE_RELATIVE, &i);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }
    switch(i) {
    case AL_TRUE:
        state.loop_queued_buffers = true;
        break;
    case AL_FALSE:
        state.loop_queued_buffers = false;
        break;
    default:
            return SND_ERROR_UNKNOWN;
    }

    if(source_state != NULL) {
        source_state[0] = state;
    }

    return SND_OK;
}

snd_result_t snd_source_state_set(snd_source_t source, snd_source_state_t state) {
    float32_vec3_t fv;
    float32_t f;
    int32_t i;

    ALuint id = g_al.source_ids[source];

    if(g_al.IsSource(id) != AL_TRUE) {
        return SND_ERROR_INVALID_PARAM;
    }


    if(float32_is_nan(state.position.x) || float32_is_nan(state.position.y) || float32_is_nan(state.position.z)) {
        return SND_ERROR_INVALID_PARAM;
    }
    g_al.Sourcefv(source, AL_POSITION, &state.position);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }

    if(float32_is_nan(state.direction.x) || float32_is_nan(state.direction.y) || float32_is_nan(state.direction.z)) {
        return SND_ERROR_INVALID_PARAM;
    }
    g_al.Sourcefv(source, AL_DIRECTION, &state.direction);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }

    if(float32_is_nan(state.velocity.x) || float32_is_nan(state.velocity.y) || float32_is_nan(state.velocity.z)) {
        return SND_ERROR_INVALID_PARAM;
    }
    g_al.Sourcefv(source, AL_VELOCITY, &state.velocity);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }


    if(state.pitch_multiplier <= 0.0f) {
        return SND_ERROR_INVALID_PARAM;
    }
    g_al.Sourcef(source, AL_PITCH, state.pitch_multiplier);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }

    if(state.gain.multiplier < 0.0f) {
        return SND_ERROR_INVALID_PARAM;
    }
    g_al.Sourcef(source, AL_GAIN, state.gain.multiplier);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }

    if(state.gain.min < 0.0f || state.gain.min > 1.0f) {
        return SND_ERROR_INVALID_PARAM;
    }
    g_al.Sourcef(source, AL_MIN_GAIN, state.gain.min);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }

    if(state.gain.max < 0.0f || state.gain.max > 1.0f) {
        return SND_ERROR_INVALID_PARAM;
    }
    g_al.Sourcef(source, AL_MAX_GAIN, state.gain.max);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }


    if(state.gain.outer_angle_multiplier < 0.0f || state.gain.outer_angle_multiplier > 1.0f) {
        return SND_ERROR_INVALID_PARAM;
    }
    g_al.Sourcef(source, AL_CONE_OUTER_GAIN, state.gain.outer_angle_multiplier);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }

    if(state.distance.reference < 0.0f) {
        return SND_ERROR_INVALID_PARAM;
    }
    g_al.Sourcef(source, AL_REFERENCE_DISTANCE, state.distance.reference);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }

    if(state.distance.max < 0.0f) {
        return SND_ERROR_INVALID_PARAM;
    }
    g_al.Sourcef(source, AL_MAX_DISTANCE, state.distance.max);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }

    if(state.distance.rolloff_factor < 0.0f) {
        return SND_ERROR_INVALID_PARAM;
    }
    g_al.Sourcef(source, AL_ROLLOFF_FACTOR, state.distance.rolloff_factor);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }

    if(state.cone.inner_angle < 0.0f || state.cone.inner_angle > 360.0f) {
        return SND_ERROR_INVALID_PARAM;
    }
    g_al.Sourcef(source, AL_CONE_INNER_ANGLE, state.cone.inner_angle);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }

    if(state.cone.outer_angle < 0.0f || state.cone.outer_angle > 360.0f) {
        return SND_ERROR_INVALID_PARAM;
    }
    g_al.Sourcef(source, AL_CONE_OUTER_ANGLE, state.cone.outer_angle);
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }

    if(state.position_relative_to_listener != true && state.position_relative_to_listener != false) {
        return SND_ERROR_INVALID_PARAM;
    }
    if(state.position_relative_to_listener) {
        g_al.Sourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);
    } else {
        g_al.Sourcei(source, AL_SOURCE_RELATIVE, AL_FALSE);
    }
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }

    if(state.loop_queued_buffers != true && state.loop_queued_buffers != false) {
        return SND_ERROR_INVALID_PARAM;
    }
    if(state.loop_queued_buffers) {
        g_al.Sourcei(source, AL_LOOPING, AL_TRUE);
    } else {
        g_al.Sourcei(source, AL_LOOPING, AL_FALSE);
    }
    if(g_al.GetError() != AL_NO_ERROR) {
        return SND_ERROR_UNKNOWN;
    }

    return SND_OK;
}

snd_result_t snd_source_destroy(snd_source_t source) {
    if(g_al.IsSource(source) != AL_TRUE) {
        return SND_ERROR_INVALID_PARAM;
    }

    g_al.DeleteSources(1, &buffer);
    switch(g_al.GetError()) {
    case AL_NO_ERROR:
        break;
    default:
        return SND_ERROR_UNKNOWN;
    }

    return SND_OK;
}

    alSourcei
AL_BUFFER
AL_SOURCE_STATE


*/

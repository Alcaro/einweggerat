// Mini audio library. Public domain. See "unlicense" statement at the end of this file.
// mini_al - v0.3 - 2017-06-19
//
// David Reid - davidreidsoftware@gmail.com

// ABOUT
// =====
// mini_al is a small library for making it easy to connect to a playback or capture device and send
// or receive data from said device. It's focused on being simple and light-weight so don't expect
// all the bells and whistles. Indeed, this is not a full packaged audio library - it's just for
// connecting to a device and handling data transmission.
//
// mini_al uses an asynchronous API. Every device is created with it's own thread, with audio data
// being either delivered to the application from the device (in the case of recording/capture) or
// delivered from the application to the device in the case of playback. Synchronous APIs are not
// supported in the interest of keeping the library as small and light-weight as possible.
//
// Supported Backends:
//   - DirectSound
//   - WASAPI
//   - ALSA
//   - OpenSL|ES / Android
//   - OpenAL
//   - Null (Silence)
//   - ... and more in the future.
//     - Core Audio (OSX, iOS)
//     - Maybe OSS
//
// Supported Formats:
//   - Unsigned 8-bit PCM
//   - Signed 16-bit PCM
//   - Signed 24-bit PCM (tightly packed)
//   - Signed 32-bit PCM
//   - IEEE 32-bit floating point PCM
//
//
// USAGE
// =====
// mini_al is a single-file library. To use it, do something like the following in one .c file.
//   #define MAL_IMPLEMENTATION
//   #include "mini_al.h"
//
// You can then #include this file in other parts of the program as you would with any other header file.
//
// The implementation of this library will try #include-ing necessary headers for each backend. If you do not have
// the development packages for any particular backend you can disable it by #define-ing the appropriate MAL_NO_*
// option before #include-ing the implementation.
//
//
// Building (Windows)
// ------------------
// You do not need to link to anything for the Windows build, but you will need dsound.h in your include paths.
//
//
// Building (Linux)
// ----------------
// The Linux build uses ALSA for it's backend so you will need to install the relevant ALSA development pacakges
// for your preferred distro. It also uses pthreads.
//
// Linking: -lasound -lpthread -ldl
//
//
// Playback Example
// ----------------
//   mal_uint32 on_send_samples(mal_device* pDevice, mal_uint32 frameCount, void* pSamples)
//   {
//       // This callback is set at initialization time and will be called when a playback device needs more 
//       // data. You need to write as many frames as you can to pSamples (but no more than frameCount) and
//       // then return the number of frames you wrote.
//       //
//       // The user data (pDevice->pUserData) is set by mal_device_init().
//       return (mal_uint32)drwav_read_f32((drwav*)pDevice->pUserData, frameCount * pDevice->channels, (float*)pSamples) / pDevice->channels;
//   }
//
//   ...
//
//   mal_context context;
//   if (mal_context_init(NULL, 0, &context) != MAL_SUCCESS) {
//       printf("Failed to initialize context.");
//       return -3;
//   }
//
//   mal_device_config config = mal_device_config_init_playback(mal_format_s16, wav.channels, wav.sampleRate, on_send_frames_to_device);
//
//   mal_device device;
//   mal_result result = mal_device_init(&context, mal_device_type_playback, NULL, &config, pMyData, &device);
//   if (result != MAL_SUCCESS) {
//       return -1;
//   }
//
//   mal_device_start(&device);     // The device is sleeping by default so you'll need to start it manually.
//
//   ...
//
//   mal_device_uninit(&device);    // This will stop the device so no need to do that manually.
//
//
//
// NOTES
// =====
// - This library uses an asynchronous API for delivering and requesting audio data. Each device will have
//   it's own worker thread which is managed by the library.
// - If mal_device_init() is called with a device that's not aligned to the platform's natural alignment
//   boundary (4 bytes on 32-bit, 8 bytes on 64-bit), it will _not_ be thread-safe. The reason for this
//   is that it depends on members of mal_device being correctly aligned for atomic assignments.
// - Sample data is always little-endian and interleaved. For example, mal_format_s16 means signed 16-bit
//   integer samples, interleaved. Let me know if you need non-interleaved and I'll look into it.
//
//
//
// BACKEND NUANCES
// ===============
// - The absolute best latency I am able to get on DirectSound is about 10 milliseconds. This seems very
//   consistent so I'm suspecting there's some kind of hard coded limit there or something.
// - DirectSound currently supports a maximum of 4 periods.
// - To capture audio on Android, remember to add the RECORD_AUDIO permission to your manifest:
//     <uses-permission android:name="android.permission.RECORD_AUDIO" />
//
//
//
// OPTIONS
// =======
// #define these options before including this file.
//
// #define MAL_NO_DSOUND
//   Disables the DirectSound backend.
//
// #define MAL_NO_WASAPI
//   Disables the WASAPI backend.
//
// #define MAL_NO_ALSA
//   Disables the ALSA backend.
//
// #define MAL_NO_OPENSLES
//   Disables the OpenSL|ES backend.
//
// #define MAL_NO_OPENAL
//   Disables the OpenAL backend.
//
// #define MAL_NO_NULL
//   Disables the null backend.
//
// #define MAL_DEFAULT_BUFFER_SIZE_IN_MILLISECONDS
//   When a buffer size of 0 is specified when a device is initialized, it will default to a size with
//   this number of milliseconds worth of data. Note that some backends may adjust this setting if that
//   particular backend has unusual latency characteristics.
//
// #define MAL_DEFAULT_PERIODS
//   When a period count of 0 is specified when a device is initialized, it will default to this.

#ifndef mini_al_h
#define mini_al_h

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable:4201)   // nonstandard extension used: nameless struct/union
#endif

// Platform/backend detection.
#ifdef _WIN32
    #define MAL_WIN32
    #if (!defined(WINAPI_FAMILY) || WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP)
        #define MAL_WIN32_DESKTOP
    #endif
#else
    #define MAL_POSIX
    #include <pthread.h>    // Unfortunate #include, but needed for pthread_t, pthread_mutex_t and pthread_cond_t types.

    #ifdef __linux__
        #define MAL_LINUX
    #endif
    #ifdef __APPLE__
        #define MAL_APPLE
    #endif
    #ifdef __ANDROID__
        #define MAL_ANDROID
    #endif
#endif

#if !defined(MAL_NO_WASAPI) && defined(MAL_WIN32)
    #define MAL_ENABLE_WASAPI
#endif
#if !defined(MAL_NO_DSOUND) && defined(MAL_WIN32) && defined(MAL_WIN32_DESKTOP)
    #define MAL_ENABLE_DSOUND
#endif
#if !defined(MAL_NO_ALSA) && defined(MAL_LINUX) && !defined(MAL_ANDROID)
    #define MAL_ENABLE_ALSA
#endif
#if !defined(MAL_NO_OPENSLES) && defined(MAL_ANDROID)
    #define MAL_ENABLE_OPENSLES
#endif
#if !defined(MAL_NO_OPENAL)
    #define MAL_ENABLE_OPENAL
#endif
#if !defined(MAL_NO_NULL)
    #define MAL_ENABLE_NULL
#endif


#if defined(_MSC_VER) && _MSC_VER < 1600
typedef   signed char    mal_int8;
typedef unsigned char    mal_uint8;
typedef   signed short   mal_int16;
typedef unsigned short   mal_uint16;
typedef   signed int     mal_int32;
typedef unsigned int     mal_uint32;
typedef   signed __int64 mal_int64;
typedef unsigned __int64 mal_uint64;
#else
#include <stdint.h>
typedef int8_t           mal_int8;
typedef uint8_t          mal_uint8;
typedef int16_t          mal_int16;
typedef uint16_t         mal_uint16;
typedef int32_t          mal_int32;
typedef uint32_t         mal_uint32;
typedef int64_t          mal_int64;
typedef uint64_t         mal_uint64;
#endif
typedef mal_uint8        mal_bool8;
typedef mal_uint32       mal_bool32;
#define MAL_TRUE         1
#define MAL_FALSE        0

typedef void* mal_handle;
typedef void* mal_ptr;
typedef void (* mal_proc)();

#ifdef MAL_WIN32
    typedef mal_handle mal_thread;
    typedef mal_handle mal_mutex;
    typedef mal_handle mal_event;
#else
    typedef pthread_t mal_thread;
    typedef pthread_mutex_t mal_mutex;
    typedef struct
    {
        pthread_mutex_t mutex;
        pthread_cond_t condition;
        mal_uint32 value;
    } mal_event;
#endif

#define MAL_MAX_PERIODS_DSOUND                          4
#define MAL_MAX_PERIODS_OPENAL                          4

typedef mal_uint8 mal_channel;
#define MAL_CHANNEL_NONE                                0
#define MAL_CHANNEL_FRONT_LEFT                          1
#define MAL_CHANNEL_FRONT_RIGHT                         2
#define MAL_CHANNEL_FRONT_CENTER                        3
#define MAL_CHANNEL_LFE                                 4
#define MAL_CHANNEL_BACK_LEFT                           5
#define MAL_CHANNEL_BACK_RIGHT                          6
#define MAL_CHANNEL_FRONT_LEFT_CENTER                   7
#define MAL_CHANNEL_FRONT_RIGHT_CENTER                  8
#define MAL_CHANNEL_BACK_CENTER                         9
#define MAL_CHANNEL_SIDE_LEFT                           10
#define MAL_CHANNEL_SIDE_RIGHT                          11
#define MAL_CHANNEL_TOP_CENTER                          12
#define MAL_CHANNEL_TOP_FRONT_LEFT                      13
#define MAL_CHANNEL_TOP_FRONT_CENTER                    14
#define MAL_CHANNEL_TOP_FRONT_RIGHT                     15
#define MAL_CHANNEL_TOP_BACK_LEFT                       16
#define MAL_CHANNEL_TOP_BACK_CENTER                     17
#define MAL_CHANNEL_TOP_BACK_RIGHT                      18
#define MAL_CHANNEL_MONO                                MAL_CHANNEL_FRONT_CENTER
#define MAL_MAX_CHANNELS                                18

#define MAL_MAX_SAMPLE_SIZE_IN_BYTES                    8

typedef int mal_result;
#define MAL_SUCCESS                                      0
#define MAL_ERROR                                       -1      // A generic error.
#define MAL_INVALID_ARGS                                -2
#define MAL_OUT_OF_MEMORY                               -3
#define MAL_FORMAT_NOT_SUPPORTED                        -4
#define MAL_NO_BACKEND                                  -5
#define MAL_NO_DEVICE                                   -6
#define MAL_API_NOT_FOUND                               -7
#define MAL_DEVICE_BUSY                                 -8
#define MAL_DEVICE_NOT_INITIALIZED                      -9
#define MAL_DEVICE_ALREADY_STARTED                      -10
#define MAL_DEVICE_ALREADY_STARTING                     -11
#define MAL_DEVICE_ALREADY_STOPPED                      -12
#define MAL_DEVICE_ALREADY_STOPPING                     -13
#define MAL_FAILED_TO_MAP_DEVICE_BUFFER                 -14
#define MAL_FAILED_TO_INIT_BACKEND                      -15
#define MAL_FAILED_TO_READ_DATA_FROM_CLIENT             -16
#define MAL_FAILED_TO_START_BACKEND_DEVICE              -17
#define MAL_FAILED_TO_STOP_BACKEND_DEVICE               -18
#define MAL_FAILED_TO_CREATE_MUTEX                      -19
#define MAL_FAILED_TO_CREATE_EVENT                      -20
#define MAL_FAILED_TO_CREATE_THREAD                     -21
#define MAL_INVALID_DEVICE_CONFIG                       -22
#define MAL_DSOUND_FAILED_TO_CREATE_DEVICE              -1024
#define MAL_DSOUND_FAILED_TO_SET_COOP_LEVEL             -1025
#define MAL_DSOUND_FAILED_TO_CREATE_BUFFER              -1026
#define MAL_DSOUND_FAILED_TO_QUERY_INTERFACE            -1027
#define MAL_DSOUND_FAILED_TO_SET_NOTIFICATIONS          -1028
#define MAL_ALSA_FAILED_TO_OPEN_DEVICE                  -2048
#define MAL_ALSA_FAILED_TO_SET_HW_PARAMS                -2049
#define MAL_ALSA_FAILED_TO_SET_SW_PARAMS                -2050
#define MAL_WASAPI_FAILED_TO_CREATE_DEVICE_ENUMERATOR   -3072
#define MAL_WASAPI_FAILED_TO_CREATE_DEVICE              -3073
#define MAL_WASAPI_FAILED_TO_ACTIVATE_DEVICE            -3074
#define MAL_WASAPI_FAILED_TO_INITIALIZE_DEVICE          -3075
#define MAL_WASAPI_FAILED_TO_FIND_BEST_FORMAT           -3076

typedef struct mal_device mal_device;

typedef void       (* mal_recv_proc)(mal_device* pDevice, mal_uint32 frameCount, const void* pSamples);
typedef mal_uint32 (* mal_send_proc)(mal_device* pDevice, mal_uint32 frameCount, void* pSamples);
typedef void       (* mal_stop_proc)(mal_device* pDevice);
typedef void       (* mal_log_proc) (mal_device* pDevice, const char* message);

typedef enum
{
    mal_backend_null,
    mal_backend_wasapi,
    mal_backend_dsound,
    mal_backend_alsa,
    mal_backend_opensl,
    mal_backend_openal
} mal_backend;

typedef enum
{
    mal_device_type_playback,
    mal_device_type_capture
} mal_device_type;

typedef enum
{
    // I like to keep these explicitly defined because they're used as a key into a lookup table. When items are
    // added to this, make sure there are no gaps and that they're added to the lookup table in mal_get_sample_size_in_bytes().
    mal_format_u8  = 0,
    mal_format_s16 = 1,   // Seems to be the most widely supported format.
    mal_format_s24 = 2,   // Tightly packed. 3 bytes per sample.
    mal_format_s32 = 3,
    mal_format_f32 = 4,
} mal_format;

typedef enum
{
    mal_channel_mix_mode_basic,     // Drop excess channels; zeroed out extra channels.
    mal_channel_mix_mode_blend,     // Blend channels based on locality.
} mal_channel_mix_mode;

typedef union
{
#ifdef MAL_ANDROIND
    mal_uint32 opensl;      // OpenSL|ES uses a 32-bit unsigned integer for identification.
#endif
#ifdef MAL_LINUX
    char alsa[32];          // ALSA uses a name string for identification.
#endif
#ifdef MAL_WIN32
    mal_uint8 dsound[16];   // DirectSound uses a GUID for identification.
    wchar_t wasapi[64];     // WASAPI uses a wchar_t string for identification which is also annoyingly long...
#endif
    char openal[256];       // OpenAL seems to use human-readable device names as the ID.
} mal_device_id;

typedef struct
{
    mal_device_id id;
    char name[256];
} mal_device_info;

typedef struct
{
    int64_t counter;
} mal_timer;


typedef struct mal_src mal_src;
typedef mal_uint32 (* mal_src_read_proc)(mal_uint32 frameCount, void* pFramesOut, void* pUserData); // Returns the number of frames that were read.

typedef enum
{
    mal_src_algorithm_none,
    mal_src_algorithm_linear
} mal_src_algorithm;

#define MAL_SRC_CACHE_SIZE_IN_FRAMES    512
typedef struct
{
    mal_src* pSRC;
    float pCachedFrames[MAL_MAX_CHANNELS * MAL_SRC_CACHE_SIZE_IN_FRAMES];
    mal_uint32 cachedFrameCount;
    mal_uint32 iNextFrame;
} mal_src_cache;

typedef struct
{
    mal_uint32 sampleRateIn;
    mal_uint32 sampleRateOut;
    mal_format formatIn;
    mal_format formatOut;
    mal_uint32 channels;
    mal_src_algorithm algorithm;
    mal_uint32 cacheSizeInFrames;  // The number of frames to read from the client at a time.
} mal_src_config;

struct mal_src
{
    mal_src_config config;
    mal_src_read_proc onRead;
    void* pUserData;
    float ratio;
    float bin[256];
    mal_src_cache cache;    // <-- For simplifying and optimizing client -> memory reading.

    union
    {
        struct
        {
            float alpha;
            mal_bool32 isBinLoaded : 1;
        } linear;
    };
};

typedef struct mal_dsp mal_dsp;
typedef mal_uint32 (* mal_dsp_read_proc)(mal_uint32 frameCount, void* pSamplesOut, void* pUserData);

typedef struct
{
    mal_format  formatIn;
    mal_uint32  channelsIn;
    mal_uint32  sampleRateIn;
    mal_channel channelMapIn[MAL_MAX_CHANNELS];
    mal_format  formatOut;
    mal_uint32  channelsOut;
    mal_uint32  sampleRateOut;
    mal_channel channelMapOut[MAL_MAX_CHANNELS];
    mal_uint32  cacheSizeInFrames;
} mal_dsp_config;

struct mal_dsp
{
    mal_dsp_config config;
    mal_dsp_read_proc onRead;
    void* pUserDataForOnRead;
    mal_src src;    // For sample rate conversion.
    mal_channel channelMapInPostMix[MAL_MAX_CHANNELS];   // <-- When mixing, new channels may need to be created. This represents the channel map after mixing.
    mal_channel channelShuffleTable[MAL_MAX_CHANNELS];
    mal_bool32 isChannelMappingRequired : 1;
    mal_bool32 isSRCRequired : 1;
    mal_bool32 isPassthrough : 1;       // <-- Will be set to true when the DSP pipeline is an optimized passthrough.
};


typedef struct
{
    mal_format format;
    mal_uint32 channels;
    mal_uint32 sampleRate;
    mal_channel channelMap[MAL_MAX_CHANNELS];
    mal_uint32 bufferSizeInFrames;
    mal_uint32 periods;
    mal_recv_proc onRecvCallback;
    mal_send_proc onSendCallback;
    mal_stop_proc onStopCallback;
    mal_log_proc  onLogCallback;
} mal_device_config;

typedef struct
{
    mal_backend backend;    // DirectSound, ALSA, etc.

    union
    {
        struct
        {
            /*IMMDeviceEnumerator**/ mal_ptr pDeviceEnumerator;
        } wasapi;

        struct
        {
            /*HMODULE*/ mal_handle hDSoundDLL;
        } dsound;

        struct
        {
            int _unused;
        } alsa;

        struct
        {
            int _unused;
        } opensl;

        struct
        {
            /*HMODULE*/ mal_handle hOpenAL;     // OpenAL32.dll, etc.
            mal_proc alcCreateContext;
            mal_proc alcMakeContextCurrent;
            mal_proc alcProcessContext;
            mal_proc alcSuspendContext;
            mal_proc alcDestroyContext;
            mal_proc alcGetCurrentContext;
            mal_proc alcGetContextsDevice;
            mal_proc alcOpenDevice;
            mal_proc alcCloseDevice;
            mal_proc alcGetError;
            mal_proc alcIsExtensionPresent;
            mal_proc alcGetProcAddress;
            mal_proc alcGetEnumValue;
            mal_proc alcGetString;
            mal_proc alcGetIntegerv;
            mal_proc alcCaptureOpenDevice;
            mal_proc alcCaptureCloseDevice;
            mal_proc alcCaptureStart;
            mal_proc alcCaptureStop;
            mal_proc alcCaptureSamples;

            mal_proc alEnable;
            mal_proc alDisable;
            mal_proc alIsEnabled;
            mal_proc alGetString;
            mal_proc alGetBooleanv;
            mal_proc alGetIntegerv;
            mal_proc alGetFloatv;
            mal_proc alGetDoublev;
            mal_proc alGetBoolean;
            mal_proc alGetInteger;
            mal_proc alGetFloat;
            mal_proc alGetDouble;
            mal_proc alGetError;
            mal_proc alIsExtensionPresent;
            mal_proc alGetProcAddress;
            mal_proc alGetEnumValue;
            mal_proc alGenSources;
            mal_proc alDeleteSources;
            mal_proc alIsSource;
            mal_proc alSourcef;
            mal_proc alSource3f;
            mal_proc alSourcefv;
            mal_proc alSourcei;
            mal_proc alSource3i;
            mal_proc alSourceiv;
            mal_proc alGetSourcef;
            mal_proc alGetSource3f;
            mal_proc alGetSourcefv;
            mal_proc alGetSourcei;
            mal_proc alGetSource3i;
            mal_proc alGetSourceiv;
            mal_proc alSourcePlayv;
            mal_proc alSourceStopv;
            mal_proc alSourceRewindv;
            mal_proc alSourcePausev;
            mal_proc alSourcePlay;
            mal_proc alSourceStop;
            mal_proc alSourceRewind;
            mal_proc alSourcePause;
            mal_proc alSourceQueueBuffers;
            mal_proc alSourceUnqueueBuffers;
            mal_proc alGenBuffers;
            mal_proc alDeleteBuffers;
            mal_proc alIsBuffer;
            mal_proc alBufferData;
            mal_proc alBufferf;
            mal_proc alBuffer3f;
            mal_proc alBufferfv;
            mal_proc alBufferi;
            mal_proc alBuffer3i;
            mal_proc alBufferiv;
            mal_proc alGetBufferf;
            mal_proc alGetBuffer3f;
            mal_proc alGetBufferfv;
            mal_proc alGetBufferi;
            mal_proc alGetBuffer3i;
            mal_proc alGetBufferiv;

            mal_uint32 isFloat32Supported   : 1;
            mal_uint32 isMCFormatsSupported : 1;
        } openal;

        struct
        {
            int _unused;
        } null_device;
    };

    union
    {
        struct
        {
            /*HMODULE*/ mal_handle hOle32DLL;
            mal_proc CoInitializeEx;
            mal_proc CoUninitialize;
            mal_proc CoCreateInstance;
            mal_proc CoTaskMemFree;
            mal_proc PropVariantClear;
        } win32;

        int _unused;
    };
} mal_context;

struct mal_device
{
    mal_context* pContext;
    mal_device_type type;
    mal_format format;
    mal_uint32 channels;
    mal_uint32 sampleRate;
    mal_uint8  channelMap[MAL_MAX_CHANNELS];
    mal_uint32 bufferSizeInFrames;
    mal_uint32 periods;
    mal_uint32 state;
    mal_recv_proc onRecv;
    mal_send_proc onSend;
    mal_stop_proc onStop;
    mal_log_proc onLog;
    void* pUserData;        // Application defined data.
    mal_mutex lock;
    mal_event wakeupEvent;
    mal_event startEvent;
    mal_event stopEvent;
    mal_thread thread;
    mal_result workResult;  // This is set by the worker thread after it's finished doing a job.
    mal_uint32 flags;       // MAL_DEVICE_FLAG_*
    mal_format internalFormat;
    mal_uint32 internalChannels;
    mal_uint32 internalSampleRate;
    mal_uint8  internalChannelMap[MAL_MAX_CHANNELS];
    mal_dsp dsp;                    // Samples run through this to convert samples to a format suitable for use by the backend.
    mal_uint32 _dspFrameCount;      // Internal use only. Used when running the device -> DSP -> client pipeline. See mal_device__on_read_from_device().
    const mal_uint8* _dspFrames;    // ^^^ AS ABOVE ^^^


    union
    {
        struct
        {
            /*IMMDevice**/ mal_ptr pDevice;
            /*IAudioClient*/ mal_ptr pAudioClient;
            /*IAudioRenderClient */ mal_ptr pRenderClient;
            /*IAudioCaptureClient */ mal_ptr pCaptureClient;
            /*HANDLE*/ mal_handle hStopEvent;
            mal_bool32 breakFromMainLoop;
        } wasapi;

        struct
        {
            /*HMODULE*/ mal_handle hDSoundDLL;
            /*LPDIRECTSOUND8*/ mal_ptr pPlayback;
            /*LPDIRECTSOUNDBUFFER*/ mal_ptr pPlaybackPrimaryBuffer;
            /*LPDIRECTSOUNDBUFFER*/ mal_ptr pPlaybackBuffer;
            /*LPDIRECTSOUNDCAPTURE8*/ mal_ptr pCapture;
            /*LPDIRECTSOUNDCAPTUREBUFFER8*/ mal_ptr pCaptureBuffer;
            /*LPDIRECTSOUNDNOTIFY*/ mal_ptr pNotify;
            /*HANDLE*/ mal_handle pNotifyEvents[MAL_MAX_PERIODS_DSOUND];  // One event handle for each period.
            /*HANDLE*/ mal_handle hStopEvent;
            mal_uint32 lastProcessedFrame;      // This is circular.
            mal_bool32 breakFromMainLoop;
        } dsound;

        struct
        {
            /*snd_pcm_t**/ mal_ptr pPCM;
            mal_bool32 isUsingMMap;
            mal_bool32 breakFromMainLoop;
            void* pIntermediaryBuffer;
        } alsa;

        struct
        {
            /*SLObjectItf*/ mal_ptr pOutputMixObj;
            /*SLOutputMixItf*/ mal_ptr pOutputMix;
            /*SLObjectItf*/ mal_ptr pAudioPlayerObj;
            /*SLPlayItf*/ mal_ptr pAudioPlayer;
            /*SLObjectItf*/ mal_ptr pAudioRecorderObj;
            /*SLRecordItf*/ mal_ptr pAudioRecorder;
            /*SLAndroidSimpleBufferQueueItf*/ mal_ptr pBufferQueue;
            mal_uint32 periodSizeInFrames;
            mal_uint32 currentBufferIndex;
            mal_uint8* pBuffer;                 // This is malloc()'d and is used for storing audio data. Typed as mal_uint8 for easy offsetting.
        } opensl;

        struct
        {
            /*ALCcontext**/ mal_ptr pContextALC;
            /*ALCdevice**/ mal_ptr pDeviceALC;
            /*ALuint*/ mal_uint32 sourceAL;
            /*ALuint*/ mal_uint32 buffersAL[MAL_MAX_PERIODS_OPENAL];
            /*ALenum*/ mal_uint32 formatAL;
            mal_uint32 subBufferSizeInFrames;   // This is the size of each of the OpenAL buffers (buffersAL).
            mal_uint8* pIntermediaryBuffer;     // This is malloc()'d and is used as the destination for reading from the client. Typed as mal_uint8 for easy offsetting.
            mal_uint32 iNextBuffer;             // The next buffer to unenqueue and then re-enqueue as new data is read.
            mal_bool32 breakFromMainLoop;
        } openal;

        struct
        {
            mal_timer timer;
            mal_uint32 lastProcessedFrame;      // This is circular.
            mal_bool32 breakFromMainLoop;
            mal_uint8* pBuffer;                 // This is malloc()'d and is used as the destination for reading from the client. Typed as mal_uint8 for easy offsetting.
        } null_device;
    };
};
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif

// Initializes a context.
//
// The context is used for selecting and initializing the relevant backends.
//
// Note that the location of the device cannot change throughout it's lifetime. Consider allocating
// the mal_context object with malloc() if this is an issue. The reason for this is that the pointer
// is stored in the mal_device structure.
//
// <backends> is used to allow the application to prioritize backends depending on it's specific
// requirements. This can be null in which case it uses the default priority, which is as follows:
//   - DirectSound
//   - WASAPI
//   - ALSA
//   - OpenSL|ES
//   - OpenAL
//   - Null
//
// Return Value:
//   - MAL_SUCCESS if successful.
//   - MAL_INVALID_ARGS
//       One or more of the input arguments is invalid.
//   - MAL_NO_BACKEND
//       There is no supported backend, or there was an error loading it (such as a missing dll/so).
//
// Thread Safety: UNSAFE
//
// Effeciency: LOW
//   This will dynamically load backends DLLs/SOs (such as dsound.dll).
mal_result mal_context_init(mal_backend backends[], mal_uint32 backendCount, mal_context* pContext);

// Uninitializes a context.
//
// Results are undefined if you call this while a related is still active.
//
// Return Value:
//   - MAL_SUCCESS if successful.
//   - MAL_INVALID_ARGS
//       One or more of the input arguments is invalid.
//   - MAL_NO_BACKEND
//       The device has an unknown backend. This probably means the context of <pContext> has been trashed.
//
// Thread Safety: UNSAFE
//
// Efficiency: LOW
//   This will unload the backend DLLs/SOs.
mal_result mal_context_uninit(mal_context* pContext);

// Enumerates over each device of the given type (playback or capture).
//
// It is _not_ safe to assume the first enumerated device is the default device.
//
// Return Value:
//   - MAL_SUCCESS if successful.
//   - MAL_INVALID_ARGS
//       One or more of the input arguments is invalid.
//   - MAL_NO_BACKEND
//       There is no supported backend, or there was an error loading it (such as a missing dll/so).
//
// Thread Safety: SAFE, SEE NOTES.
//   This API uses an application-defined buffer for output. This is thread-safe so long as the
//   application ensures mutal exclusion to the output buffer at their level.
//
// Efficiency: LOW
//   This API dynamically links to backend DLLs/SOs (such as dsound.dll).
mal_result mal_enumerate_devices(mal_context* pContext, mal_device_type type, mal_uint32* pCount, mal_device_info* pInfo);

// Initializes a device.
//
// The device ID (pDeviceID) can be null, in which case the default device is used. Otherwise, you
// can retrieve the ID by calling mal_enumerate_devices() and using the ID from the returned data.
// Set pDeviceID to NULL to use the default device. Do _not_ rely on the first device ID returned
// by mal_enumerate_devices() to be the default device.
//
// This will try it's hardest to create a valid device, even if it means adjusting input arguments.
// Look at pDevice->internalChannels, pDevice->internalSampleRate, etc. to determine the actual
// properties after initialization.
//
// If <bufferSizeInFrames> is 0, it will default to MAL_DEFAULT_BUFFER_SIZE_IN_MILLISECONDS. If
// <periods> is set to 0 it will default to MAL_DEFAULT_PERIODS.
//
// The <periods> property controls how frequently the background thread is woken to check for more
// data. It's tied to the buffer size, so as an example, if your buffer size is equivalent to 10
// milliseconds and you have 2 periods, the CPU will wake up approximately every 5 milliseconds.
//
// Consider using mal_device_config_init(), mal_device_config_init_playback(), etc. to make it easier
// to initialize a mal_device_config object.
//
// Return Value:
//   - MAL_SUCCESS if successful.
//   - MAL_INVALID_ARGS
//       One or more of the input arguments is invalid.
//   - MAL_NO_BACKEND
//       There is no supported backend, or there was an error loading it (such as a missing dll/so).
//   - MAL_OUT_OF_MEMORY
//       A necessary memory allocation failed, likely due to running out of memory.
//   - MAL_FORMAT_NOT_SUPPORTED
//       The specified format is not supported by the backend. mini_al does not currently do any
//       software format conversions which means initialization must fail if the backend does not
//       natively support it.
//   - MAL_FAILED_TO_INIT_BACKEND
//       There was a backend-specific error during initialization.
//
// Thread Safety: UNSAFE
//   It is not safe to call this function simultaneously for different devices because some backends
//   depend on and mutate global state (such as OpenSL|ES). The same applies to calling this as the
//   same time as mal_device_uninit().
//
//   Results are undefined if you try using a device before this function has returned.
//
// Efficiency: LOW
//   This is just slow due to the nature of it being an initialization API.
mal_result mal_device_init(mal_context* pContext, mal_device_type type, mal_device_id* pDeviceID, mal_device_config* pConfig, void* pUserData, mal_device* pDevice);

// Uninitializes a device.
//
// This will explicitly stop the device. You do not need to call mal_device_stop() beforehand, but it's
// harmless if you do.
//
// Return Value:
//   - MAL_SUCCESS if successful.
//   - MAL_INVALID_ARGS
//       pDevice is NULL.
//   - MAL_DEVICE_NOT_INITIALIZED
//       The device is not currently or was never initialized.
//
// Thread Safety: UNSAFE
//   As soon as this API is called the device should be considered undefined. All bets are off if you
//   try using the device at the same time as uninitializing it.
//
// Efficiency: LOW
//   This will stop the device with mal_device_stop() which is a slow, synchronized call. It also needs
//   to destroy internal objects like the backend-specific objects and the background thread.
void mal_device_uninit(mal_device* pDevice);

// Sets the callback to use when the application has received data from the device.
//
// Thread Safety: SAFE
//   This API is implemented as a simple atomic assignment.
//
// Efficiency: HIGH
//   This is just an atomic assignment.
void mal_device_set_recv_callback(mal_device* pDevice, mal_recv_proc proc);

// Sets the callback to use when the application needs to send data to the device for playback.
//
// Note that the implementation of this callback must copy over as many samples as is available. The
// return value specifies how many samples were written to the output buffer. The backend will fill
// any leftover samples with silence.
//
// Thread Safety: SAFE
//   This API is implemented as a simple atomic assignment.
//
// Efficiency: HIGH
//   This is just an atomic assignment.
void mal_device_set_send_callback(mal_device* pDevice, mal_send_proc proc);

// Sets the callback to use when the device has stopped, either explicitly or as a result of an error.
//
// Thread Safety: SAFE
//   This API is implemented as a simple atomic assignment.
//
// Efficiency: HIGH
//   This is just an atomic assignment.
void mal_device_set_stop_callback(mal_device* pDevice, mal_stop_proc proc);

// Activates the device. For playback devices this begins playback. For capture devices it begins
// recording.
//
// For a playback device, this will retrieve an initial chunk of audio data from the client before
// returning. The reason for this is to ensure there is valid audio data in the buffer, which needs
// to be done _before_ the device begins playback.
//
// Return Value:
//   - MAL_SUCCESS if successful.
//   - MAL_INVALID_ARGS
//       One or more of the input arguments is invalid.
//   - MAL_DEVICE_NOT_INITIALIZED
//       The device is not currently or was never initialized.
//   - MAL_DEVICE_BUSY
//       The device is in the process of stopping. This will only happen if mal_device_start() and
//       mal_device_stop() is called simultaneous on separate threads. This will never be returned in
//       single-threaded applications.
//   - MAL_DEVICE_ALREADY_STARTING
//       The device is already in the process of starting. This will never be returned in single-threaded
//       applications.
//   - MAL_DEVICE_ALREADY_STARTED
//       The device is already started.
//   - MAL_FAILED_TO_READ_DATA_FROM_CLIENT
//       Failed to read the initial chunk of audio data from the client. This initial chunk of data is
//       required so that the device has valid audio data as soon as it starts playing. This will never
//       be returned for capture devices.
//   - MAL_FAILED_TO_START_BACKEND_DEVICE
//       There was a backend-specific error starting the device.
//
// Thread Safety: SAFE
//
// Efficiency: LOW
//   This API waits until the backend device has been started for real by the worker thread. It also
//   waits on a mutex for thread-safety.
mal_result mal_device_start(mal_device* pDevice);

// Puts the device to sleep, but does not uninitialize it. Use mal_device_start() to start it up again.
//
// Return Value:
//   - MAL_SUCCESS if successful.
//   - MAL_INVALID_ARGS
//       One or more of the input arguments is invalid.
//   - MAL_DEVICE_NOT_INITIALIZED
//       The device is not currently or was never initialized.
//   - MAL_DEVICE_BUSY
//       The device is in the process of starting. This will only happen if mal_device_start() and
//       mal_device_stop() is called simultaneous on separate threads. This will never be returned in
//       single-threaded applications.
//   - MAL_DEVICE_ALREADY_STOPPING
//       The device is already in the process of stopping. This will never be returned in single-threaded
//       applications.
//   - MAL_DEVICE_ALREADY_STOPPED
//       The device is already stopped.
//   - MAL_FAILED_TO_STOP_BACKEND_DEVICE
//       There was a backend-specific error stopping the device.
//
// Thread Safety: SAFE
//
// Efficiency: LOW
//   This API needs to wait on the worker thread to stop the backend device properly before returning. It
//   also waits on a mutex for thread-safety.
mal_result mal_device_stop(mal_device* pDevice);

// Determines whether or not the device is started.
//
// Return Value:
//   True if the device is started, false otherwise.
//
// Thread Safety: SAFE
//   If another thread calls mal_device_start() or mal_device_stop() at this same time as this function
//   is called, there's a very small chance the return value will be out of sync.
//
// Efficiency: HIGH
//   This is implemented with a simple accessor.
mal_bool32 mal_device_is_started(mal_device* pDevice);

// Retrieves the size of the buffer in bytes for the given device.
//
// Thread Safety: SAFE
//   This is calculated from constant values which are set at initialization time and never change.
//
// Efficiency: HIGH
//   This is implemented with just a few 32-bit integer multiplications.
mal_uint32 mal_device_get_buffer_size_in_bytes(mal_device* pDevice);

// Retrieves the size of a sample in bytes for the given format.
//
// Thread Safety: SAFE
//   This is API is pure.
//
// Efficiency: HIGH
//   This is implemented with a lookup table.
mal_uint32 mal_get_sample_size_in_bytes(mal_format format);

// Helper function for initializing a mal_device_config object.
//
// This is just a helper API, and as such the returned object can be safely modified as needed.
//
// The default channel mapping is based on the channel count, as per the table below. Note that these
// can be freely changed after this function returns if you are needing something in particular.
//
// | Channel Count | Mapping                      |
// |---------------|------------------------------|
// | 1 (Mono)      | 0: MAL_CHANNEL_FRONT_CENTER  |
// |---------------|------------------------------|
// | 2 (Stereo)    | 0: MAL_CHANNEL_FRONT_LEFT    |
// |               | 1: MAL_CHANNEL_FRONT_RIGHT   |
// |---------------|------------------------------|
// | 3 (2.1)       | 0: MAL_CHANNEL_FRONT_LEFT    |
// |               | 1: MAL_CHANNEL_FRONT_RIGHT   |
// |               | 2: MAL_CHANNEL_LFE           |
// |---------------|------------------------------|
// | 4 (Quad)      | 0: MAL_CHANNEL_FRONT_LEFT    |
// |               | 1: MAL_CHANNEL_FRONT_RIGHT   |
// |               | 2: MAL_CHANNEL_BACK_LEFT     |
// |               | 3: MAL_CHANNEL_BACK_RIGHT    |
// |---------------|------------------------------|
// | 5 (4.1)       | 0: MAL_CHANNEL_FRONT_LEFT    |
// |               | 1: MAL_CHANNEL_FRONT_RIGHT   |
// |               | 2: MAL_CHANNEL_BACK_LEFT     |
// |               | 3: MAL_CHANNEL_BACK_RIGHT    |
// |               | 4: MAL_CHANNEL_LFE           |
// |---------------|------------------------------|
// | 6 (5.1)       | 0: MAL_CHANNEL_FRONT_LEFT    |
// |               | 1: MAL_CHANNEL_FRONT_RIGHT   |
// |               | 2: MAL_CHANNEL_FRONT_CENTER  |
// |               | 3: MAL_CHANNEL_LFE           |
// |               | 4: MAL_CHANNEL_BACK_LEFT     |
// |               | 5: MAL_CHANNEL_BACK_RIGHT    |
// |---------------|------------------------------|
// | 8 (7.1)       | 0: MAL_CHANNEL_FRONT_LEFT    |
// |               | 1: MAL_CHANNEL_FRONT_RIGHT   |
// |               | 2: MAL_CHANNEL_FRONT_CENTER  |
// |               | 3: MAL_CHANNEL_LFE           |
// |               | 4: MAL_CHANNEL_BACK_LEFT     |
// |               | 5: MAL_CHANNEL_BACK_RIGHT    |
// |               | 6: MAL_CHANNEL_SIDE_LEFT     |
// |               | 7: MAL_CHANNEL_SIDE_RIGHT    |
// |---------------|------------------------------|
// | Other         | All channels set to 0. This  |
// |               | is equivalent to the same    |
// |               | mapping as the device.       |
// |---------------|------------------------------|
//
// Thread Safety: SAFE
//
// Efficiency: HIGH
//   This just returns a stack allocated object and consists of just a few assignments.
mal_device_config mal_device_config_init(mal_format format, mal_uint32 channels, mal_uint32 sampleRate, mal_recv_proc onRecvCallback, mal_send_proc onSendCallback);

// A simplified version of mal_device_config_init() for capture devices.
static inline mal_device_config mal_device_config_init_capture(mal_format format, mal_uint32 channels, mal_uint32 sampleRate, mal_recv_proc onRecvCallback) { return mal_device_config_init(format, channels, sampleRate, onRecvCallback, NULL); }

// A simplified version of mal_device_config_init() for playback devices.
static inline mal_device_config mal_device_config_init_playback(mal_format format, mal_uint32 channels, mal_uint32 sampleRate, mal_send_proc onSendCallback) { return mal_device_config_init(format, channels, sampleRate, NULL, onSendCallback); }




///////////////////////////////////////////////////////////////////////////////
//
// SRC
//
///////////////////////////////////////////////////////////////////////////////

// Initializes a sample rate conversion object.
mal_result mal_src_init(mal_src_config* pConfig, mal_src_read_proc onRead, void* pUserData, mal_src* pSRC);

// Reads a number of frames.
//
// Returns the number of frames actually read.
mal_uint32 mal_src_read_frames(mal_src* pSRC, mal_uint32 frameCount, void* pFramesOut);



///////////////////////////////////////////////////////////////////////////////
//
// DSP
//
///////////////////////////////////////////////////////////////////////////////
#if 0
#include "tools/mal_build/bin/mini_al_dsp.h"
#else
// Initializes a DSP object.
mal_result mal_dsp_init(mal_dsp_config* pConfig, mal_dsp_read_proc onRead, void* pUserData, mal_dsp* pDSP);

// Reads a number of frames and runs them through the DSP processor.
mal_uint32 mal_dsp_read_frames(mal_dsp* pDSP, mal_uint32 frameCount, void* pFramesOut);



///////////////////////////////////////////////////////////////////////////////
//
// Miscellaneous Helpers
//
///////////////////////////////////////////////////////////////////////////////

// Blends two frames in floating point format.
void mal_blend_f32(float* pOut, float* pInA, float* pInB, float factor, mal_uint32 channels);



///////////////////////////////////////////////////////////////////////////////
//
// Format Conversion
//
///////////////////////////////////////////////////////////////////////////////
void mal_pcm_u8_to_s16(short* pOut, const unsigned char* pIn, unsigned int count);
void mal_pcm_u8_to_s24(void* pOut, const unsigned char* pIn, unsigned int count);
void mal_pcm_u8_to_s32(int* pOut, const unsigned char* pIn, unsigned int count);
void mal_pcm_u8_to_f32(float* pOut, const unsigned char* pIn, unsigned int count);
void mal_pcm_s16_to_u8(unsigned char* pOut, const short* pIn, unsigned int count);
void mal_pcm_s16_to_s24(void* pOut, const short* pIn, unsigned int count);
void mal_pcm_s16_to_s32(int* pOut, const short* pIn, unsigned int count);
void mal_pcm_s16_to_f32(float* pOut, const short* pIn, unsigned int count);
void mal_pcm_s24_to_u8(unsigned char* pOut, const void* pIn, unsigned int count);
void mal_pcm_s24_to_s16(short* pOut, const void* pIn, unsigned int count);
void mal_pcm_s24_to_s32(int* pOut, const void* pIn, unsigned int count);
void mal_pcm_s24_to_f32(float* pOut, const void* pIn, unsigned int count);
void mal_pcm_s32_to_u8(unsigned char* pOut, const int* pIn, unsigned int count);
void mal_pcm_s32_to_s16(short* pOut, const int* pIn, unsigned int count);
void mal_pcm_s32_to_s24(void* pOut, const int* pIn, unsigned int count);
void mal_pcm_s32_to_f32(float* pOut, const int* pIn, unsigned int count);
void mal_pcm_f32_to_u8(unsigned char* pOut, const float* pIn, unsigned int count);
void mal_pcm_f32_to_s16(short* pOut, const float* pIn, unsigned int count);
void mal_pcm_f32_to_s24(void* pOut, const float* pIn, unsigned int count);
void mal_pcm_f32_to_s32(int* pOut, const float* pIn, unsigned int count);
void mal_pcm_convert(void* pOut, mal_format formatOut, const void* pIn, mal_format formatIn, unsigned int sampleCount);


#endif

#ifdef __cplusplus
}
#endif
#endif  //mini_al_h


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//
// IMPLEMENTATION
//
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
#ifdef MAL_IMPLEMENTATION
#include <assert.h>

#ifdef MAL_WIN32
#include <windows.h>
#else
#include <stdlib.h> // For malloc()/free()
#include <string.h> // For memset()
#endif

#ifdef MAL_POSIX
#include <unistd.h>
#include <dlfcn.h>
#endif

#ifdef MAL_ENABLE_ALSA
#include <stdio.h>  // Needed for sprintf() which is used for "hw:%d,%d" formatting. TODO: Remove this later.
#endif

#if !defined(MAL_64BIT) && !defined(MAL_32BIT)
#ifdef _WIN32
#ifdef _WIN64
#define MAL_64BIT
#else
#define MAL_32BIT
#endif
#endif
#endif

#if !defined(MAL_64BIT) && !defined(MAL_32BIT)
#ifdef __GNUC__
#ifdef __LP64__
#define MAL_64BIT
#else
#define MAL_32BIT
#endif
#endif
#endif

#if !defined(MAL_64BIT) && !defined(MAL_32BIT)
#include <stdint.h>
#if INTPTR_MAX == INT64_MAX
#define MAL_64BIT
#else
#define MAL_32BIT
#endif
#endif


#ifdef MAL_WIN32
    #define MAL_THREADCALL WINAPI
    typedef unsigned long mal_thread_result;
#else
    #define MAL_THREADCALL
    typedef void* mal_thread_result;
#endif
typedef mal_thread_result (MAL_THREADCALL * mal_thread_entry_proc)(void* pData);

#ifdef MAL_WIN32
typedef HRESULT (WINAPI * MAL_PFN_CoInitializeEx)(LPVOID pvReserved, DWORD  dwCoInit);
typedef void    (WINAPI * MAL_PFN_CoUninitialize)();
typedef HRESULT (WINAPI * MAL_PFN_CoCreateInstance)(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID *ppv);
typedef void    (WINAPI * MAL_PFN_CoTaskMemFree)(_In_opt_ LPVOID pv);
typedef HRESULT (WINAPI * MAL_PFN_PropVariantClear)(PROPVARIANT *pvar);
#endif


#define MAL_STATE_UNINITIALIZED     0
#define MAL_STATE_STOPPED           1   // The device's default state after initialization.
#define MAL_STATE_STARTED           2   // The worker thread is in it's main loop waiting for the driver to request or deliver audio data.
#define MAL_STATE_STARTING          3   // Transitioning from a stopped state to started.
#define MAL_STATE_STOPPING          4   // Transitioning from a started state to stopped.

#define MAL_DEVICE_FLAG_USING_DEFAULT_BUFFER_SIZE   (1 << 0)
#define MAL_DEVICE_FLAG_USING_DEFAULT_PERIODS       (1 << 1)


// The default size of the device's buffer in milliseconds.
//
// If this is too small you may get underruns and overruns in which case you'll need to either increase
// this value or use an explicit buffer size.
#ifndef MAL_DEFAULT_BUFFER_SIZE_IN_MILLISECONDS
#define MAL_DEFAULT_BUFFER_SIZE_IN_MILLISECONDS     25
#endif

// Default periods when none is specified in mal_device_init(). More periods means more work on the CPU.
#ifndef MAL_DEFAULT_PERIODS
#define MAL_DEFAULT_PERIODS                         2
#endif


///////////////////////////////////////////////////////////////////////////////
//
// Standard Library Stuff
//
///////////////////////////////////////////////////////////////////////////////
#ifndef mal_zero_memory
#ifdef MAL_WIN32
#define mal_zero_memory(p, sz) ZeroMemory((p), (sz))
#else
#define mal_zero_memory(p, sz) memset((p), 0, (sz))
#endif
#endif

#define mal_zero_object(p) mal_zero_memory((p), sizeof(*(p)))

#ifndef mal_copy_memory
#ifdef MAL_WIN32
#define mal_copy_memory(dst, src, sz) CopyMemory((dst), (src), (sz))
#else
#define mal_copy_memory(dst, src, sz) memcpy((dst), (src), (sz))
#endif
#endif

#ifndef mal_malloc
#ifdef MAL_WIN32
#define mal_malloc(sz) HeapAlloc(GetProcessHeap(), 0, (sz))
#else
#define mal_malloc(sz) malloc((sz))
#endif
#endif

#ifndef mal_free
#ifdef MAL_WIN32
#define mal_free(p) HeapFree(GetProcessHeap(), 0, (p))
#else
#define mal_free(p) free((p))
#endif
#endif

#ifndef mal_assert
#ifdef MAL_WIN32
#define mal_assert(condition) assert(condition)
#else
#define mal_assert(condition) assert(condition)
#endif
#endif

#define mal_countof(x)  (sizeof(x) / sizeof(x[0]))
#define mal_max(x, y)   (((x) > (y)) ? (x) : (y))
#define mal_min(x, y)   (((x) < (y)) ? (x) : (y))

#define mal_buffer_frame_capacity(buffer, channels, format) (sizeof(buffer) / mal_get_sample_size_in_bytes(format) / (channels))


// Return Values:
//   0:  Success
//   22: EINVAL
//   34: ERANGE
//
// Not using symbolic constants for errors because I want to avoid #including errno.h
static int mal_strncpy_s(char* dst, size_t dstSizeInBytes, const char* src, size_t count)
{
    if (dst == 0) {
        return 22;
    }
    if (dstSizeInBytes == 0) {
        return 22;
    }
    if (src == 0) {
        dst[0] = '\0';
        return 22;
    }

    size_t maxcount = count;
    if (count == ((size_t)-1) || count >= dstSizeInBytes) {        // -1 = _TRUNCATE
        maxcount = dstSizeInBytes - 1;
    }

    size_t i;
    for (i = 0; i < maxcount && src[i] != '\0'; ++i) {
        dst[i] = src[i];
    }

    if (src[i] == '\0' || i == count || count == ((size_t)-1)) {
        dst[i] = '\0';
        return 0;
    }

    dst[0] = '\0';
    return 34;
}

int mal_strcmp(const char* str1, const char* str2)
{
    if (str1 == str2) return  0;

    // These checks differ from the standard implementation. It's not important, but I prefer
    // it just for sanity.
    if (str1 == NULL) return -1;
    if (str2 == NULL) return  1;

    for (;;) {
        if (str1[0] == '\0') {
            break;
        }
        if (str1[0] != str2[0]) {
            break;
        }

        str1 += 1;
        str2 += 1;
    }

    return ((unsigned char*)str1)[0] - ((unsigned char*)str2)[0];
}


// Thanks to good old Bit Twiddling Hacks for this one: http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
static inline unsigned int mal_next_power_of_2(unsigned int x)
{
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x++;

    return x;
}

static inline unsigned int mal_prev_power_of_2(unsigned int x)
{
    return mal_next_power_of_2(x) >> 1;
}


// Clamps an f32 sample to -1..1
static inline float mal_clip_f32(float x)
{
    if (x < -1) return -1;
    if (x > +1) return +1;
    return x;
}

static inline float mal_mix_f32(float x, float y, float a)
{
    return x*(1-a) + y*a;
}


///////////////////////////////////////////////////////////////////////////////
//
// Atomics
//
///////////////////////////////////////////////////////////////////////////////
#if defined(_WIN32) && defined(_MSC_VER)
#define mal_memory_barrier()            MemoryBarrier()
#define mal_atomic_exchange_32(a, b)    InterlockedExchange((LONG*)a, (LONG)b)
#define mal_atomic_exchange_64(a, b)    InterlockedExchange64((LONGLONG*)a, (LONGLONG)b)
#define mal_atomic_increment_32(a)      InterlockedIncrement((LONG*)a)
#define mal_atomic_decrement_32(a)      InterlockedDecrement((LONG*)a)
#else
#define mal_memory_barrier()            __sync_synchronize()
#define mal_atomic_exchange_32(a, b)    (void)__sync_lock_test_and_set(a, b); __sync_synchronize()
#define mal_atomic_exchange_64(a, b)    (void)__sync_lock_test_and_set(a, b); __sync_synchronize()
#define mal_atomic_increment_32(a)      __sync_add_and_fetch(a, 1)
#define mal_atomic_decrement_32(a)      __sync_sub_and_fetch(a, 1)
#endif

#ifdef MAL_64BIT
#define mal_atomic_exchange_ptr mal_atomic_exchange_64
#endif
#ifdef MAL_32BIT
#define mal_atomic_exchange_ptr mal_atomic_exchange_32
#endif


///////////////////////////////////////////////////////////////////////////////
//
// Timing
//
///////////////////////////////////////////////////////////////////////////////
#ifdef MAL_WIN32
static LARGE_INTEGER g_mal_TimerFrequency = {{0}};
void mal_timer_init(mal_timer* pTimer)
{
    if (g_mal_TimerFrequency.QuadPart == 0) {
        QueryPerformanceFrequency(&g_mal_TimerFrequency);
    }

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    pTimer->counter = (uint64_t)counter.QuadPart;
}

double mal_timer_get_time_in_seconds(mal_timer* pTimer)
{
    LARGE_INTEGER counter;
    if (!QueryPerformanceCounter(&counter)) {
        return 0;
    }

    return (counter.QuadPart - pTimer->counter) / (double)g_mal_TimerFrequency.QuadPart;
}
#else
void mal_timer_init(mal_timer* pTimer)
{
    struct timespec newTime;
    clock_gettime(CLOCK_MONOTONIC, &newTime);

    pTimer->counter = (newTime.tv_sec * 1000000000) + newTime.tv_nsec;
}

double mal_timer_get_time_in_seconds(mal_timer* pTimer)
{
    struct timespec newTime;
    clock_gettime(CLOCK_MONOTONIC, &newTime);

    uint64_t newTimeCounter = (newTime.tv_sec * 1000000000) + newTime.tv_nsec;
    uint64_t oldTimeCounter = pTimer->counter;

    return (newTimeCounter - oldTimeCounter) / 1000000000.0;
}
#endif


///////////////////////////////////////////////////////////////////////////////
//
// Dynamic Linking
//
///////////////////////////////////////////////////////////////////////////////
mal_handle mal_dlopen(const char* filename)
{
#ifdef _WIN32
    return (mal_handle)LoadLibraryA(filename);
#else
    return (mal_handle)dlopen(filename, RTLD_NOW);
#endif
}

void mal_dlclose(mal_handle handle)
{
#ifdef _WIN32
    CloseHandle((HANDLE)handle);
#else
    dlclose((void*)handle);
#endif
}

mal_proc mal_dlsym(mal_handle handle, const char* symbol)
{
#ifdef _WIN32
    return (mal_proc)GetProcAddress((HMODULE)handle, symbol);
#else
    return (mal_proc)dlsym((void*)handle, symbol);
#endif
}


///////////////////////////////////////////////////////////////////////////////
//
// Threading
//
///////////////////////////////////////////////////////////////////////////////
#ifdef MAL_WIN32
mal_bool32 mal_thread_create__win32(mal_thread* pThread, mal_thread_entry_proc entryProc, void* pData)
{
    *pThread = CreateThread(NULL, 0, entryProc, pData, 0, NULL);
    if (*pThread == NULL) {
        return MAL_FALSE;
    }

    return MAL_TRUE;
}

void mal_thread_wait__win32(mal_thread* pThread)
{
    WaitForSingleObject(*pThread, INFINITE);
}

void mal_sleep__win32(mal_uint32 milliseconds)
{
    Sleep((DWORD)milliseconds);
}


mal_bool32 mal_mutex_create__win32(mal_mutex* pMutex)
{
    *pMutex = CreateEventA(NULL, FALSE, TRUE, NULL);
    if (*pMutex == NULL) {
        return MAL_FALSE;
    }

    return MAL_TRUE;
}

void mal_mutex_delete__win32(mal_mutex* pMutex)
{
    CloseHandle(*pMutex);
}

void mal_mutex_lock__win32(mal_mutex* pMutex)
{
    WaitForSingleObject(*pMutex, INFINITE);
}

void mal_mutex_unlock__win32(mal_mutex* pMutex)
{
    SetEvent(*pMutex);
}


mal_bool32 mal_event_create__win32(mal_event* pEvent)
{
    *pEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (*pEvent == NULL) {
        return MAL_FALSE;
    }

    return MAL_TRUE;
}

void mal_event_delete__win32(mal_event* pEvent)
{
    CloseHandle(*pEvent);
}

mal_bool32 mal_event_wait__win32(mal_event* pEvent)
{
    return WaitForSingleObject(*pEvent, INFINITE) == WAIT_OBJECT_0;
}

mal_bool32 mal_event_signal__win32(mal_event* pEvent)
{
    return SetEvent(*pEvent);
}
#endif


#ifdef MAL_POSIX
mal_bool32 mal_thread_create__posix(mal_thread* pThread, mal_thread_entry_proc entryProc, void* pData)
{
    return pthread_create(pThread, NULL, entryProc, pData) == 0;
}

void mal_thread_wait__posix(mal_thread* pThread)
{
    pthread_join(*pThread, NULL);
}

void mal_sleep__posix(mal_uint32 milliseconds)
{
    usleep(milliseconds * 1000);    // <-- usleep is in microseconds.
}


mal_bool32 mal_mutex_create__posix(mal_mutex* pMutex)
{
    return pthread_mutex_init(pMutex, NULL) == 0;
}

void mal_mutex_delete__posix(mal_mutex* pMutex)
{
    pthread_mutex_destroy(pMutex);
}

void mal_mutex_lock__posix(mal_mutex* pMutex)
{
    pthread_mutex_lock(pMutex);
}

void mal_mutex_unlock__posix(mal_mutex* pMutex)
{
    pthread_mutex_unlock(pMutex);
}


mal_bool32 mal_event_create__posix(mal_event* pEvent)
{
    if (pthread_mutex_init(&pEvent->mutex, NULL) != 0) {
        return MAL_FALSE;
    }

    if (pthread_cond_init(&pEvent->condition, NULL) != 0) {
        return MAL_FALSE;
    }

    pEvent->value = 0;
    return MAL_TRUE;
}

void mal_event_delete__posix(mal_event* pEvent)
{
    pthread_cond_destroy(&pEvent->condition);
    pthread_mutex_destroy(&pEvent->mutex);
}

mal_bool32 mal_event_wait__posix(mal_event* pEvent)
{
    pthread_mutex_lock(&pEvent->mutex);
    {
        while (pEvent->value == 0) {
            pthread_cond_wait(&pEvent->condition, &pEvent->mutex);
        }

        pEvent->value = 0;  // Auto-reset.
    }
    pthread_mutex_unlock(&pEvent->mutex);

    return MAL_TRUE;
}

mal_bool32 mal_event_signal__posix(mal_event* pEvent)
{
    pthread_mutex_lock(&pEvent->mutex);
    {
        pEvent->value = 1;
        pthread_cond_signal(&pEvent->condition);
    }
    pthread_mutex_unlock(&pEvent->mutex);

    return MAL_TRUE;
}
#endif

mal_bool32 mal_thread_create(mal_thread* pThread, mal_thread_entry_proc entryProc, void* pData)
{
    if (pThread == NULL || entryProc == NULL) return MAL_FALSE;

#ifdef MAL_WIN32
    return mal_thread_create__win32(pThread, entryProc, pData);
#endif
#ifdef MAL_POSIX
    return mal_thread_create__posix(pThread, entryProc, pData);
#endif
}

void mal_thread_wait(mal_thread* pThread)
{
    if (pThread == NULL) return;

#ifdef MAL_WIN32
    mal_thread_wait__win32(pThread);
#endif
#ifdef MAL_POSIX
    mal_thread_wait__posix(pThread);
#endif
}

void mal_sleep(mal_uint32 milliseconds)
{
#ifdef MAL_WIN32
    mal_sleep__win32(milliseconds);
#endif
#ifdef MAL_POSIX
    mal_sleep__posix(milliseconds);
#endif
}


mal_bool32 mal_mutex_create(mal_mutex* pMutex)
{
    if (pMutex == NULL) return MAL_FALSE;

#ifdef MAL_WIN32
    return mal_mutex_create__win32(pMutex);
#endif
#ifdef MAL_POSIX
    return mal_mutex_create__posix(pMutex);
#endif
}

void mal_mutex_delete(mal_mutex* pMutex)
{
    if (pMutex == NULL) return;

#ifdef MAL_WIN32
    mal_mutex_delete__win32(pMutex);
#endif
#ifdef MAL_POSIX
    mal_mutex_delete__posix(pMutex);
#endif
}

void mal_mutex_lock(mal_mutex* pMutex)
{
    if (pMutex == NULL) return;

#ifdef MAL_WIN32
    mal_mutex_lock__win32(pMutex);
#endif
#ifdef MAL_POSIX
    mal_mutex_lock__posix(pMutex);
#endif
}

void mal_mutex_unlock(mal_mutex* pMutex)
{
    if (pMutex == NULL) return;

#ifdef MAL_WIN32
    mal_mutex_unlock__win32(pMutex);
#endif
#ifdef MAL_POSIX
    mal_mutex_unlock__posix(pMutex);
#endif
}


mal_bool32 mal_event_create(mal_event* pEvent)
{
    if (pEvent == NULL) return MAL_FALSE;

#ifdef MAL_WIN32
    return mal_event_create__win32(pEvent);
#endif
#ifdef MAL_POSIX
    return mal_event_create__posix(pEvent);
#endif
}

void mal_event_delete(mal_event* pEvent)
{
    if (pEvent == NULL) return;

#ifdef MAL_WIN32
    mal_event_delete__win32(pEvent);
#endif
#ifdef MAL_POSIX
    mal_event_delete__posix(pEvent);
#endif
}

mal_bool32 mal_event_wait(mal_event* pEvent)
{
    if (pEvent == NULL) return MAL_FALSE;

#ifdef MAL_WIN32
    return mal_event_wait__win32(pEvent);
#endif
#ifdef MAL_POSIX
    return mal_event_wait__posix(pEvent);
#endif
}

mal_bool32 mal_event_signal(mal_event* pEvent)
{
    if (pEvent == NULL) return MAL_FALSE;

#ifdef MAL_WIN32
    return mal_event_signal__win32(pEvent);
#endif
#ifdef MAL_POSIX
    return mal_event_signal__posix(pEvent);
#endif
}


// Posts a log message.
static void mal_log(mal_device* pDevice, const char* message)
{
    if (pDevice == NULL) return;

    mal_log_proc onLog = pDevice->onLog;
    if (onLog) {
        onLog(pDevice, message);
    }
}

// Posts an error. Throw a breakpoint in here if you're needing to debug. The return value is always "resultCode".
static mal_result mal_post_error(mal_device* pDevice, const char* message, mal_result resultCode)
{
    mal_log(pDevice, message);
    return resultCode;
}



// The callback for reading from the client -> DSP -> device.
static inline mal_uint32 mal_device__on_read_from_client(mal_uint32 frameCount, void* pFramesOut, void* pUserData)
{
    mal_device* pDevice = (mal_device*)pUserData;
    mal_assert(pDevice != NULL);

    mal_send_proc onSend = pDevice->onSend;
    if (onSend) {
        return onSend(pDevice, frameCount, pFramesOut);
    }

    return 0;
}

// The callback for reading from the device -> DSP -> client.
static inline mal_uint32 mal_device__on_read_from_device(mal_uint32 frameCount, void* pFramesOut, void* pUserData)
{
    mal_device* pDevice = (mal_device*)pUserData;
    mal_assert(pDevice != NULL);

    if (pDevice->_dspFrameCount == 0) {
        return 0;   // Nothing left.
    }

    mal_uint32 framesToRead = frameCount;
    if (framesToRead > pDevice->_dspFrameCount) {
        framesToRead = pDevice->_dspFrameCount;
    }

    mal_uint32 bytesToRead = framesToRead * pDevice->internalChannels * mal_get_sample_size_in_bytes(pDevice->internalFormat);
    mal_copy_memory(pFramesOut, pDevice->_dspFrames, bytesToRead);
    pDevice->_dspFrameCount -= framesToRead;
    pDevice->_dspFrames += bytesToRead;

    return framesToRead;
}

// A helper function for reading sample data from the client. Returns the number of samples read from the client. Remaining samples
// are filled with silence.
static inline mal_uint32 mal_device__read_frames_from_client(mal_device* pDevice, mal_uint32 frameCount, void* pSamples)
{
    mal_assert(pDevice != NULL);
    mal_assert(frameCount > 0);
    mal_assert(pSamples != NULL);

    mal_uint32 framesRead = mal_dsp_read_frames(&pDevice->dsp, frameCount, pSamples);
    mal_uint32 samplesRead = framesRead * pDevice->internalChannels;
    mal_uint32 sampleSize = mal_get_sample_size_in_bytes(pDevice->internalFormat);
    mal_uint32 consumedBytes = samplesRead*sampleSize;
    mal_uint32 remainingBytes = ((frameCount * pDevice->internalChannels) - samplesRead)*sampleSize;
    mal_zero_memory((mal_uint8*)pSamples + consumedBytes, remainingBytes);

    return samplesRead;
}

// A helper for sending sample data to the client.
static inline void mal_device__send_frames_to_client(mal_device* pDevice, mal_uint32 frameCount, const void* pSamples)
{
    mal_assert(pDevice != NULL);
    mal_assert(frameCount > 0);
    mal_assert(pSamples != NULL);

    mal_recv_proc onRecv = pDevice->onRecv;
    if (onRecv) {
        pDevice->_dspFrameCount = frameCount;
        pDevice->_dspFrames = (const mal_uint8*)pSamples;

        mal_uint8 chunkBuffer[4096];
        mal_uint32 chunkFrameCount = sizeof(chunkBuffer) / mal_get_sample_size_in_bytes(pDevice->format) / pDevice->channels;

        for (;;) {
            mal_uint32 framesJustRead = mal_dsp_read_frames(&pDevice->dsp, chunkFrameCount, chunkBuffer);
            if (framesJustRead == 0) {
                break;
            }

            onRecv(pDevice, framesJustRead, chunkBuffer);
        }
    }
}

// A helper for changing the state of the device.
static inline void mal_device__set_state(mal_device* pDevice, mal_uint32 newState)
{
    mal_atomic_exchange_32(&pDevice->state, newState);
}

// A helper for getting the state of the device.
static inline mal_uint32 mal_device__get_state(mal_device* pDevice)
{
    return pDevice->state;
}


#ifdef MAL_WIN32
static GUID MAL_GUID_KSDATAFORMAT_SUBTYPE_PCM        = {0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static GUID MAL_GUID_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = {0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
//static GUID MAL_GUID_KSDATAFORMAT_SUBTYPE_ALAW       = {0x00000006, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
//static GUID MAL_GUID_KSDATAFORMAT_SUBTYPE_MULAW      = {0x00000007, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
#endif



///////////////////////////////////////////////////////////////////////////////
//
// Null Backend
//
///////////////////////////////////////////////////////////////////////////////
#ifdef MAL_ENABLE_NULL
mal_result mal_context_init__null(mal_context* pContext)
{
    mal_assert(pContext != NULL);

    // The null backend always works.
    (void)pContext;
    return MAL_SUCCESS;
}

mal_result mal_context_uninit__null(mal_context* pContext)
{
    mal_assert(pContext != NULL);
    mal_assert(pContext->backend == mal_backend_null);

    (void)pContext;
    return MAL_SUCCESS;
}

static mal_result mal_enumerate_devices__null(mal_context* pContext, mal_device_type type, mal_uint32* pCount, mal_device_info* pInfo)
{
    (void)pContext;

    mal_uint32 infoSize = *pCount;
    *pCount = 1;    // There's only one "device" each for playback and recording for the null backend.

    if (pInfo != NULL && infoSize > 0) {
        mal_zero_object(pInfo);

        if (type == mal_device_type_playback) {
            mal_strncpy_s(pInfo->name, sizeof(pInfo->name), "NULL Playback Device", (size_t)-1);
        } else {
            mal_strncpy_s(pInfo->name, sizeof(pInfo->name), "NULL Capture Device", (size_t)-1);
        }
    }

    return MAL_SUCCESS;
}

static void mal_device_uninit__null(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);
    mal_free(pDevice->null_device.pBuffer);
}

static mal_result mal_device_init__null(mal_context* pContext, mal_device_type type, mal_device_id* pDeviceID, mal_device_config* pConfig, mal_device* pDevice)
{
    (void)pContext;
    (void)type;
    (void)pDeviceID;

    mal_assert(pDevice != NULL);
    mal_zero_object(&pDevice->null_device);

    pDevice->bufferSizeInFrames = pConfig->bufferSizeInFrames;
    pDevice->periods = pConfig->periods;

    pDevice->null_device.pBuffer = (mal_uint8*)mal_malloc(pDevice->bufferSizeInFrames * pDevice->channels * mal_get_sample_size_in_bytes(pDevice->format));
    if (pDevice->null_device.pBuffer == NULL) {
        return MAL_OUT_OF_MEMORY;
    }

    mal_zero_memory(pDevice->null_device.pBuffer, mal_device_get_buffer_size_in_bytes(pDevice));

    return MAL_SUCCESS;
}

static mal_result mal_device__start_backend__null(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    mal_timer_init(&pDevice->null_device.timer);
    pDevice->null_device.lastProcessedFrame = 0;

    return MAL_SUCCESS;
}

static mal_result mal_device__stop_backend__null(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);
    (void)pDevice;

    return MAL_SUCCESS;
}

static mal_result mal_device__break_main_loop__null(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    pDevice->null_device.breakFromMainLoop = MAL_TRUE;
    return MAL_SUCCESS;
}

static mal_bool32 mal_device__get_current_frame__null(mal_device* pDevice, mal_uint32* pCurrentPos)
{
    mal_assert(pDevice != NULL);
    mal_assert(pCurrentPos != NULL);
    *pCurrentPos = 0;

    mal_uint64 currentFrameAbs = (mal_uint64)(mal_timer_get_time_in_seconds(&pDevice->null_device.timer) * pDevice->sampleRate) / pDevice->channels;

    *pCurrentPos = currentFrameAbs % pDevice->bufferSizeInFrames;
    return MAL_TRUE;
}

static mal_uint32 mal_device__get_available_frames__null(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    mal_uint32 currentFrame;
    if (!mal_device__get_current_frame__null(pDevice, &currentFrame)) {
        return 0;
    }

    // In a playback device the last processed frame should always be ahead of the current frame. The space between
    // the last processed and current frame (moving forward, starting from the last processed frame) is the amount
    // of space available to write.
    //
    // For a recording device it's the other way around - the last processed frame is always _behind_ the current
    // frame and the space between is the available space.
    mal_uint32 totalFrameCount = pDevice->bufferSizeInFrames;
    if (pDevice->type == mal_device_type_playback) {
        mal_uint32 committedBeg = currentFrame;
        mal_uint32 committedEnd = pDevice->null_device.lastProcessedFrame;
        if (committedEnd <= committedBeg) {
            committedEnd += totalFrameCount;    // Wrap around.
        }

        mal_uint32 committedSize = (committedEnd - committedBeg);
        mal_assert(committedSize <= totalFrameCount);

        return totalFrameCount - committedSize;
    } else {
        mal_uint32 validBeg = pDevice->null_device.lastProcessedFrame;
        mal_uint32 validEnd = currentFrame;
        if (validEnd < validBeg) {
            validEnd += totalFrameCount;        // Wrap around.
        }

        mal_uint32 validSize = (validEnd - validBeg);
        mal_assert(validSize <= totalFrameCount);

        return validSize;
    }
}

static mal_uint32 mal_device__wait_for_frames__null(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    while (!pDevice->null_device.breakFromMainLoop) {
        mal_uint32 framesAvailable = mal_device__get_available_frames__null(pDevice);
        if (framesAvailable > 0) {
            return framesAvailable;
        }

        mal_sleep(16);
    }

    // We'll get here if the loop was terminated. Just return whatever's available.
    return mal_device__get_available_frames__null(pDevice);
}

static mal_result mal_device__main_loop__null(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    pDevice->null_device.breakFromMainLoop = MAL_FALSE;
    while (!pDevice->null_device.breakFromMainLoop) {
        mal_uint32 framesAvailable = mal_device__wait_for_frames__null(pDevice);
        if (framesAvailable == 0) {
            continue;
        }

        // If it's a playback device, don't bother grabbing more data if the device is being stopped.
        if (pDevice->null_device.breakFromMainLoop && pDevice->type == mal_device_type_playback) {
            return MAL_FALSE;
        }

        if (framesAvailable + pDevice->null_device.lastProcessedFrame > pDevice->bufferSizeInFrames) {
            framesAvailable = pDevice->bufferSizeInFrames - pDevice->null_device.lastProcessedFrame;
        }

        mal_uint32 sampleCount = framesAvailable * pDevice->channels;
        mal_uint32 lockOffset  = pDevice->null_device.lastProcessedFrame * pDevice->channels * mal_get_sample_size_in_bytes(pDevice->format);
        mal_uint32 lockSize    = sampleCount * mal_get_sample_size_in_bytes(pDevice->format);

        if (pDevice->type == mal_device_type_playback) {
            if (pDevice->null_device.breakFromMainLoop) {
                return MAL_FALSE;
            }

            mal_device__read_frames_from_client(pDevice, framesAvailable, pDevice->null_device.pBuffer + lockOffset);
        } else {
            mal_zero_memory(pDevice->null_device.pBuffer + lockOffset, lockSize);
            mal_device__send_frames_to_client(pDevice, framesAvailable, pDevice->null_device.pBuffer + lockOffset);
        }

        pDevice->null_device.lastProcessedFrame = (pDevice->null_device.lastProcessedFrame + framesAvailable) % pDevice->bufferSizeInFrames;
    }

    return MAL_SUCCESS;
}
#endif


///////////////////////////////////////////////////////////////////////////////
//
// WIN32 COMMON
//
///////////////////////////////////////////////////////////////////////////////
#if defined(MAL_ENABLE_WASAPI) || defined(MAL_ENABLE_DSOUND)
#include <mmreg.h>

// Converts an individual Win32-style channel identifier (SPEAKER_FRONT_LEFT, etc.) to mini_al.
static mal_uint8 mal_channel_id_to_mal__win32(DWORD id)
{
    switch (id)
    {
        case SPEAKER_FRONT_LEFT:            return MAL_CHANNEL_FRONT_LEFT;
        case SPEAKER_FRONT_RIGHT:           return MAL_CHANNEL_FRONT_RIGHT;
        case SPEAKER_FRONT_CENTER:          return MAL_CHANNEL_FRONT_CENTER;
        case SPEAKER_LOW_FREQUENCY:         return MAL_CHANNEL_LFE;
        case SPEAKER_BACK_LEFT:             return MAL_CHANNEL_BACK_LEFT;
        case SPEAKER_BACK_RIGHT:            return MAL_CHANNEL_BACK_RIGHT;
        case SPEAKER_FRONT_LEFT_OF_CENTER:  return MAL_CHANNEL_FRONT_LEFT_CENTER;
        case SPEAKER_FRONT_RIGHT_OF_CENTER: return MAL_CHANNEL_FRONT_RIGHT_CENTER;
        case SPEAKER_BACK_CENTER:           return MAL_CHANNEL_BACK_CENTER;
        case SPEAKER_SIDE_LEFT:             return MAL_CHANNEL_SIDE_LEFT;
        case SPEAKER_SIDE_RIGHT:            return MAL_CHANNEL_SIDE_RIGHT;
        case SPEAKER_TOP_CENTER:            return MAL_CHANNEL_TOP_CENTER;
        case SPEAKER_TOP_FRONT_LEFT:        return MAL_CHANNEL_TOP_FRONT_LEFT;
        case SPEAKER_TOP_FRONT_CENTER:      return MAL_CHANNEL_TOP_FRONT_CENTER;
        case SPEAKER_TOP_FRONT_RIGHT:       return MAL_CHANNEL_TOP_FRONT_RIGHT;
        case SPEAKER_TOP_BACK_LEFT:         return MAL_CHANNEL_TOP_BACK_LEFT;
        case SPEAKER_TOP_BACK_CENTER:       return MAL_CHANNEL_TOP_BACK_CENTER;
        case SPEAKER_TOP_BACK_RIGHT:        return MAL_CHANNEL_TOP_BACK_RIGHT;
        default: return 0;
    }
}

// Converts an individual mini_al channel identifier (MAL_CHANNEL_FRONT_LEFT, etc.) to Win32-style.
static DWORD mal_channel_id_to_win32(DWORD id)
{
    switch (id)
    {
        case MAL_CHANNEL_FRONT_LEFT:         return SPEAKER_FRONT_LEFT;           
        case MAL_CHANNEL_FRONT_RIGHT:        return SPEAKER_FRONT_RIGHT;          
        case MAL_CHANNEL_FRONT_CENTER:       return SPEAKER_FRONT_CENTER;         
        case MAL_CHANNEL_LFE:                return SPEAKER_LOW_FREQUENCY;        
        case MAL_CHANNEL_BACK_LEFT:          return SPEAKER_BACK_LEFT;            
        case MAL_CHANNEL_BACK_RIGHT:         return SPEAKER_BACK_RIGHT;           
        case MAL_CHANNEL_FRONT_LEFT_CENTER:  return SPEAKER_FRONT_LEFT_OF_CENTER; 
        case MAL_CHANNEL_FRONT_RIGHT_CENTER: return SPEAKER_FRONT_RIGHT_OF_CENTER;
        case MAL_CHANNEL_BACK_CENTER:        return SPEAKER_BACK_CENTER;          
        case MAL_CHANNEL_SIDE_LEFT:          return SPEAKER_SIDE_LEFT;            
        case MAL_CHANNEL_SIDE_RIGHT:         return SPEAKER_SIDE_RIGHT;           
        case MAL_CHANNEL_TOP_CENTER:         return SPEAKER_TOP_CENTER;           
        case MAL_CHANNEL_TOP_FRONT_LEFT:     return SPEAKER_TOP_FRONT_LEFT;       
        case MAL_CHANNEL_TOP_FRONT_CENTER:   return SPEAKER_TOP_FRONT_CENTER;     
        case MAL_CHANNEL_TOP_FRONT_RIGHT:    return SPEAKER_TOP_FRONT_RIGHT;      
        case MAL_CHANNEL_TOP_BACK_LEFT:      return SPEAKER_TOP_BACK_LEFT;        
        case MAL_CHANNEL_TOP_BACK_CENTER:    return SPEAKER_TOP_BACK_CENTER;      
        case MAL_CHANNEL_TOP_BACK_RIGHT:     return SPEAKER_TOP_BACK_RIGHT;       
        default: return 0;
    }
}

// Converts a channel mapping to a Win32-style channel mask.
static DWORD mal_channel_map_to_channel_mask__win32(mal_uint8 channelMap[MAL_MAX_CHANNELS], mal_uint32 channels)
{
    DWORD dwChannelMask = 0;
    for (mal_uint32 iChannel = 0; iChannel < channels; ++iChannel) {
        dwChannelMask |= mal_channel_id_to_win32(channelMap[iChannel]);
    }

    return dwChannelMask;
}

// Converts a Win32-style channel mask to a mini_al channel map.
static void mal_channel_mask_to_channel_map__win32(DWORD dwChannelMask, mal_uint32 channels, mal_uint8 channelMap[MAL_MAX_CHANNELS])
{
    if (channels == 1 && dwChannelMask == 0) {
        channelMap[0] = MAL_CHANNEL_FRONT_CENTER;
    } else if (channels == 2 && dwChannelMask == 0) {
        channelMap[0] = MAL_CHANNEL_FRONT_LEFT;
        channelMap[1] = MAL_CHANNEL_FRONT_RIGHT;
    } else {
        // Just iterate over each bit.
        mal_uint32 iChannel = 0;
        for (mal_uint32 iBit = 0; iBit < 32; ++iBit) {
            DWORD bitValue = (dwChannelMask & (1 << iBit));
            if (bitValue != 0) {
                // The bit is set.
                channelMap[iChannel] = mal_channel_id_to_mal__win32(bitValue);
                iChannel += 1;
            }
        }
    }
}
#endif


///////////////////////////////////////////////////////////////////////////////
//
// WASAPI Backend
//
///////////////////////////////////////////////////////////////////////////////
#ifdef MAL_ENABLE_WASAPI
#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable:4091)   // 'typedef ': ignored on left of '' when no variable is declared
#endif
#include <audioclient.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
//#include <functiondiscoverykeys_devpkey.h>
#if defined(_MSC_VER)
    #pragma warning(pop)
#endif

const PROPERTYKEY g_malPKEY_Device_FriendlyName = {{0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 14};

const IID g_malCLSID_MMDeviceEnumerator_Instance = {0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}}; // BCDE0395-E52F-467C-8E3D-C4579291692E = __uuidof(MMDeviceEnumerator)
const IID g_malIID_IMMDeviceEnumerator_Instance  = {0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}}; // A95664D2-9614-4F35-A746-DE8DB63617E6 = __uuidof(IMMDeviceEnumerator)
const IID g_malIID_IAudioClient_Instance         = {0x1CB9AD4C, 0xDBFA, 0x4C32, {0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2}}; // 1CB9AD4C-DBFA-4C32-B178-C2F568A703B2 = __uuidof(IAudioClient)
const IID g_malIID_IAudioRenderClient_Instance   = {0xF294ACFC, 0x3146, 0x4483, {0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2}}; // F294ACFC-3146-4483-A7BF-ADDCA7C260E2 = __uuidof(IAudioRenderClient)
const IID g_malIID_IAudioCaptureClient_Instance  = {0xC8ADBD64, 0xE71E, 0x48A0, {0xA4, 0xDE, 0x18, 0x5C, 0x39, 0x5C, 0xD3, 0x17}}; // C8ADBD64-E71E-48A0-A4DE-185C395CD317 = __uuidof(IAudioCaptureClient)

#ifdef __cplusplus
#define g_malCLSID_MMDeviceEnumerator g_malCLSID_MMDeviceEnumerator_Instance
#define g_malIID_IMMDeviceEnumerator  g_malIID_IMMDeviceEnumerator_Instance
#define g_malIID_IAudioClient         g_malIID_IAudioClient_Instance
#define g_malIID_IAudioRenderClient   g_malIID_IAudioRenderClient_Instance
#define g_malIID_IAudioCaptureClient  g_malIID_IAudioCaptureClient_Instance
#else
#define g_malCLSID_MMDeviceEnumerator &g_malCLSID_MMDeviceEnumerator_Instance
#define g_malIID_IMMDeviceEnumerator  &g_malIID_IMMDeviceEnumerator_Instance
#define g_malIID_IAudioClient         &g_malIID_IAudioClient_Instance
#define g_malIID_IAudioRenderClient   &g_malIID_IAudioRenderClient_Instance
#define g_malIID_IAudioCaptureClient  &g_malIID_IAudioCaptureClient_Instance
#endif

#ifdef __cplusplus
#define mal_is_guid_equal(a, b) IsEqualGUID(a, b)
#else
#define mal_is_guid_equal(a, b) IsEqualGUID(&a, &b)
#endif

mal_result mal_context_init__wasapi(mal_context* pContext)
{
    mal_assert(pContext != NULL);

    // Validate the WASAPI is available by grabbing an MMDeviceEnumerator object.
    HRESULT hr = ((MAL_PFN_CoCreateInstance)pContext->win32.CoCreateInstance)(g_malCLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, g_malIID_IMMDeviceEnumerator, (void**)&pContext->wasapi.pDeviceEnumerator);
    if (FAILED(hr)) {
        return MAL_NO_BACKEND;
    }

    return MAL_SUCCESS;
}

mal_result mal_context_uninit__wasapi(mal_context* pContext)
{
    mal_assert(pContext != NULL);
    mal_assert(pContext->backend == mal_backend_wasapi);

    if (pContext->wasapi.pDeviceEnumerator) {
#ifdef __cplusplus
        ((IMMDeviceEnumerator*)pContext->wasapi.pDeviceEnumerator)->Release();
#else
        ((IMMDeviceEnumerator*)pContext->wasapi.pDeviceEnumerator)->lpVtbl->Release((IMMDeviceEnumerator*)pContext->wasapi.pDeviceEnumerator);
#endif
    }

    return MAL_SUCCESS;
}

static mal_result mal_enumerate_devices__wasapi(mal_context* pContext, mal_device_type type, mal_uint32* pCount, mal_device_info* pInfo)
{
    mal_uint32 infoSize = *pCount;
    *pCount = 0;

    IMMDeviceEnumerator* pDeviceEnumerator = (IMMDeviceEnumerator*)pContext->wasapi.pDeviceEnumerator;
    mal_assert(pDeviceEnumerator != NULL);

    IMMDeviceCollection* pDeviceCollection;
#ifdef __cplusplus
    HRESULT hr = pDeviceEnumerator->EnumAudioEndpoints((type == mal_device_type_playback) ? eRender : eCapture, DEVICE_STATE_ACTIVE, &pDeviceCollection);
#else
    HRESULT hr = pDeviceEnumerator->lpVtbl->EnumAudioEndpoints(pDeviceEnumerator, (type == mal_device_type_playback) ? eRender : eCapture, DEVICE_STATE_ACTIVE, &pDeviceCollection);
#endif
    if (FAILED(hr)) {
        return MAL_NO_DEVICE;
    }

    UINT count;
#ifdef __cplusplus
    hr = pDeviceCollection->GetCount(&count);
#else
    hr = pDeviceCollection->lpVtbl->GetCount(pDeviceCollection, &count);
#endif
    if (FAILED(hr)) {
#ifdef __cplusplus
        pDeviceCollection->Release();
#else
        pDeviceCollection->lpVtbl->Release(pDeviceCollection);
#endif
        return MAL_NO_DEVICE;
    }

    for (mal_uint32 iDevice = 0; iDevice < infoSize && iDevice < count; ++iDevice) {
        mal_zero_object(pInfo);

        IMMDevice* pDevice;
#ifdef __cplusplus
        hr = pDeviceCollection->Item(iDevice, &pDevice);
#else
        hr = pDeviceCollection->lpVtbl->Item(pDeviceCollection, iDevice, &pDevice);
#endif
        if (SUCCEEDED(hr)) {
            // ID.
            LPWSTR id;
#ifdef __cplusplus
            hr = pDevice->GetId(&id);
#else
            hr = pDevice->lpVtbl->GetId(pDevice, &id);
#endif
            if (SUCCEEDED(hr)) {
                size_t idlen = wcslen(id);
                if (idlen+sizeof(wchar_t) > sizeof(pInfo->id.wasapi)) {
                    ((MAL_PFN_CoTaskMemFree)pContext->win32.CoTaskMemFree)(id);
                    mal_assert(MAL_FALSE);  // NOTE: If this is triggered, please report it. It means the format of the ID must haved change and is too long to fit in our fixed sized buffer.
                    continue;
                }

                memcpy(pInfo->id.wasapi, id, idlen * sizeof(wchar_t));
                pInfo->id.wasapi[idlen] = '\0';

                ((MAL_PFN_CoTaskMemFree)pContext->win32.CoTaskMemFree)(id);
            }

            // Description / Friendly Name.
            IPropertyStore *pProperties;
#ifdef __cplusplus
            hr = pDevice->OpenPropertyStore(STGM_READ, &pProperties);
#else
            hr = pDevice->lpVtbl->OpenPropertyStore(pDevice, STGM_READ, &pProperties);
#endif
            if (SUCCEEDED(hr)) {
                PROPVARIANT varName;
                PropVariantInit(&varName);
#ifdef __cplusplus
                hr = pProperties->GetValue(g_malPKEY_Device_FriendlyName, &varName);
#else
                hr = pProperties->lpVtbl->GetValue(pProperties, &g_malPKEY_Device_FriendlyName, &varName);
#endif
                if (SUCCEEDED(hr)) {
                    WideCharToMultiByte(CP_UTF8, 0, varName.pwszVal, -1, pInfo->name, sizeof(pInfo->name), 0, FALSE);
                    ((MAL_PFN_PropVariantClear)pContext->win32.PropVariantClear)(&varName);
                }

#ifdef __cplusplus
                pProperties->Release();
#else
                pProperties->lpVtbl->Release(pProperties);
#endif
            }
        }

        pInfo += 1;
        *pCount += 1;
    }

#ifdef __cplusplus
    pDeviceCollection->Release();
#else
    pDeviceCollection->lpVtbl->Release(pDeviceCollection);
#endif
    return MAL_SUCCESS;
}

static void mal_device_uninit__wasapi(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    if (pDevice->wasapi.pRenderClient) {
#ifdef __cplusplus
        ((IAudioRenderClient*)pDevice->wasapi.pRenderClient)->Release();
#else
        ((IAudioRenderClient*)pDevice->wasapi.pRenderClient)->lpVtbl->Release((IAudioRenderClient*)pDevice->wasapi.pRenderClient);
#endif
    }
    if (pDevice->wasapi.pCaptureClient) {
#ifdef __cplusplus
        ((IAudioCaptureClient*)pDevice->wasapi.pCaptureClient)->Release();
#else
        ((IAudioCaptureClient*)pDevice->wasapi.pCaptureClient)->lpVtbl->Release((IAudioCaptureClient*)pDevice->wasapi.pCaptureClient);
#endif
    }

    if (pDevice->wasapi.pAudioClient) {
#ifdef __cplusplus
        ((IAudioClient*)pDevice->wasapi.pAudioClient)->Release();
#else
        ((IAudioClient*)pDevice->wasapi.pAudioClient)->lpVtbl->Release((IAudioClient*)pDevice->wasapi.pAudioClient);
#endif
    }

    if (pDevice->wasapi.pDevice) {
#ifdef __cplusplus
        ((IMMDevice*)pDevice->wasapi.pDevice)->Release();
#else
        ((IMMDevice*)pDevice->wasapi.pDevice)->lpVtbl->Release((IMMDevice*)pDevice->wasapi.pDevice);
#endif
    }

    if (pDevice->wasapi.hStopEvent) {
        CloseHandle(pDevice->wasapi.hStopEvent);
    }
}

static mal_result mal_device__find_best_format__wasapi(mal_device* pDevice, WAVEFORMATEXTENSIBLE* pBestFormat)
{
    mal_assert(pDevice != NULL);
    mal_assert(pBestFormat != NULL);

    WAVEFORMATEXTENSIBLE wf;
    mal_zero_object(&wf);
    wf.Format.cbSize               = sizeof(wf);
    wf.Format.wFormatTag           = WAVE_FORMAT_EXTENSIBLE;
    wf.Format.nChannels            = (WORD)pDevice->channels;
    wf.Format.nSamplesPerSec       = (DWORD)pDevice->sampleRate;
    wf.Format.wBitsPerSample       = (WORD)mal_get_sample_size_in_bytes(pDevice->format)*8;
    wf.Format.nBlockAlign          = (wf.Format.nChannels * wf.Format.wBitsPerSample) / 8;
    wf.Format.nAvgBytesPerSec      = wf.Format.nBlockAlign * wf.Format.nSamplesPerSec;
    wf.Samples.wValidBitsPerSample = wf.Format.wBitsPerSample;
    wf.dwChannelMask               = mal_channel_map_to_channel_mask__win32(pDevice->channelMap, pDevice->channels);
    if (pDevice->format == mal_format_f32) {
        wf.SubFormat = MAL_GUID_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    } else {
        wf.SubFormat = MAL_GUID_KSDATAFORMAT_SUBTYPE_PCM;
    }

    HRESULT hr = AUDCLNT_E_UNSUPPORTED_FORMAT;
    WAVEFORMATEXTENSIBLE* pBestFormatTemp;
#ifdef __cplusplus
    hr = ((IAudioClient*)pDevice->wasapi.pAudioClient)->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, (WAVEFORMATEX*)&wf, (WAVEFORMATEX**)&pBestFormatTemp);
#else
    hr = ((IAudioClient*)pDevice->wasapi.pAudioClient)->lpVtbl->IsFormatSupported((IAudioClient*)pDevice->wasapi.pAudioClient, AUDCLNT_SHAREMODE_SHARED, (WAVEFORMATEX*)&wf, (WAVEFORMATEX**)&pBestFormatTemp);
#endif
    if (hr != S_OK && hr != S_FALSE) {
    #ifdef __cplusplus
        hr = ((IAudioClient*)pDevice->wasapi.pAudioClient)->GetMixFormat((WAVEFORMATEX**)&pBestFormatTemp);
    #else
        hr = ((IAudioClient*)pDevice->wasapi.pAudioClient)->lpVtbl->GetMixFormat((IAudioClient*)pDevice->wasapi.pAudioClient, (WAVEFORMATEX**)&pBestFormatTemp);
    #endif
        if (hr != S_OK) {
            return MAL_WASAPI_FAILED_TO_FIND_BEST_FORMAT;
        }
    }

    if (pBestFormatTemp != NULL) {
        mal_copy_memory(pBestFormat, pBestFormatTemp, sizeof(*pBestFormat));
        ((MAL_PFN_CoTaskMemFree)pDevice->pContext->win32.CoTaskMemFree)(pBestFormatTemp);
    } else {
        mal_copy_memory(pBestFormat, &wf, sizeof(*pBestFormat));
    }

    return MAL_SUCCESS;
}

static mal_result mal_device_init__wasapi(mal_context* pContext, mal_device_type type, mal_device_id* pDeviceID, mal_device_config* pConfig, mal_device* pDevice)
{
    (void)pContext;

    mal_assert(pDevice != NULL);
    mal_zero_object(&pDevice->wasapi);

    IMMDeviceEnumerator* pDeviceEnumerator;
    HRESULT hr = ((MAL_PFN_CoCreateInstance)pContext->win32.CoCreateInstance)(g_malCLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, g_malIID_IMMDeviceEnumerator, (void**)&pDeviceEnumerator);
    if (FAILED(hr)) {
        mal_device_uninit__wasapi(pDevice);
        return mal_post_error(pDevice, "[WASAPI] Failed to create IMMDeviceEnumerator.", MAL_WASAPI_FAILED_TO_CREATE_DEVICE_ENUMERATOR);
    }

    if (pDeviceID == NULL) {
#ifdef __cplusplus
        hr = pDeviceEnumerator->GetDefaultAudioEndpoint((type == mal_device_type_playback) ? eRender : eCapture, eConsole, (IMMDevice**)&pDevice->wasapi.pDevice);
#else
        hr = pDeviceEnumerator->lpVtbl->GetDefaultAudioEndpoint(pDeviceEnumerator, (type == mal_device_type_playback) ? eRender : eCapture, eConsole, (IMMDevice**)&pDevice->wasapi.pDevice);
#endif
        if (FAILED(hr)) {
#ifdef __cplusplus
            pDeviceEnumerator->Release();
#else
            pDeviceEnumerator->lpVtbl->Release(pDeviceEnumerator);
#endif
            mal_device_uninit__wasapi(pDevice);
            return mal_post_error(pDevice, "[WASAPI] Failed to create default backend device.", MAL_WASAPI_FAILED_TO_CREATE_DEVICE);
        }
    } else {
#ifdef __cplusplus
        hr = pDeviceEnumerator->GetDevice(pDeviceID->wasapi, (IMMDevice**)&pDevice->wasapi.pDevice);
#else
        hr = pDeviceEnumerator->lpVtbl->GetDevice(pDeviceEnumerator, pDeviceID->wasapi, (IMMDevice**)&pDevice->wasapi.pDevice);
#endif
        if (FAILED(hr)) {
#ifdef __cplusplus
            pDeviceEnumerator->Release();
#else
            pDeviceEnumerator->lpVtbl->Release(pDeviceEnumerator);
#endif
            mal_device_uninit__wasapi(pDevice);
            return mal_post_error(pDevice, "[WASAPI] Failed to create backend device.", MAL_WASAPI_FAILED_TO_CREATE_DEVICE);
        }
    }

#ifdef __cplusplus
    pDeviceEnumerator->Release();
#else
    pDeviceEnumerator->lpVtbl->Release(pDeviceEnumerator);
#endif

#ifdef __cplusplus
    hr = ((IMMDevice*)pDevice->wasapi.pDevice)->Activate(g_malIID_IAudioClient, CLSCTX_ALL, NULL, &pDevice->wasapi.pAudioClient);
#else
    hr = ((IMMDevice*)pDevice->wasapi.pDevice)->lpVtbl->Activate((IMMDevice*)pDevice->wasapi.pDevice, g_malIID_IAudioClient, CLSCTX_ALL, NULL, &pDevice->wasapi.pAudioClient);
#endif
    if (FAILED(hr)) {
        mal_device_uninit__wasapi(pDevice);
        return mal_post_error(pDevice, "[WASAPI] Failed to activate device.", MAL_WASAPI_FAILED_TO_ACTIVATE_DEVICE);
    }

    REFERENCE_TIME bufferDurationInMicroseconds = ((mal_uint64)pConfig->bufferSizeInFrames * 1000 * 1000) / pConfig->sampleRate;

    WAVEFORMATEXTENSIBLE wf;
    mal_result result = mal_device__find_best_format__wasapi(pDevice, &wf);
    if (result != MAL_SUCCESS) {
        mal_device_uninit__wasapi(pDevice);
        return mal_post_error(pDevice, "[WASAPI] Failed to find best device mix format.", MAL_WASAPI_FAILED_TO_ACTIVATE_DEVICE);
    }

    if (mal_is_guid_equal(wf.SubFormat, MAL_GUID_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
        pDevice->internalFormat = mal_format_f32;
    } else {
        if (wf.Format.wBitsPerSample == 32) {
            pDevice->internalFormat = mal_format_s32;
        } else if (wf.Format.wBitsPerSample == 24) {
            pDevice->internalFormat = mal_format_s24;
        } else if (wf.Format.wBitsPerSample == 16) {
            pDevice->internalFormat = mal_format_s16;
        } else if (wf.Format.wBitsPerSample == 8) {
            pDevice->internalFormat = mal_format_u8;
        } else {
            mal_device_uninit__wasapi(pDevice);
            return mal_post_error(pDevice, "[WASAPI] Device's native format is not supported.", MAL_FORMAT_NOT_SUPPORTED);
        }
    }

    pDevice->internalChannels = wf.Format.nChannels;
    pDevice->internalSampleRate = wf.Format.nSamplesPerSec;

    // Get the internal channel map based on the channel mask.
    mal_channel_mask_to_channel_map__win32(wf.dwChannelMask, pDevice->internalChannels, pDevice->internalChannelMap);

#ifdef __cplusplus
    hr = ((IAudioClient*)pDevice->wasapi.pAudioClient)->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, bufferDurationInMicroseconds*10, 0, (WAVEFORMATEX*)&wf, NULL);
#else
    hr = ((IAudioClient*)pDevice->wasapi.pAudioClient)->lpVtbl->Initialize((IAudioClient*)pDevice->wasapi.pAudioClient, AUDCLNT_SHAREMODE_SHARED, 0, bufferDurationInMicroseconds*10, 0, (WAVEFORMATEX*)&wf, NULL);
#endif
    if (FAILED(hr)) {
        mal_device_uninit__wasapi(pDevice);
        return mal_post_error(pDevice, "[WASAPI] Failed to initialize device.", MAL_WASAPI_FAILED_TO_INITIALIZE_DEVICE);
    }

#ifdef __cplusplus
    hr = ((IAudioClient*)pDevice->wasapi.pAudioClient)->GetBufferSize(&pDevice->bufferSizeInFrames);
#else
    hr = ((IAudioClient*)pDevice->wasapi.pAudioClient)->lpVtbl->GetBufferSize((IAudioClient*)pDevice->wasapi.pAudioClient, &pDevice->bufferSizeInFrames);
#endif
    if (FAILED(hr)) {
        mal_device_uninit__wasapi(pDevice);
        return mal_post_error(pDevice, "[WASAPI] Failed to get audio client's actual buffer size.", MAL_WASAPI_FAILED_TO_INITIALIZE_DEVICE);
    }

#ifdef __cplusplus
    if (type == mal_device_type_playback) {
        hr = ((IAudioClient*)pDevice->wasapi.pAudioClient)->GetService(g_malIID_IAudioRenderClient, &pDevice->wasapi.pRenderClient);
    } else {
        hr = ((IAudioClient*)pDevice->wasapi.pAudioClient)->GetService(g_malIID_IAudioCaptureClient, &pDevice->wasapi.pCaptureClient);
    }
#else
    if (type == mal_device_type_playback) {
        hr = ((IAudioClient*)pDevice->wasapi.pAudioClient)->lpVtbl->GetService((IAudioClient*)pDevice->wasapi.pAudioClient, g_malIID_IAudioRenderClient, &pDevice->wasapi.pRenderClient);
    } else {
        hr = ((IAudioClient*)pDevice->wasapi.pAudioClient)->lpVtbl->GetService((IAudioClient*)pDevice->wasapi.pAudioClient, g_malIID_IAudioCaptureClient, &pDevice->wasapi.pCaptureClient);
    }
#endif

    if (FAILED(hr)) {
        mal_device_uninit__wasapi(pDevice);
        return mal_post_error(pDevice, "[WASAPI] Failed to get audio client service.", MAL_WASAPI_FAILED_TO_INITIALIZE_DEVICE);
    }


    // When the device is playing the worker thread will be waiting on a bunch of notification events. To return from
    // this wait state we need to signal a special event.
    pDevice->wasapi.hStopEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (pDevice->wasapi.hStopEvent == NULL) {
        mal_device_uninit__wasapi(pDevice);
        return mal_post_error(pDevice, "[WASAPI] Failed to create event for main loop break notification.", MAL_FAILED_TO_CREATE_EVENT);
    }
    
    return MAL_SUCCESS;
}

static mal_result mal_device__start_backend__wasapi(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    // Playback devices need to have an initial chunk of data loaded.
    if (pDevice->type == mal_device_type_playback) {
        BYTE* pData;
#ifdef __cplusplus
        HRESULT hr = ((IAudioRenderClient*)pDevice->wasapi.pRenderClient)->GetBuffer(pDevice->bufferSizeInFrames, &pData);
#else
        HRESULT hr = ((IAudioRenderClient*)pDevice->wasapi.pRenderClient)->lpVtbl->GetBuffer((IAudioRenderClient*)pDevice->wasapi.pRenderClient, pDevice->bufferSizeInFrames, &pData);
#endif
        if (FAILED(hr)) {
            return MAL_FAILED_TO_READ_DATA_FROM_CLIENT;
        }

        mal_device__read_frames_from_client(pDevice, pDevice->bufferSizeInFrames, pData);

#ifdef __cplusplus
        hr = ((IAudioRenderClient*)pDevice->wasapi.pRenderClient)->ReleaseBuffer(pDevice->bufferSizeInFrames, 0);
#else
        hr = ((IAudioRenderClient*)pDevice->wasapi.pRenderClient)->lpVtbl->ReleaseBuffer((IAudioRenderClient*)pDevice->wasapi.pRenderClient, pDevice->bufferSizeInFrames, 0);
#endif
        if (FAILED(hr)) {
            return MAL_FAILED_TO_READ_DATA_FROM_CLIENT;
        }
    }

#ifdef __cplusplus
    HRESULT hr = ((IAudioClient*)pDevice->wasapi.pAudioClient)->Start();
#else
    HRESULT hr = ((IAudioClient*)pDevice->wasapi.pAudioClient)->lpVtbl->Start((IAudioClient*)pDevice->wasapi.pAudioClient);
#endif
    if (FAILED(hr)) {
        return MAL_FAILED_TO_START_BACKEND_DEVICE;
    }

    return MAL_SUCCESS;
}

static mal_result mal_device__stop_backend__wasapi(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);
#ifdef __cplusplus
    HRESULT hr = ((IAudioClient*)pDevice->wasapi.pAudioClient)->Stop();
#else
    HRESULT hr = ((IAudioClient*)pDevice->wasapi.pAudioClient)->lpVtbl->Stop((IAudioClient*)pDevice->wasapi.pAudioClient);
#endif
    if (FAILED(hr)) {
        return MAL_FAILED_TO_STOP_BACKEND_DEVICE;
    }

    return MAL_SUCCESS;
}

static mal_result mal_device__break_main_loop__wasapi(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    // The main loop will be waiting on a bunch of events via the WaitForMultipleObjects() API. One of those events
    // is a special event we use for forcing that function to return.
    pDevice->wasapi.breakFromMainLoop = MAL_TRUE;
    SetEvent(pDevice->wasapi.hStopEvent);
    return MAL_SUCCESS;
}

static mal_uint32 mal_device__get_available_frames__wasapi(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    if (pDevice->type == mal_device_type_playback) {
        UINT32 paddingFramesCount;
#ifdef __cplusplus
        HRESULT hr = ((IAudioClient*)pDevice->wasapi.pAudioClient)->GetCurrentPadding(&paddingFramesCount);
#else
        HRESULT hr = ((IAudioClient*)pDevice->wasapi.pAudioClient)->lpVtbl->GetCurrentPadding((IAudioClient*)pDevice->wasapi.pAudioClient, &paddingFramesCount);
#endif
        if (FAILED(hr)) {
            return 0;
        }

        return pDevice->bufferSizeInFrames - paddingFramesCount;
    } else {
        UINT32 framesAvailable;
#ifdef __cplusplus
        HRESULT hr = ((IAudioCaptureClient*)pDevice->wasapi.pCaptureClient)->GetNextPacketSize(&framesAvailable);
#else
        HRESULT hr = ((IAudioCaptureClient*)pDevice->wasapi.pCaptureClient)->lpVtbl->GetNextPacketSize((IAudioCaptureClient*)pDevice->wasapi.pCaptureClient, &framesAvailable);
#endif
        if (FAILED(hr)) {
            return 0;
        }

        return framesAvailable;
    }
}

static mal_uint32 mal_device__wait_for_frames__wasapi(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    while (!pDevice->wasapi.breakFromMainLoop) {
        mal_uint32 framesAvailable = mal_device__get_available_frames__wasapi(pDevice);
        if (framesAvailable > 0) {
            return framesAvailable;
        }

        DWORD timeoutInMilliseconds = 1;
        WaitForSingleObject(pDevice->wasapi.hStopEvent, timeoutInMilliseconds);
    }

    // We'll get here if the loop was terminated. Just return whatever's available.
    return mal_device__get_available_frames__wasapi(pDevice);
}

static mal_result mal_device__main_loop__wasapi(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    // Make sure the stop event is not signaled to ensure we don't end up immediately returning from WaitForMultipleObjects().
    ResetEvent(pDevice->wasapi.hStopEvent);

    pDevice->wasapi.breakFromMainLoop = MAL_FALSE;
    while (!pDevice->wasapi.breakFromMainLoop) {
        mal_uint32 framesAvailable = mal_device__wait_for_frames__wasapi(pDevice);
        if (framesAvailable == 0) {
            continue;
        }

        // If it's a playback device, don't bother grabbing more data if the device is being stopped.
        if (pDevice->wasapi.breakFromMainLoop && pDevice->type == mal_device_type_playback) {
            return MAL_FALSE;
        }

        if (pDevice->type == mal_device_type_playback) {
            BYTE* pData;
#ifdef __cplusplus
            HRESULT hr = ((IAudioRenderClient*)pDevice->wasapi.pRenderClient)->GetBuffer(framesAvailable, &pData);
#else
            HRESULT hr = ((IAudioRenderClient*)pDevice->wasapi.pRenderClient)->lpVtbl->GetBuffer((IAudioRenderClient*)pDevice->wasapi.pRenderClient, framesAvailable, &pData);
#endif
            if (FAILED(hr)) {
                return MAL_FAILED_TO_READ_DATA_FROM_CLIENT;
            }

            mal_device__read_frames_from_client(pDevice, framesAvailable, pData);

#ifdef __cplusplus
            hr = ((IAudioRenderClient*)pDevice->wasapi.pRenderClient)->ReleaseBuffer(framesAvailable, 0);
#else
            hr = ((IAudioRenderClient*)pDevice->wasapi.pRenderClient)->lpVtbl->ReleaseBuffer((IAudioRenderClient*)pDevice->wasapi.pRenderClient, framesAvailable, 0);
#endif
            if (FAILED(hr)) {
                return MAL_FAILED_TO_READ_DATA_FROM_CLIENT;
            }
        } else {
            UINT32 framesRemaining = framesAvailable;
            while (framesRemaining > 0) {
                BYTE* pData;
                UINT32 framesToSend;
                DWORD flags;
#ifdef __cplusplus
                HRESULT hr = ((IAudioCaptureClient*)pDevice->wasapi.pCaptureClient)->GetBuffer(&pData, &framesToSend, &flags, NULL, NULL);
#else
                HRESULT hr = ((IAudioCaptureClient*)pDevice->wasapi.pCaptureClient)->lpVtbl->GetBuffer((IAudioCaptureClient*)pDevice->wasapi.pCaptureClient, &pData, &framesToSend, &flags, NULL, NULL);
#endif
                if (FAILED(hr)) {
                    break;
                }

                // NOTE: Do we need to handle the case when the AUDCLNT_BUFFERFLAGS_SILENT bit is set in <flags>?
                mal_device__send_frames_to_client(pDevice, framesToSend, pData);

#ifdef __cplusplus
                hr = ((IAudioCaptureClient*)pDevice->wasapi.pCaptureClient)->ReleaseBuffer(framesToSend);
#else
                hr = ((IAudioCaptureClient*)pDevice->wasapi.pCaptureClient)->lpVtbl->ReleaseBuffer((IAudioCaptureClient*)pDevice->wasapi.pCaptureClient, framesToSend);
#endif
                if (FAILED(hr)) {
                    break;
                }

                framesRemaining -= framesToSend;
            }
        }
    }

    return MAL_SUCCESS;
}
#endif

///////////////////////////////////////////////////////////////////////////////
//
// DirectSound Backend
//
///////////////////////////////////////////////////////////////////////////////
#ifdef MAL_ENABLE_DSOUND
#include <dsound.h>

static GUID MAL_GUID_NULL                               = {0x00000000, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
static GUID _g_mal_GUID_IID_DirectSoundNotify           = {0xb0210783, 0x89cd, 0x11d0, {0xaf, 0x08, 0x00, 0xa0, 0xc9, 0x25, 0xcd, 0x16}};
static GUID _g_mal_GUID_IID_IDirectSoundCaptureBuffer8  = {0x00990df4, 0x0dbb, 0x4872, {0x83, 0x3e, 0x6d, 0x30, 0x3e, 0x80, 0xae, 0xb6}};

// TODO: Remove these.
static GUID _g_mal_GUID_KSDATAFORMAT_SUBTYPE_PCM        = {0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static GUID _g_mal_GUID_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = {0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
//static GUID _g_mal_GUID_KSDATAFORMAT_SUBTYPE_ALAW       = {0x00000006, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
//static GUID _g_mal_GUID_KSDATAFORMAT_SUBTYPE_MULAW      = {0x00000007, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
#ifdef __cplusplus
static GUID g_mal_GUID_IID_DirectSoundNotify            = _g_mal_GUID_IID_DirectSoundNotify;
static GUID g_mal_GUID_IID_IDirectSoundCaptureBuffer8   = _g_mal_GUID_IID_IDirectSoundCaptureBuffer8;
#else
static GUID* g_mal_GUID_IID_DirectSoundNotify           = &_g_mal_GUID_IID_DirectSoundNotify;
static GUID* g_mal_GUID_IID_IDirectSoundCaptureBuffer8  = &_g_mal_GUID_IID_IDirectSoundCaptureBuffer8;
#endif

typedef HRESULT (WINAPI * mal_DirectSoundCreate8Proc)(LPCGUID pcGuidDevice, LPDIRECTSOUND8 *ppDS8, LPUNKNOWN pUnkOuter);
typedef HRESULT (WINAPI * mal_DirectSoundEnumerateAProc)(LPDSENUMCALLBACKA pDSEnumCallback, LPVOID pContext);
typedef HRESULT (WINAPI * mal_DirectSoundCaptureCreate8Proc)(LPCGUID pcGuidDevice, LPDIRECTSOUNDCAPTURE8 *ppDSC8, LPUNKNOWN pUnkOuter);
typedef HRESULT (WINAPI * mal_DirectSoundCaptureEnumerateAProc)(LPDSENUMCALLBACKA pDSEnumCallback, LPVOID pContext);

static HMODULE mal_open_dsound_dll()
{
    return LoadLibraryW(L"dsound.dll");
}

static void mal_close_dsound_dll(HMODULE hModule)
{
    FreeLibrary(hModule);
}


mal_result mal_context_init__dsound(mal_context* pContext)
{
    mal_assert(pContext != NULL);

    (void)pContext;
    return MAL_SUCCESS;
}

mal_result mal_context_uninit__dsound(mal_context* pContext)
{
    mal_assert(pContext != NULL);
    mal_assert(pContext->backend == mal_backend_dsound);

    (void)pContext;
    return MAL_SUCCESS;
}


typedef struct
{
    mal_uint32 deviceCount;
    mal_uint32 infoCount;
    mal_device_info* pInfo;
} mal_device_enum_data__dsound;

static BOOL CALLBACK mal_enum_devices_callback__dsound(LPGUID lpGuid, LPCSTR lpcstrDescription, LPCSTR lpcstrModule, LPVOID lpContext)
{
    (void)lpcstrModule;

    mal_device_enum_data__dsound* pData = (mal_device_enum_data__dsound*)lpContext;
    mal_assert(pData != NULL);

    if (pData->pInfo != NULL) {
        if (pData->infoCount > 0) {
            mal_zero_object(pData->pInfo);
            mal_strncpy_s(pData->pInfo->name, sizeof(pData->pInfo->name), lpcstrDescription, (size_t)-1);

            if (lpGuid != NULL) {
                mal_copy_memory(pData->pInfo->id.dsound, lpGuid, 16);
            } else {
                mal_zero_memory(pData->pInfo->id.dsound, 16);
            }

            pData->pInfo += 1;
            pData->infoCount -= 1;
            pData->deviceCount += 1;
        }
    } else {
        pData->deviceCount += 1;
    }

    return TRUE;
}

static mal_result mal_enumerate_devices__dsound(mal_context* pContext, mal_device_type type, mal_uint32* pCount, mal_device_info* pInfo)
{
    (void)pContext;

    mal_uint32 infoSize = *pCount;
    *pCount = 0;

    mal_device_enum_data__dsound enumData;
    enumData.deviceCount = 0;
    enumData.infoCount = infoSize;
    enumData.pInfo = pInfo;

    HMODULE dsoundDLL = mal_open_dsound_dll();
    if (dsoundDLL == NULL) {
        return MAL_NO_BACKEND;
    }

    if (type == mal_device_type_playback) {
        mal_DirectSoundEnumerateAProc pDirectSoundEnumerateA = (mal_DirectSoundEnumerateAProc)GetProcAddress(dsoundDLL, "DirectSoundEnumerateA");
        if (pDirectSoundEnumerateA) {
            pDirectSoundEnumerateA(mal_enum_devices_callback__dsound, &enumData);
        }
    } else {
        mal_DirectSoundCaptureEnumerateAProc pDirectSoundCaptureEnumerateA = (mal_DirectSoundCaptureEnumerateAProc)GetProcAddress(dsoundDLL, "DirectSoundCaptureEnumerateA");
        if (pDirectSoundCaptureEnumerateA) {
            pDirectSoundCaptureEnumerateA(mal_enum_devices_callback__dsound, &enumData);
        }
    }


    mal_close_dsound_dll(dsoundDLL);

    *pCount = enumData.deviceCount;
    return MAL_SUCCESS;
}

static void mal_device_uninit__dsound(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    if (pDevice->dsound.hDSoundDLL != NULL) {
        if (pDevice->dsound.pNotify) {
            IDirectSoundNotify_Release((LPDIRECTSOUNDNOTIFY)pDevice->dsound.pNotify);
        }

        if (pDevice->dsound.hStopEvent) {
            CloseHandle(pDevice->dsound.hStopEvent);
        }
        for (mal_uint32 i = 0; i < pDevice->periods; ++i) {
            if (pDevice->dsound.pNotifyEvents[i]) {
                CloseHandle(pDevice->dsound.pNotifyEvents[i]);
            }
        }

        if (pDevice->dsound.pCaptureBuffer) {
            IDirectSoundCaptureBuffer8_Release((LPDIRECTSOUNDBUFFER8)pDevice->dsound.pCaptureBuffer);
        }
        if (pDevice->dsound.pCapture) {
            IDirectSoundCapture_Release((LPDIRECTSOUNDCAPTURE8)pDevice->dsound.pCapture);
        }

        if (pDevice->dsound.pPlaybackBuffer) {
            IDirectSoundBuffer_Release((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackBuffer);
        }
        if (pDevice->dsound.pPlaybackPrimaryBuffer) {
            IDirectSoundBuffer_Release((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackPrimaryBuffer);
        }
        if (pDevice->dsound.pPlayback != NULL) {
            IDirectSound_Release((LPDIRECTSOUND8)pDevice->dsound.pPlayback);
        }

        mal_close_dsound_dll((HMODULE)pDevice->dsound.hDSoundDLL);
    }
}

static mal_result mal_device_init__dsound(mal_context* pContext, mal_device_type type, mal_device_id* pDeviceID, mal_device_config* pConfig, mal_device* pDevice)
{
    (void)pContext;

    mal_assert(pDevice != NULL);
    mal_zero_object(&pDevice->dsound);

    pDevice->dsound.hDSoundDLL = (mal_handle)mal_open_dsound_dll();
    if (pDevice->dsound.hDSoundDLL == NULL) {
        return MAL_NO_BACKEND;
    }

    // Check that we have a valid format.
    GUID subformat;
    switch (pConfig->format)
    {
        case mal_format_u8:
        case mal_format_s16:
        case mal_format_s24:
        case mal_format_s32:
        {
            subformat = _g_mal_GUID_KSDATAFORMAT_SUBTYPE_PCM;
        } break;

        case mal_format_f32:
        {
            subformat = _g_mal_GUID_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
        } break;

        default:
        return MAL_FORMAT_NOT_SUPPORTED;
    }


    WAVEFORMATEXTENSIBLE wf;
    mal_zero_object(&wf);
    wf.Format.cbSize               = sizeof(wf);
    wf.Format.wFormatTag           = WAVE_FORMAT_EXTENSIBLE;
    wf.Format.nChannels            = (WORD)pConfig->channels;
    wf.Format.nSamplesPerSec       = (DWORD)pConfig->sampleRate;
    wf.Format.wBitsPerSample       = (WORD)mal_get_sample_size_in_bytes(pConfig->format)*8;
    wf.Format.nBlockAlign          = (wf.Format.nChannels * wf.Format.wBitsPerSample) / 8;
    wf.Format.nAvgBytesPerSec      = wf.Format.nBlockAlign * wf.Format.nSamplesPerSec;
    wf.Samples.wValidBitsPerSample = wf.Format.wBitsPerSample;
    wf.dwChannelMask               = mal_channel_map_to_channel_mask__win32(pConfig->channelMap, pConfig->channels);
    wf.SubFormat                   = subformat;

    DWORD bufferSizeInBytes = 0;

    // Unfortunately DirectSound uses different APIs and data structures for playback and catpure devices :(
    if (type == mal_device_type_playback) {
        mal_DirectSoundCreate8Proc pDirectSoundCreate8 = (mal_DirectSoundCreate8Proc)GetProcAddress((HMODULE)pDevice->dsound.hDSoundDLL, "DirectSoundCreate8");
        if (pDirectSoundCreate8 == NULL) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] Could not find DirectSoundCreate8().", MAL_API_NOT_FOUND);
        }

        if (FAILED(pDirectSoundCreate8((pDeviceID == NULL) ? NULL : (LPCGUID)pDeviceID->dsound, (LPDIRECTSOUND8*)&pDevice->dsound.pPlayback, NULL))) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] DirectSoundCreate8() failed for playback device.", MAL_DSOUND_FAILED_TO_CREATE_DEVICE);
        }

        // The cooperative level must be set before doing anything else.
        if (FAILED(IDirectSound_SetCooperativeLevel((LPDIRECTSOUND8)pDevice->dsound.pPlayback, GetForegroundWindow(), DSSCL_PRIORITY))) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] IDirectSound_SetCooperateiveLevel() failed for playback device.", MAL_DSOUND_FAILED_TO_SET_COOP_LEVEL);
        }

        DSBUFFERDESC descDSPrimary;
        mal_zero_object(&descDSPrimary);
        descDSPrimary.dwSize  = sizeof(DSBUFFERDESC);
        descDSPrimary.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_CTRLVOLUME;
        if (FAILED(IDirectSound_CreateSoundBuffer((LPDIRECTSOUND8)pDevice->dsound.pPlayback, &descDSPrimary, (LPDIRECTSOUNDBUFFER*)&pDevice->dsound.pPlaybackPrimaryBuffer, NULL))) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] IDirectSound_CreateSoundBuffer() failed for playback device's primary buffer.", MAL_DSOUND_FAILED_TO_CREATE_BUFFER);
        }

        // From MSDN:
        //
        // The method succeeds even if the hardware does not support the requested format; DirectSound sets the buffer to the closest
        // supported format. To determine whether this has happened, an application can call the GetFormat method for the primary buffer
        // and compare the result with the format that was requested with the SetFormat method.
        if (FAILED(IDirectSoundBuffer_SetFormat((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackPrimaryBuffer, (WAVEFORMATEX*)&wf))) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] Failed to set format of playback device's primary buffer.", MAL_FORMAT_NOT_SUPPORTED);
        }

        // Get the _actual_ properties of the buffer. This is silly API design...
        DWORD requiredSize;
        if (FAILED(IDirectSoundBuffer_GetFormat((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackPrimaryBuffer, NULL, 0, &requiredSize))) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] Failed to retrieve the actual format of the playback device's primary buffer.", MAL_FORMAT_NOT_SUPPORTED);
        }

        char rawdata[1024];
        WAVEFORMATEXTENSIBLE* pActualFormat = (WAVEFORMATEXTENSIBLE*)rawdata;
        if (FAILED(IDirectSoundBuffer_GetFormat((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackPrimaryBuffer, (WAVEFORMATEX*)pActualFormat, requiredSize, NULL))) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] Failed to retrieve the actual format of the playback device's primary buffer.", MAL_FORMAT_NOT_SUPPORTED);
        }

        pDevice->internalChannels = pActualFormat->Format.nChannels;
        pDevice->internalSampleRate = pActualFormat->Format.nSamplesPerSec;
        bufferSizeInBytes = pDevice->bufferSizeInFrames * pDevice->internalChannels * mal_get_sample_size_in_bytes(pDevice->format);

        // Get the internal channel map based on the channel mask.
        mal_channel_mask_to_channel_map__win32(pActualFormat->dwChannelMask, pDevice->internalChannels, pDevice->internalChannelMap);


        // Meaning of dwFlags (from MSDN):
        //
        // DSBCAPS_CTRLPOSITIONNOTIFY
        //   The buffer has position notification capability.
        //
        // DSBCAPS_GLOBALFOCUS
        //   With this flag set, an application using DirectSound can continue to play its buffers if the user switches focus to
        //   another application, even if the new application uses DirectSound.
        //
        // DSBCAPS_GETCURRENTPOSITION2
        //   In the first version of DirectSound, the play cursor was significantly ahead of the actual playing sound on emulated
        //   sound cards; it was directly behind the write cursor. Now, if the DSBCAPS_GETCURRENTPOSITION2 flag is specified, the
        //   application can get a more accurate play cursor.
        DSBUFFERDESC descDS;
        mal_zero_object(&descDS);
        descDS.dwSize = sizeof(DSBUFFERDESC);
        descDS.dwFlags = DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_GLOBALFOCUS | DSBCAPS_GETCURRENTPOSITION2;
        descDS.dwBufferBytes = bufferSizeInBytes;
        descDS.lpwfxFormat = (WAVEFORMATEX*)&wf;
        if (FAILED(IDirectSound_CreateSoundBuffer((LPDIRECTSOUND8)pDevice->dsound.pPlayback, &descDS, (LPDIRECTSOUNDBUFFER*)&pDevice->dsound.pPlaybackBuffer, NULL))) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] IDirectSound_CreateSoundBuffer() failed for playback device's secondary buffer.", MAL_DSOUND_FAILED_TO_CREATE_BUFFER);
        }

        // Notifications are set up via a DIRECTSOUNDNOTIFY object which is retrieved from the buffer.
        if (FAILED(IDirectSoundBuffer8_QueryInterface((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackBuffer, g_mal_GUID_IID_DirectSoundNotify, (void**)&pDevice->dsound.pNotify))) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] IDirectSoundBuffer8_QueryInterface() failed for playback device's IDirectSoundNotify object.", MAL_DSOUND_FAILED_TO_QUERY_INTERFACE);
        }
    } else {
        // The default buffer size is treated slightly differently for DirectSound which, for some reason, seems to
        // have worse latency with capture than playback (sometimes _much_ worse).
        if (pDevice->flags & MAL_DEVICE_FLAG_USING_DEFAULT_BUFFER_SIZE) {
            pDevice->bufferSizeInFrames *= 2; // <-- Might need to fiddle with this to find a more ideal value. May even be able to just add a fixed amount rather than scaling.
        }

        mal_DirectSoundCaptureCreate8Proc pDirectSoundCaptureCreate8 = (mal_DirectSoundCaptureCreate8Proc)GetProcAddress((HMODULE)pDevice->dsound.hDSoundDLL, "DirectSoundCaptureCreate8");
        if (pDirectSoundCaptureCreate8 == NULL) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] Could not find DirectSoundCreate8().", MAL_API_NOT_FOUND);
        }

        if (FAILED(pDirectSoundCaptureCreate8((pDeviceID == NULL) ? NULL : (LPCGUID)pDeviceID->dsound, (LPDIRECTSOUNDCAPTURE8*)&pDevice->dsound.pCapture, NULL))) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] DirectSoundCaptureCreate8() failed for capture device.", MAL_DSOUND_FAILED_TO_CREATE_DEVICE);
        }

        bufferSizeInBytes = pDevice->bufferSizeInFrames * pDevice->channels * mal_get_sample_size_in_bytes(pDevice->format);

        DSCBUFFERDESC descDS;
        mal_zero_object(&descDS);
        descDS.dwSize = sizeof(descDS);
        descDS.dwFlags = 0;
        descDS.dwBufferBytes = bufferSizeInBytes;
        descDS.lpwfxFormat = (WAVEFORMATEX*)&wf;
        LPDIRECTSOUNDCAPTUREBUFFER pDSCB_Temp;
        if (FAILED(IDirectSoundCapture_CreateCaptureBuffer((LPDIRECTSOUNDCAPTURE8)pDevice->dsound.pCapture, &descDS, &pDSCB_Temp, NULL))) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] IDirectSoundCapture_CreateCaptureBuffer() failed for capture device.", MAL_DSOUND_FAILED_TO_CREATE_BUFFER);
        }

        HRESULT hr = IDirectSoundCapture_QueryInterface(pDSCB_Temp, g_mal_GUID_IID_IDirectSoundCaptureBuffer8, (LPVOID*)&pDevice->dsound.pCaptureBuffer);
        IDirectSoundCaptureBuffer_Release(pDSCB_Temp);
        if (FAILED(hr)) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] IDirectSoundCapture_QueryInterface() failed for capture device's IDirectSoundCaptureBuffer8 object.", MAL_DSOUND_FAILED_TO_QUERY_INTERFACE);
        }

        // Notifications are set up via a DIRECTSOUNDNOTIFY object which is retrieved from the buffer.
        if (FAILED(IDirectSoundCaptureBuffer8_QueryInterface((LPDIRECTSOUNDCAPTUREBUFFER)pDevice->dsound.pCaptureBuffer, g_mal_GUID_IID_DirectSoundNotify, (void**)&pDevice->dsound.pNotify))) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] IDirectSoundCaptureBuffer8_QueryInterface() failed for capture device's IDirectSoundNotify object.", MAL_DSOUND_FAILED_TO_QUERY_INTERFACE);
        }
    }

    // We need a notification for each period. The notification offset is slightly different depending on whether or not the
    // device is a playback or capture device. For a playback device we want to be notified when a period just starts playing,
    // whereas for a capture device we want to be notified when a period has just _finished_ capturing.
    mal_uint32 periodSizeInBytes = pDevice->bufferSizeInFrames / pDevice->periods;
    DSBPOSITIONNOTIFY notifyPoints[MAL_MAX_PERIODS_DSOUND];  // One notification event for each period.
    for (mal_uint32 i = 0; i < pDevice->periods; ++i) {
        pDevice->dsound.pNotifyEvents[i] = CreateEventA(NULL, FALSE, FALSE, NULL);
        if (pDevice->dsound.pNotifyEvents[i] == NULL) {
            mal_device_uninit__dsound(pDevice);
            return mal_post_error(pDevice, "[DirectSound] Failed to create event for buffer notifications.", MAL_FAILED_TO_CREATE_EVENT);
        }

        // The notification offset is in bytes.
        notifyPoints[i].dwOffset = i * periodSizeInBytes;
        notifyPoints[i].hEventNotify = pDevice->dsound.pNotifyEvents[i];
    }

    if (FAILED(IDirectSoundNotify_SetNotificationPositions((LPDIRECTSOUNDNOTIFY)pDevice->dsound.pNotify, pDevice->periods, notifyPoints))) {
        mal_device_uninit__dsound(pDevice);
        return mal_post_error(pDevice, "[DirectSound] IDirectSoundNotify_SetNotificationPositions() failed.", MAL_DSOUND_FAILED_TO_SET_NOTIFICATIONS);
    }

    // When the device is playing the worker thread will be waiting on a bunch of notification events. To return from
    // this wait state we need to signal a special event.
    pDevice->dsound.hStopEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (pDevice->dsound.hStopEvent == NULL) {
        mal_device_uninit__dsound(pDevice);
        return mal_post_error(pDevice, "[DirectSound] Failed to create event for main loop break notification.", MAL_FAILED_TO_CREATE_EVENT);
    }

    return MAL_SUCCESS;
}


static mal_result mal_device__start_backend__dsound(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    if (pDevice->type == mal_device_type_playback) {
        // Before playing anything we need to grab an initial group of samples from the client.
        mal_uint32 framesToRead = pDevice->bufferSizeInFrames / pDevice->periods;
        mal_uint32 desiredLockSize = framesToRead * pDevice->channels * mal_get_sample_size_in_bytes(pDevice->format);

        void* pLockPtr;
        DWORD actualLockSize;
        void* pLockPtr2;
        DWORD actualLockSize2;
        if (SUCCEEDED(IDirectSoundBuffer_Lock((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackBuffer, 0, desiredLockSize, &pLockPtr, &actualLockSize, &pLockPtr2, &actualLockSize2, 0))) {
            framesToRead = actualLockSize / mal_get_sample_size_in_bytes(pDevice->format) / pDevice->channels;
            mal_device__read_frames_from_client(pDevice, framesToRead, pLockPtr);
            IDirectSoundBuffer_Unlock((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackBuffer, pLockPtr, actualLockSize, pLockPtr2, actualLockSize2);

            pDevice->dsound.lastProcessedFrame = framesToRead;
            if (FAILED(IDirectSoundBuffer_Play((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackBuffer, 0, 0, DSBPLAY_LOOPING))) {
                return mal_post_error(pDevice, "[DirectSound] IDirectSoundBuffer_Play() failed.", MAL_FAILED_TO_START_BACKEND_DEVICE);
            }
        } else {
            return mal_post_error(pDevice, "[DirectSound] IDirectSoundBuffer_Lock() failed.", MAL_FAILED_TO_MAP_DEVICE_BUFFER);
        }
    } else {
        if (FAILED(IDirectSoundCaptureBuffer8_Start((LPDIRECTSOUNDCAPTUREBUFFER8)pDevice->dsound.pCaptureBuffer, DSCBSTART_LOOPING))) {
            return mal_post_error(pDevice, "[DirectSound] IDirectSoundCaptureBuffer8_Start() failed.", MAL_FAILED_TO_START_BACKEND_DEVICE);
        }
    }

    return MAL_SUCCESS;
}

static mal_result mal_device__stop_backend__dsound(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    if (pDevice->type == mal_device_type_playback) {
        if (FAILED(IDirectSoundBuffer_Stop((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackBuffer))) {
            return mal_post_error(pDevice, "[DirectSound] IDirectSoundBuffer_Stop() failed.", MAL_FAILED_TO_STOP_BACKEND_DEVICE);
        }

        IDirectSoundBuffer_SetCurrentPosition((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackBuffer, 0);
    } else {
        if (FAILED(IDirectSoundCaptureBuffer_Stop((LPDIRECTSOUNDCAPTUREBUFFER)pDevice->dsound.pCaptureBuffer))) {
            return mal_post_error(pDevice, "[DirectSound] IDirectSoundCaptureBuffer_Stop() failed.", MAL_FAILED_TO_STOP_BACKEND_DEVICE);
        }
    }

    return MAL_SUCCESS;
}

static mal_result mal_device__break_main_loop__dsound(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    // The main loop will be waiting on a bunch of events via the WaitForMultipleObjects() API. One of those events
    // is a special event we use for forcing that function to return.
    pDevice->dsound.breakFromMainLoop = MAL_TRUE;
    SetEvent(pDevice->dsound.hStopEvent);
    return MAL_SUCCESS;
}

static mal_bool32 mal_device__get_current_frame__dsound(mal_device* pDevice, mal_uint32* pCurrentPos)
{
    mal_assert(pDevice != NULL);
    mal_assert(pCurrentPos != NULL);
    *pCurrentPos = 0;

    DWORD dwCurrentPosition;
    if (pDevice->type == mal_device_type_playback) {
        if (FAILED(IDirectSoundBuffer_GetCurrentPosition((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackBuffer, NULL, &dwCurrentPosition))) {
            return MAL_FALSE;
        }
    } else {
        if (FAILED(IDirectSoundCaptureBuffer8_GetCurrentPosition((LPDIRECTSOUNDCAPTUREBUFFER8)pDevice->dsound.pCaptureBuffer, &dwCurrentPosition, NULL))) {
            return MAL_FALSE;
        }
    }

    *pCurrentPos = (mal_uint32)dwCurrentPosition / mal_get_sample_size_in_bytes(pDevice->format) / pDevice->channels;
    return MAL_TRUE;
}

static mal_uint32 mal_device__get_available_frames__dsound(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    mal_uint32 currentFrame;
    if (!mal_device__get_current_frame__dsound(pDevice, &currentFrame)) {
        return 0;
    }

    // In a playback device the last processed frame should always be ahead of the current frame. The space between
    // the last processed and current frame (moving forward, starting from the last processed frame) is the amount
    // of space available to write.
    //
    // For a recording device it's the other way around - the last processed frame is always _behind_ the current
    // frame and the space between is the available space.
    mal_uint32 totalFrameCount = pDevice->bufferSizeInFrames;
    if (pDevice->type == mal_device_type_playback) {
        mal_uint32 committedBeg = currentFrame;
        mal_uint32 committedEnd;
        committedEnd = pDevice->dsound.lastProcessedFrame;
        if (committedEnd <= committedBeg) {
            committedEnd += totalFrameCount;
        }

        mal_uint32 committedSize = (committedEnd - committedBeg);
        mal_assert(committedSize <= totalFrameCount);

        return totalFrameCount - committedSize;
    } else {
        mal_uint32 validBeg = pDevice->dsound.lastProcessedFrame;
        mal_uint32 validEnd = currentFrame;
        if (validEnd < validBeg) {
            validEnd += totalFrameCount;        // Wrap around.
        }

        mal_uint32 validSize = (validEnd - validBeg);
        mal_assert(validSize <= totalFrameCount);

        return validSize;
    }
}

static mal_uint32 mal_device__wait_for_frames__dsound(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    // The timeout to use for putting the thread to sleep is based on the size of the buffer and the period count.
    DWORD timeoutInMilliseconds = (pDevice->bufferSizeInFrames / (pDevice->sampleRate/1000)) / pDevice->periods;
    if (timeoutInMilliseconds < 1) {
        timeoutInMilliseconds = 1;
    }

    unsigned int eventCount = pDevice->periods + 1;
    HANDLE pEvents[MAL_MAX_PERIODS_DSOUND + 1];   // +1 for the stop event.
    mal_copy_memory(pEvents, pDevice->dsound.pNotifyEvents, sizeof(HANDLE) * pDevice->periods);
    pEvents[eventCount-1] = pDevice->dsound.hStopEvent;

    while (!pDevice->dsound.breakFromMainLoop) {
        mal_uint32 framesAvailable = mal_device__get_available_frames__dsound(pDevice);
        if (framesAvailable > 0) {
            return framesAvailable;
        }

        // If we get here it means we weren't able to find any frames. We'll just wait here for a bit.
        WaitForMultipleObjects(eventCount, pEvents, FALSE, timeoutInMilliseconds);
    }

    // We'll get here if the loop was terminated. Just return whatever's available.
    return mal_device__get_available_frames__dsound(pDevice);
}

static mal_result mal_device__main_loop__dsound(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    // Make sure the stop event is not signaled to ensure we don't end up immediately returning from WaitForMultipleObjects().
    ResetEvent(pDevice->dsound.hStopEvent);

    pDevice->dsound.breakFromMainLoop = MAL_FALSE;
    while (!pDevice->dsound.breakFromMainLoop) {
        mal_uint32 framesAvailable = mal_device__wait_for_frames__dsound(pDevice);
        if (framesAvailable == 0) {
            continue;
        }

        // If it's a playback device, don't bother grabbing more data if the device is being stopped.
        if (pDevice->dsound.breakFromMainLoop && pDevice->type == mal_device_type_playback) {
            return MAL_FALSE;
        }

        DWORD lockOffset = pDevice->dsound.lastProcessedFrame * pDevice->channels * mal_get_sample_size_in_bytes(pDevice->format);
        DWORD lockSize   = framesAvailable * pDevice->channels * mal_get_sample_size_in_bytes(pDevice->format);

        if (pDevice->type == mal_device_type_playback) {
            void* pLockPtr;
            DWORD actualLockSize;
            void* pLockPtr2;
            DWORD actualLockSize2;
            if (FAILED(IDirectSoundBuffer_Lock((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackBuffer, lockOffset, lockSize, &pLockPtr, &actualLockSize, &pLockPtr2, &actualLockSize2, 0))) {
                return mal_post_error(pDevice, "[DirectSound] IDirectSoundBuffer_Lock() failed.", MAL_FAILED_TO_MAP_DEVICE_BUFFER);
            }

            mal_uint32 frameCount = actualLockSize / mal_get_sample_size_in_bytes(pDevice->format) / pDevice->channels;
            mal_device__read_frames_from_client(pDevice, frameCount, pLockPtr);
            pDevice->dsound.lastProcessedFrame = (pDevice->dsound.lastProcessedFrame + frameCount) % pDevice->bufferSizeInFrames;

            IDirectSoundBuffer_Unlock((LPDIRECTSOUNDBUFFER)pDevice->dsound.pPlaybackBuffer, pLockPtr, actualLockSize, pLockPtr2, actualLockSize2);
        } else {
            void* pLockPtr;
            DWORD actualLockSize;
            void* pLockPtr2;
            DWORD actualLockSize2;
            if (FAILED(IDirectSoundCaptureBuffer_Lock((LPDIRECTSOUNDCAPTUREBUFFER)pDevice->dsound.pCaptureBuffer, lockOffset, lockSize, &pLockPtr, &actualLockSize, &pLockPtr2, &actualLockSize2, 0))) {
                return mal_post_error(pDevice, "[DirectSound] IDirectSoundCaptureBuffer_Lock() failed.", MAL_FAILED_TO_MAP_DEVICE_BUFFER);
            }

            mal_uint32 frameCount = actualLockSize / mal_get_sample_size_in_bytes(pDevice->format) / pDevice->channels;
            mal_device__send_frames_to_client(pDevice, frameCount, pLockPtr);
            pDevice->dsound.lastProcessedFrame = (pDevice->dsound.lastProcessedFrame + frameCount) % pDevice->bufferSizeInFrames;

            IDirectSoundCaptureBuffer_Unlock((LPDIRECTSOUNDCAPTUREBUFFER)pDevice->dsound.pCaptureBuffer, pLockPtr, actualLockSize, pLockPtr2, actualLockSize2);
        }
    }

    return MAL_SUCCESS;
}
#endif


///////////////////////////////////////////////////////////////////////////////
//
// ALSA Backend
//
///////////////////////////////////////////////////////////////////////////////
#ifdef MAL_ENABLE_ALSA
#include <alsa/asoundlib.h>

mal_channel mal_convert_alsa_channel_position_to_mal_channel(unsigned int alsaChannelPos)
{
    switch (alsaChannelPos)
    {
        case SND_CHMAP_FL:  return MAL_CHANNEL_FRONT_LEFT;
        case SND_CHMAP_FR:  return MAL_CHANNEL_FRONT_RIGHT;
        case SND_CHMAP_RL:  return MAL_CHANNEL_BACK_LEFT;         /* rear left */
        case SND_CHMAP_RR:  return MAL_CHANNEL_BACK_RIGHT;         /* rear right */
        case SND_CHMAP_FC:  return MAL_CHANNEL_FRONT_CENTER;         /* front center */
        case SND_CHMAP_LFE: return MAL_CHANNEL_LFE;           /* LFE */
        case SND_CHMAP_SL:  return MAL_CHANNEL_SIDE_LEFT;         /* side left */
        case SND_CHMAP_SR:  return MAL_CHANNEL_SIDE_RIGHT;         /* side right */
        case SND_CHMAP_RC:  return MAL_CHANNEL_BACK_CENTER;         /* rear center */
        case SND_CHMAP_FLC: return MAL_CHANNEL_FRONT_LEFT_CENTER;        /* front left center */
        case SND_CHMAP_FRC: return MAL_CHANNEL_FRONT_RIGHT_CENTER;        /* front right center */
        case SND_CHMAP_RLC: return 0;        /* rear left center */
        case SND_CHMAP_RRC: return 0;        /* rear right center */
        case SND_CHMAP_FLW: return 0;        /* front left wide */
        case SND_CHMAP_FRW: return 0;        /* front right wide */
        case SND_CHMAP_FLH: return 0;        /* front left high */
        case SND_CHMAP_FCH: return 0;        /* front center high */
        case SND_CHMAP_FRH: return 0;        /* front right high */
        case SND_CHMAP_TC:  return MAL_CHANNEL_TOP_CENTER;         /* top center */
        case SND_CHMAP_TFL: return MAL_CHANNEL_TOP_FRONT_LEFT;        /* top front left */
        case SND_CHMAP_TFR: return MAL_CHANNEL_TOP_FRONT_RIGHT;        /* top front right */
        case SND_CHMAP_TFC: return MAL_CHANNEL_TOP_FRONT_CENTER;        /* top front center */
        case SND_CHMAP_TRL: return MAL_CHANNEL_TOP_BACK_LEFT;        /* top rear left */
        case SND_CHMAP_TRR: return MAL_CHANNEL_TOP_BACK_RIGHT;        /* top rear right */
        case SND_CHMAP_TRC: return MAL_CHANNEL_TOP_BACK_CENTER;        /* top rear center */
        default: break;
    }
    
    return 0;
}

mal_result mal_context_init__alsa(mal_context* pContext)
{
    mal_assert(pContext != NULL);

    (void)pContext;
    return MAL_SUCCESS;
}

mal_result mal_context_uninit__alsa(mal_context* pContext)
{
    mal_assert(pContext != NULL);
    mal_assert(pContext->backend == mal_backend_alsa);

    (void)pContext;
    return MAL_SUCCESS;
}

static const char* mal_find_char(const char* str, char c, int* index)
{
    int i = 0;
    for (;;) {
        if (str[i] == '\0') {
            if (index) *index = -1;
            return NULL;
        }

        if (str[i] == c) {
            if (index) *index = i;
            return str + i;
        }

        i += 1;
    }

    // Should never get here, but treat it as though the character was not found to make me feel
    // better inside.
    if (index) *index = -1;
    return NULL;
}

// Waits for a number of frames to become available for either capture or playback. The return
// value is the number of frames available.
//
// This will return early if the main loop is broken with mal_device__break_main_loop().
static mal_uint32 mal_device__wait_for_frames__alsa(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    while (!pDevice->alsa.breakFromMainLoop) {
        snd_pcm_sframes_t framesAvailable = snd_pcm_avail((snd_pcm_t*)pDevice->alsa.pPCM);
        if (framesAvailable > 0) {
            return framesAvailable;
        }

        if (framesAvailable < 0) {
            if (framesAvailable == -EPIPE) {
                if (snd_pcm_recover((snd_pcm_t*)pDevice->alsa.pPCM, framesAvailable, MAL_TRUE) < 0) {
                    return 0;
                }

                framesAvailable = snd_pcm_avail((snd_pcm_t*)pDevice->alsa.pPCM);
                if (framesAvailable < 0) {
                    return 0;
                }
            }
        }

        const int timeoutInMilliseconds = 20;  // <-- The larger this value, the longer it'll take to stop the device!
        int waitResult = snd_pcm_wait((snd_pcm_t*)pDevice->alsa.pPCM, timeoutInMilliseconds);
        if (waitResult < 0) {
            snd_pcm_recover((snd_pcm_t*)pDevice->alsa.pPCM, waitResult, MAL_TRUE);
        }
    }

    // We'll get here if the loop was terminated. Just return whatever's available.
    snd_pcm_sframes_t framesAvailable = snd_pcm_avail((snd_pcm_t*)pDevice->alsa.pPCM);
    if (framesAvailable < 0) {
        return 0;
    }

    return framesAvailable;
}

static mal_bool32 mal_device_write__alsa(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);
    if (!mal_device_is_started(pDevice)) {
        return MAL_FALSE;
    }
    if (pDevice->alsa.breakFromMainLoop) {
        return MAL_FALSE;
    }


    if (pDevice->alsa.pIntermediaryBuffer == NULL) {
        // mmap.
        mal_uint32 framesAvailable = mal_device__wait_for_frames__alsa(pDevice);
        if (framesAvailable == 0) {
            return MAL_FALSE;
        }

        // Don't bother asking the client for more audio data if we're just stopping the device anyway.
        if (pDevice->alsa.breakFromMainLoop) {
            return MAL_FALSE;
        }

        const snd_pcm_channel_area_t* pAreas;
        snd_pcm_uframes_t mappedOffset;
        snd_pcm_uframes_t mappedFrames = framesAvailable;
        while (framesAvailable > 0) {
            int result = snd_pcm_mmap_begin((snd_pcm_t*)pDevice->alsa.pPCM, &pAreas, &mappedOffset, &mappedFrames);
            if (result < 0) {
                return MAL_FALSE;
            }

            void* pBuffer = (mal_uint8*)pAreas[0].addr + ((pAreas[0].first + (mappedOffset * pAreas[0].step)) / 8);
            mal_device__read_frames_from_client(pDevice, mappedFrames, pBuffer);

            result = snd_pcm_mmap_commit((snd_pcm_t*)pDevice->alsa.pPCM, mappedOffset, mappedFrames);
            if (result < 0 || (snd_pcm_uframes_t)result != mappedFrames) {
                snd_pcm_recover((snd_pcm_t*)pDevice->alsa.pPCM, result, MAL_TRUE);
                return MAL_FALSE;
            }

            framesAvailable -= mappedFrames;
        }
    } else {
        // readi/writei.
        while (!pDevice->alsa.breakFromMainLoop) {
            mal_uint32 framesAvailable = mal_device__wait_for_frames__alsa(pDevice);
            if (framesAvailable == 0) {
                continue;
            }

            // Don't bother asking the client for more audio data if we're just stopping the device anyway.
            if (pDevice->alsa.breakFromMainLoop) {
                return MAL_FALSE;
            }

            mal_device__read_frames_from_client(pDevice, framesAvailable, pDevice->alsa.pIntermediaryBuffer);

            snd_pcm_sframes_t framesWritten = snd_pcm_writei((snd_pcm_t*)pDevice->alsa.pPCM, pDevice->alsa.pIntermediaryBuffer, framesAvailable);
            if (framesWritten < 0) {
                if (framesWritten == -EAGAIN) {
                    continue;   // Just keep trying...
                } else if (framesWritten == -EPIPE) {
                    // Underrun. Just recover and try writing again.
                    if (snd_pcm_recover((snd_pcm_t*)pDevice->alsa.pPCM, framesWritten, MAL_TRUE) < 0) {
                        return MAL_FALSE;
                    }

                    framesWritten = snd_pcm_writei((snd_pcm_t*)pDevice->alsa.pPCM, pDevice->alsa.pIntermediaryBuffer, framesAvailable);
                    if (framesWritten < 0) {
                        return MAL_FALSE;
                    }

                    break;  // Success.
                } else {
                    return MAL_FALSE;
                }
            } else {
                break;  // Success.
            }
        }
    }

    return MAL_TRUE;
}

static mal_bool32 mal_device_read__alsa(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);
    if (!mal_device_is_started(pDevice)) {
        return MAL_FALSE;
    }
    if (pDevice->alsa.breakFromMainLoop) {
        return MAL_FALSE;
    }

    mal_uint32 framesToSend = 0;
    void* pBuffer = NULL;
    if (pDevice->alsa.pIntermediaryBuffer == NULL) {
        // mmap.
        mal_uint32 framesAvailable = mal_device__wait_for_frames__alsa(pDevice);
        if (framesAvailable == 0) {
            return MAL_FALSE;
        }

        const snd_pcm_channel_area_t* pAreas;
        snd_pcm_uframes_t mappedOffset;
        snd_pcm_uframes_t mappedFrames = framesAvailable;
        while (framesAvailable > 0) {
            int result = snd_pcm_mmap_begin((snd_pcm_t*)pDevice->alsa.pPCM, &pAreas, &mappedOffset, &mappedFrames);
            if (result < 0) {
                return MAL_FALSE;
            }

            void* pBuffer = (mal_uint8*)pAreas[0].addr + ((pAreas[0].first + (mappedOffset * pAreas[0].step)) / 8);
            mal_device__send_frames_to_client(pDevice, mappedFrames, pBuffer);

            result = snd_pcm_mmap_commit((snd_pcm_t*)pDevice->alsa.pPCM, mappedOffset, mappedFrames);
            if (result < 0 || (snd_pcm_uframes_t)result != mappedFrames) {
                snd_pcm_recover((snd_pcm_t*)pDevice->alsa.pPCM, result, MAL_TRUE);
                return MAL_FALSE;
            }

            framesAvailable -= mappedFrames;
        }
    } else {
        // readi/writei.
        snd_pcm_sframes_t framesRead = 0;
        while (!pDevice->alsa.breakFromMainLoop) {
            mal_uint32 framesAvailable = mal_device__wait_for_frames__alsa(pDevice);
            if (framesAvailable == 0) {
                continue;
            }

            framesRead = snd_pcm_readi((snd_pcm_t*)pDevice->alsa.pPCM, pDevice->alsa.pIntermediaryBuffer, framesAvailable);
            if (framesRead < 0) {
                if (framesRead == -EAGAIN) {
                    continue;   // Just keep trying...
                } else if (framesRead == -EPIPE) {
                    // Overrun. Just recover and try reading again.
                    if (snd_pcm_recover((snd_pcm_t*)pDevice->alsa.pPCM, framesRead, MAL_TRUE) < 0) {
                        return MAL_FALSE;
                    }

                    framesRead = snd_pcm_readi((snd_pcm_t*)pDevice->alsa.pPCM, pDevice->alsa.pIntermediaryBuffer, framesAvailable);
                    if (framesRead < 0) {
                        return MAL_FALSE;
                    }

                    break;  // Success.
                } else {
                    return MAL_FALSE;
                }
            } else {
                break;  // Success.
            }
        }

        framesToSend = framesRead;
        pBuffer = pDevice->alsa.pIntermediaryBuffer;
    }

    if (framesToSend > 0) {
        mal_device__send_frames_to_client(pDevice, framesToSend, pBuffer);
    }


    if (pDevice->alsa.pIntermediaryBuffer == NULL) {
        // mmap.
    } else {
        // readi/writei.
    }

    return MAL_TRUE;
}


static mal_result mal_enumerate_devices__alsa(mal_context* pContext, mal_device_type type, mal_uint32* pCount, mal_device_info* pInfo)
{
    (void)pContext;

    mal_uint32 infoSize = *pCount;
    *pCount = 0;

    // What I've learned about device iteration with ALSA
    // ==================================================
    //
    // The preferred method for enumerating devices is to use snd_device_name_hint() and family. The
    // reason this is preferred is because it includes user-space devices like the "default" device
    // which goes through PulseAudio. The problem, however, is that it is extremely un-user-friendly
    // because it enumerates a _lot_ of devices. On my test machine I have only a typical output device
    // for speakers/headerphones and a microphone - this results 52 devices getting enumerated!
    //
    // One way to pull this back a bit is to ignore all but "hw" devices. At initialization time we
    // can simply append "plug" to the ID string to enable software conversions.
    //
    // An alternative enumeration technique is to use snd_card_next() and family. The problem with this
    // one, which is significant, is that it does _not_ include user-space devices.
    //
    // ---
    //
    // During my testing I have discovered that snd_pcm_open() can fail on names returned by the "NAME"
    // hint returned by snd_device_name_get_hint(). To resolve this I have needed to parse the NAME
    // string and convert it to "hw:%d,%d" format.

    char** ppDeviceHints;
    if (snd_device_name_hint(-1, "pcm", (void***)&ppDeviceHints) < 0) {
        return MAL_NO_BACKEND;
    }

    char** ppNextDeviceHint = ppDeviceHints;
    while (*ppNextDeviceHint != NULL) {
        char* NAME = snd_device_name_get_hint(*ppNextDeviceHint, "NAME");
        char* DESC = snd_device_name_get_hint(*ppNextDeviceHint, "DESC");
        char* IOID = snd_device_name_get_hint(*ppNextDeviceHint, "IOID");

        if (IOID == NULL ||
            (type == mal_device_type_playback && strcmp(IOID, "Output") == 0) ||
            (type == mal_device_type_capture  && strcmp(IOID, "Input" ) == 0))
        {
            // Experiment. Skip over any non "hw" devices to try and pull back on the number
            // of enumerated devices.
            int colonPos;
            mal_find_char(NAME, ':', &colonPos);
            if (colonPos == -1 || (colonPos == 2 && (NAME[0]=='h' && NAME[1]=='w'))) {
                if (pInfo != NULL) {
                    if (infoSize > 0) {
                        mal_zero_object(pInfo);

                        // NAME is the ID.
                        mal_strncpy_s(pInfo->id.alsa, sizeof(pInfo->id.alsa), NAME ? NAME : "", (size_t)-1);

                        // NAME -> "hw:%d,%d"
                        if (colonPos != -1 && NAME != NULL) {
                            // We need to convert the NAME string to "hw:%d,%d" format.
                            char* cardStr = NAME + 3;
                            for (;;) {
                                if (cardStr[0] == '\0') {
                                    cardStr = NULL;
                                    break;
                                }
                                if (cardStr[0] == 'C' && cardStr[1] == 'A' && cardStr[2] == 'R' && cardStr[3] == 'D' && cardStr[4] == '=') {
                                    cardStr = cardStr + 5;
                                    break;
                                }

                                cardStr += 1;
                            }

                            if (cardStr != NULL) {
                                char* deviceStr = cardStr + 1;
                                for (;;) {
                                    if (deviceStr[0] == '\0') {
                                        deviceStr = NULL;
                                        break;
                                    }
                                    if (deviceStr[0] == ',') {
                                        deviceStr[0] = '\0';    // This is the comma after the "CARD=###" part.
                                    } else {
                                        if (deviceStr[0] == 'D' && deviceStr[1] == 'E' && deviceStr[2] == 'V' && deviceStr[3] == '=') {
                                            deviceStr = deviceStr + 4;
                                            break;
                                        }
                                    }

                                    deviceStr += 1;
                                }

                                if (deviceStr != NULL) {
                                    int cardIndex = snd_card_get_index(cardStr);
                                    if (cardIndex >= 0) {
                                        sprintf(pInfo->id.alsa, "hw:%d,%s", cardIndex, deviceStr);
                                    }
                                }
                            }
                        }


                        // DESC is the name, followed by the description on a new line.
                        int lfPos = 0;
                        mal_find_char(DESC, '\n', &lfPos);
                        mal_strncpy_s(pInfo->name, sizeof(pInfo->name), DESC ? DESC : "", (lfPos != -1) ? (size_t)lfPos : (size_t)-1);

                        pInfo += 1;
                        infoSize -= 1;
                        *pCount += 1;
                    }
                } else {
                    *pCount += 1;
                }
            }
        }

        free(NAME);
        free(DESC);
        free(IOID);
        ppNextDeviceHint += 1;
    }

    snd_device_name_free_hint((void**)ppDeviceHints);
    return MAL_SUCCESS;
}

static void mal_device_uninit__alsa(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    if ((snd_pcm_t*)pDevice->alsa.pPCM) {
        snd_pcm_close((snd_pcm_t*)pDevice->alsa.pPCM);

        if (pDevice->alsa.pIntermediaryBuffer != NULL) {
            mal_free(pDevice->alsa.pIntermediaryBuffer);
        }
    }
}

static mal_result mal_device_init__alsa(mal_context* pContext, mal_device_type type, mal_device_id* pDeviceID, mal_device_config* pConfig, mal_device* pDevice)
{
    (void)pContext;

    mal_assert(pDevice != NULL);
    mal_zero_object(&pDevice->alsa);

    snd_pcm_format_t formatALSA;
    switch (pConfig->format)
    {
        case mal_format_u8:  formatALSA = SND_PCM_FORMAT_U8;       break;
        case mal_format_s16: formatALSA = SND_PCM_FORMAT_S16_LE;   break;
        case mal_format_s24: formatALSA = SND_PCM_FORMAT_S24_3LE;  break;
        case mal_format_s32: formatALSA = SND_PCM_FORMAT_S32_LE;   break;
        case mal_format_f32: formatALSA = SND_PCM_FORMAT_FLOAT_LE; break;
        return mal_post_error(pDevice, "[ALSA] Format not supported.", MAL_FORMAT_NOT_SUPPORTED);
    }

    char deviceName[32];
    if (pDeviceID == NULL) {
        mal_strncpy_s(deviceName, sizeof(deviceName), "default", (size_t)-1);
    } else {
        // For now, convert "hw" devices to "plughw". The reason for this is that mini_al has not been thoroughly tested
        // with non "plughw" devices.
        if (pDeviceID->alsa[0] == 'h' && pDeviceID->alsa[1] == 'w' && pDeviceID->alsa[2] == ':') {
            deviceName[0] = 'p'; deviceName[1] = 'l'; deviceName[2] = 'u'; deviceName[3] = 'g';
            mal_strncpy_s(deviceName+4, sizeof(deviceName-4), pDeviceID->alsa, (size_t)-1);
        } else {
            mal_strncpy_s(deviceName, sizeof(deviceName), pDeviceID->alsa, (size_t)-1);
        }

    }

    if (snd_pcm_open((snd_pcm_t**)&pDevice->alsa.pPCM, deviceName, (type == mal_device_type_playback) ? SND_PCM_STREAM_PLAYBACK : SND_PCM_STREAM_CAPTURE, 0) < 0) {
        if (mal_strcmp(deviceName, "default") == 0 || mal_strcmp(deviceName, "pulse") == 0) {
            // We may have failed to open the "default" or "pulse" device, in which case try falling back to "plughw:0,0".
            mal_strncpy_s(deviceName, sizeof(deviceName), "plughw:0,0", (size_t)-1);
            if (snd_pcm_open((snd_pcm_t**)&pDevice->alsa.pPCM, deviceName, (type == mal_device_type_playback) ? SND_PCM_STREAM_PLAYBACK : SND_PCM_STREAM_CAPTURE, 0) < 0) {
                mal_device_uninit__alsa(pDevice);
                return mal_post_error(pDevice, "[ALSA] snd_pcm_open() failed.", MAL_ALSA_FAILED_TO_OPEN_DEVICE);
            }
        } else {
            mal_device_uninit__alsa(pDevice);
            return mal_post_error(pDevice, "[ALSA] snd_pcm_open() failed.", MAL_ALSA_FAILED_TO_OPEN_DEVICE);
        }
    }


    snd_pcm_hw_params_t* pHWParams = NULL;
    snd_pcm_hw_params_alloca(&pHWParams);

    if (snd_pcm_hw_params_any((snd_pcm_t*)pDevice->alsa.pPCM, pHWParams) < 0) {
        mal_device_uninit__alsa(pDevice);
        return mal_post_error(pDevice, "[ALSA] Failed to initialize hardware parameters. snd_pcm_hw_params_any() failed.", MAL_ALSA_FAILED_TO_SET_HW_PARAMS);
    }


    // Most important properties first.

    // Sample Rate
    if (snd_pcm_hw_params_set_rate_near((snd_pcm_t*)pDevice->alsa.pPCM, pHWParams, &pConfig->sampleRate, 0) < 0) {
        mal_device_uninit__alsa(pDevice);
        return mal_post_error(pDevice, "[ALSA] Sample rate not supported. snd_pcm_hw_params_set_rate_near() failed.", MAL_FORMAT_NOT_SUPPORTED);
    }
    pDevice->internalSampleRate = pConfig->sampleRate;

    // Channels.
    if (snd_pcm_hw_params_set_channels_near((snd_pcm_t*)pDevice->alsa.pPCM, pHWParams, &pConfig->channels) < 0) {
        mal_device_uninit__alsa(pDevice);
        return mal_post_error(pDevice, "[ALSA] Failed to set channel count. snd_pcm_hw_params_set_channels_near() failed.", MAL_FORMAT_NOT_SUPPORTED);
    }
    pDevice->internalChannels = pConfig->channels;


    // Format.
    if (snd_pcm_hw_params_set_format((snd_pcm_t*)pDevice->alsa.pPCM, pHWParams, formatALSA) < 0) {
        mal_device_uninit__alsa(pDevice);
        return mal_post_error(pDevice, "[ALSA] Format not supported. snd_pcm_hw_params_set_format() failed.", MAL_FORMAT_NOT_SUPPORTED);
    }


    // Buffer Size
    snd_pcm_uframes_t actualBufferSize = pConfig->bufferSizeInFrames;
    if (snd_pcm_hw_params_set_buffer_size_near((snd_pcm_t*)pDevice->alsa.pPCM, pHWParams, &actualBufferSize) < 0) {
        mal_device_uninit__alsa(pDevice);
        return mal_post_error(pDevice, "[ALSA] Failed to set buffer size for device. snd_pcm_hw_params_set_buffer_size() failed.", MAL_FORMAT_NOT_SUPPORTED);
    }


    // Periods.
    int dir = 0;
    if (snd_pcm_hw_params_set_periods_near((snd_pcm_t*)pDevice->alsa.pPCM, pHWParams, &pConfig->periods, &dir) < 0) {
        mal_device_uninit__alsa(pDevice);
        return mal_post_error(pDevice, "[ALSA] Failed to set period count. snd_pcm_hw_params_set_periods_near() failed.", MAL_FORMAT_NOT_SUPPORTED);
    }

    pDevice->bufferSizeInFrames = actualBufferSize;
    pDevice->periods = pConfig->periods;



    // MMAP Mode
    //
    // Try using interleaved MMAP access. If this fails, fall back to standard readi/writei.
    pDevice->alsa.isUsingMMap = MAL_FALSE;
#ifdef MAL_ENABLE_EXPERIMENTAL_ALSA_MMAP
    if (snd_pcm_hw_params_set_access((snd_pcm_t*)pDevice->alsa.pPCM, pHWParams, SND_PCM_ACCESS_MMAP_INTERLEAVED) == 0) {
        pDevice->alsa.isUsingMMap = MAL_TRUE;
        mal_log(pDevice, "USING MMAP\n");
    }
#endif

    if (!pDevice->alsa.isUsingMMap) {
        if (snd_pcm_hw_params_set_access((snd_pcm_t*)pDevice->alsa.pPCM, pHWParams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {;
            mal_device_uninit__alsa(pDevice);
            return mal_post_error(pDevice, "[ALSA] Failed to set access mode to neither SND_PCM_ACCESS_MMAP_INTERLEAVED nor SND_PCM_ACCESS_RW_INTERLEAVED. snd_pcm_hw_params_set_access() failed.", MAL_FORMAT_NOT_SUPPORTED);
        }
    }


    // Apply hardware parameters.
    if (snd_pcm_hw_params((snd_pcm_t*)pDevice->alsa.pPCM, pHWParams) < 0) {
        mal_device_uninit__alsa(pDevice);
        return mal_post_error(pDevice, "[ALSA] Failed to set hardware parameters. snd_pcm_hw_params() failed.", MAL_ALSA_FAILED_TO_SET_SW_PARAMS);
    }



    snd_pcm_sw_params_t* pSWParams = NULL;
    snd_pcm_sw_params_alloca(&pSWParams);

    if (snd_pcm_sw_params_current((snd_pcm_t*)pDevice->alsa.pPCM, pSWParams) != 0) {
        mal_device_uninit__alsa(pDevice);
        return mal_post_error(pDevice, "[ALSA] Failed to initialize software parameters. snd_pcm_sw_params_current() failed.", MAL_ALSA_FAILED_TO_SET_SW_PARAMS);
    }

    if (snd_pcm_sw_params_set_avail_min((snd_pcm_t*)pDevice->alsa.pPCM, pSWParams, (pDevice->sampleRate/1000) * 1) != 0) {
        mal_device_uninit__alsa(pDevice);
        return mal_post_error(pDevice, "[ALSA] snd_pcm_sw_params_set_avail_min() failed.", MAL_FORMAT_NOT_SUPPORTED);
    }

    if (type == mal_device_type_playback) {
        if (snd_pcm_sw_params_set_start_threshold((snd_pcm_t*)pDevice->alsa.pPCM, pSWParams, (pDevice->sampleRate/1000) * 1) != 0) { //mal_prev_power_of_2(pDevice->bufferSizeInFrames/pDevice->periods)
            mal_device_uninit__alsa(pDevice);
            return mal_post_error(pDevice, "[ALSA] Failed to set start threshold for playback device. snd_pcm_sw_params_set_start_threshold() failed.", MAL_ALSA_FAILED_TO_SET_SW_PARAMS);
        }
    }

    if (snd_pcm_sw_params((snd_pcm_t*)pDevice->alsa.pPCM, pSWParams) != 0) {
        mal_device_uninit__alsa(pDevice);
        return mal_post_error(pDevice, "[ALSA] Failed to set software parameters. snd_pcm_sw_params() failed.", MAL_ALSA_FAILED_TO_SET_SW_PARAMS);
    }



    // If we're _not_ using mmap we need to use an intermediary buffer.
    if (!pDevice->alsa.isUsingMMap) {
        pDevice->alsa.pIntermediaryBuffer = mal_malloc(pDevice->bufferSizeInFrames * pDevice->channels * mal_get_sample_size_in_bytes(pDevice->format));
        if (pDevice->alsa.pIntermediaryBuffer == NULL) {
            mal_device_uninit__alsa(pDevice);
            return mal_post_error(pDevice, "[ALSA] Failed to set software parameters. snd_pcm_sw_params() failed.", MAL_OUT_OF_MEMORY);
        }
    }
    
    
    
    // Grab the internal channel map. For now we're not going to bother trying to change the channel map and
    // instead just do it ourselves.
    snd_pcm_chmap_t* pChmap = snd_pcm_get_chmap((snd_pcm_t*)pDevice->alsa.pPCM);
    if (pChmap != NULL) {
        mal_assert(pChmap->channels == pDevice->internalChannels);
        for (mal_uint32 iChannel = 0; iChannel < pDevice->internalChannels; ++iChannel) {
            pDevice->internalChannelMap[iChannel] = mal_convert_alsa_channel_position_to_mal_channel(pChmap->pos[iChannel]);
        }

        free(pChmap);
        pChmap = NULL;
    } else {
        // Could not retrieve the channel map. Fall back to a hard-coded assumption.
        if (pDevice->internalChannels == 1) {           // Mono
            pDevice->internalChannelMap[0] = MAL_CHANNEL_FRONT_CENTER;
        } else if (pDevice->internalChannels == 2) {    // Stereo
            pDevice->internalChannelMap[0] = MAL_CHANNEL_FRONT_LEFT;
            pDevice->internalChannelMap[1] = MAL_CHANNEL_FRONT_RIGHT;
        } else if (pDevice->internalChannels == 3) {    // 2.1
            pDevice->internalChannelMap[0] = MAL_CHANNEL_FRONT_LEFT;
            pDevice->internalChannelMap[1] = MAL_CHANNEL_FRONT_RIGHT;
            pDevice->internalChannelMap[2] = MAL_CHANNEL_LFE;
        } else if (pDevice->internalChannels == 4) {    // 4.0
            pDevice->internalChannelMap[0] = MAL_CHANNEL_FRONT_LEFT;
            pDevice->internalChannelMap[1] = MAL_CHANNEL_FRONT_RIGHT;
            pDevice->internalChannelMap[2] = MAL_CHANNEL_SIDE_LEFT;
            pDevice->internalChannelMap[3] = MAL_CHANNEL_SIDE_RIGHT;
        } else if (pDevice->internalChannels == 5) {    // Not sure about this one. 4.1?
            pDevice->internalChannelMap[0] = MAL_CHANNEL_FRONT_LEFT;
            pDevice->internalChannelMap[1] = MAL_CHANNEL_FRONT_RIGHT;
            pDevice->internalChannelMap[2] = MAL_CHANNEL_SIDE_LEFT;
            pDevice->internalChannelMap[3] = MAL_CHANNEL_SIDE_RIGHT;
            pDevice->internalChannelMap[4] = MAL_CHANNEL_LFE;
        } else if (pDevice->internalChannels == 6) {    // 5.1
            pDevice->internalChannelMap[0] = MAL_CHANNEL_FRONT_LEFT;
            pDevice->internalChannelMap[1] = MAL_CHANNEL_FRONT_RIGHT;
            pDevice->internalChannelMap[2] = MAL_CHANNEL_SIDE_LEFT;
            pDevice->internalChannelMap[3] = MAL_CHANNEL_SIDE_RIGHT;
            pDevice->internalChannelMap[4] = MAL_CHANNEL_FRONT_CENTER;
            pDevice->internalChannelMap[5] = MAL_CHANNEL_LFE;
        } else if (pDevice->internalChannels == 7) {    // Not sure about this one.
            pDevice->internalChannelMap[0] = MAL_CHANNEL_FRONT_LEFT;
            pDevice->internalChannelMap[1] = MAL_CHANNEL_FRONT_RIGHT;
            pDevice->internalChannelMap[2] = MAL_CHANNEL_SIDE_LEFT;
            pDevice->internalChannelMap[3] = MAL_CHANNEL_SIDE_RIGHT;
            pDevice->internalChannelMap[4] = MAL_CHANNEL_FRONT_CENTER;
            pDevice->internalChannelMap[5] = MAL_CHANNEL_LFE;
            pDevice->internalChannelMap[6] = MAL_CHANNEL_BACK_CENTER;
        } else {
            // I don't know what mapping to use in this case, but I'm making it upwards compatible with 7.1. Good luck!
            mal_assert(pDevice->internalChannels >= 8);
            pDevice->internalChannelMap[0] = MAL_CHANNEL_FRONT_LEFT;
            pDevice->internalChannelMap[1] = MAL_CHANNEL_FRONT_RIGHT;
            pDevice->internalChannelMap[2] = MAL_CHANNEL_SIDE_LEFT;
            pDevice->internalChannelMap[3] = MAL_CHANNEL_SIDE_RIGHT;
            pDevice->internalChannelMap[4] = MAL_CHANNEL_FRONT_CENTER;
            pDevice->internalChannelMap[5] = MAL_CHANNEL_LFE;
            pDevice->internalChannelMap[6] = MAL_CHANNEL_BACK_LEFT;
            pDevice->internalChannelMap[7] = MAL_CHANNEL_BACK_RIGHT;

            // Beyond 7.1 I'm just guessing...
            if (pDevice->internalChannels == 9) {
                pDevice->internalChannelMap[8] = MAL_CHANNEL_BACK_CENTER;
            } else if (pDevice->internalChannels == 10) {
                pDevice->internalChannelMap[8] = MAL_CHANNEL_FRONT_LEFT_CENTER;
                pDevice->internalChannelMap[9] = MAL_CHANNEL_FRONT_RIGHT_CENTER;
            } else if (pDevice->internalChannels == 11) {
                pDevice->internalChannelMap[ 8] = MAL_CHANNEL_FRONT_LEFT_CENTER;
                pDevice->internalChannelMap[ 9] = MAL_CHANNEL_FRONT_RIGHT_CENTER;
                pDevice->internalChannelMap[10] = MAL_CHANNEL_BACK_CENTER;
            } else {
                mal_assert(pDevice->internalChannels >= 12);
                for (mal_uint8 iChannel = 11; iChannel < pDevice->internalChannels; ++iChannel) {
                    pDevice->internalChannelMap[iChannel] = iChannel + 1;
                }
            }
        }
    }

    return MAL_SUCCESS;
}


static mal_result mal_device__start_backend__alsa(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    // Prepare the device first...
    snd_pcm_prepare((snd_pcm_t*)pDevice->alsa.pPCM);

    // ... and then grab an initial chunk from the client. After this is done, the device should
    // automatically start playing, since that's how we configured the software parameters.
    if (pDevice->type == mal_device_type_playback) {
        mal_device_write__alsa(pDevice);
    } else {
        snd_pcm_start((snd_pcm_t*)pDevice->alsa.pPCM);
    }

    return MAL_SUCCESS;
}

static mal_result mal_device__stop_backend__alsa(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    snd_pcm_drop((snd_pcm_t*)pDevice->alsa.pPCM);
    return MAL_SUCCESS;
}

static mal_result mal_device__break_main_loop__alsa(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    // Fallback. We just set a variable to tell the worker thread to terminate after handling the
    // next bunch of frames. This is a slow way of handling this.
    pDevice->alsa.breakFromMainLoop = MAL_TRUE;
    return MAL_SUCCESS;
}

static mal_result mal_device__main_loop__alsa(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    pDevice->alsa.breakFromMainLoop = MAL_FALSE;
    if (pDevice->type == mal_device_type_playback) {
        // Playback. Read from client, write to device.
        while (!pDevice->alsa.breakFromMainLoop && mal_device_write__alsa(pDevice)) {
        }
    } else {
        // Capture. Read from device, write to client.
        while (!pDevice->alsa.breakFromMainLoop && mal_device_read__alsa(pDevice)) {
        }
    }

    return MAL_SUCCESS;
}
#endif


///////////////////////////////////////////////////////////////////////////////
//
// OpenSL|ES Backend
//
///////////////////////////////////////////////////////////////////////////////
#ifdef MAL_ENABLE_OPENSLES
#include <SLES/OpenSLES.h>
#ifdef MAL_ANDROID
#include <SLES/OpenSLES_Android.h>
#endif

// Converts an individual OpenSL-style channel identifier (SPEAKER_FRONT_LEFT, etc.) to mini_al.
static mal_uint8 mal_channel_id_to_mal__opensl(SLuint32 id)
{
    switch (id)
    {
        case SL_SPEAKER_FRONT_LEFT:            return MAL_CHANNEL_FRONT_LEFT;
        case SL_SPEAKER_FRONT_RIGHT:           return MAL_CHANNEL_FRONT_RIGHT;
        case SL_SPEAKER_FRONT_CENTER:          return MAL_CHANNEL_FRONT_CENTER;
        case SL_SPEAKER_LOW_FREQUENCY:         return MAL_CHANNEL_LFE;
        case SL_SPEAKER_BACK_LEFT:             return MAL_CHANNEL_BACK_LEFT;
        case SL_SPEAKER_BACK_RIGHT:            return MAL_CHANNEL_BACK_RIGHT;
        case SL_SPEAKER_FRONT_LEFT_OF_CENTER:  return MAL_CHANNEL_FRONT_LEFT_CENTER;
        case SL_SPEAKER_FRONT_RIGHT_OF_CENTER: return MAL_CHANNEL_FRONT_RIGHT_CENTER;
        case SL_SPEAKER_BACK_CENTER:           return MAL_CHANNEL_BACK_CENTER;
        case SL_SPEAKER_SIDE_LEFT:             return MAL_CHANNEL_SIDE_LEFT;
        case SL_SPEAKER_SIDE_RIGHT:            return MAL_CHANNEL_SIDE_RIGHT;
        case SL_SPEAKER_TOP_CENTER:            return MAL_CHANNEL_TOP_CENTER;
        case SL_SPEAKER_TOP_FRONT_LEFT:        return MAL_CHANNEL_TOP_FRONT_LEFT;
        case SL_SPEAKER_TOP_FRONT_CENTER:      return MAL_CHANNEL_TOP_FRONT_CENTER;
        case SL_SPEAKER_TOP_FRONT_RIGHT:       return MAL_CHANNEL_TOP_FRONT_RIGHT;
        case SL_SPEAKER_TOP_BACK_LEFT:         return MAL_CHANNEL_TOP_BACK_LEFT;
        case SL_SPEAKER_TOP_BACK_CENTER:       return MAL_CHANNEL_TOP_BACK_CENTER;
        case SL_SPEAKER_TOP_BACK_RIGHT:        return MAL_CHANNEL_TOP_BACK_RIGHT;
        default: return 0;
    }
}

// Converts an individual mini_al channel identifier (MAL_CHANNEL_FRONT_LEFT, etc.) to OpenSL-style.
static SLuint32 mal_channel_id_to_opensl(mal_uint8 id)
{
    switch (id)
    {
        case MAL_CHANNEL_FRONT_LEFT:         return SL_SPEAKER_FRONT_LEFT;           
        case MAL_CHANNEL_FRONT_RIGHT:        return SL_SPEAKER_FRONT_RIGHT;          
        case MAL_CHANNEL_FRONT_CENTER:       return SL_SPEAKER_FRONT_CENTER;         
        case MAL_CHANNEL_LFE:                return SL_SPEAKER_LOW_FREQUENCY;        
        case MAL_CHANNEL_BACK_LEFT:          return SL_SPEAKER_BACK_LEFT;            
        case MAL_CHANNEL_BACK_RIGHT:         return SL_SPEAKER_BACK_RIGHT;           
        case MAL_CHANNEL_FRONT_LEFT_CENTER:  return SL_SPEAKER_FRONT_LEFT_OF_CENTER; 
        case MAL_CHANNEL_FRONT_RIGHT_CENTER: return SL_SPEAKER_FRONT_RIGHT_OF_CENTER;
        case MAL_CHANNEL_BACK_CENTER:        return SL_SPEAKER_BACK_CENTER;          
        case MAL_CHANNEL_SIDE_LEFT:          return SL_SPEAKER_SIDE_LEFT;            
        case MAL_CHANNEL_SIDE_RIGHT:         return SL_SPEAKER_SIDE_RIGHT;           
        case MAL_CHANNEL_TOP_CENTER:         return SL_SPEAKER_TOP_CENTER;           
        case MAL_CHANNEL_TOP_FRONT_LEFT:     return SL_SPEAKER_TOP_FRONT_LEFT;       
        case MAL_CHANNEL_TOP_FRONT_CENTER:   return SL_SPEAKER_TOP_FRONT_CENTER;     
        case MAL_CHANNEL_TOP_FRONT_RIGHT:    return SL_SPEAKER_TOP_FRONT_RIGHT;      
        case MAL_CHANNEL_TOP_BACK_LEFT:      return SL_SPEAKER_TOP_BACK_LEFT;        
        case MAL_CHANNEL_TOP_BACK_CENTER:    return SL_SPEAKER_TOP_BACK_CENTER;      
        case MAL_CHANNEL_TOP_BACK_RIGHT:     return SL_SPEAKER_TOP_BACK_RIGHT;       
        default: return 0;
    }
}

// Converts a channel mapping to an OpenSL-style channel mask.
static SLuint32 mal_channel_map_to_channel_mask__opensl(mal_uint8 channelMap[MAL_MAX_CHANNELS], mal_uint32 channels)
{
    SLuint32 channelMask = 0;
    for (mal_uint32 iChannel = 0; iChannel < channels; ++iChannel) {
        channelMask |= mal_channel_id_to_opensl(channelMap[iChannel]);
    }

    return channelMask;
}

// Converts an OpenSL-style channel mask to a mini_al channel map.
static void mal_channel_mask_to_channel_map__opensl(SLuint32 channelMask, mal_uint32 channels, mal_uint8 channelMap[MAL_MAX_CHANNELS])
{
    if (channels == 2 && channelMask == 0) {
        channelMap[0] = MAL_CHANNEL_FRONT_LEFT;
        channelMap[1] = MAL_CHANNEL_FRONT_RIGHT;
    } else {
        // Just iterate over each bit.
        mal_uint32 iChannel = 0;
        for (mal_uint32 iBit = 0; iBit < 32; ++iBit) {
            SLuint32 bitValue = (channelMask & (1 << iBit));
            if (bitValue != 0) {
                // The bit is set.
                channelMap[iChannel] = mal_channel_id_to_mal__opensl(bitValue);
                iChannel += 1;
            }
        }
    }
}

SLuint32 mal_round_to_standard_sample_rate__opensl(SLuint32 samplesPerSec)
{
    if (samplesPerSec <= SL_SAMPLINGRATE_8) {
        return SL_SAMPLINGRATE_8;
    }
    if (samplesPerSec <= SL_SAMPLINGRATE_11_025) {
        return SL_SAMPLINGRATE_11_025;
    }
    if (samplesPerSec <= SL_SAMPLINGRATE_12) {
        return SL_SAMPLINGRATE_12;
    }
    if (samplesPerSec <= SL_SAMPLINGRATE_16) {
        return SL_SAMPLINGRATE_16;
    }
    if (samplesPerSec <= SL_SAMPLINGRATE_22_05) {
        return SL_SAMPLINGRATE_22_05;
    }
    if (samplesPerSec <= SL_SAMPLINGRATE_24) {
        return SL_SAMPLINGRATE_24;
    }
    if (samplesPerSec <= SL_SAMPLINGRATE_32) {
        return SL_SAMPLINGRATE_32;
    }
    if (samplesPerSec <= SL_SAMPLINGRATE_44_1) {
        return SL_SAMPLINGRATE_44_1;
    }
    if (samplesPerSec <= SL_SAMPLINGRATE_48) {
        return SL_SAMPLINGRATE_48;
    }

    // Android doesn't support more than 48000.
#ifndef MAL_ANDROID
    if (samplesPerSec <= SL_SAMPLINGRATE_64) {
        return SL_SAMPLINGRATE_64;
    }
    if (samplesPerSec <= SL_SAMPLINGRATE_88_2) {
        return SL_SAMPLINGRATE_88_2;
    }
    if (samplesPerSec <= SL_SAMPLINGRATE_96) {
        return SL_SAMPLINGRATE_96;
    }
    if (samplesPerSec <= SL_SAMPLINGRATE_192) {
        return SL_SAMPLINGRATE_192;
    }
#endif

    return SL_SAMPLINGRATE_16;
}

mal_result mal_context_init__opensl(mal_context* pContext)
{
    mal_assert(pContext != NULL);

    (void)pContext;
    return MAL_SUCCESS;
}

mal_result mal_context_uninit__opensl(mal_context* pContext)
{
    mal_assert(pContext != NULL);
    mal_assert(pContext->backend == mal_backend_opensl);

    (void)pContext;
    return MAL_SUCCESS;
}

mal_result mal_enumerate_devices__opensl(mal_context* pContext, mal_device_type type, mal_uint32* pCount, mal_device_info* pInfo)
{
    (void)pContext;

    mal_uint32 infoSize = *pCount;
    *pCount = 0;

    SLObjectItf engineObj;
    SLresult resultSL = slCreateEngine(&engineObj, 0, NULL, 0, NULL, NULL);
    if (resultSL != SL_RESULT_SUCCESS) {
        return MAL_NO_BACKEND;
    }

    (*engineObj)->Realize(engineObj, SL_BOOLEAN_FALSE);

    // TODO: Test Me.
    //
    // This is currently untested, so for now we are just returning default devices.
#if 0
    SLuint32 pDeviceIDs[128];
    SLint32 deviceCount = sizeof(pDeviceIDs) / sizeof(pDeviceIDs[0]);

    SLAudioIODeviceCapabilitiesItf deviceCaps;
    resultSL = (*engineObj)->GetInterface(engineObj, SL_IID_AUDIOIODEVICECAPABILITIES, &deviceCaps);
    if (resultSL != SL_RESULT_SUCCESS) {
        // The interface may not be supported so just report a default device.
        (*engineObj)->Destroy(engineObj);
        goto return_default_device;
    }

    if (type == mal_device_type_playback) {
        resultSL = (*deviceCaps)->GetAvailableAudioOutputs(deviceCaps, &deviceCount, pDeviceIDs);
        if (resultSL != SL_RESULT_SUCCESS) {
            (*engineObj)->Destroy(engineObj);
            return MAL_NO_DEVICE;
        }
    } else {
        resultSL = (*deviceCaps)->GetAvailableAudioInputs(deviceCaps, &deviceCount, pDeviceIDs);
        if (resultSL != SL_RESULT_SUCCESS) {
            (*engineObj)->Destroy(engineObj);
            return MAL_NO_DEVICE;
        }
    }

    for (SLint32 iDevice = 0; iDevice < deviceCount; ++iDevice) {
        if (pInfo != NULL) {
            if (infoSize > 0) {
                mal_zero_object(pInfo);
                pInfo->id.opensl = pDeviceIDs[iDevice];

                mal_bool32 isValidDevice = MAL_TRUE;
                if (type == mal_device_type_playback) {
                    SLAudioOutputDescriptor desc;
                    resultSL = (*deviceCaps)->QueryAudioOutputCapabilities(deviceCaps, pInfo->id.opensl, &desc);
                    if (resultSL != SL_RESULT_SUCCESS) {
                        isValidDevice = MAL_FALSE;
                    }

                    mal_strncpy_s(pInfo->name, sizeof(pInfo->name), (const char*)desc.pDeviceName, (size_t)-1);
                } else {
                    SLAudioInputDescriptor desc;
                    resultSL = (*deviceCaps)->QueryAudioInputCapabilities(deviceCaps, pInfo->id.opensl, &desc);
                    if (resultSL != SL_RESULT_SUCCESS) {
                        isValidDevice = MAL_FALSE;
                    }

                    mal_strncpy_s(pInfo->name, sizeof(pInfo->name), (const char*)desc.deviceName, (size_t)-1);
                }

                if (isValidDevice) {
                    pInfo += 1;
                    infoSize -= 1;
                    *pCount += 1;
                }
            }
        } else {
            *pCount += 1;
        }
    }

    (*engineObj)->Destroy(engineObj);
    return MAL_SUCCESS;
#else
    (*engineObj)->Destroy(engineObj);
    goto return_default_device;
#endif

return_default_device:
    *pCount = 1;
    if (pInfo != NULL) {
        if (infoSize > 0) {
            if (type == mal_device_type_playback) {
                pInfo->id.opensl = SL_DEFAULTDEVICEID_AUDIOOUTPUT;
                mal_strncpy_s(pInfo->name, sizeof(pInfo->name), "Default Playback Device", (size_t)-1);
            } else {
                pInfo->id.opensl = SL_DEFAULTDEVICEID_AUDIOINPUT;
                mal_strncpy_s(pInfo->name, sizeof(pInfo->name), "Default Capture Device", (size_t)-1);
            }
        }
    }

    return MAL_SUCCESS;
}


// OpenSL|ES has one-per-application objects :(
static SLObjectItf g_malEngineObjectSL = NULL;
static SLEngineItf g_malEngineSL = NULL;
static mal_uint32 g_malOpenSLInitCounter = 0;

#define MAL_OPENSL_OBJ(p)         (*((SLObjectItf)(p)))
#define MAL_OPENSL_OUTPUTMIX(p)   (*((SLOutputMixItf)(p)))
#define MAL_OPENSL_PLAY(p)        (*((SLPlayItf)(p)))
#define MAL_OPENSL_RECORD(p)      (*((SLRecordItf)(p)))

#ifdef MAL_ANDROID
#define MAL_OPENSL_BUFFERQUEUE(p) (*((SLAndroidSimpleBufferQueueItf)(p)))
#else
#define MAL_OPENSL_BUFFERQUEUE(p) (*((SLBufferQueueItf)(p)))
#endif

#ifdef MAL_ANDROID
//static void mal_buffer_queue_callback__opensl_android(SLAndroidSimpleBufferQueueItf pBufferQueue, SLuint32 eventFlags, const void* pBuffer, SLuint32 bufferSize, SLuint32 dataUsed, void* pContext)
static void mal_buffer_queue_callback__opensl_android(SLAndroidSimpleBufferQueueItf pBufferQueue, void* pUserData)
{
    (void)pBufferQueue;

    // For now, only supporting Android implementations of OpenSL|ES since that's the only one I've
    // been able to test with and I currently depend on Android-specific extensions (simple buffer
    // queues).
#ifndef MAL_ANDROID
    return MAL_NO_BACKEND;
#endif

    mal_device* pDevice = (mal_device*)pUserData;
    mal_assert(pDevice != NULL);

    // For now, don't do anything unless the buffer was fully processed. From what I can tell, it looks like
    // OpenSL|ES 1.1 improves on buffer queues to the point that we could much more intelligently handle this,
    // but unfortunately it looks like Android is only supporting OpenSL|ES 1.0.1 for now :(
    if (pDevice->state != MAL_STATE_STARTED) {
        return;
    }

    size_t periodSizeInBytes = pDevice->opensl.periodSizeInFrames * pDevice->internalChannels * mal_get_sample_size_in_bytes(pDevice->internalFormat);
    mal_uint8* pBuffer = pDevice->opensl.pBuffer + (pDevice->opensl.currentBufferIndex * periodSizeInBytes);

    if (pDevice->type == mal_device_type_playback) {
        if (pDevice->state != MAL_STATE_STARTED) {
            return;
        }

        mal_device__read_frames_from_client(pDevice, pDevice->opensl.periodSizeInFrames, pBuffer);

        SLresult resultSL = MAL_OPENSL_BUFFERQUEUE(pDevice->opensl.pBufferQueue)->Enqueue((SLAndroidSimpleBufferQueueItf)pDevice->opensl.pBufferQueue, pBuffer, periodSizeInBytes);
        if (resultSL != SL_RESULT_SUCCESS) {
            return;
        }
    } else {
        mal_device__send_frames_to_client(pDevice, pDevice->opensl.periodSizeInFrames, pBuffer);

        SLresult resultSL = MAL_OPENSL_BUFFERQUEUE(pDevice->opensl.pBufferQueue)->Enqueue((SLAndroidSimpleBufferQueueItf)pDevice->opensl.pBufferQueue, pBuffer, periodSizeInBytes);
        if (resultSL != SL_RESULT_SUCCESS) {
            return;
        }
    }

    pDevice->opensl.currentBufferIndex = (pDevice->opensl.currentBufferIndex + 1) % pDevice->periods;
}
#endif

static void mal_device_uninit__opensl(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    // Uninit device.
    if (pDevice->type == mal_device_type_playback) {
        if (pDevice->opensl.pAudioPlayerObj) MAL_OPENSL_OBJ(pDevice->opensl.pAudioPlayerObj)->Destroy((SLObjectItf)pDevice->opensl.pAudioPlayerObj);
        if (pDevice->opensl.pOutputMixObj) MAL_OPENSL_OBJ(pDevice->opensl.pOutputMixObj)->Destroy((SLObjectItf)pDevice->opensl.pOutputMixObj);
    } else {
        if (pDevice->opensl.pAudioRecorderObj) MAL_OPENSL_OBJ(pDevice->opensl.pAudioRecorderObj)->Destroy((SLObjectItf)pDevice->opensl.pAudioRecorderObj);
    }

    mal_free(pDevice->opensl.pBuffer);
    

    // Uninit global data.
    if (g_malOpenSLInitCounter > 0) {
        if (mal_atomic_decrement_32(&g_malOpenSLInitCounter) == 0) {
            (*g_malEngineObjectSL)->Destroy(g_malEngineObjectSL);
        }
    }
}

static mal_result mal_device_init__opensl(mal_context* pContext, mal_device_type type, mal_device_id* pDeviceID, mal_device_config* pConfig, mal_device* pDevice)
{
    (void)pContext;

    // For now, only supporting Android implementations of OpenSL|ES since that's the only one I've
    // been able to test with and I currently depend on Android-specific extensions (simple buffer
    // queues).
#ifndef MAL_ANDROID
    return MAL_NO_BACKEND;
#endif

    // Use s32 as the internal format for when floating point is specified.
    if (pConfig->format == mal_format_f32) {
        pDevice->internalFormat = mal_format_s32;
    }

    // Initialize global data first if applicable.
    if (mal_atomic_increment_32(&g_malOpenSLInitCounter) == 1) {
        SLresult resultSL = slCreateEngine(&g_malEngineObjectSL, 0, NULL, 0, NULL, NULL);
        if (resultSL != SL_RESULT_SUCCESS) {
            mal_atomic_decrement_32(&g_malOpenSLInitCounter);
            return mal_post_error(pDevice, "slCreateEngine() failed.", MAL_NO_BACKEND);
        }

        (*g_malEngineObjectSL)->Realize(g_malEngineObjectSL, SL_BOOLEAN_FALSE);

        resultSL = (*g_malEngineObjectSL)->GetInterface(g_malEngineObjectSL, SL_IID_ENGINE, &g_malEngineSL);
        if (resultSL != SL_RESULT_SUCCESS) {
            (*g_malEngineObjectSL)->Destroy(g_malEngineObjectSL);
            mal_atomic_decrement_32(&g_malOpenSLInitCounter);
            return mal_post_error(pDevice, "Failed to retrieve SL_IID_ENGINE interface.", MAL_NO_BACKEND);
        }
    }


    // Now we can start initializing the device properly.
    mal_assert(pDevice != NULL);
    mal_zero_object(&pDevice->opensl);

    pDevice->opensl.currentBufferIndex = 0;
    pDevice->opensl.periodSizeInFrames = pConfig->bufferSizeInFrames / pConfig->periods;
    pDevice->bufferSizeInFrames = pDevice->opensl.periodSizeInFrames * pConfig->periods;

    SLDataLocator_AndroidSimpleBufferQueue queue;
    queue.locatorType = SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE;
    queue.numBuffers = pConfig->periods;

    SLDataFormat_PCM* pFormat = NULL;

#if defined(MAL_ANDROID) && __ANDROID_API__ >= 21
    SLAndroidDataFormat_PCM_EX pcmEx;
    if (pDevice->format == mal_format_f32 /*|| pDevice->format == mal_format_f64*/) {
        pcmEx.formatType = SL_ANDROID_DATAFORMAT_PCM_EX;
        pcmEx.representation = SL_ANDROID_PCM_REPRESENTATION_FLOAT;
    } else {
        pcmEx.formatType = SL_DATAFORMAT_PCM;
    }
    pFormat = (SLDataFormat_PCM*)&pcmEx;
#else
    SLDataFormat_PCM pcm;
    pcm.formatType = SL_DATAFORMAT_PCM;
    pFormat = &pcm;
#endif

    pFormat->numChannels   = pDevice->channels;
    pFormat->samplesPerSec = mal_round_to_standard_sample_rate__opensl(pDevice->sampleRate * 1000);  // In millihertz.
    pFormat->bitsPerSample = mal_get_sample_size_in_bytes(pDevice->format)*8;
    pFormat->containerSize = pFormat->bitsPerSample;  // Always tightly packed for now.
    pFormat->channelMask   = mal_channel_map_to_channel_mask__opensl(pConfig->channelMap, pFormat->numChannels);
    pFormat->endianness    = SL_BYTEORDER_LITTLEENDIAN;

    // Android has a few restrictions on the format as documented here: https://developer.android.com/ndk/guides/audio/opensl-for-android.html
    //  - Only mono and stereo is supported.
    //  - Only u8 and s16 formats are supported.
    //  - Limited to a sample rate of 48000.
#ifdef MAL_ANDROID
    if (pFormat->numChannels > 2) {
        pFormat->numChannels = 2;
    }
#if __ANDROID_API__ >= 21
    if (pFormat->formatType == SL_ANDROID_DATAFORMAT_PCM_EX) {
        // It's floating point.
        mal_assert(pcmEx.representation == SL_ANDROID_PCM_REPRESENTATION_FLOAT);
        if (pFormat->bitsPerSample > 32) {
            pFormat->bitsPerSample = 32;
        }
    } else {
        if (pFormat->bitsPerSample > 16) {
            pFormat->bitsPerSample = 16;
        }
    }
#else
    if (pFormat->bitsPerSample > 16) {
        pFormat->bitsPerSample = 16;
    }
#endif
    pFormat->containerSize = pFormat->bitsPerSample;  // Always tightly packed for now.

    if (pFormat->samplesPerSec > SL_SAMPLINGRATE_48) {
        pFormat->samplesPerSec = SL_SAMPLINGRATE_48;
    }
#endif

    if (type == mal_device_type_playback) {
        SLresult resultSL = (*g_malEngineSL)->CreateOutputMix(g_malEngineSL, (SLObjectItf*)&pDevice->opensl.pOutputMixObj, 0, NULL, NULL);
        if (resultSL != SL_RESULT_SUCCESS) {
            mal_device_uninit__opensl(pDevice);
            return mal_post_error(pDevice, "Failed to create output mix.", MAL_NO_BACKEND);
        }

        if (MAL_OPENSL_OBJ(pDevice->opensl.pOutputMixObj)->Realize((SLObjectItf)pDevice->opensl.pOutputMixObj, SL_BOOLEAN_FALSE)) {
            mal_device_uninit__opensl(pDevice);
            return mal_post_error(pDevice, "Failed to realize output mix object.", MAL_NO_BACKEND);
        }

        if (MAL_OPENSL_OBJ(pDevice->opensl.pOutputMixObj)->GetInterface((SLObjectItf)pDevice->opensl.pOutputMixObj, SL_IID_OUTPUTMIX, &pDevice->opensl.pOutputMix) != SL_RESULT_SUCCESS) {
            mal_device_uninit__opensl(pDevice);
            return mal_post_error(pDevice, "Failed to retrieve SL_IID_OUTPUTMIX interface.", MAL_NO_BACKEND);
        }

        // Set the output device.
        if (pDeviceID != NULL) {
            MAL_OPENSL_OUTPUTMIX(pDevice->opensl.pOutputMix)->ReRoute((SLOutputMixItf)pDevice->opensl.pOutputMix, 1, &pDeviceID->opensl);
        }

        SLDataSource source;
        source.pLocator = &queue;
        source.pFormat = pFormat;

        SLDataLocator_OutputMix outmixLocator;
        outmixLocator.locatorType = SL_DATALOCATOR_OUTPUTMIX;
        outmixLocator.outputMix = (SLObjectItf)pDevice->opensl.pOutputMixObj;

        SLDataSink sink;
        sink.pLocator = &outmixLocator;
        sink.pFormat = NULL;

        const SLInterfaceID itfIDs1[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
        const SLboolean itfIDsRequired1[] = {SL_BOOLEAN_TRUE};
        resultSL = (*g_malEngineSL)->CreateAudioPlayer(g_malEngineSL, (SLObjectItf*)&pDevice->opensl.pAudioPlayerObj, &source, &sink, 1, itfIDs1, itfIDsRequired1);
        if (resultSL == SL_RESULT_CONTENT_UNSUPPORTED) {
            // Unsupported format. Fall back to something safer and try again. If this fails, just abort.
            pFormat->formatType = SL_DATAFORMAT_PCM;
            pFormat->numChannels = 2;
            pFormat->samplesPerSec = SL_SAMPLINGRATE_16;
            pFormat->bitsPerSample = 16;
            pFormat->containerSize = pFormat->bitsPerSample;  // Always tightly packed for now.
            pFormat->channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
            resultSL = (*g_malEngineSL)->CreateAudioPlayer(g_malEngineSL, (SLObjectItf*)&pDevice->opensl.pAudioPlayerObj, &source, &sink, 1, itfIDs1, itfIDsRequired1);
        }

        if (resultSL != SL_RESULT_SUCCESS) {
            mal_device_uninit__opensl(pDevice);
            return mal_post_error(pDevice, "Failed to create audio player.", MAL_NO_BACKEND);
        }


        if (MAL_OPENSL_OBJ(pDevice->opensl.pAudioPlayerObj)->Realize((SLObjectItf)pDevice->opensl.pAudioPlayerObj, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS) {
            mal_device_uninit__opensl(pDevice);
            return mal_post_error(pDevice, "Failed to realize audio player.", MAL_NO_BACKEND);
        }

        if (MAL_OPENSL_OBJ(pDevice->opensl.pAudioPlayerObj)->GetInterface((SLObjectItf)pDevice->opensl.pAudioPlayerObj, SL_IID_PLAY, &pDevice->opensl.pAudioPlayer) != SL_RESULT_SUCCESS) {
            mal_device_uninit__opensl(pDevice);
            return mal_post_error(pDevice, "Failed to retrieve SL_IID_PLAY interface.", MAL_NO_BACKEND);
        }

        if (MAL_OPENSL_OBJ(pDevice->opensl.pAudioPlayerObj)->GetInterface((SLObjectItf)pDevice->opensl.pAudioPlayerObj, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &pDevice->opensl.pBufferQueue) != SL_RESULT_SUCCESS) {
            mal_device_uninit__opensl(pDevice);
            return mal_post_error(pDevice, "Failed to retrieve SL_IID_ANDROIDSIMPLEBUFFERQUEUE interface.", MAL_NO_BACKEND);
        }

        if (MAL_OPENSL_BUFFERQUEUE(pDevice->opensl.pBufferQueue)->RegisterCallback((SLAndroidSimpleBufferQueueItf)pDevice->opensl.pBufferQueue, mal_buffer_queue_callback__opensl_android, pDevice) != SL_RESULT_SUCCESS) {
            mal_device_uninit__opensl(pDevice);
            return mal_post_error(pDevice, "Failed to register buffer queue callback.", MAL_NO_BACKEND);
        }
    } else {
        SLDataLocator_IODevice locatorDevice;
        locatorDevice.locatorType = SL_DATALOCATOR_IODEVICE;
        locatorDevice.deviceType = SL_IODEVICE_AUDIOINPUT;
        locatorDevice.deviceID = (pDeviceID == NULL) ? SL_DEFAULTDEVICEID_AUDIOINPUT : pDeviceID->opensl;
        locatorDevice.device = NULL;

        SLDataSource source;
        source.pLocator = &locatorDevice;
        source.pFormat = NULL;

        SLDataSink sink;
        sink.pLocator = &queue;
        sink.pFormat = pFormat;

        const SLInterfaceID itfIDs1[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
        const SLboolean itfIDsRequired1[] = {SL_BOOLEAN_TRUE};
        SLresult resultSL = (*g_malEngineSL)->CreateAudioRecorder(g_malEngineSL, (SLObjectItf*)&pDevice->opensl.pAudioRecorderObj, &source, &sink, 1, itfIDs1, itfIDsRequired1);
        if (resultSL == SL_RESULT_CONTENT_UNSUPPORTED) {
            // Unsupported format. Fall back to something safer and try again. If this fails, just abort.
            pFormat->formatType = SL_DATAFORMAT_PCM;
            pFormat->numChannels = 1;
            pFormat->samplesPerSec = SL_SAMPLINGRATE_16;
            pFormat->bitsPerSample = 16;
            pFormat->containerSize = pFormat->bitsPerSample;  // Always tightly packed for now.
            pFormat->channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
            resultSL = (*g_malEngineSL)->CreateAudioRecorder(g_malEngineSL, (SLObjectItf*)&pDevice->opensl.pAudioRecorderObj, &source, &sink, 1, itfIDs1, itfIDsRequired1);
        }

        if (resultSL != SL_RESULT_SUCCESS) {
            mal_device_uninit__opensl(pDevice);
            return mal_post_error(pDevice, "Failed to create audio recorder.", MAL_NO_BACKEND);
        }

        if (MAL_OPENSL_OBJ(pDevice->opensl.pAudioRecorderObj)->Realize((SLObjectItf)pDevice->opensl.pAudioRecorderObj, SL_BOOLEAN_FALSE) != SL_RESULT_SUCCESS) {
            mal_device_uninit__opensl(pDevice);
            return mal_post_error(pDevice, "Failed to realize audio recorder.", MAL_NO_BACKEND);
        }

        if (MAL_OPENSL_OBJ(pDevice->opensl.pAudioRecorderObj)->GetInterface((SLObjectItf)pDevice->opensl.pAudioRecorderObj, SL_IID_RECORD, &pDevice->opensl.pAudioRecorder) != SL_RESULT_SUCCESS) {
            mal_device_uninit__opensl(pDevice);
            return mal_post_error(pDevice, "Failed to retrieve SL_IID_RECORD interface.", MAL_NO_BACKEND);
        }

        if (MAL_OPENSL_OBJ(pDevice->opensl.pAudioRecorderObj)->GetInterface((SLObjectItf)pDevice->opensl.pAudioRecorderObj, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &pDevice->opensl.pBufferQueue) != SL_RESULT_SUCCESS) {
            mal_device_uninit__opensl(pDevice);
            return mal_post_error(pDevice, "Failed to retrieve SL_IID_ANDROIDSIMPLEBUFFERQUEUE interface.", MAL_NO_BACKEND);
        }

        if (MAL_OPENSL_BUFFERQUEUE(pDevice->opensl.pBufferQueue)->RegisterCallback((SLAndroidSimpleBufferQueueItf)pDevice->opensl.pBufferQueue, mal_buffer_queue_callback__opensl_android, pDevice) != SL_RESULT_SUCCESS) {
            mal_device_uninit__opensl(pDevice);
            return mal_post_error(pDevice, "Failed to register buffer queue callback.", MAL_NO_BACKEND);
        }
    }


    // The internal format is determined by the pFormat object.
    mal_bool32 isFloatingPoint = MAL_FALSE;
#if defined(MAL_ANDROID) && __ANDROID_API__ >= 21
    if (pFormat->formatType == SL_ANDROID_DATAFORMAT_PCM_EX) {
        mal_assert(pcmEx.representation == SL_ANDROID_PCM_REPRESENTATION_FLOAT);
        isFloatingPoint = MAL_TRUE;
    }
#endif
    if (isFloatingPoint) {
        if (pFormat->bitsPerSample == 32) {
            pDevice->internalFormat = mal_format_f32;
        }
#if 0
        if (pFormat->bitsPerSample == 64) {
            pDevice->internalFormat = mal_format_f64;
        }
#endif
    } else {
        if (pFormat->bitsPerSample == 8) {
            pDevice->internalFormat = mal_format_u8;
        } else if (pFormat->bitsPerSample == 16) {
            pDevice->internalFormat = mal_format_s16;
        } else if (pFormat->bitsPerSample == 24) {
            pDevice->internalFormat = mal_format_s24;
        } else if (pFormat->bitsPerSample == 32) {
            pDevice->internalFormat = mal_format_s32;
        }
    }

    pDevice->internalChannels = pFormat->numChannels;
    pDevice->internalSampleRate = pFormat->samplesPerSec / 1000;
    mal_channel_mask_to_channel_map__opensl(pFormat->channelMask, pDevice->internalChannels, pDevice->internalChannelMap);


    size_t bufferSizeInBytes = pDevice->bufferSizeInFrames * pDevice->internalChannels * mal_get_sample_size_in_bytes(pDevice->internalFormat);
    pDevice->opensl.pBuffer = (mal_uint8*)mal_malloc(bufferSizeInBytes);
    if (pDevice->opensl.pBuffer == NULL) {
        mal_device_uninit__opensl(pDevice);
        return mal_post_error(pDevice, "Failed to allocate memory for data buffer.", MAL_OUT_OF_MEMORY);
    }

    mal_zero_memory(pDevice->opensl.pBuffer, bufferSizeInBytes);

    return MAL_SUCCESS;
}

static mal_result mal_device__start_backend__opensl(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    if (pDevice->type == mal_device_type_playback) {
        SLresult resultSL = MAL_OPENSL_PLAY(pDevice->opensl.pAudioPlayer)->SetPlayState((SLPlayItf)pDevice->opensl.pAudioPlayer, SL_PLAYSTATE_PLAYING);
        if (resultSL != SL_RESULT_SUCCESS) {
            return MAL_FAILED_TO_START_BACKEND_DEVICE;
        }

        // We need to enqueue a buffer for each period.
        mal_device__read_frames_from_client(pDevice, pDevice->bufferSizeInFrames, pDevice->opensl.pBuffer);

        size_t periodSizeInBytes = pDevice->opensl.periodSizeInFrames * pDevice->internalChannels * mal_get_sample_size_in_bytes(pDevice->internalFormat);
        for (mal_uint32 iPeriod = 0; iPeriod < pDevice->periods; ++iPeriod) {
            resultSL = MAL_OPENSL_BUFFERQUEUE(pDevice->opensl.pBufferQueue)->Enqueue((SLAndroidSimpleBufferQueueItf)pDevice->opensl.pBufferQueue, pDevice->opensl.pBuffer + (periodSizeInBytes * iPeriod), periodSizeInBytes);
            if (resultSL != SL_RESULT_SUCCESS) {
                MAL_OPENSL_PLAY(pDevice->opensl.pAudioPlayer)->SetPlayState((SLPlayItf)pDevice->opensl.pAudioPlayer, SL_PLAYSTATE_STOPPED);
                return MAL_FAILED_TO_START_BACKEND_DEVICE;
            }
        }
    } else {
        SLresult resultSL = MAL_OPENSL_RECORD(pDevice->opensl.pAudioRecorder)->SetRecordState((SLRecordItf)pDevice->opensl.pAudioRecorder, SL_RECORDSTATE_RECORDING);
        if (resultSL != SL_RESULT_SUCCESS) {
            return MAL_FAILED_TO_START_BACKEND_DEVICE;
        }

        size_t periodSizeInBytes = pDevice->opensl.periodSizeInFrames * pDevice->internalChannels * mal_get_sample_size_in_bytes(pDevice->internalFormat);
        for (mal_uint32 iPeriod = 0; iPeriod < pDevice->periods; ++iPeriod) {
            resultSL = MAL_OPENSL_BUFFERQUEUE(pDevice->opensl.pBufferQueue)->Enqueue((SLAndroidSimpleBufferQueueItf)pDevice->opensl.pBufferQueue, pDevice->opensl.pBuffer + (periodSizeInBytes * iPeriod), periodSizeInBytes);
            if (resultSL != SL_RESULT_SUCCESS) {
                MAL_OPENSL_RECORD(pDevice->opensl.pAudioRecorder)->SetRecordState((SLRecordItf)pDevice->opensl.pAudioRecorder, SL_RECORDSTATE_STOPPED);
                return MAL_FAILED_TO_START_BACKEND_DEVICE;
            }
        }
    }

    return MAL_SUCCESS;
}

static mal_result mal_device__stop_backend__opensl(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    if (pDevice->type == mal_device_type_playback) {
        SLresult resultSL = MAL_OPENSL_PLAY(pDevice->opensl.pAudioPlayer)->SetPlayState((SLPlayItf)pDevice->opensl.pAudioPlayer, SL_PLAYSTATE_STOPPED);
        if (resultSL != SL_RESULT_SUCCESS) {
            return MAL_FAILED_TO_STOP_BACKEND_DEVICE;
        }
    } else {
        SLresult resultSL = MAL_OPENSL_RECORD(pDevice->opensl.pAudioRecorder)->SetRecordState((SLRecordItf)pDevice->opensl.pAudioRecorder, SL_RECORDSTATE_STOPPED);
        if (resultSL != SL_RESULT_SUCCESS) {
            return MAL_FAILED_TO_STOP_BACKEND_DEVICE;
        }
    }

    // Make sure any queued buffers are cleared.
    MAL_OPENSL_BUFFERQUEUE(pDevice->opensl.pBufferQueue)->Clear((SLAndroidSimpleBufferQueueItf)pDevice->opensl.pBufferQueue);

    // Make sure the client is aware that the device has stopped. There may be an OpenSL|ES callback for this, but I haven't found it.
    mal_device__set_state(pDevice, MAL_STATE_STOPPED);
    if (pDevice->onStop) {
        pDevice->onStop(pDevice);
    }

    return MAL_SUCCESS;
}
#endif  // OpenSL|ES

///////////////////////////////////////////////////////////////////////////////
//
// OpenAL Backend
//
///////////////////////////////////////////////////////////////////////////////
#ifdef MAL_ENABLE_OPENAL
#ifdef MAL_WIN32
#define MAL_AL_APIENTRY __cdecl
#else
#define MAL_AL_APIENTRY
#endif

typedef struct mal_ALCdevice_struct  mal_ALCdevice;
typedef struct mal_ALCcontext_struct mal_ALCcontext;
typedef char                         mal_ALCboolean;
typedef char                         mal_ALCchar;
typedef signed char                  mal_ALCbyte;
typedef unsigned char                mal_ALCubyte;
typedef short                        mal_ALCshort;
typedef unsigned short               mal_ALCushort;
typedef int                          mal_ALCint;
typedef unsigned int                 mal_ALCuint;
typedef int                          mal_ALCsizei;
typedef int                          mal_ALCenum;
typedef float                        mal_ALCfloat;
typedef double                       mal_ALCdouble;
typedef void                         mal_ALCvoid;

typedef mal_ALCboolean               mal_ALboolean;
typedef mal_ALCchar                  mal_ALchar;
typedef mal_ALCbyte                  mal_ALbyte;
typedef mal_ALCubyte                 mal_ALubyte;
typedef mal_ALCshort                 mal_ALshort;
typedef mal_ALCushort                mal_ALushort;
typedef mal_ALCint                   mal_ALint;
typedef mal_ALCuint                  mal_ALuint;
typedef mal_ALCsizei                 mal_ALsizei;
typedef mal_ALCenum                  mal_ALenum;
typedef mal_ALCfloat                 mal_ALfloat;
typedef mal_ALCdouble                mal_ALdouble;
typedef mal_ALCvoid                  mal_ALvoid;

#define MAL_ALC_DEVICE_SPECIFIER            0x1005
#define MAL_ALC_CAPTURE_DEVICE_SPECIFIER    0x310
#define MAL_ALC_CAPTURE_SAMPLES             0x312

#define MAL_AL_SOURCE_STATE                 0x1010
#define MAL_AL_INITIAL                      0x1011
#define MAL_AL_PLAYING                      0x1012
#define MAL_AL_PAUSED                       0x1013
#define MAL_AL_STOPPED                      0x1014
#define MAL_AL_BUFFERS_PROCESSED            0x1016

#define MAL_AL_FORMAT_MONO8                 0x1100
#define MAL_AL_FORMAT_MONO16                0x1101
#define MAL_AL_FORMAT_STEREO8               0x1102
#define MAL_AL_FORMAT_STEREO16              0x1103
#define MAL_AL_FORMAT_MONO_FLOAT32          0x10010
#define MAL_AL_FORMAT_STEREO_FLOAT32        0x10011
#define MAL_AL_FORMAT_51CHN16               0x120B
#define MAL_AL_FORMAT_51CHN32               0x120C
#define MAL_AL_FORMAT_51CHN8                0x120A
#define MAL_AL_FORMAT_61CHN16               0x120E
#define MAL_AL_FORMAT_61CHN32               0x120F
#define MAL_AL_FORMAT_61CHN8                0x120D
#define MAL_AL_FORMAT_71CHN16               0x1211
#define MAL_AL_FORMAT_71CHN32               0x1212
#define MAL_AL_FORMAT_71CHN8                0x1210
#define MAL_AL_FORMAT_QUAD16                0x1205
#define MAL_AL_FORMAT_QUAD32                0x1206
#define MAL_AL_FORMAT_QUAD8                 0x1204
#define MAL_AL_FORMAT_REAR16                0x1208
#define MAL_AL_FORMAT_REAR32                0x1209
#define MAL_AL_FORMAT_REAR8                 0x1207

typedef mal_ALCcontext*    (MAL_AL_APIENTRY * MAL_LPALCCREATECONTEXT)      (mal_ALCdevice *device, const mal_ALCint *attrlist);
typedef mal_ALCboolean     (MAL_AL_APIENTRY * MAL_LPALCMAKECONTEXTCURRENT) (mal_ALCcontext *context);
typedef void               (MAL_AL_APIENTRY * MAL_LPALCPROCESSCONTEXT)     (mal_ALCcontext *context);
typedef void               (MAL_AL_APIENTRY * MAL_LPALCSUSPENDCONTEXT)     (mal_ALCcontext *context);
typedef void               (MAL_AL_APIENTRY * MAL_LPALCDESTROYCONTEXT)     (mal_ALCcontext *context);
typedef mal_ALCcontext*    (MAL_AL_APIENTRY * MAL_LPALCGETCURRENTCONTEXT)  (void);
typedef mal_ALCdevice*     (MAL_AL_APIENTRY * MAL_LPALCGETCONTEXTSDEVICE)  (mal_ALCcontext *context);
typedef mal_ALCdevice*     (MAL_AL_APIENTRY * MAL_LPALCOPENDEVICE)         (const mal_ALCchar *devicename);
typedef mal_ALCboolean     (MAL_AL_APIENTRY * MAL_LPALCCLOSEDEVICE)        (mal_ALCdevice *device);
typedef mal_ALCenum        (MAL_AL_APIENTRY * MAL_LPALCGETERROR)           (mal_ALCdevice *device);
typedef mal_ALCboolean     (MAL_AL_APIENTRY * MAL_LPALCISEXTENSIONPRESENT) (mal_ALCdevice *device, const mal_ALCchar *extname);
typedef void*              (MAL_AL_APIENTRY * MAL_LPALCGETPROCADDRESS)     (mal_ALCdevice *device, const mal_ALCchar *funcname);
typedef mal_ALCenum        (MAL_AL_APIENTRY * MAL_LPALCGETENUMVALUE)       (mal_ALCdevice *device, const mal_ALCchar *enumname);
typedef const mal_ALCchar* (MAL_AL_APIENTRY * MAL_LPALCGETSTRING)          (mal_ALCdevice *device, mal_ALCenum param);
typedef void               (MAL_AL_APIENTRY * MAL_LPALCGETINTEGERV)        (mal_ALCdevice *device, mal_ALCenum param, mal_ALCsizei size, mal_ALCint *values);
typedef mal_ALCdevice*     (MAL_AL_APIENTRY * MAL_LPALCCAPTUREOPENDEVICE)  (const mal_ALCchar *devicename, mal_ALCuint frequency, mal_ALCenum format, mal_ALCsizei buffersize);
typedef mal_ALCboolean     (MAL_AL_APIENTRY * MAL_LPALCCAPTURECLOSEDEVICE) (mal_ALCdevice *device);
typedef void               (MAL_AL_APIENTRY * MAL_LPALCCAPTURESTART)       (mal_ALCdevice *device);
typedef void               (MAL_AL_APIENTRY * MAL_LPALCCAPTURESTOP)        (mal_ALCdevice *device);
typedef void               (MAL_AL_APIENTRY * MAL_LPALCCAPTURESAMPLES)     (mal_ALCdevice *device, mal_ALCvoid *buffer, mal_ALCsizei samples);

typedef void               (MAL_AL_APIENTRY * MAL_LPALENABLE)              (mal_ALenum capability);
typedef void               (MAL_AL_APIENTRY * MAL_LPALDISABLE)             (mal_ALenum capability);
typedef mal_ALboolean      (MAL_AL_APIENTRY * MAL_LPALISENABLED)           (mal_ALenum capability);
typedef const mal_ALchar*  (MAL_AL_APIENTRY * MAL_LPALGETSTRING)           (mal_ALenum param);
typedef void               (MAL_AL_APIENTRY * MAL_LPALGETBOOLEANV)         (mal_ALenum param, mal_ALboolean *values);
typedef void               (MAL_AL_APIENTRY * MAL_LPALGETINTEGERV)         (mal_ALenum param, mal_ALint *values);
typedef void               (MAL_AL_APIENTRY * MAL_LPALGETFLOATV)           (mal_ALenum param, mal_ALfloat *values);
typedef void               (MAL_AL_APIENTRY * MAL_LPALGETDOUBLEV)          (mal_ALenum param, mal_ALdouble *values);
typedef mal_ALboolean      (MAL_AL_APIENTRY * MAL_LPALGETBOOLEAN)          (mal_ALenum param);
typedef mal_ALint          (MAL_AL_APIENTRY * MAL_LPALGETINTEGER)          (mal_ALenum param);
typedef mal_ALfloat        (MAL_AL_APIENTRY * MAL_LPALGETFLOAT)            (mal_ALenum param);
typedef mal_ALdouble       (MAL_AL_APIENTRY * MAL_LPALGETDOUBLE)           (mal_ALenum param);
typedef mal_ALenum         (MAL_AL_APIENTRY * MAL_LPALGETERROR)            (void);
typedef mal_ALboolean      (MAL_AL_APIENTRY * MAL_LPALISEXTENSIONPRESENT)  (const mal_ALchar *extname);
typedef void*              (MAL_AL_APIENTRY * MAL_LPALGETPROCADDRESS)      (const mal_ALchar *fname);
typedef mal_ALenum         (MAL_AL_APIENTRY * MAL_LPALGETENUMVALUE)        (const mal_ALchar *ename);
typedef void               (MAL_AL_APIENTRY * MAL_LPALGENSOURCES)          (mal_ALsizei n, mal_ALuint *sources);
typedef void               (MAL_AL_APIENTRY * MAL_LPALDELETESOURCES)       (mal_ALsizei n, const mal_ALuint *sources);
typedef mal_ALboolean      (MAL_AL_APIENTRY * MAL_LPALISSOURCE)            (mal_ALuint source);
typedef void               (MAL_AL_APIENTRY * MAL_LPALSOURCEF)             (mal_ALuint source, mal_ALenum param, mal_ALfloat value);
typedef void               (MAL_AL_APIENTRY * MAL_LPALSOURCE3F)            (mal_ALuint source, mal_ALenum param, mal_ALfloat value1, mal_ALfloat value2, mal_ALfloat value3);
typedef void               (MAL_AL_APIENTRY * MAL_LPALSOURCEFV)            (mal_ALuint source, mal_ALenum param, const mal_ALfloat *values);
typedef void               (MAL_AL_APIENTRY * MAL_LPALSOURCEI)             (mal_ALuint source, mal_ALenum param, mal_ALint value);
typedef void               (MAL_AL_APIENTRY * MAL_LPALSOURCE3I)            (mal_ALuint source, mal_ALenum param, mal_ALint value1, mal_ALint value2, mal_ALint value3);
typedef void               (MAL_AL_APIENTRY * MAL_LPALSOURCEIV)            (mal_ALuint source, mal_ALenum param, const mal_ALint *values);
typedef void               (MAL_AL_APIENTRY * MAL_LPALGETSOURCEF)          (mal_ALuint source, mal_ALenum param, mal_ALfloat *value);
typedef void               (MAL_AL_APIENTRY * MAL_LPALGETSOURCE3F)         (mal_ALuint source, mal_ALenum param, mal_ALfloat *value1, mal_ALfloat *value2, mal_ALfloat *value3);
typedef void               (MAL_AL_APIENTRY * MAL_LPALGETSOURCEFV)         (mal_ALuint source, mal_ALenum param, mal_ALfloat *values);
typedef void               (MAL_AL_APIENTRY * MAL_LPALGETSOURCEI)          (mal_ALuint source, mal_ALenum param, mal_ALint *value);
typedef void               (MAL_AL_APIENTRY * MAL_LPALGETSOURCE3I)         (mal_ALuint source, mal_ALenum param, mal_ALint *value1, mal_ALint *value2, mal_ALint *value3);
typedef void               (MAL_AL_APIENTRY * MAL_LPALGETSOURCEIV)         (mal_ALuint source, mal_ALenum param, mal_ALint *values);
typedef void               (MAL_AL_APIENTRY * MAL_LPALSOURCEPLAYV)         (mal_ALsizei n, const mal_ALuint *sources);
typedef void               (MAL_AL_APIENTRY * MAL_LPALSOURCESTOPV)         (mal_ALsizei n, const mal_ALuint *sources);
typedef void               (MAL_AL_APIENTRY * MAL_LPALSOURCEREWINDV)       (mal_ALsizei n, const mal_ALuint *sources);
typedef void               (MAL_AL_APIENTRY * MAL_LPALSOURCEPAUSEV)        (mal_ALsizei n, const mal_ALuint *sources);
typedef void               (MAL_AL_APIENTRY * MAL_LPALSOURCEPLAY)          (mal_ALuint source);
typedef void               (MAL_AL_APIENTRY * MAL_LPALSOURCESTOP)          (mal_ALuint source);
typedef void               (MAL_AL_APIENTRY * MAL_LPALSOURCEREWIND)        (mal_ALuint source);
typedef void               (MAL_AL_APIENTRY * MAL_LPALSOURCEPAUSE)         (mal_ALuint source);
typedef void               (MAL_AL_APIENTRY * MAL_LPALSOURCEQUEUEBUFFERS)  (mal_ALuint source, mal_ALsizei nb, const mal_ALuint *buffers);
typedef void               (MAL_AL_APIENTRY * MAL_LPALSOURCEUNQUEUEBUFFERS)(mal_ALuint source, mal_ALsizei nb, mal_ALuint *buffers);
typedef void               (MAL_AL_APIENTRY * MAL_LPALGENBUFFERS)          (mal_ALsizei n, mal_ALuint *buffers);
typedef void               (MAL_AL_APIENTRY * MAL_LPALDELETEBUFFERS)       (mal_ALsizei n, const mal_ALuint *buffers);
typedef mal_ALboolean      (MAL_AL_APIENTRY * MAL_LPALISBUFFER)            (mal_ALuint buffer);
typedef void               (MAL_AL_APIENTRY * MAL_LPALBUFFERDATA)          (mal_ALuint buffer, mal_ALenum format, const mal_ALvoid *data, mal_ALsizei size, mal_ALsizei freq);
typedef void               (MAL_AL_APIENTRY * MAL_LPALBUFFERF)             (mal_ALuint buffer, mal_ALenum param, mal_ALfloat value);
typedef void               (MAL_AL_APIENTRY * MAL_LPALBUFFER3F)            (mal_ALuint buffer, mal_ALenum param, mal_ALfloat value1, mal_ALfloat value2, mal_ALfloat value3);
typedef void               (MAL_AL_APIENTRY * MAL_LPALBUFFERFV)            (mal_ALuint buffer, mal_ALenum param, const mal_ALfloat *values);
typedef void               (MAL_AL_APIENTRY * MAL_LPALBUFFERI)             (mal_ALuint buffer, mal_ALenum param, mal_ALint value);
typedef void               (MAL_AL_APIENTRY * MAL_LPALBUFFER3I)            (mal_ALuint buffer, mal_ALenum param, mal_ALint value1, mal_ALint value2, mal_ALint value3);
typedef void               (MAL_AL_APIENTRY * MAL_LPALBUFFERIV)            (mal_ALuint buffer, mal_ALenum param, const mal_ALint *values);
typedef void               (MAL_AL_APIENTRY * MAL_LPALGETBUFFERF)          (mal_ALuint buffer, mal_ALenum param, mal_ALfloat *value);
typedef void               (MAL_AL_APIENTRY * MAL_LPALGETBUFFER3F)         (mal_ALuint buffer, mal_ALenum param, mal_ALfloat *value1, mal_ALfloat *value2, mal_ALfloat *value3);
typedef void               (MAL_AL_APIENTRY * MAL_LPALGETBUFFERFV)         (mal_ALuint buffer, mal_ALenum param, mal_ALfloat *values);
typedef void               (MAL_AL_APIENTRY * MAL_LPALGETBUFFERI)          (mal_ALuint buffer, mal_ALenum param, mal_ALint *value);
typedef void               (MAL_AL_APIENTRY * MAL_LPALGETBUFFER3I)         (mal_ALuint buffer, mal_ALenum param, mal_ALint *value1, mal_ALint *value2, mal_ALint *value3);
typedef void               (MAL_AL_APIENTRY * MAL_LPALGETBUFFERIV)         (mal_ALuint buffer, mal_ALenum param, mal_ALint *values);

mal_result mal_context_init__openal(mal_context* pContext)
{
    mal_assert(pContext != NULL);

    const char* libName =
#ifdef MAL_WIN32
        "OpenAL32.dll";
#endif
#ifdef MAL_LINUX
        "libopenal.so";
#endif
#ifdef MAL_APPLE
        // I don't own a Mac so a contribution here would be much appreciated! Just don't know what the library is called...
#endif

    pContext->openal.hOpenAL = mal_dlopen(libName);

#ifdef MAL_WIN32
    // Special case for Win32 - try "soft_oal.dll" for OpenAL-Soft drop-ins.
    if (pContext->openal.hOpenAL == NULL) {
        pContext->openal.hOpenAL = mal_dlopen("soft_oal.dll");
    }
#endif

    if (pContext->openal.hOpenAL == NULL) {
        return MAL_FAILED_TO_INIT_BACKEND;
    }

    pContext->openal.alcCreateContext       = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alcCreateContext");
    pContext->openal.alcMakeContextCurrent  = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alcMakeContextCurrent");
    pContext->openal.alcProcessContext      = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alcProcessContext");
    pContext->openal.alcSuspendContext      = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alcSuspendContext");
    pContext->openal.alcDestroyContext      = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alcDestroyContext");
    pContext->openal.alcGetCurrentContext   = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alcGetCurrentContext");
    pContext->openal.alcGetContextsDevice   = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alcGetContextsDevice");
    pContext->openal.alcOpenDevice          = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alcOpenDevice");
    pContext->openal.alcCloseDevice         = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alcCloseDevice");
    pContext->openal.alcGetError            = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alcGetError");
    pContext->openal.alcIsExtensionPresent  = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alcIsExtensionPresent");
    pContext->openal.alcGetProcAddress      = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alcGetProcAddress");
    pContext->openal.alcGetEnumValue        = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alcGetEnumValue");
    pContext->openal.alcGetString           = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alcGetString");
    pContext->openal.alcGetIntegerv         = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alcGetIntegerv");
    pContext->openal.alcCaptureOpenDevice   = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alcCaptureOpenDevice");
    pContext->openal.alcCaptureCloseDevice  = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alcCaptureCloseDevice");
    pContext->openal.alcCaptureStart        = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alcCaptureStart");
    pContext->openal.alcCaptureStop         = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alcCaptureStop");
    pContext->openal.alcCaptureSamples      = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alcCaptureSamples");

    pContext->openal.alEnable               = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alEnable");
    pContext->openal.alDisable              = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alDisable");
    pContext->openal.alIsEnabled            = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alIsEnabled");
    pContext->openal.alGetString            = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alGetString");
    pContext->openal.alGetBooleanv          = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alGetBooleanv");
    pContext->openal.alGetIntegerv          = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alGetIntegerv");
    pContext->openal.alGetFloatv            = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alGetFloatv");
    pContext->openal.alGetDoublev           = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alGetDoublev");
    pContext->openal.alGetBoolean           = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alGetBoolean");
    pContext->openal.alGetInteger           = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alGetInteger");
    pContext->openal.alGetFloat             = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alGetFloat");
    pContext->openal.alGetDouble            = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alGetDouble");
    pContext->openal.alGetError             = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alGetError");
    pContext->openal.alIsExtensionPresent   = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alIsExtensionPresent");
    pContext->openal.alGetProcAddress       = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alGetProcAddress");
    pContext->openal.alGetEnumValue         = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alGetEnumValue");
    pContext->openal.alGenSources           = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alGenSources");
    pContext->openal.alDeleteSources        = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alDeleteSources");
    pContext->openal.alIsSource             = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alIsSource");
    pContext->openal.alSourcef              = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alSourcef");
    pContext->openal.alSource3f             = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alSource3f");
    pContext->openal.alSourcefv             = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alSourcefv");
    pContext->openal.alSourcei              = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alSourcei");
    pContext->openal.alSource3i             = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alSource3i");
    pContext->openal.alSourceiv             = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alSourceiv");
    pContext->openal.alGetSourcef           = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alGetSourcef");
    pContext->openal.alGetSource3f          = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alGetSource3f");
    pContext->openal.alGetSourcefv          = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alGetSourcefv");
    pContext->openal.alGetSourcei           = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alGetSourcei");
    pContext->openal.alGetSource3i          = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alGetSource3i");
    pContext->openal.alGetSourceiv          = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alGetSourceiv");
    pContext->openal.alSourcePlayv          = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alSourcePlayv");
    pContext->openal.alSourceStopv          = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alSourceStopv");
    pContext->openal.alSourceRewindv        = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alSourceRewindv");
    pContext->openal.alSourcePausev         = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alSourcePausev");
    pContext->openal.alSourcePlay           = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alSourcePlay");
    pContext->openal.alSourceStop           = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alSourceStop");
    pContext->openal.alSourceRewind         = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alSourceRewind");
    pContext->openal.alSourcePause          = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alSourcePause");
    pContext->openal.alSourceQueueBuffers   = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alSourceQueueBuffers");
    pContext->openal.alSourceUnqueueBuffers = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alSourceUnqueueBuffers");
    pContext->openal.alGenBuffers           = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alGenBuffers");
    pContext->openal.alDeleteBuffers        = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alDeleteBuffers");
    pContext->openal.alIsBuffer             = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alIsBuffer");
    pContext->openal.alBufferData           = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alBufferData");
    pContext->openal.alBufferf              = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alBufferf");
    pContext->openal.alBuffer3f             = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alBuffer3f");
    pContext->openal.alBufferfv             = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alBufferfv");
    pContext->openal.alBufferi              = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alBufferi");
    pContext->openal.alBuffer3i             = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alBuffer3i");
    pContext->openal.alBufferiv             = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alBufferiv");
    pContext->openal.alGetBufferf           = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alGetBufferf");
    pContext->openal.alGetBuffer3f          = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alGetBuffer3f");
    pContext->openal.alGetBufferfv          = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alGetBufferfv");
    pContext->openal.alGetBufferi           = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alGetBufferi");
    pContext->openal.alGetBuffer3i          = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alGetBuffer3i");
    pContext->openal.alGetBufferiv          = (mal_proc)mal_dlsym(pContext->openal.hOpenAL, "alGetBufferiv");

    // We depend on the ALC_ENUMERATION_EXT extension.
    if (!((MAL_LPALCISEXTENSIONPRESENT)pContext->openal.alcIsExtensionPresent)(NULL, "ALC_ENUMERATION_EXT")) {
        mal_dlclose(pContext->openal.hOpenAL);
        return MAL_FAILED_TO_INIT_BACKEND;
    }

    pContext->openal.isFloat32Supported = ((MAL_LPALISEXTENSIONPRESENT)pContext->openal.alIsExtensionPresent)("AL_EXT_float32");
    pContext->openal.isMCFormatsSupported = ((MAL_LPALISEXTENSIONPRESENT)pContext->openal.alIsExtensionPresent)("AL_EXT_MCFORMATS");
    
    return MAL_SUCCESS;
}

mal_result mal_context_uninit__openal(mal_context* pContext)
{
    mal_assert(pContext != NULL);
    mal_assert(pContext->backend == mal_backend_openal);

    mal_dlclose(pContext->openal.hOpenAL);
    return MAL_SUCCESS;
}

mal_result mal_enumerate_devices__openal(mal_context* pContext, mal_device_type type, mal_uint32* pCount, mal_device_info* pInfo)
{
    mal_uint32 infoCapacity = *pCount;
    *pCount = 0;

    const mal_ALCchar* pDeviceNames = ((MAL_LPALCGETSTRING)pContext->openal.alcGetString)(NULL, (type == mal_device_type_playback) ? MAL_ALC_DEVICE_SPECIFIER : MAL_ALC_CAPTURE_DEVICE_SPECIFIER);
    if (pDeviceNames == NULL) {
        return MAL_NO_DEVICE;
    }
    
    // Each device is stored in pDeviceNames, separated by a null-terminator. The string itself is double-null-terminated.
    const mal_ALCchar* pNextDeviceName = pDeviceNames;
    for (;;) {
        *pCount += 1;

        if (pInfo != NULL && infoCapacity > 0) {
            mal_strncpy_s(pInfo->id.openal, sizeof(pInfo->id.openal), (const char*)pNextDeviceName, (size_t)-1);
            mal_strncpy_s(pInfo->name,      sizeof(pInfo->name),      (const char*)pNextDeviceName, (size_t)-1);

            pInfo += 1;
            infoCapacity -= 1;
        }

        // Move to the next device name.
        while (*pNextDeviceName != '\0') {
            pNextDeviceName += 1;
        }

        // If we've reached the double-null-terminator, we're done.
        pNextDeviceName += 1;
        if (*pNextDeviceName == '\0') {
            break;
        }
    };

    return MAL_SUCCESS;
}

static void mal_device_uninit__openal(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    ((MAL_LPALCMAKECONTEXTCURRENT)pDevice->pContext->openal.alcMakeContextCurrent)(NULL);
    ((MAL_LPALCDESTROYCONTEXT)pDevice->pContext->openal.alcDestroyContext)((mal_ALCcontext*)pDevice->openal.pContextALC);

    if (pDevice->type == mal_device_type_playback) {
        ((MAL_LPALCCLOSEDEVICE)pDevice->pContext->openal.alcCloseDevice)((mal_ALCdevice*)pDevice->openal.pDeviceALC);
    } else {
        ((MAL_LPALCCAPTURECLOSEDEVICE)pDevice->pContext->openal.alcCaptureCloseDevice)((mal_ALCdevice*)pDevice->openal.pDeviceALC);
    }

    mal_free(pDevice->openal.pIntermediaryBuffer);
}

static mal_result mal_device_init__openal(mal_context* pContext, mal_device_type type, mal_device_id* pDeviceID, mal_device_config* pConfig, mal_device* pDevice)
{
    if (pDevice->periods > MAL_MAX_PERIODS_OPENAL) {
        pDevice->periods = MAL_MAX_PERIODS_OPENAL;
    }

    // OpenAL has bad latency in my testing :(
    if (pDevice->flags & MAL_DEVICE_FLAG_USING_DEFAULT_BUFFER_SIZE) {
        pDevice->bufferSizeInFrames *= 4;
    }

    mal_ALCsizei bufferSizeInSamplesAL = pConfig->bufferSizeInFrames;
    mal_ALCuint frequencyAL = pConfig->sampleRate;

    mal_uint32 channelsAL = 0;

    // OpenAL currently only supports only mono and stereo. TODO: Check for the AL_EXT_MCFORMATS extension and use one of those formats for quad, 5.1, etc.
    mal_ALCenum formatAL = 0;
    if (pConfig->channels == 1) {
        // Mono.
        channelsAL = 1;
        if (pConfig->format == mal_format_f32) {
            if (pContext->openal.isFloat32Supported) {
                formatAL = MAL_AL_FORMAT_MONO_FLOAT32;
            } else {
                formatAL = MAL_AL_FORMAT_MONO16;
            }
        } else if (pConfig->format == mal_format_s32) {
            formatAL = MAL_AL_FORMAT_MONO16;
        } else if (pConfig->format == mal_format_s24) {
            formatAL = MAL_AL_FORMAT_MONO16;
        } else if (pConfig->format == mal_format_s16) {
            formatAL = MAL_AL_FORMAT_MONO16;
        } else if (pConfig->format == mal_format_u8) {
            formatAL = MAL_AL_FORMAT_MONO8;
        }
    } else {
        // Stereo.
        channelsAL = 2;
        if (pConfig->format == mal_format_f32) {
            if (pContext->openal.isFloat32Supported) {
                formatAL = MAL_AL_FORMAT_STEREO_FLOAT32;
            } else {
                formatAL = MAL_AL_FORMAT_STEREO16;
            }
        } else if (pConfig->format == mal_format_s32) {
            formatAL = MAL_AL_FORMAT_STEREO16;
        } else if (pConfig->format == mal_format_s24) {
            formatAL = MAL_AL_FORMAT_STEREO16;
        } else if (pConfig->format == mal_format_s16) {
            formatAL = MAL_AL_FORMAT_STEREO16;
        } else if (pConfig->format == mal_format_u8) {
            formatAL = MAL_AL_FORMAT_STEREO8;
        }
    }

    if (formatAL == 0) {
        return MAL_FORMAT_NOT_SUPPORTED;
    }

    bufferSizeInSamplesAL *= channelsAL;


    // OpenAL feels a bit unintuitive to me... The global object is a device, and it would appear that each device can have
    // many context's...
    mal_ALCdevice* pDeviceALC = NULL;
    if (type == mal_device_type_playback) {
        pDeviceALC = ((MAL_LPALCOPENDEVICE)pContext->openal.alcOpenDevice)((pDeviceID == NULL) ? NULL : pDeviceID->openal);
    } else {
        pDeviceALC = ((MAL_LPALCCAPTUREOPENDEVICE)pContext->openal.alcCaptureOpenDevice)((pDeviceID == NULL) ? NULL : pDeviceID->openal, frequencyAL, formatAL, bufferSizeInSamplesAL);
    }

    if (pDeviceALC == NULL) {
        return MAL_FAILED_TO_INIT_BACKEND;
    }

    // A context is only required for playback.
    mal_ALCcontext* pContextALC = NULL;
    if (pDevice->type == mal_device_type_playback) {
        pContextALC = ((MAL_LPALCCREATECONTEXT)pContext->openal.alcCreateContext)(pDeviceALC, NULL);
        if (pContextALC == NULL) {
            ((MAL_LPALCCLOSEDEVICE)pDevice->pContext->openal.alcCloseDevice)(pDeviceALC);
            return MAL_FAILED_TO_INIT_BACKEND;
        }

        ((MAL_LPALCMAKECONTEXTCURRENT)pDevice->pContext->openal.alcMakeContextCurrent)(pContextALC);

        mal_ALuint sourceAL;
        ((MAL_LPALGENSOURCES)pDevice->pContext->openal.alGenSources)(1, &sourceAL);
        pDevice->openal.sourceAL = sourceAL;

        // We create the buffers, but only fill and queue them when the device is started.
        mal_ALuint buffersAL[MAL_MAX_PERIODS_OPENAL];
        ((MAL_LPALGENBUFFERS)pDevice->pContext->openal.alGenBuffers)(pDevice->periods, buffersAL);
        for (mal_uint32 i = 0; i < pDevice->periods; ++i) {
            pDevice->openal.buffersAL[i] = buffersAL[i];
        }
    }

    pDevice->internalChannels = channelsAL;
    pDevice->internalSampleRate = frequencyAL;

    // The internal format is a little bit straight with OpenAL.
    switch (formatAL)
    {
        case MAL_AL_FORMAT_MONO8:
        case MAL_AL_FORMAT_STEREO8:
        case MAL_AL_FORMAT_REAR8:
        case MAL_AL_FORMAT_QUAD8:
        case MAL_AL_FORMAT_51CHN8:
        case MAL_AL_FORMAT_61CHN8:
        case MAL_AL_FORMAT_71CHN8:
        {
            pDevice->internalFormat = mal_format_u8;
        } break;

        case MAL_AL_FORMAT_MONO16:
        case MAL_AL_FORMAT_STEREO16:
        case MAL_AL_FORMAT_REAR16:
        case MAL_AL_FORMAT_QUAD16:
        case MAL_AL_FORMAT_51CHN16:
        case MAL_AL_FORMAT_61CHN16:
        case MAL_AL_FORMAT_71CHN16:
        {
            pDevice->internalFormat = mal_format_s16;
        } break;

        case MAL_AL_FORMAT_REAR32:
        case MAL_AL_FORMAT_QUAD32:
        case MAL_AL_FORMAT_51CHN32:
        case MAL_AL_FORMAT_61CHN32:
        case MAL_AL_FORMAT_71CHN32:
        {
            pDevice->internalFormat = mal_format_s32;
        } break;

        case MAL_AL_FORMAT_MONO_FLOAT32:
        case MAL_AL_FORMAT_STEREO_FLOAT32:
        {
            pDevice->internalFormat = mal_format_f32;
        } break;
    }

    // From what I can tell, the ordering of channels is fixed for OpenAL.
    switch (formatAL)
    {
        case MAL_AL_FORMAT_MONO8:
        case MAL_AL_FORMAT_MONO16:
        case MAL_AL_FORMAT_MONO_FLOAT32:
        {
            pDevice->internalChannelMap[0] = MAL_CHANNEL_FRONT_CENTER;
        } break;

        case MAL_AL_FORMAT_STEREO8:
        case MAL_AL_FORMAT_STEREO16:
        case MAL_AL_FORMAT_STEREO_FLOAT32:
        {
            pDevice->internalChannelMap[0] = MAL_CHANNEL_FRONT_LEFT;
            pDevice->internalChannelMap[1] = MAL_CHANNEL_FRONT_RIGHT;
        } break;

        case MAL_AL_FORMAT_REAR8:
        case MAL_AL_FORMAT_REAR16:
        case MAL_AL_FORMAT_REAR32:
        {
            pDevice->internalChannelMap[0] = MAL_CHANNEL_BACK_LEFT;
            pDevice->internalChannelMap[1] = MAL_CHANNEL_BACK_RIGHT;
        } break;

        case MAL_AL_FORMAT_QUAD8:
        case MAL_AL_FORMAT_QUAD16:
        case MAL_AL_FORMAT_QUAD32:
        {
            pDevice->internalChannelMap[0] = MAL_CHANNEL_FRONT_LEFT;
            pDevice->internalChannelMap[1] = MAL_CHANNEL_FRONT_RIGHT;
            pDevice->internalChannelMap[2] = MAL_CHANNEL_BACK_LEFT;
            pDevice->internalChannelMap[3] = MAL_CHANNEL_BACK_RIGHT;
        } break;

        case MAL_AL_FORMAT_51CHN8:
        case MAL_AL_FORMAT_51CHN16:
        case MAL_AL_FORMAT_51CHN32:
        {
            pDevice->internalChannelMap[0] = MAL_CHANNEL_FRONT_LEFT;
            pDevice->internalChannelMap[1] = MAL_CHANNEL_FRONT_RIGHT;
            pDevice->internalChannelMap[2] = MAL_CHANNEL_FRONT_CENTER;
            pDevice->internalChannelMap[3] = MAL_CHANNEL_LFE;
            pDevice->internalChannelMap[4] = MAL_CHANNEL_BACK_LEFT;
            pDevice->internalChannelMap[5] = MAL_CHANNEL_BACK_RIGHT;
        } break;

        case MAL_AL_FORMAT_61CHN8:
        case MAL_AL_FORMAT_61CHN16:
        case MAL_AL_FORMAT_61CHN32:
        {
            pDevice->internalChannelMap[0] = MAL_CHANNEL_FRONT_LEFT;
            pDevice->internalChannelMap[1] = MAL_CHANNEL_FRONT_RIGHT;
            pDevice->internalChannelMap[2] = MAL_CHANNEL_FRONT_CENTER;
            pDevice->internalChannelMap[3] = MAL_CHANNEL_LFE;
            pDevice->internalChannelMap[4] = MAL_CHANNEL_BACK_CENTER;
            pDevice->internalChannelMap[5] = MAL_CHANNEL_SIDE_LEFT;
            pDevice->internalChannelMap[6] = MAL_CHANNEL_SIDE_RIGHT;
        } break;

        case MAL_AL_FORMAT_71CHN8:
        case MAL_AL_FORMAT_71CHN16:
        case MAL_AL_FORMAT_71CHN32:
        {
            pDevice->internalChannelMap[0] = MAL_CHANNEL_FRONT_LEFT;
            pDevice->internalChannelMap[1] = MAL_CHANNEL_FRONT_RIGHT;
            pDevice->internalChannelMap[2] = MAL_CHANNEL_FRONT_CENTER;
            pDevice->internalChannelMap[3] = MAL_CHANNEL_LFE;
            pDevice->internalChannelMap[4] = MAL_CHANNEL_BACK_LEFT;
            pDevice->internalChannelMap[5] = MAL_CHANNEL_BACK_RIGHT;
            pDevice->internalChannelMap[6] = MAL_CHANNEL_SIDE_LEFT;
            pDevice->internalChannelMap[7] = MAL_CHANNEL_SIDE_RIGHT;
        } break;

        default: break;
    }

    pDevice->openal.pDeviceALC = pDeviceALC;
    pDevice->openal.pContextALC = pContextALC;
    pDevice->openal.formatAL = formatAL;
    pDevice->openal.subBufferSizeInFrames = pDevice->bufferSizeInFrames / pDevice->periods;
    pDevice->openal.pIntermediaryBuffer = (mal_uint8*)mal_malloc(pDevice->openal.subBufferSizeInFrames * channelsAL * mal_get_sample_size_in_bytes(pDevice->internalFormat));
    if (pDevice->openal.pIntermediaryBuffer == NULL) {
        mal_device_uninit__openal(pDevice);
        return MAL_OUT_OF_MEMORY;
    }

    return MAL_SUCCESS;
}

static mal_result mal_device__start_backend__openal(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    if (pDevice->type == mal_device_type_playback) {
        // Playback.
        //
        // When starting playback we want to ensure each buffer is filled and queued before playing the source.
        pDevice->openal.iNextBuffer = 0;

        ((MAL_LPALCMAKECONTEXTCURRENT)pDevice->pContext->openal.alcMakeContextCurrent)((mal_ALCcontext*)pDevice->openal.pContextALC);

        for (mal_uint32 i = 0; i < pDevice->periods; ++i) {
            mal_device__read_frames_from_client(pDevice, pDevice->openal.subBufferSizeInFrames, pDevice->openal.pIntermediaryBuffer);

            mal_ALuint bufferAL = pDevice->openal.buffersAL[i];
            ((MAL_LPALBUFFERDATA)pDevice->pContext->openal.alBufferData)(bufferAL, pDevice->openal.formatAL, pDevice->openal.pIntermediaryBuffer, pDevice->openal.subBufferSizeInFrames * pDevice->internalChannels * mal_get_sample_size_in_bytes(pDevice->internalFormat), pDevice->internalSampleRate);
            ((MAL_LPALSOURCEQUEUEBUFFERS)pDevice->pContext->openal.alSourceQueueBuffers)(pDevice->openal.sourceAL, 1, &bufferAL);
        }

        // Start the source only after filling and queueing each buffer.
        ((MAL_LPALSOURCEPLAY)pDevice->pContext->openal.alSourcePlay)(pDevice->openal.sourceAL);
    } else {
        // Capture.
        ((MAL_LPALCCAPTURESTART)pDevice->pContext->openal.alcCaptureStart)((mal_ALCdevice*)pDevice->openal.pDeviceALC);
    }

    return MAL_SUCCESS;
}

static mal_result mal_device__stop_backend__openal(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    if (pDevice->type == mal_device_type_playback) {
        ((MAL_LPALCMAKECONTEXTCURRENT)pDevice->pContext->openal.alcMakeContextCurrent)((mal_ALCcontext*)pDevice->openal.pContextALC);
        ((MAL_LPALSOURCESTOP)pDevice->pContext->openal.alSourceStop)(pDevice->openal.sourceAL);
    } else {
        ((MAL_LPALCCAPTURESTOP)pDevice->pContext->openal.alcCaptureStop)((mal_ALCdevice*)pDevice->openal.pDeviceALC);
    }

    return MAL_SUCCESS;
}

static mal_result mal_device__break_main_loop__openal(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    pDevice->openal.breakFromMainLoop = MAL_TRUE;
    return MAL_SUCCESS;
}

static mal_uint32 mal_device__get_available_frames__openal(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    if (pDevice->type == mal_device_type_playback) {
        ((MAL_LPALCMAKECONTEXTCURRENT)pDevice->pContext->openal.alcMakeContextCurrent)((mal_ALCcontext*)pDevice->openal.pContextALC);

        mal_ALint processedBufferCount = 0;
        ((MAL_LPALGETSOURCEI)pDevice->pContext->openal.alGetSourcei)(pDevice->openal.sourceAL, MAL_AL_BUFFERS_PROCESSED, &processedBufferCount);

        return processedBufferCount * pDevice->openal.subBufferSizeInFrames;
    } else {
        mal_ALint samplesAvailable = 0;
        ((MAL_LPALCGETINTEGERV)pDevice->pContext->openal.alcGetIntegerv)((mal_ALCdevice*)pDevice->openal.pDeviceALC, MAL_ALC_CAPTURE_SAMPLES, 1, &samplesAvailable);

        return samplesAvailable / pDevice->channels;
    }
}

static mal_uint32 mal_device__wait_for_frames__openal(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    while (!pDevice->openal.breakFromMainLoop) {
        mal_uint32 framesAvailable = mal_device__get_available_frames__openal(pDevice);
        if (framesAvailable > 0) {
            return framesAvailable;
        }

        mal_sleep(1);
    }

    // We'll get here if the loop was terminated. When capturing we want to return whatever is available. For playback we just drop it.
    if (pDevice->type == mal_device_type_playback) {
        return 0;
    } else {
        return mal_device__get_available_frames__openal(pDevice);
    }
}

static mal_result mal_device__main_loop__openal(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    pDevice->openal.breakFromMainLoop = MAL_FALSE;
    while (!pDevice->openal.breakFromMainLoop) {
        mal_uint32 framesAvailable = mal_device__wait_for_frames__openal(pDevice);
        if (framesAvailable == 0) {
            continue;
        }

        // If it's a playback device, don't bother grabbing more data if the device is being stopped.
        if (pDevice->openal.breakFromMainLoop && pDevice->type == mal_device_type_playback) {
            return MAL_FALSE;
        }

        if (pDevice->type == mal_device_type_playback) {
            while (framesAvailable > 0) {
                mal_uint32 framesToRead = (framesAvailable > pDevice->openal.subBufferSizeInFrames) ? pDevice->openal.subBufferSizeInFrames : framesAvailable;

                mal_ALuint bufferAL = pDevice->openal.buffersAL[pDevice->openal.iNextBuffer];
                pDevice->openal.iNextBuffer = (pDevice->openal.iNextBuffer + 1) % pDevice->periods;

                mal_device__read_frames_from_client(pDevice, framesToRead, pDevice->openal.pIntermediaryBuffer);

                ((MAL_LPALCMAKECONTEXTCURRENT)pDevice->pContext->openal.alcMakeContextCurrent)((mal_ALCcontext*)pDevice->openal.pContextALC);
                ((MAL_LPALSOURCEUNQUEUEBUFFERS)pDevice->pContext->openal.alSourceUnqueueBuffers)(pDevice->openal.sourceAL, 1, &bufferAL);
                ((MAL_LPALBUFFERDATA)pDevice->pContext->openal.alBufferData)(bufferAL, pDevice->openal.formatAL, pDevice->openal.pIntermediaryBuffer, pDevice->openal.subBufferSizeInFrames * pDevice->internalChannels * mal_get_sample_size_in_bytes(pDevice->internalFormat), pDevice->internalSampleRate);
                ((MAL_LPALSOURCEQUEUEBUFFERS)pDevice->pContext->openal.alSourceQueueBuffers)(pDevice->openal.sourceAL, 1, &bufferAL);

                framesAvailable -= framesToRead;
            }


            // There's a chance the source has stopped playing due to there not being any buffer's queue. Make sure it's restarted.
            mal_ALenum state;
            ((MAL_LPALGETSOURCEI)pDevice->pContext->openal.alGetSourcei)(pDevice->openal.sourceAL, MAL_AL_SOURCE_STATE, &state);

            if (state != MAL_AL_PLAYING) {
                ((MAL_LPALSOURCEPLAY)pDevice->pContext->openal.alSourcePlay)(pDevice->openal.sourceAL);
            }
        } else {
            while (framesAvailable > 0) {
                mal_uint32 framesToSend = (framesAvailable > pDevice->openal.subBufferSizeInFrames) ? pDevice->openal.subBufferSizeInFrames : framesAvailable;
                ((MAL_LPALCCAPTURESAMPLES)pDevice->pContext->openal.alcCaptureSamples)((mal_ALCdevice*)pDevice->openal.pDeviceALC, pDevice->openal.pIntermediaryBuffer, framesToSend);

                mal_device__send_frames_to_client(pDevice, framesToSend, pDevice->openal.pIntermediaryBuffer);
                framesAvailable -= framesToSend;
            }
        }
    }

    return MAL_SUCCESS;
}
#endif  // OpenAL


mal_bool32 mal__is_channel_map_valid(mal_channel* channelMap, mal_uint32 channels)
{
    mal_assert(channels > 0);

    // A channel cannot be present in the channel map more than once.
    for (mal_uint32 iChannel = 0; iChannel < channels; ++iChannel) {
        for (mal_uint32 jChannel = iChannel + 1; jChannel < channels; ++jChannel) {
            if (channelMap[iChannel] == channelMap[jChannel]) {
                return MAL_FALSE;
            }
        }
    }

    return MAL_TRUE;
}


static mal_result mal_device__start_backend(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    mal_result result = MAL_NO_BACKEND;
#ifdef MAL_ENABLE_WASAPI
    if (pDevice->pContext->backend == mal_backend_wasapi) {
        result = mal_device__start_backend__wasapi(pDevice);
    }
#endif
#ifdef MAL_ENABLE_DSOUND
    if (pDevice->pContext->backend == mal_backend_dsound) {
        result = mal_device__start_backend__dsound(pDevice);
    }
#endif
#ifdef MAL_ENABLE_ALSA
    if (pDevice->pContext->backend == mal_backend_alsa) {
        result = mal_device__start_backend__alsa(pDevice);
    }
#endif
#ifdef MAL_ENABLE_OPENAL
    if (pDevice->pContext->backend == mal_backend_openal) {
        result = mal_device__start_backend__openal(pDevice);
    }
#endif
#ifdef MAL_ENABLE_NULL
    if (pDevice->pContext->backend == mal_backend_null) {
        result = mal_device__start_backend__null(pDevice);
    }
#endif

    return result;
}

static mal_result mal_device__stop_backend(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    mal_result result = MAL_NO_BACKEND;
#ifdef MAL_ENABLE_WASAPI
    if (pDevice->pContext->backend == mal_backend_wasapi) {
        result = mal_device__stop_backend__wasapi(pDevice);
    }
#endif
#ifdef MAL_ENABLE_DSOUND
    if (pDevice->pContext->backend == mal_backend_dsound) {
        result = mal_device__stop_backend__dsound(pDevice);
    }
#endif
#ifdef MAL_ENABLE_ALSA
    if (pDevice->pContext->backend == mal_backend_alsa) {
        result = mal_device__stop_backend__alsa(pDevice);
    }
#endif
#ifdef MAL_ENABLE_OPENAL
    if (pDevice->pContext->backend == mal_backend_openal) {
        result = mal_device__stop_backend__openal(pDevice);
    }
#endif
#ifdef MAL_ENABLE_NULL
    if (pDevice->pContext->backend == mal_backend_null) {
        result = mal_device__stop_backend__null(pDevice);
    }
#endif

    return result;
}

static mal_result mal_device__break_main_loop(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    mal_result result = MAL_NO_BACKEND;
#ifdef MAL_ENABLE_WASAPI
    if (pDevice->pContext->backend == mal_backend_wasapi) {
        result = mal_device__break_main_loop__wasapi(pDevice);
    }
#endif
#ifdef MAL_ENABLE_DSOUND
    if (pDevice->pContext->backend == mal_backend_dsound) {
        result = mal_device__break_main_loop__dsound(pDevice);
    }
#endif
#ifdef MAL_ENABLE_ALSA
    if (pDevice->pContext->backend == mal_backend_alsa) {
        result = mal_device__break_main_loop__alsa(pDevice);
    }
#endif
#ifdef MAL_ENABLE_OPENAL
    if (pDevice->pContext->backend == mal_backend_openal) {
        result = mal_device__break_main_loop__openal(pDevice);
    }
#endif
#ifdef MAL_ENABLE_NULL
    if (pDevice->pContext->backend == mal_backend_null) {
        result = mal_device__break_main_loop__null(pDevice);
    }
#endif

    return result;
}

static mal_result mal_device__main_loop(mal_device* pDevice)
{
    mal_assert(pDevice != NULL);

    mal_result result = MAL_NO_BACKEND;
#ifdef MAL_ENABLE_WASAPI
    if (pDevice->pContext->backend == mal_backend_wasapi) {
        result = mal_device__main_loop__wasapi(pDevice);
    }
#endif
#ifdef MAL_ENABLE_DSOUND
    if (pDevice->pContext->backend == mal_backend_dsound) {
        result = mal_device__main_loop__dsound(pDevice);
    }
#endif
#ifdef MAL_ENABLE_ALSA
    if (pDevice->pContext->backend == mal_backend_alsa) {
        result = mal_device__main_loop__alsa(pDevice);
    }
#endif
#ifdef MAL_ENABLE_OPENAL
    if (pDevice->pContext->backend == mal_backend_openal) {
        result = mal_device__main_loop__openal(pDevice);
    }
#endif
#ifdef MAL_ENABLE_NULL
    if (pDevice->pContext->backend == mal_backend_null) {
        result = mal_device__main_loop__null(pDevice);
    }
#endif

    return result;
}

mal_thread_result MAL_THREADCALL mal_worker_thread(void* pData)
{
    mal_device* pDevice = (mal_device*)pData;
    mal_assert(pDevice != NULL);
    
#ifdef MAL_WIN32
    ((MAL_PFN_CoInitializeEx)pDevice->pContext->win32.CoInitializeEx)(NULL, COINIT_MULTITHREADED);
#endif

    // This is only used to prevent posting onStop() when the device is first initialized.
    mal_bool32 skipNextStopEvent = MAL_TRUE;

    for (;;) {
        // At the start of iteration the device is stopped - we must explicitly mark it as such.
        mal_device__stop_backend(pDevice);

        if (!skipNextStopEvent) {
            mal_stop_proc onStop = pDevice->onStop;
            if (onStop) {
                onStop(pDevice);
            }
        } else {
            skipNextStopEvent = MAL_FALSE;
        }


        // Let the other threads know that the device has stopped.
        mal_device__set_state(pDevice, MAL_STATE_STOPPED);
        mal_event_signal(&pDevice->stopEvent);

        // We use an event to wait for a request to wake up.
        mal_event_wait(&pDevice->wakeupEvent);

        // Default result code.
        pDevice->workResult = MAL_SUCCESS;

        // Just break if we're terminating.
        if (mal_device__get_state(pDevice) == MAL_STATE_UNINITIALIZED) {
            break;
        }


        // Getting here means we just started the device and we need to wait for the device to
        // either deliver us data (recording) or request more data (playback).
        mal_assert(mal_device__get_state(pDevice) == MAL_STATE_STARTING);

        pDevice->workResult = mal_device__start_backend(pDevice);
        if (pDevice->workResult != MAL_SUCCESS) {
            mal_event_signal(&pDevice->startEvent);
            continue;
        }

        // The thread that requested the device to start playing is waiting for this thread to start the
        // device for real, which is now.
        mal_device__set_state(pDevice, MAL_STATE_STARTED);
        mal_event_signal(&pDevice->startEvent);

        // Now we just enter the main loop. The main loop can be broken with mal_device__break_main_loop().
        mal_device__main_loop(pDevice);
    }

    // Make sure we aren't continuously waiting on a stop event.
    mal_event_signal(&pDevice->stopEvent);  // <-- Is this still needed?

#ifdef MAL_WIN32
    ((MAL_PFN_CoUninitialize)pDevice->pContext->win32.CoUninitialize)();
#endif

    return (mal_thread_result)0;
}


// Helper for determining whether or not the given device is initialized.
mal_bool32 mal_device__is_initialized(mal_device* pDevice)
{
    if (pDevice == NULL) return MAL_FALSE;
    return mal_device__get_state(pDevice) != MAL_STATE_UNINITIALIZED;
}


#ifdef MAL_WIN32
mal_result mal_context_uninit_backend_apis__win32(mal_context* pContext)
{
    ((MAL_PFN_CoUninitialize)pContext->win32.CoUninitialize)();
    mal_dlclose(pContext->win32.hOle32DLL);

    return MAL_SUCCESS;
}

mal_result mal_context_init_backend_apis__win32(mal_context* pContext)
{
    // Ole32.dll
    pContext->win32.hOle32DLL = mal_dlopen("ole32.dll");
    if (pContext->win32.hOle32DLL == NULL) {
        return MAL_FAILED_TO_INIT_BACKEND;
    }

    pContext->win32.CoInitializeEx   = (mal_proc)mal_dlsym(pContext->win32.hOle32DLL, "CoInitializeEx");
    pContext->win32.CoUninitialize   = (mal_proc)mal_dlsym(pContext->win32.hOle32DLL, "CoUninitialize");
    pContext->win32.CoCreateInstance = (mal_proc)mal_dlsym(pContext->win32.hOle32DLL, "CoCreateInstance");
    pContext->win32.CoTaskMemFree    = (mal_proc)mal_dlsym(pContext->win32.hOle32DLL, "CoTaskMemFree");
    pContext->win32.PropVariantClear = (mal_proc)mal_dlsym(pContext->win32.hOle32DLL, "PropVariantClear");


    ((MAL_PFN_CoInitializeEx)pContext->win32.CoInitializeEx)(NULL, COINIT_MULTITHREADED);

    return MAL_SUCCESS;
}
#else
mal_result mal_context_uninit_backend_apis__nix(mal_context* pContext)
{
    (void)pContext;
    return MAL_SUCCESS;
}

mal_result mal_context_init_backend_apis__nix(mal_context* pContext)
{
    (void)pContext;
    return MAL_SUCCESS;
}
#endif

mal_result mal_context_init_backend_apis(mal_context* pContext)
{
    mal_result result = MAL_NO_BACKEND;
#ifdef MAL_WIN32
    result = mal_context_init_backend_apis__win32(pContext);
#else
    result = mal_context_init_backend_apis__nix(pContext);
#endif


    return result;
}

mal_result mal_context_uninit_backend_apis(mal_context* pContext)
{
    mal_result result = MAL_NO_BACKEND;
#ifdef MAL_WIN32
    result = mal_context_uninit_backend_apis__win32(pContext);
#else
    result = mal_context_uninit_backend_apis__nix(pContext);
#endif

    return result;
}

mal_result mal_context_init(mal_backend backends[], mal_uint32 backendCount, mal_context* pContext)
{
    if (pContext == NULL) return MAL_INVALID_ARGS;
    mal_zero_object(pContext);

    // Backend APIs need to be initialized first. This is where external libraries will be loaded and linked.
    mal_result result = mal_context_init_backend_apis(pContext);
    if (result != MAL_SUCCESS) {
        return result;
    }

    static mal_backend defaultBackends[] = {
        mal_backend_dsound,
        mal_backend_wasapi,
        mal_backend_alsa,
        mal_backend_opensl,
        mal_backend_openal,
        mal_backend_null
    };

    if (backends == NULL) {
        backends = defaultBackends;
        backendCount = sizeof(defaultBackends) / sizeof(defaultBackends[0]);
    }

    mal_assert(backends != NULL);

    for (mal_uint32 iBackend = 0; iBackend < backendCount; ++iBackend) {
        mal_backend backend = backends[iBackend];

        switch (backend) {
        #ifdef MAL_ENABLE_WASAPI
            case mal_backend_wasapi:
            {
                result = mal_context_init__wasapi(pContext);
                if (result == MAL_SUCCESS) {
                    pContext->backend = mal_backend_wasapi;
                    return result;
                }
            } break;
        #endif
        #ifdef MAL_ENABLE_DSOUND
            case mal_backend_dsound:
            {
                result = mal_context_init__dsound(pContext);
                if (result == MAL_SUCCESS) {
                    pContext->backend = mal_backend_dsound;
                    return result;
                }
            } break;
        #endif
        #ifdef MAL_ENABLE_ALSA
            case mal_backend_alsa:
            {
                result = mal_context_init__alsa(pContext);
                if (result == MAL_SUCCESS) {
                    pContext->backend = mal_backend_alsa;
                    return result;
                }
            } break;
        #endif
        #ifdef MAL_ENABLE_OPENSLES
            case mal_backend_opensl:
            {
                result = mal_context_init__opensl(pContext);
                if (result == MAL_SUCCESS) {
                    pContext->backend = mal_backend_opensl;
                    return result;
                }
            } break;
        #endif
        #ifdef MAL_ENABLE_OPENAL
            case mal_backend_openal:
            {
                result = mal_context_init__openal(pContext);
                if (result == MAL_SUCCESS) {
                    pContext->backend = mal_backend_openal;
                    return result;
                }
            } break;
        #endif
        #ifdef MAL_ENABLE_NULL
            case mal_backend_null:
            {
                result = mal_context_init__null(pContext);
                if (result == MAL_SUCCESS) {
                    pContext->backend = mal_backend_null;
                    return result;
                }
            } break;
        #endif

            default: break;
        }
    }

    mal_zero_object(pContext);  // Safety.
    return MAL_NO_BACKEND;
}

mal_result mal_context_uninit(mal_context* pContext)
{
    if (pContext == NULL) return MAL_INVALID_ARGS;
    
    switch (pContext->backend) {
    #ifdef MAL_ENABLE_WASAPI
        case mal_backend_wasapi:
        {
            return mal_context_uninit__wasapi(pContext);
        } break;
    #endif
    #ifdef MAL_ENABLE_DSOUND
        case mal_backend_dsound:
        {
            return mal_context_uninit__dsound(pContext);
        } break;
    #endif
    #ifdef MAL_ENABLE_ALSA
        case mal_backend_alsa:
        {
            return mal_context_uninit__alsa(pContext);
        } break;
    #endif
    #ifdef MAL_ENABLE_OPENSLES
        case mal_backend_opensl:
        {
            return mal_context_uninit__opensl(pContext);
        } break;
    #endif
    #ifdef MAL_ENABLE_OPENAL
        case mal_backend_openal:
        {
            return mal_context_uninit__openal(pContext);
        } break;
    #endif
    #ifdef MAL_ENABLE_NULL
        case mal_backend_null:
        {
            return mal_context_uninit__null(pContext);
        } break;
    #endif

        default: break;
    }

    mal_context_uninit_backend_apis(pContext);

    mal_assert(MAL_FALSE);
    return MAL_NO_BACKEND;
}


mal_result mal_enumerate_devices(mal_context* pContext, mal_device_type type, mal_uint32* pCount, mal_device_info* pInfo)
{
    if (pCount == NULL) return mal_post_error(NULL, "mal_enumerate_devices() called with invalid arguments (pCount == 0).", MAL_INVALID_ARGS);

    switch (pContext->backend)
    {
    #ifdef MAL_ENABLE_WASAPI
        case mal_backend_wasapi:
        {
            return mal_enumerate_devices__wasapi(pContext, type, pCount, pInfo);
        } break;
    #endif
    #ifdef MAL_ENABLE_DSOUND
        case mal_backend_dsound:
        {
            return mal_enumerate_devices__dsound(pContext, type, pCount, pInfo);
        } break;
    #endif
    #ifdef MAL_ENABLE_ALSA
        case mal_backend_alsa:
        {
            return mal_enumerate_devices__alsa(pContext, type, pCount, pInfo);
        } break;
    #endif
    #ifdef MAL_ENABLE_OPENSLES
        case mal_backend_opensl:
        {
            return mal_enumerate_devices__opensl(pContext, type, pCount, pInfo);
        } break;
    #endif
    #ifdef MAL_ENABLE_OPENAL
        case mal_backend_openal:
        {
            return mal_enumerate_devices__openal(pContext, type, pCount, pInfo);
        } break;
    #endif
    #ifdef MAL_ENABLE_NULL
        case mal_backend_null:
        {
            return mal_enumerate_devices__null(pContext, type, pCount, pInfo);
        } break;
    #endif

        default: break;
    }

    mal_assert(MAL_FALSE);
    return MAL_NO_BACKEND;
}

mal_result mal_device_init(mal_context* pContext, mal_device_type type, mal_device_id* pDeviceID, mal_device_config* pConfig, void* pUserData, mal_device* pDevice)
{
    if (pDevice  == NULL) return mal_post_error(pDevice, "mal_device_init() called with invalid arguments (pDevice == NULL).",  MAL_INVALID_ARGS);

    mal_zero_object(pDevice);
    pDevice->pContext = pContext;

    // Set the user data and log callback ASAP to ensure it is available for the entire initialization process.
    pDevice->pUserData = pUserData;
    pDevice->onLog  = pConfig->onLogCallback;
    pDevice->onStop = pConfig->onStopCallback;
    pDevice->onSend = pConfig->onSendCallback;
    pDevice->onRecv = pConfig->onRecvCallback;

    if (((mal_uint64)pDevice % sizeof(pDevice)) != 0) {
        if (pDevice->onLog) {
            pDevice->onLog(pDevice, "WARNING: mal_device_init() called for a device that is not properly aligned. Thread safety is not supported.");
        }
    }


    if (pContext == NULL) {
        return mal_post_error(pDevice, "mal_device_init() called with invalid arguments (pContext == NULL).", MAL_INVALID_ARGS);
    }


    // Basic config validation.
    if (pConfig  == NULL) {
        return mal_post_error(pDevice, "mal_device_init() called with invalid arguments (pConfig == NULL).",  MAL_INVALID_ARGS);
    }

    if (pConfig->channels == 0) {
        return mal_post_error(pDevice, "mal_device_init() called with an invalid config. Channel count must be greater than 0.", MAL_INVALID_DEVICE_CONFIG);
    }
    if (pConfig->channels > MAL_MAX_CHANNELS) {
        return mal_post_error(pDevice, "mal_device_init() called with an invalid config. Channel count cannot exceed 18.", MAL_INVALID_DEVICE_CONFIG);
    }

    if (pConfig->sampleRate == 0) {
        return mal_post_error(pDevice, "mal_device_init() called with an invalid config. Sample rate must be greater than 0.", MAL_INVALID_DEVICE_CONFIG);
    }

    if (!mal__is_channel_map_valid(pConfig->channelMap, pConfig->channels)) {
        return mal_post_error(pDevice, "mal_device_init() called with invalid arguments. Channel map is invalid.", MAL_INVALID_DEVICE_CONFIG);
    }


    // Default buffer size and periods.
    if (pConfig->bufferSizeInFrames == 0) {
        pConfig->bufferSizeInFrames = (pConfig->sampleRate/1000) * MAL_DEFAULT_BUFFER_SIZE_IN_MILLISECONDS;
        pDevice->flags |= MAL_DEVICE_FLAG_USING_DEFAULT_BUFFER_SIZE;
    }
    if (pConfig->periods == 0) {
        pConfig->periods = MAL_DEFAULT_PERIODS;
        pDevice->flags |= MAL_DEVICE_FLAG_USING_DEFAULT_PERIODS;
    }

    pDevice->type = type;
    pDevice->format = pConfig->format;
    pDevice->channels = pConfig->channels;
    mal_copy_memory(pDevice->channelMap, pConfig->channelMap, sizeof(pConfig->channelMap[0]) * pConfig->channels);
    pDevice->sampleRate = pConfig->sampleRate;
    pDevice->bufferSizeInFrames = pConfig->bufferSizeInFrames;
    pDevice->periods = pConfig->periods;
    
    // The internal format, channel count and sample rate can be modified by the backend.
    pDevice->internalFormat = pDevice->format;
    pDevice->internalChannels = pDevice->channels;
    pDevice->internalSampleRate = pDevice->sampleRate;
    mal_copy_memory(pDevice->internalChannelMap, pDevice->channelMap, sizeof(pDevice->channelMap));

    if (!mal_mutex_create(&pDevice->lock)) {
        return mal_post_error(pDevice, "Failed to create mutex.", MAL_FAILED_TO_CREATE_MUTEX);
    }

    // When the device is started, the worker thread is the one that does the actual startup of the backend device. We
    // use a semaphore to wait for the background thread to finish the work. The same applies for stopping the device.
    //
    // Each of these semaphores is released internally by the worker thread when the work is completed. The start
    // semaphore is also used to wake up the worker thread.
    if (!mal_event_create(&pDevice->wakeupEvent)) {
        mal_mutex_delete(&pDevice->lock);
        return mal_post_error(pDevice, "Failed to create worker thread wakeup event.", MAL_FAILED_TO_CREATE_EVENT);
    }
    if (!mal_event_create(&pDevice->startEvent)) {
        mal_event_delete(&pDevice->wakeupEvent);
        mal_mutex_delete(&pDevice->lock);
        return mal_post_error(pDevice, "Failed to create worker thread start event.", MAL_FAILED_TO_CREATE_EVENT);
    }
    if (!mal_event_create(&pDevice->stopEvent)) {
        mal_event_delete(&pDevice->startEvent);
        mal_event_delete(&pDevice->wakeupEvent);
        mal_mutex_delete(&pDevice->lock);
        return mal_post_error(pDevice, "Failed to create worker thread stop event.", MAL_FAILED_TO_CREATE_EVENT);
    }


    mal_result result = MAL_NO_BACKEND;
    switch (pContext->backend)
    {
    #ifdef MAL_ENABLE_WASAPI
        case mal_backend_wasapi:
        {
            result = mal_device_init__wasapi(pContext, type, pDeviceID, pConfig, pDevice);
        } break;
    #endif
    #ifdef MAL_ENABLE_DSOUND
        case mal_backend_dsound:
        {
            result = mal_device_init__dsound(pContext, type, pDeviceID, pConfig, pDevice);
        } break;
    #endif
    #ifdef MAL_ENABLE_ALSA
        case mal_backend_alsa:
        {
            result = mal_device_init__alsa(pContext, type, pDeviceID, pConfig, pDevice);
        } break;
    #endif
    #ifdef MAL_ENABLE_OPENSLES
        case mal_backend_opensl:
        {
            result = mal_device_init__opensl(pContext, type, pDeviceID, pConfig, pDevice);
        } break;
    #endif
    #ifdef MAL_ENABLE_OPENAL
        case mal_backend_openal:
        {
            result = mal_device_init__openal(pContext, type, pDeviceID, pConfig, pDevice);
        } break;
    #endif
    #ifdef MAL_ENABLE_NULL
        case mal_backend_null:
        {
            result = mal_device_init__null(pContext, type, pDeviceID, pConfig, pDevice);
        } break;
    #endif

        default: break;
    }

    if (result != MAL_SUCCESS) {
        return MAL_NO_BACKEND;  // The error message will have been posted with mal_post_error() by the source of the error so don't bother calling it here.
    }


    // We need a DSP object which is where samples are moved through in order to convert them to the
    // format required by the backend.
    mal_dsp_config dspConfig;
    dspConfig.cacheSizeInFrames = pDevice->bufferSizeInFrames;
    if (type == mal_device_type_playback) {
        dspConfig.formatIn      = pDevice->format;
        dspConfig.channelsIn    = pDevice->channels;
        dspConfig.sampleRateIn  = pDevice->sampleRate;
        mal_copy_memory(dspConfig.channelMapIn, pDevice->channelMap, sizeof(dspConfig.channelMapIn));
        dspConfig.formatOut     = pDevice->internalFormat;
        dspConfig.channelsOut   = pDevice->internalChannels;
        dspConfig.sampleRateOut = pDevice->internalSampleRate;
        mal_copy_memory(dspConfig.channelMapOut, pDevice->internalChannelMap, sizeof(dspConfig.channelMapOut));
        mal_dsp_init(&dspConfig, mal_device__on_read_from_client, pDevice, &pDevice->dsp);
    } else {
        dspConfig.formatIn      = pDevice->internalFormat;
        dspConfig.channelsIn    = pDevice->internalChannels;
        dspConfig.sampleRateIn  = pDevice->internalSampleRate;
        mal_copy_memory(dspConfig.channelMapIn, pDevice->internalChannelMap, sizeof(dspConfig.channelMapIn));
        dspConfig.formatOut     = pDevice->format;
        dspConfig.channelsOut   = pDevice->channels;
        dspConfig.sampleRateOut = pDevice->sampleRate;
        mal_copy_memory(dspConfig.channelMapOut, pDevice->channelMap, sizeof(dspConfig.channelMapOut));
        mal_dsp_init(&dspConfig, mal_device__on_read_from_device, pDevice, &pDevice->dsp);
    }

    


    // Some backends don't require the worker thread.
    if (pContext->backend != mal_backend_opensl) {
        // The worker thread.
        if (!mal_thread_create(&pDevice->thread, mal_worker_thread, pDevice)) {
            mal_device_uninit(pDevice);
            return mal_post_error(pDevice, "Failed to create worker thread.", MAL_FAILED_TO_CREATE_THREAD);
        }

        // Wait for the worker thread to put the device into it's stopped state for real.
        mal_event_wait(&pDevice->stopEvent);
    } else {
        mal_device__set_state(pDevice, MAL_STATE_STOPPED);
    }

    mal_assert(mal_device__get_state(pDevice) == MAL_STATE_STOPPED);
    return MAL_SUCCESS;
}

void mal_device_uninit(mal_device* pDevice)
{
    if (!mal_device__is_initialized(pDevice)) return;

    // Make sure the device is stopped first. The backends will probably handle this naturally,
    // but I like to do it explicitly for my own sanity.
    if (mal_device_is_started(pDevice)) {
        while (mal_device_stop(pDevice) == MAL_DEVICE_BUSY) {
            mal_sleep(1);
        }
    }

    // Putting the device into an uninitialized state will make the worker thread return.
    mal_device__set_state(pDevice, MAL_STATE_UNINITIALIZED);

    // Wake up the worker thread and wait for it to properly terminate.
    if (pDevice->pContext->backend != mal_backend_opensl) {
        mal_event_signal(&pDevice->wakeupEvent);
        mal_thread_wait(&pDevice->thread);
    }

    mal_event_delete(&pDevice->stopEvent);
    mal_event_delete(&pDevice->startEvent);
    mal_event_delete(&pDevice->wakeupEvent);
    mal_mutex_delete(&pDevice->lock);

#ifdef MAL_ENABLE_WASAPI
    if (pDevice->pContext->backend == mal_backend_wasapi) {
        mal_device_uninit__wasapi(pDevice);
    }
#endif
#ifdef MAL_ENABLE_DSOUND
    if (pDevice->pContext->backend == mal_backend_dsound) {
        mal_device_uninit__dsound(pDevice);
    }
#endif
#ifdef MAL_ENABLE_ALSA
    if (pDevice->pContext->backend == mal_backend_alsa) {
        mal_device_uninit__alsa(pDevice);
    }
#endif
#ifdef MAL_ENABLE_OPENSLES
    if (pDevice->pContext->backend == mal_backend_opensl) {
        mal_device_uninit__opensl(pDevice);
    }
#endif
#ifdef MAL_ENABLE_OPENAL
    if (pDevice->pContext->backend == mal_backend_openal) {
        mal_device_uninit__openal(pDevice);
    }
#endif
#ifdef MAL_ENABLE_NULL
    if (pDevice->pContext->backend == mal_backend_null) {
        mal_device_uninit__null(pDevice);
    }
#endif

    mal_zero_object(pDevice);
}

void mal_device_set_recv_callback(mal_device* pDevice, mal_recv_proc proc)
{
    if (pDevice == NULL) return;
    mal_atomic_exchange_ptr(&pDevice->onRecv, proc);
}

void mal_device_set_send_callback(mal_device* pDevice, mal_send_proc proc)
{
    if (pDevice == NULL) return;
    mal_atomic_exchange_ptr(&pDevice->onSend, proc);
}

void mal_device_set_stop_callback(mal_device* pDevice, mal_stop_proc proc)
{
    if (pDevice == NULL) return;
    mal_atomic_exchange_ptr(&pDevice->onStop, proc);
}

mal_result mal_device_start(mal_device* pDevice)
{
    if (pDevice == NULL) return mal_post_error(pDevice, "mal_device_start() called with invalid arguments (pDevice == NULL).", MAL_INVALID_ARGS);
    if (mal_device__get_state(pDevice) == MAL_STATE_UNINITIALIZED) return mal_post_error(pDevice, "mal_device_start() called for an uninitialized device.", MAL_DEVICE_NOT_INITIALIZED);
    
    mal_result result = MAL_ERROR;
    mal_mutex_lock(&pDevice->lock);
    {
        // Be a bit more descriptive if the device is already started or is already in the process of starting. This is likely
        // a bug with the application.
        if (mal_device__get_state(pDevice) == MAL_STATE_STARTING) {
            mal_mutex_unlock(&pDevice->lock);
            return mal_post_error(pDevice, "mal_device_start() called while another thread is already starting it.", MAL_DEVICE_ALREADY_STARTING);
        }
        if (mal_device__get_state(pDevice) == MAL_STATE_STARTED) {
            mal_mutex_unlock(&pDevice->lock);
            return mal_post_error(pDevice, "mal_device_start() called for a device that's already started.", MAL_DEVICE_ALREADY_STARTED);
        }

        // The device needs to be in a stopped state. If it's not, we just let the caller know the device is busy.
        if (mal_device__get_state(pDevice) != MAL_STATE_STOPPED) {
            mal_mutex_unlock(&pDevice->lock);
            return mal_post_error(pDevice, "mal_device_start() called while another thread is in the process of stopping it.", MAL_DEVICE_BUSY);
        }

        mal_device__set_state(pDevice, MAL_STATE_STARTING);

        // Asynchronous backends need to be handled differently.
#ifdef MAL_ENABLE_OPENSLES
        if (pDevice->pContext->backend == mal_backend_opensl) {
            mal_device__start_backend__opensl(pDevice);
            mal_device__set_state(pDevice, MAL_STATE_STARTED);
        } else
#endif
        // Synchronous backends.
        {
            mal_event_signal(&pDevice->wakeupEvent);

            // Wait for the worker thread to finish starting the device. Note that the worker thread will be the one
            // who puts the device into the started state. Don't call mal_device__set_state() here.
            mal_event_wait(&pDevice->startEvent);
            result = pDevice->workResult;
        }
    }
    mal_mutex_unlock(&pDevice->lock);

    return result;
}

mal_result mal_device_stop(mal_device* pDevice)
{
    if (pDevice == NULL) return mal_post_error(pDevice, "mal_device_stop() called with invalid arguments (pDevice == NULL).", MAL_INVALID_ARGS);
    if (mal_device__get_state(pDevice) == MAL_STATE_UNINITIALIZED) return mal_post_error(pDevice, "mal_device_stop() called for an uninitialized device.", MAL_DEVICE_NOT_INITIALIZED);

    mal_result result = MAL_ERROR;
    mal_mutex_lock(&pDevice->lock);
    {
        // Be a bit more descriptive if the device is already stopped or is already in the process of stopping. This is likely
        // a bug with the application.
        if (mal_device__get_state(pDevice) == MAL_STATE_STOPPING) {
            mal_mutex_unlock(&pDevice->lock);
            return mal_post_error(pDevice, "mal_device_stop() called while another thread is already stopping it.", MAL_DEVICE_ALREADY_STOPPING);
        }
        if (mal_device__get_state(pDevice) == MAL_STATE_STOPPED) {
            mal_mutex_unlock(&pDevice->lock);
            return mal_post_error(pDevice, "mal_device_stop() called for a device that's already stopped.", MAL_DEVICE_ALREADY_STOPPED);
        }

        // The device needs to be in a started state. If it's not, we just let the caller know the device is busy.
        if (mal_device__get_state(pDevice) != MAL_STATE_STARTED) {
            mal_mutex_unlock(&pDevice->lock);
            return mal_post_error(pDevice, "mal_device_stop() called while another thread is in the process of starting it.", MAL_DEVICE_BUSY);
        }

        mal_device__set_state(pDevice, MAL_STATE_STOPPING);

        // There's no need to wake up the thread like we do when starting.

        // Asynchronous backends need to be handled differently.
#ifdef MAL_ENABLE_OPENSLES
        if (pDevice->pContext->backend == mal_backend_opensl) {
            mal_device__stop_backend__opensl(pDevice);
        } else
#endif
        // Synchronous backends.
        {
            // When we get here the worker thread is likely in a wait state while waiting for the backend device to deliver or request
            // audio data. We need to force these to return as quickly as possible.
            mal_device__break_main_loop(pDevice);

            // We need to wait for the worker thread to become available for work before returning. Note that the worker thread will be
            // the one who puts the device into the stopped state. Don't call mal_device__set_state() here.
            mal_event_wait(&pDevice->stopEvent);
            result = MAL_SUCCESS;
        }
    }
    mal_mutex_unlock(&pDevice->lock);

    return result;
}

mal_bool32 mal_device_is_started(mal_device* pDevice)
{
    if (pDevice == NULL) return MAL_FALSE;
    return mal_device__get_state(pDevice) == MAL_STATE_STARTED;
}

mal_uint32 mal_device_get_buffer_size_in_bytes(mal_device* pDevice)
{
    if (pDevice == NULL) return 0;
    return pDevice->bufferSizeInFrames * pDevice->channels * mal_get_sample_size_in_bytes(pDevice->format);
}

mal_uint32 mal_get_sample_size_in_bytes(mal_format format)
{
    mal_uint32 sizes[] = {
        1,  // u8
        2,  // s16
        3,  // s24
        4,  // s32
        4,  // f32
    };
    return sizes[format];
}

mal_device_config mal_device_config_init(mal_format format, mal_uint32 channels, mal_uint32 sampleRate, mal_recv_proc onRecvCallback, mal_send_proc onSendCallback)
{
    mal_device_config config;
    mal_zero_object(&config);

    config.format = format;
    config.channels = channels;
    config.sampleRate = sampleRate;
    config.onRecvCallback = onRecvCallback;
    config.onSendCallback = onSendCallback;

    switch (channels)
    {
        case 1:
        {
            config.channelMap[0] = MAL_CHANNEL_FRONT_CENTER;
        } break;

        case 2:
        {
            config.channelMap[0] = MAL_CHANNEL_FRONT_LEFT;
            config.channelMap[1] = MAL_CHANNEL_FRONT_RIGHT;
        } break;

        case 3:
        {
            config.channelMap[0] = MAL_CHANNEL_FRONT_LEFT;
            config.channelMap[1] = MAL_CHANNEL_FRONT_RIGHT;
            config.channelMap[2] = MAL_CHANNEL_LFE;
        } break;

        case 4:
        {
            config.channelMap[0] = MAL_CHANNEL_FRONT_LEFT;
            config.channelMap[1] = MAL_CHANNEL_FRONT_RIGHT;
            config.channelMap[2] = MAL_CHANNEL_BACK_LEFT;
            config.channelMap[3] = MAL_CHANNEL_BACK_RIGHT;
        } break;

        case 5:
        {
            config.channelMap[0] = MAL_CHANNEL_FRONT_LEFT;
            config.channelMap[1] = MAL_CHANNEL_FRONT_RIGHT;
            config.channelMap[2] = MAL_CHANNEL_BACK_LEFT;
            config.channelMap[3] = MAL_CHANNEL_BACK_RIGHT;
            config.channelMap[4] = MAL_CHANNEL_LFE;
        } break;

        case 6:
        {
            config.channelMap[0] = MAL_CHANNEL_FRONT_LEFT;
            config.channelMap[1] = MAL_CHANNEL_FRONT_RIGHT;
            config.channelMap[2] = MAL_CHANNEL_FRONT_CENTER;
            config.channelMap[3] = MAL_CHANNEL_LFE;
            config.channelMap[4] = MAL_CHANNEL_BACK_LEFT;
            config.channelMap[5] = MAL_CHANNEL_BACK_RIGHT;
        } break;

        case 8:
        {
            config.channelMap[0] = MAL_CHANNEL_FRONT_LEFT;
            config.channelMap[1] = MAL_CHANNEL_FRONT_RIGHT;
            config.channelMap[2] = MAL_CHANNEL_FRONT_CENTER;
            config.channelMap[3] = MAL_CHANNEL_LFE;
            config.channelMap[4] = MAL_CHANNEL_BACK_LEFT;
            config.channelMap[5] = MAL_CHANNEL_BACK_RIGHT;
            config.channelMap[6] = MAL_CHANNEL_SIDE_LEFT;
            config.channelMap[7] = MAL_CHANNEL_SIDE_RIGHT;
        } break;

        default:
        {
            // Just leave it all blank in this case. This will use the same mapping as the device's native mapping.
        } break;
    }

    return config;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SRC
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void mal_src_cache_init(mal_src* pSRC, mal_src_cache* pCache)
{
    mal_assert(pSRC != NULL);
    mal_assert(pCache != NULL);

    pCache->pSRC = pSRC;
    pCache->cachedFrameCount = 0;
    pCache->iNextFrame = 0;
}

mal_uint32 mal_src_cache_read_frames(mal_src_cache* pCache, mal_uint32 frameCount, float* pFramesOut)
{
    mal_assert(pCache != NULL);
    mal_assert(pCache->pSRC != NULL);
    mal_assert(pCache->pSRC->onRead != NULL);
    mal_assert(frameCount > 0);
    mal_assert(pFramesOut != NULL);

    mal_uint32 channels = pCache->pSRC->config.channels;

    mal_uint32 totalFramesRead = 0;
    while (frameCount > 0) {
        // If there's anything in memory go ahead and copy that over first.
        mal_uint32 framesRemainingInMemory = pCache->cachedFrameCount - pCache->iNextFrame;
        mal_uint32 framesToReadFromMemory = frameCount;
        if (framesToReadFromMemory > framesRemainingInMemory) {
            framesToReadFromMemory = framesRemainingInMemory;
        }

        mal_copy_memory(pFramesOut, pCache->pCachedFrames + pCache->iNextFrame*channels, framesToReadFromMemory * channels * sizeof(float));
        pCache->iNextFrame += framesToReadFromMemory;

        totalFramesRead += framesToReadFromMemory;
        frameCount -= framesToReadFromMemory;
        if (frameCount == 0) {
            break;
        }


        // At this point there are still more frames to read from the client, so we'll need to reload the cache with fresh data.
        mal_assert(frameCount > 0);
        pFramesOut += framesToReadFromMemory * channels;

        pCache->iNextFrame = 0;
        pCache->cachedFrameCount = 0;
        if (pCache->pSRC->config.formatIn == mal_format_f32) {
            // No need for a conversion - read straight into the cache.
            mal_uint32 framesToReadFromClient = mal_countof(pCache->pCachedFrames) / pCache->pSRC->config.channels;
            if (framesToReadFromClient > pCache->pSRC->config.cacheSizeInFrames) {
                framesToReadFromClient = pCache->pSRC->config.cacheSizeInFrames;
            }

            pCache->cachedFrameCount = pCache->pSRC->onRead(framesToReadFromClient, pCache->pCachedFrames, pCache->pSRC->pUserData);
        } else {
            // A format conversion is required which means we need to use an intermediary buffer.
            mal_uint8 pIntermediaryBuffer[sizeof(pCache->pCachedFrames)];
            mal_uint32 framesToReadFromClient = mal_min(mal_buffer_frame_capacity(pIntermediaryBuffer, channels, pCache->pSRC->config.formatIn), mal_buffer_frame_capacity(pCache->pCachedFrames, channels, mal_format_f32));
            if (framesToReadFromClient > pCache->pSRC->config.cacheSizeInFrames) {
                framesToReadFromClient = pCache->pSRC->config.cacheSizeInFrames;
            }

            pCache->cachedFrameCount = pCache->pSRC->onRead(framesToReadFromClient, pIntermediaryBuffer, pCache->pSRC->pUserData);

            // Convert to f32.
            mal_pcm_convert(pCache->pCachedFrames, mal_format_f32, pIntermediaryBuffer, pCache->pSRC->config.formatIn, pCache->cachedFrameCount * channels);
        }


        // Get out of this loop if nothing was able to be retrieved.
        if (pCache->cachedFrameCount == 0) {
            break;
        }
    }

    return totalFramesRead;
}


mal_uint32 mal_src_read_frames_passthrough(mal_src* pSRC, mal_uint32 frameCount, void* pFramesOut);
mal_uint32 mal_src_read_frames_linear(mal_src* pSRC, mal_uint32 frameCount, void* pFramesOut);

mal_result mal_src_init(mal_src_config* pConfig, mal_src_read_proc onRead, void* pUserData, mal_src* pSRC)
{
    if (pSRC == NULL) return MAL_INVALID_ARGS;
    mal_zero_object(pSRC);

    if (pConfig == NULL || onRead == NULL) return MAL_INVALID_ARGS;
    if (pConfig->channels == 0 || pConfig->channels > MAL_MAX_CHANNELS) return MAL_INVALID_ARGS;

    pSRC->config = *pConfig;
    pSRC->onRead = onRead;
    pSRC->pUserData = pUserData;

    // If the in and out sample rates are the same, fall back to the passthrough algorithm.
    if (pSRC->config.sampleRateIn == pSRC->config.sampleRateOut) {
        pSRC->config.algorithm = mal_src_algorithm_none;
    }

    if (pSRC->config.cacheSizeInFrames > MAL_SRC_CACHE_SIZE_IN_FRAMES || pSRC->config.cacheSizeInFrames == 0) {
        pSRC->config.cacheSizeInFrames = MAL_SRC_CACHE_SIZE_IN_FRAMES;
    }

    pSRC->ratio = (float)pSRC->config.sampleRateIn / pSRC->config.sampleRateOut;

    mal_src_cache_init(pSRC, &pSRC->cache);
    return MAL_SUCCESS;
}

mal_uint32 mal_src_read_frames(mal_src* pSRC, mal_uint32 frameCount, void* pFramesOut)
{
    if (pSRC == NULL || frameCount == 0 || pFramesOut == NULL) return 0;

    // Could just use a function pointer instead of a switch for this...
    switch (pSRC->config.algorithm)
    {
        case mal_src_algorithm_none:   return mal_src_read_frames_passthrough(pSRC, frameCount, pFramesOut);
        case mal_src_algorithm_linear: return mal_src_read_frames_linear(pSRC, frameCount, pFramesOut);
        default: return 0;
    }
}

mal_uint32 mal_src_read_frames_passthrough(mal_src* pSRC, mal_uint32 frameCount, void* pFramesOut)
{
    mal_assert(pSRC != NULL);
    mal_assert(frameCount > 0);
    mal_assert(pFramesOut != NULL);

    // Fast path. No need for data conversion - just pass right through.
    if (pSRC->config.formatIn == pSRC->config.formatOut) {
        return pSRC->onRead(frameCount, pFramesOut, pSRC->pUserData);
    }

    // Slower path. Need to do a format conversion.
    mal_uint32 totalFramesRead = 0;
    while (frameCount > 0) {
        mal_uint8 pStagingBuffer[MAL_MAX_CHANNELS * 2048];
        mal_uint32 stagingBufferSizeInFrames = sizeof(pStagingBuffer) / mal_get_sample_size_in_bytes(pSRC->config.formatIn) / pSRC->config.channels;
        mal_uint32 framesToRead = stagingBufferSizeInFrames;
        if (framesToRead > frameCount) {
            framesToRead = frameCount;
        }

        mal_uint32 framesRead = pSRC->onRead(framesToRead, pStagingBuffer, pSRC->pUserData);
        if (framesRead == 0) {
            break;
        }

        mal_pcm_convert(pFramesOut, pSRC->config.formatOut, pStagingBuffer, pSRC->config.formatIn, framesRead * pSRC->config.channels);

        pFramesOut  = (mal_uint8*)pFramesOut + (framesRead * pSRC->config.channels * mal_get_sample_size_in_bytes(pSRC->config.formatOut));
        frameCount -= framesRead;
        totalFramesRead += framesRead;
    }

    return totalFramesRead;
}

mal_uint32 mal_src_read_frames_linear(mal_src* pSRC, mal_uint32 frameCount, void* pFramesOut)
{
    mal_assert(pSRC != NULL);
    mal_assert(frameCount > 0);
    mal_assert(pFramesOut != NULL);

    // Load the bin if it's not been loaded yet.
    if (!pSRC->linear.isBinLoaded) {
        mal_uint32 framesRead = mal_src_cache_read_frames(&pSRC->cache, 2, pSRC->bin);
        if (framesRead == 0) {
            return 0;
        }

        if (framesRead == 1) {
            mal_pcm_convert(pFramesOut, pSRC->config.formatOut, pSRC->bin, mal_format_f32, framesRead * pSRC->config.channels);
            return framesRead;
        }

        pSRC->linear.isBinLoaded = MAL_TRUE;
    }


    float factor = pSRC->ratio;

    mal_uint32 totalFramesRead = 0;
    while (frameCount > 0) {
        // The bin is where the previous and next frames are located.
        float* pPrevFrame = pSRC->bin;
        float* pNextFrame = pSRC->bin + pSRC->config.channels;

        float pFrame[MAL_MAX_CHANNELS];
        mal_blend_f32(pFrame, pPrevFrame, pNextFrame, pSRC->linear.alpha, pSRC->config.channels);

        pSRC->linear.alpha += factor;

        // The new alpha value is how we determine whether or not we need to read fresh frames.
        mal_uint32 framesToReadFromClient = (mal_uint32)pSRC->linear.alpha;
        pSRC->linear.alpha = pSRC->linear.alpha - framesToReadFromClient;

        for (mal_uint32 i = 0; i < framesToReadFromClient; ++i) {
            for (mal_uint32 j = 0; j < pSRC->config.channels; ++j) {
                pPrevFrame[j] = pNextFrame[j];
            }

            mal_uint32 framesRead = mal_src_cache_read_frames(&pSRC->cache, 1, pNextFrame);
            if (framesRead == 0) {
                for (mal_uint32 j = 0; j < pSRC->config.channels; ++j) {
                    pNextFrame[j] = 0;
                }

                pSRC->linear.isBinLoaded = MAL_FALSE;
                return totalFramesRead; // We've exhausted the client data.
            }
        }
        
        mal_pcm_convert(pFramesOut, pSRC->config.formatOut, pFrame, mal_format_f32, 1 * pSRC->config.channels);

        pFramesOut  = (mal_uint8*)pFramesOut + (1 * pSRC->config.channels * mal_get_sample_size_in_bytes(pSRC->config.formatOut));
        frameCount -= 1;
        totalFramesRead += 1;
    }

    return totalFramesRead;
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FORMAT CONVERSION
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void mal_pcm_u8_to_s16(short* pOut, const unsigned char* pIn, unsigned int count);
void mal_pcm_u8_to_s24(void* pOut, const unsigned char* pIn, unsigned int count);
void mal_pcm_u8_to_s32(int* pOut, const unsigned char* pIn, unsigned int count);
void mal_pcm_u8_to_f32(float* pOut, const unsigned char* pIn, unsigned int count);
void mal_pcm_s16_to_u8(unsigned char* pOut, const short* pIn, unsigned int count);
void mal_pcm_s16_to_s24(void* pOut, const short* pIn, unsigned int count);
void mal_pcm_s16_to_s32(int* pOut, const short* pIn, unsigned int count);
void mal_pcm_s16_to_f32(float* pOut, const short* pIn, unsigned int count);
void mal_pcm_s24_to_u8(unsigned char* pOut, const void* pIn, unsigned int count);
void mal_pcm_s24_to_s16(short* pOut, const void* pIn, unsigned int count);
void mal_pcm_s24_to_s32(int* pOut, const void* pIn, unsigned int count);
void mal_pcm_s24_to_f32(float* pOut, const void* pIn, unsigned int count);
void mal_pcm_s32_to_u8(unsigned char* pOut, const int* pIn, unsigned int count);
void mal_pcm_s32_to_s16(short* pOut, const int* pIn, unsigned int count);
void mal_pcm_s32_to_s24(void* pOut, const int* pIn, unsigned int count);
void mal_pcm_s32_to_f32(float* pOut, const int* pIn, unsigned int count);
void mal_pcm_f32_to_u8(unsigned char* pOut, const float* pIn, unsigned int count);
void mal_pcm_f32_to_s16(short* pOut, const float* pIn, unsigned int count);
void mal_pcm_f32_to_s24(void* pOut, const float* pIn, unsigned int count);
void mal_pcm_f32_to_s32(int* pOut, const float* pIn, unsigned int count);

void mal_pcm_convert(void* pOut, mal_format formatOut, const void* pIn, mal_format formatIn, unsigned int sampleCount)
{
    if (formatOut == formatIn) {
        mal_copy_memory(pOut, pIn, sampleCount * mal_get_sample_size_in_bytes(formatOut));
        return;
    }

    switch (formatIn)
    {
        case mal_format_u8:
        {
            switch (formatOut)
            {
                case mal_format_s16: mal_pcm_u8_to_s16((short*)pOut, (const unsigned char*)pIn, sampleCount); return;
                case mal_format_s24: mal_pcm_u8_to_s24(        pOut, (const unsigned char*)pIn, sampleCount); return;
                case mal_format_s32: mal_pcm_u8_to_s32(  (int*)pOut, (const unsigned char*)pIn, sampleCount); return;
                case mal_format_f32: mal_pcm_u8_to_f32((float*)pOut, (const unsigned char*)pIn, sampleCount); return;
                default: break;
            }
        } break;

        case mal_format_s16:
        {
            switch (formatOut)
            {
                case mal_format_u8:  mal_pcm_s16_to_u8( (unsigned char*)pOut, (const short*)pIn, sampleCount); return;
                case mal_format_s24: mal_pcm_s16_to_s24(                pOut, (const short*)pIn, sampleCount); return;
                case mal_format_s32: mal_pcm_s16_to_s32(          (int*)pOut, (const short*)pIn, sampleCount); return;
                case mal_format_f32: mal_pcm_s16_to_f32(        (float*)pOut, (const short*)pIn, sampleCount); return;
                default: break;
            }
        } break;

        case mal_format_s24:
        {
            switch (formatOut)
            {
                case mal_format_u8:  mal_pcm_s24_to_u8( (unsigned char*)pOut, pIn, sampleCount); return;
                case mal_format_s16: mal_pcm_s24_to_s16(        (short*)pOut, pIn, sampleCount); return;
                case mal_format_s32: mal_pcm_s24_to_s32(          (int*)pOut, pIn, sampleCount); return;
                case mal_format_f32: mal_pcm_s24_to_f32(        (float*)pOut, pIn, sampleCount); return;
                default: break;
            }
        } break;

        case mal_format_s32:
        {
            switch (formatOut)
            {
                case mal_format_u8:  mal_pcm_s32_to_u8( (unsigned char*)pOut, (const int*)pIn, sampleCount); return;
                case mal_format_s16: mal_pcm_s32_to_s16(        (short*)pOut, (const int*)pIn, sampleCount); return;
                case mal_format_s24: mal_pcm_s32_to_s24(                pOut, (const int*)pIn, sampleCount); return;
                case mal_format_f32: mal_pcm_s32_to_f32(        (float*)pOut, (const int*)pIn, sampleCount); return;
                default: break;
            }
        } break;

        case mal_format_f32:
        {
            switch (formatOut)
            {
                case mal_format_u8:  mal_pcm_f32_to_u8( (unsigned char*)pOut, (const float*)pIn, sampleCount); return;
                case mal_format_s16: mal_pcm_f32_to_s16(        (short*)pOut, (const float*)pIn, sampleCount); return;
                case mal_format_s24: mal_pcm_f32_to_s24(                pOut, (const float*)pIn, sampleCount); return;
                case mal_format_s32: mal_pcm_f32_to_s32(          (int*)pOut, (const float*)pIn, sampleCount); return;
                default: break;
            }
        } break;

        default: break;
    }
}


static void mal_rearrange_channels_u8(mal_uint8* pFrame, mal_uint32 channels, mal_uint8 channelMap[MAL_MAX_CHANNELS])
{
    mal_uint8 temp[MAL_MAX_CHANNELS];
    mal_copy_memory(temp, pFrame, sizeof(temp[0]) * channels);

    switch (channels) {
        case 18: pFrame[17] = temp[channelMap[17]];
        case 17: pFrame[16] = temp[channelMap[16]];
        case 16: pFrame[15] = temp[channelMap[15]];
        case 15: pFrame[14] = temp[channelMap[14]];
        case 14: pFrame[13] = temp[channelMap[13]];
        case 13: pFrame[12] = temp[channelMap[12]];
        case 12: pFrame[11] = temp[channelMap[11]];
        case 11: pFrame[10] = temp[channelMap[10]];
        case 10: pFrame[ 9] = temp[channelMap[ 9]];
        case  9: pFrame[ 8] = temp[channelMap[ 8]];
        case  8: pFrame[ 7] = temp[channelMap[ 7]];
        case  7: pFrame[ 6] = temp[channelMap[ 6]];
        case  6: pFrame[ 5] = temp[channelMap[ 5]];
        case  5: pFrame[ 4] = temp[channelMap[ 4]];
        case  4: pFrame[ 3] = temp[channelMap[ 3]];
        case  3: pFrame[ 2] = temp[channelMap[ 2]];
        case  2: pFrame[ 1] = temp[channelMap[ 1]];
        case  1: pFrame[ 0] = temp[channelMap[ 0]];
    }
}

static void mal_rearrange_channels_s16(mal_int16* pFrame, mal_uint32 channels, mal_uint8 channelMap[MAL_MAX_CHANNELS])
{
    mal_int16 temp[MAL_MAX_CHANNELS];
    mal_copy_memory(temp, pFrame, sizeof(temp[0]) * channels);

    switch (channels) {
        case 18: pFrame[17] = temp[channelMap[17]];
        case 17: pFrame[16] = temp[channelMap[16]];
        case 16: pFrame[15] = temp[channelMap[15]];
        case 15: pFrame[14] = temp[channelMap[14]];
        case 14: pFrame[13] = temp[channelMap[13]];
        case 13: pFrame[12] = temp[channelMap[12]];
        case 12: pFrame[11] = temp[channelMap[11]];
        case 11: pFrame[10] = temp[channelMap[10]];
        case 10: pFrame[ 9] = temp[channelMap[ 9]];
        case  9: pFrame[ 8] = temp[channelMap[ 8]];
        case  8: pFrame[ 7] = temp[channelMap[ 7]];
        case  7: pFrame[ 6] = temp[channelMap[ 6]];
        case  6: pFrame[ 5] = temp[channelMap[ 5]];
        case  5: pFrame[ 4] = temp[channelMap[ 4]];
        case  4: pFrame[ 3] = temp[channelMap[ 3]];
        case  3: pFrame[ 2] = temp[channelMap[ 2]];
        case  2: pFrame[ 1] = temp[channelMap[ 1]];
        case  1: pFrame[ 0] = temp[channelMap[ 0]];
    }
}

static void mal_rearrange_channels_s32(mal_int32* pFrame, mal_uint32 channels, mal_uint8 channelMap[MAL_MAX_CHANNELS])
{
    mal_int32 temp[MAL_MAX_CHANNELS];
    mal_copy_memory(temp, pFrame, sizeof(temp[0]) * channels);

    switch (channels) {
        case 18: pFrame[17] = temp[channelMap[17]];
        case 17: pFrame[16] = temp[channelMap[16]];
        case 16: pFrame[15] = temp[channelMap[15]];
        case 15: pFrame[14] = temp[channelMap[14]];
        case 14: pFrame[13] = temp[channelMap[13]];
        case 13: pFrame[12] = temp[channelMap[12]];
        case 12: pFrame[11] = temp[channelMap[11]];
        case 11: pFrame[10] = temp[channelMap[10]];
        case 10: pFrame[ 9] = temp[channelMap[ 9]];
        case  9: pFrame[ 8] = temp[channelMap[ 8]];
        case  8: pFrame[ 7] = temp[channelMap[ 7]];
        case  7: pFrame[ 6] = temp[channelMap[ 6]];
        case  6: pFrame[ 5] = temp[channelMap[ 5]];
        case  5: pFrame[ 4] = temp[channelMap[ 4]];
        case  4: pFrame[ 3] = temp[channelMap[ 3]];
        case  3: pFrame[ 2] = temp[channelMap[ 2]];
        case  2: pFrame[ 1] = temp[channelMap[ 1]];
        case  1: pFrame[ 0] = temp[channelMap[ 0]];
    }
}

static void mal_rearrange_channels_f32(float* pFrame, mal_uint32 channels, mal_uint8 channelMap[MAL_MAX_CHANNELS])
{
    float temp[MAL_MAX_CHANNELS];
    mal_copy_memory(temp, pFrame, sizeof(temp[0]) * channels);

    switch (channels) {
        case 18: pFrame[17] = temp[channelMap[17]];
        case 17: pFrame[16] = temp[channelMap[16]];
        case 16: pFrame[15] = temp[channelMap[15]];
        case 15: pFrame[14] = temp[channelMap[14]];
        case 14: pFrame[13] = temp[channelMap[13]];
        case 13: pFrame[12] = temp[channelMap[12]];
        case 12: pFrame[11] = temp[channelMap[11]];
        case 11: pFrame[10] = temp[channelMap[10]];
        case 10: pFrame[ 9] = temp[channelMap[ 9]];
        case  9: pFrame[ 8] = temp[channelMap[ 8]];
        case  8: pFrame[ 7] = temp[channelMap[ 7]];
        case  7: pFrame[ 6] = temp[channelMap[ 6]];
        case  6: pFrame[ 5] = temp[channelMap[ 5]];
        case  5: pFrame[ 4] = temp[channelMap[ 4]];
        case  4: pFrame[ 3] = temp[channelMap[ 3]];
        case  3: pFrame[ 2] = temp[channelMap[ 2]];
        case  2: pFrame[ 1] = temp[channelMap[ 1]];
        case  1: pFrame[ 0] = temp[channelMap[ 0]];
    }
}

static void mal_rearrange_channels_generic(void* pFrame, mal_uint32 channels, mal_uint8 channelMap[MAL_MAX_CHANNELS], mal_format format)
{
    mal_uint32 sampleSizeInBytes = mal_get_sample_size_in_bytes(format);

    mal_uint8 temp[MAL_MAX_CHANNELS * 8];   // x8 to ensure it's large enough for all formats.
    mal_copy_memory(temp, pFrame, sampleSizeInBytes * channels);

    switch (channels) {
        case 18: mal_copy_memory((mal_uint8*)pFrame + (17 * sampleSizeInBytes), &temp[channelMap[17] * sampleSizeInBytes], sampleSizeInBytes);
        case 17: mal_copy_memory((mal_uint8*)pFrame + (16 * sampleSizeInBytes), &temp[channelMap[16] * sampleSizeInBytes], sampleSizeInBytes);
        case 16: mal_copy_memory((mal_uint8*)pFrame + (15 * sampleSizeInBytes), &temp[channelMap[15] * sampleSizeInBytes], sampleSizeInBytes);
        case 15: mal_copy_memory((mal_uint8*)pFrame + (14 * sampleSizeInBytes), &temp[channelMap[14] * sampleSizeInBytes], sampleSizeInBytes);
        case 14: mal_copy_memory((mal_uint8*)pFrame + (13 * sampleSizeInBytes), &temp[channelMap[13] * sampleSizeInBytes], sampleSizeInBytes);
        case 13: mal_copy_memory((mal_uint8*)pFrame + (12 * sampleSizeInBytes), &temp[channelMap[12] * sampleSizeInBytes], sampleSizeInBytes);
        case 12: mal_copy_memory((mal_uint8*)pFrame + (11 * sampleSizeInBytes), &temp[channelMap[11] * sampleSizeInBytes], sampleSizeInBytes);
        case 11: mal_copy_memory((mal_uint8*)pFrame + (10 * sampleSizeInBytes), &temp[channelMap[10] * sampleSizeInBytes], sampleSizeInBytes);
        case 10: mal_copy_memory((mal_uint8*)pFrame + ( 9 * sampleSizeInBytes), &temp[channelMap[ 9] * sampleSizeInBytes], sampleSizeInBytes);
        case  9: mal_copy_memory((mal_uint8*)pFrame + ( 8 * sampleSizeInBytes), &temp[channelMap[ 8] * sampleSizeInBytes], sampleSizeInBytes);
        case  8: mal_copy_memory((mal_uint8*)pFrame + ( 7 * sampleSizeInBytes), &temp[channelMap[ 7] * sampleSizeInBytes], sampleSizeInBytes);
        case  7: mal_copy_memory((mal_uint8*)pFrame + ( 6 * sampleSizeInBytes), &temp[channelMap[ 6] * sampleSizeInBytes], sampleSizeInBytes);
        case  6: mal_copy_memory((mal_uint8*)pFrame + ( 5 * sampleSizeInBytes), &temp[channelMap[ 5] * sampleSizeInBytes], sampleSizeInBytes);
        case  5: mal_copy_memory((mal_uint8*)pFrame + ( 4 * sampleSizeInBytes), &temp[channelMap[ 4] * sampleSizeInBytes], sampleSizeInBytes);
        case  4: mal_copy_memory((mal_uint8*)pFrame + ( 3 * sampleSizeInBytes), &temp[channelMap[ 3] * sampleSizeInBytes], sampleSizeInBytes);
        case  3: mal_copy_memory((mal_uint8*)pFrame + ( 2 * sampleSizeInBytes), &temp[channelMap[ 2] * sampleSizeInBytes], sampleSizeInBytes);
        case  2: mal_copy_memory((mal_uint8*)pFrame + ( 1 * sampleSizeInBytes), &temp[channelMap[ 1] * sampleSizeInBytes], sampleSizeInBytes);
        case  1: mal_copy_memory((mal_uint8*)pFrame + ( 0 * sampleSizeInBytes), &temp[channelMap[ 0] * sampleSizeInBytes], sampleSizeInBytes);
    }
}

static void mal_rearrange_channels(void* pFrame, mal_uint32 channels, mal_uint8 channelMap[MAL_MAX_CHANNELS], mal_format format)
{
    switch (format)
    {
    case mal_format_u8:  mal_rearrange_channels_u8( (mal_uint8*)pFrame, channels, channelMap); break;
    case mal_format_s16: mal_rearrange_channels_s16((mal_int16*)pFrame, channels, channelMap); break;
    case mal_format_s32: mal_rearrange_channels_s32((mal_int32*)pFrame, channels, channelMap); break;
    case mal_format_f32: mal_rearrange_channels_f32(    (float*)pFrame, channels, channelMap); break;
    default:             mal_rearrange_channels_generic(pFrame, channels, channelMap, format); break;
    }
}

static void mal_dsp_mix_channels__dec(float* pFramesOut, mal_uint32 channelsOut, const float* pFramesIn, mal_uint32 channelsIn, mal_uint32 frameCount, mal_channel_mix_mode mode)
{
    mal_assert(pFramesOut != NULL);
    mal_assert(channelsOut > 0);
    mal_assert(pFramesIn != NULL);
    mal_assert(channelsIn > 0);
    mal_assert(channelsOut < channelsIn);

    if (mode == mal_channel_mix_mode_basic) {
        // Basic mode is where we just drop excess channels.
        for (mal_uint32 iFrame = 0; iFrame < frameCount; ++iFrame) {
            switch (channelsOut) {
                case 17: pFramesOut[iFrame*channelsOut+16] = pFramesIn[iFrame*channelsIn+16];
                case 16: pFramesOut[iFrame*channelsOut+15] = pFramesIn[iFrame*channelsIn+15];
                case 15: pFramesOut[iFrame*channelsOut+14] = pFramesIn[iFrame*channelsIn+14];
                case 14: pFramesOut[iFrame*channelsOut+13] = pFramesIn[iFrame*channelsIn+13];
                case 13: pFramesOut[iFrame*channelsOut+12] = pFramesIn[iFrame*channelsIn+12];
                case 12: pFramesOut[iFrame*channelsOut+11] = pFramesIn[iFrame*channelsIn+11];
                case 11: pFramesOut[iFrame*channelsOut+10] = pFramesIn[iFrame*channelsIn+10];
                case 10: pFramesOut[iFrame*channelsOut+ 9] = pFramesIn[iFrame*channelsIn+ 9];
                case  9: pFramesOut[iFrame*channelsOut+ 8] = pFramesIn[iFrame*channelsIn+ 8];
                case  8: pFramesOut[iFrame*channelsOut+ 7] = pFramesIn[iFrame*channelsIn+ 7];
                case  7: pFramesOut[iFrame*channelsOut+ 6] = pFramesIn[iFrame*channelsIn+ 6];
                case  6: pFramesOut[iFrame*channelsOut+ 5] = pFramesIn[iFrame*channelsIn+ 5];
                case  5: pFramesOut[iFrame*channelsOut+ 4] = pFramesIn[iFrame*channelsIn+ 4];
                case  4: pFramesOut[iFrame*channelsOut+ 3] = pFramesIn[iFrame*channelsIn+ 3];
                case  3: pFramesOut[iFrame*channelsOut+ 2] = pFramesIn[iFrame*channelsIn+ 2];
                case  2: pFramesOut[iFrame*channelsOut+ 1] = pFramesIn[iFrame*channelsIn+ 1];
                case  1: pFramesOut[iFrame*channelsOut+ 0] = pFramesIn[iFrame*channelsIn+ 0];
            }
        }
    } else {
        // Blend mode is where we just use simple averaging to blend based on spacial locality. 
        if (channelsOut == 1) {
            for (mal_uint32 iFrame = 0; iFrame < frameCount; ++iFrame) {
                float total = 0;
                switch (channelsIn) {
                    case 18: total += pFramesIn[iFrame*channelsIn+17];
                    case 17: total += pFramesIn[iFrame*channelsIn+16];
                    case 16: total += pFramesIn[iFrame*channelsIn+15];
                    case 15: total += pFramesIn[iFrame*channelsIn+14];
                    case 14: total += pFramesIn[iFrame*channelsIn+13];
                    case 13: total += pFramesIn[iFrame*channelsIn+12];
                    case 12: total += pFramesIn[iFrame*channelsIn+11];
                    case 11: total += pFramesIn[iFrame*channelsIn+10];
                    case 10: total += pFramesIn[iFrame*channelsIn+ 9];
                    case  9: total += pFramesIn[iFrame*channelsIn+ 8];
                    case  8: total += pFramesIn[iFrame*channelsIn+ 7];
                    case  7: total += pFramesIn[iFrame*channelsIn+ 6];
                    case  6: total += pFramesIn[iFrame*channelsIn+ 5];
                    case  5: total += pFramesIn[iFrame*channelsIn+ 4];
                    case  4: total += pFramesIn[iFrame*channelsIn+ 3];
                    case  3: total += pFramesIn[iFrame*channelsIn+ 2];
                    case  2: total += pFramesIn[iFrame*channelsIn+ 1];
                    case  1: total += pFramesIn[iFrame*channelsIn+ 0];
                }

                pFramesOut[iFrame+0] = total / channelsIn;
            }
        } else if (channelsOut == 2) {
            // TODO: Implement proper stereo blending.
            mal_dsp_mix_channels__dec(pFramesOut, channelsOut, pFramesIn, channelsIn, frameCount, mal_channel_mix_mode_basic);
        } else {
            // Fall back to basic mode.
            mal_dsp_mix_channels__dec(pFramesOut, channelsOut, pFramesIn, channelsIn, frameCount, mal_channel_mix_mode_basic);
        }
    }
}

static void mal_dsp_mix_channels__inc(float* pFramesOut, mal_uint32 channelsOut, const float* pFramesIn, mal_uint32 channelsIn, mal_uint32 frameCount, mal_channel_mix_mode mode)
{
    mal_assert(pFramesOut != NULL);
    mal_assert(channelsOut > 0);
    mal_assert(pFramesIn != NULL);
    mal_assert(channelsIn > 0);
    mal_assert(channelsOut > channelsIn);

    if (mode == mal_channel_mix_mode_basic) {\
        // Basic mode is where we just zero out extra channels.
        for (mal_uint32 iFrame = 0; iFrame < frameCount; ++iFrame) {
            switch (channelsIn) {
                case 17: pFramesOut[iFrame*channelsOut+16] = pFramesIn[iFrame*channelsIn+16];
                case 16: pFramesOut[iFrame*channelsOut+15] = pFramesIn[iFrame*channelsIn+15];
                case 15: pFramesOut[iFrame*channelsOut+14] = pFramesIn[iFrame*channelsIn+14];
                case 14: pFramesOut[iFrame*channelsOut+13] = pFramesIn[iFrame*channelsIn+13];
                case 13: pFramesOut[iFrame*channelsOut+12] = pFramesIn[iFrame*channelsIn+12];
                case 12: pFramesOut[iFrame*channelsOut+11] = pFramesIn[iFrame*channelsIn+11];
                case 11: pFramesOut[iFrame*channelsOut+10] = pFramesIn[iFrame*channelsIn+10];
                case 10: pFramesOut[iFrame*channelsOut+ 9] = pFramesIn[iFrame*channelsIn+ 9];
                case  9: pFramesOut[iFrame*channelsOut+ 8] = pFramesIn[iFrame*channelsIn+ 8];
                case  8: pFramesOut[iFrame*channelsOut+ 7] = pFramesIn[iFrame*channelsIn+ 7];
                case  7: pFramesOut[iFrame*channelsOut+ 6] = pFramesIn[iFrame*channelsIn+ 6];
                case  6: pFramesOut[iFrame*channelsOut+ 5] = pFramesIn[iFrame*channelsIn+ 5];
                case  5: pFramesOut[iFrame*channelsOut+ 4] = pFramesIn[iFrame*channelsIn+ 4];
                case  4: pFramesOut[iFrame*channelsOut+ 3] = pFramesIn[iFrame*channelsIn+ 3];
                case  3: pFramesOut[iFrame*channelsOut+ 2] = pFramesIn[iFrame*channelsIn+ 2];
                case  2: pFramesOut[iFrame*channelsOut+ 1] = pFramesIn[iFrame*channelsIn+ 1];
                case  1: pFramesOut[iFrame*channelsOut+ 0] = pFramesIn[iFrame*channelsIn+ 0];
            }

            // Zero out extra channels.
            switch (channelsOut - channelsIn) {
                case 17: pFramesOut[iFrame*channelsOut+16] = 0;
                case 16: pFramesOut[iFrame*channelsOut+15] = 0;
                case 15: pFramesOut[iFrame*channelsOut+14] = 0;
                case 14: pFramesOut[iFrame*channelsOut+13] = 0;
                case 13: pFramesOut[iFrame*channelsOut+12] = 0;
                case 12: pFramesOut[iFrame*channelsOut+11] = 0;
                case 11: pFramesOut[iFrame*channelsOut+10] = 0;
                case 10: pFramesOut[iFrame*channelsOut+ 9] = 0;
                case  9: pFramesOut[iFrame*channelsOut+ 8] = 0;
                case  8: pFramesOut[iFrame*channelsOut+ 7] = 0;
                case  7: pFramesOut[iFrame*channelsOut+ 6] = 0;
                case  6: pFramesOut[iFrame*channelsOut+ 5] = 0;
                case  5: pFramesOut[iFrame*channelsOut+ 4] = 0;
                case  4: pFramesOut[iFrame*channelsOut+ 3] = 0;
                case  3: pFramesOut[iFrame*channelsOut+ 2] = 0;
                case  2: pFramesOut[iFrame*channelsOut+ 1] = 0;
                case  1: pFramesOut[iFrame*channelsOut+ 0] = 0;
            }
        }
    } else {
        // Using blended mixing mode. Basically this is just the mode where audio is distributed across all channels
        // based on spacial locality.
        if (channelsIn == 1) {
            for (mal_uint32 iFrame = 0; iFrame < frameCount; ++iFrame) {
                switch (channelsOut) {
                    case 18: pFramesOut[iFrame*channelsOut+17] = pFramesIn[iFrame*channelsIn+0];
                    case 17: pFramesOut[iFrame*channelsOut+16] = pFramesIn[iFrame*channelsIn+0];
                    case 16: pFramesOut[iFrame*channelsOut+15] = pFramesIn[iFrame*channelsIn+0];
                    case 15: pFramesOut[iFrame*channelsOut+14] = pFramesIn[iFrame*channelsIn+0];
                    case 14: pFramesOut[iFrame*channelsOut+13] = pFramesIn[iFrame*channelsIn+0];
                    case 13: pFramesOut[iFrame*channelsOut+12] = pFramesIn[iFrame*channelsIn+0];
                    case 12: pFramesOut[iFrame*channelsOut+11] = pFramesIn[iFrame*channelsIn+0];
                    case 11: pFramesOut[iFrame*channelsOut+10] = pFramesIn[iFrame*channelsIn+0];
                    case 10: pFramesOut[iFrame*channelsOut+ 9] = pFramesIn[iFrame*channelsIn+0];
                    case  9: pFramesOut[iFrame*channelsOut+ 8] = pFramesIn[iFrame*channelsIn+0];
                    case  8: pFramesOut[iFrame*channelsOut+ 7] = pFramesIn[iFrame*channelsIn+0];
                    case  7: pFramesOut[iFrame*channelsOut+ 6] = pFramesIn[iFrame*channelsIn+0];
                    case  6: pFramesOut[iFrame*channelsOut+ 5] = pFramesIn[iFrame*channelsIn+0];
                    case  5: pFramesOut[iFrame*channelsOut+ 4] = pFramesIn[iFrame*channelsIn+0];
                    case  4: pFramesOut[iFrame*channelsOut+ 3] = pFramesIn[iFrame*channelsIn+0];
                    case  3: pFramesOut[iFrame*channelsOut+ 2] = pFramesIn[iFrame*channelsIn+0];
                    case  2: pFramesOut[iFrame*channelsOut+ 1] = pFramesIn[iFrame*channelsIn+0];
                    case  1: pFramesOut[iFrame*channelsOut+ 0] = pFramesIn[iFrame*channelsIn+0];
                }
            }
        } else if (channelsIn == 2) {
            // TODO: Implement an optimized stereo conversion.
            mal_dsp_mix_channels__dec(pFramesOut, channelsOut, pFramesIn, channelsIn, frameCount, mal_channel_mix_mode_basic);
        } else {
            // Fall back to basic mixing mode.
            mal_dsp_mix_channels__dec(pFramesOut, channelsOut, pFramesIn, channelsIn, frameCount, mal_channel_mix_mode_basic);
        }
    }
}

static void mal_dsp_mix_channels(float* pFramesOut, mal_uint32 channelsOut, const float* pFramesIn, mal_uint32 channelsIn, mal_uint32 frameCount, mal_channel_mix_mode mode)
{
    if (channelsIn < channelsOut) {
        // Increasing the channel count.
        mal_dsp_mix_channels__inc(pFramesOut, channelsOut, pFramesIn, channelsIn, frameCount, mode);
    } else {
        // Decreasing the channel count.
        mal_dsp_mix_channels__dec(pFramesOut, channelsOut, pFramesIn, channelsIn, frameCount, mode);
    }
}


mal_uint32 mal_dsp__src_on_read(mal_uint32 frameCount, void* pFramesOut, void* pUserData)
{
    mal_dsp* pDSP = (mal_dsp*)pUserData;
    mal_assert(pDSP != NULL);

    return pDSP->onRead(frameCount, pFramesOut, pDSP->pUserDataForOnRead);
}

mal_result mal_dsp_init(mal_dsp_config* pConfig, mal_dsp_read_proc onRead, void* pUserData, mal_dsp* pDSP)
{
    if (pDSP == NULL) return MAL_INVALID_ARGS;
    mal_zero_object(pDSP);
    pDSP->config = *pConfig;
    pDSP->onRead = onRead;
    pDSP->pUserDataForOnRead = pUserData;

    if (pDSP->config.cacheSizeInFrames > MAL_SRC_CACHE_SIZE_IN_FRAMES || pDSP->config.cacheSizeInFrames == 0) {
        pDSP->config.cacheSizeInFrames = MAL_SRC_CACHE_SIZE_IN_FRAMES;
    }

    if (pConfig->sampleRateIn != pConfig->sampleRateOut) {
        pDSP->isSRCRequired = MAL_TRUE;

        mal_src_config srcConfig;
        srcConfig.sampleRateIn = pConfig->sampleRateIn;
        srcConfig.sampleRateOut = pConfig->sampleRateOut;
        srcConfig.formatIn = pConfig->formatIn;
        srcConfig.formatOut = mal_format_f32;
        srcConfig.channels = pConfig->channelsIn;
        srcConfig.algorithm = mal_src_algorithm_linear;
        srcConfig.cacheSizeInFrames = pConfig->cacheSizeInFrames;
        mal_result result = mal_src_init(&srcConfig, mal_dsp__src_on_read, pDSP, &pDSP->src);
        if (result != MAL_SUCCESS) {
            return result;
        }
    }



    pDSP->isChannelMappingRequired = MAL_FALSE;
    if (pConfig->channelMapIn[0] != MAL_CHANNEL_NONE && pConfig->channelMapOut[0] != MAL_CHANNEL_NONE) {    // <-- Channel mapping will be ignored if the first channel map is MAL_CHANNEL_NONE.
        // When using channel mapping we need to figure out a shuffling table. The first thing to do is convert the input channel map
        // so that it contains the same number of channels as the output channel count.
        mal_uint32 channelsMin = mal_min(pConfig->channelsIn, pConfig->channelsOut);
        for (mal_uint32 iChannel = 0; iChannel < channelsMin; ++iChannel) {
            pDSP->channelMapInPostMix[iChannel] = pConfig->channelMapIn[iChannel];
        }

        // Any excess channels need to be filled with the relevant channels from the output channel map. Currently we're justing filling it with
        // the first channels that are not present in the input channel map.
        if (pConfig->channelsOut > pConfig->channelsIn) {
            for (mal_uint32 iChannel = pConfig->channelsIn; iChannel < pConfig->channelsOut; ++iChannel) {
                mal_uint8 newChannel = MAL_CHANNEL_NONE;
                for (mal_uint32 iChannelOut = 0; iChannelOut < pConfig->channelsOut; ++iChannelOut) {
                    mal_bool32 exists = MAL_FALSE;
                    for (mal_uint32 iChannelIn = 0; iChannelIn < pConfig->channelsIn; ++iChannelIn) {
                        if (pConfig->channelMapOut[iChannelOut] == pConfig->channelMapIn[iChannelIn]) {
                            exists = MAL_TRUE;
                            break;
                        }
                    }

                    if (!exists) {
                        newChannel = pConfig->channelMapOut[iChannelOut];
                        break;
                    }
                }

                pDSP->channelMapInPostMix[iChannel] = newChannel;
            }
        }

        // We only need to do a channel mapping if the map after mixing is different to the final output map.
        for (mal_uint32 iChannel = 0; iChannel < pConfig->channelsOut; ++iChannel) {
            if (pDSP->channelMapInPostMix[iChannel] != pConfig->channelMapOut[iChannel]) {
                pDSP->isChannelMappingRequired = MAL_TRUE;
                break;
            }
        }

        // Now we need to create the shuffling table.
        if (pDSP->isChannelMappingRequired) {
            for (mal_uint32 iChannelIn = 0; iChannelIn < pConfig->channelsOut; ++iChannelIn) {
                for (mal_uint32 iChannelOut = 0; iChannelOut < pConfig->channelsOut; ++iChannelOut) {
                    if (pDSP->channelMapInPostMix[iChannelOut] == pConfig->channelMapOut[iChannelIn]) {
                        pDSP->channelShuffleTable[iChannelOut] = (mal_uint8)iChannelIn;
                    }
                }
            }
        }
    }

    if (pConfig->formatIn == pConfig->formatOut && pConfig->channelsIn == pConfig->channelsOut && pConfig->sampleRateIn == pConfig->sampleRateOut && !pDSP->isChannelMappingRequired) {
        pDSP->isPassthrough = MAL_TRUE;
    } else {
        pDSP->isPassthrough = MAL_FALSE;
    }

    return MAL_SUCCESS;
}

mal_uint32 mal_dsp_read_frames(mal_dsp* pDSP, mal_uint32 frameCount, void* pFramesOut)
{
    if (pDSP == NULL || pFramesOut == NULL) return 0;

    // Fast path.
    if (pDSP->isPassthrough) {
        return pDSP->onRead(frameCount, pFramesOut, pDSP->pUserDataForOnRead);
    }


    // Slower path - where the real work is done.
    mal_uint8 pFrames[2][MAL_MAX_CHANNELS * 512 * MAL_MAX_SAMPLE_SIZE_IN_BYTES];
    mal_format pFramesFormat[2];
    mal_uint32 iFrames = 0; // <-- Used as an index into pFrames and cycles between 0 and 1.

    mal_uint32 totalFramesRead = 0;
    while (frameCount > 0) {
        iFrames = 0;

        mal_uint32 framesToRead = mal_countof(pFrames[0]) / (mal_max(pDSP->config.channelsIn, pDSP->config.channelsOut) * MAL_MAX_SAMPLE_SIZE_IN_BYTES);
        if (framesToRead > frameCount) {
            framesToRead = frameCount;
        }

        // The initial filling of sample data depends on whether or not we are using SRC.
        mal_uint32 framesRead = 0;
        if (pDSP->isSRCRequired) {
            framesRead = mal_src_read_frames(&pDSP->src, framesToRead, pFrames[iFrames]);
            pFramesFormat[iFrames] = pDSP->src.config.formatOut;  // Should always be f32.
        } else {
            framesRead = pDSP->onRead(framesToRead, pFrames[iFrames], pDSP->pUserDataForOnRead);
            pFramesFormat[iFrames] = pDSP->config.formatIn;
        }

        if (framesRead == 0) {
            break;
        }
        

        // Channel mixing. The input format must be in f32 which may require a conversion.
        if (pDSP->config.channelsIn != pDSP->config.channelsOut) {
            if (pFramesFormat[iFrames] != mal_format_f32) {
                mal_pcm_convert(pFrames[(iFrames + 1) % 2], mal_format_f32, pFrames[iFrames], pDSP->config.formatIn, framesRead * pDSP->config.channelsIn);
                iFrames = (iFrames + 1) % 2;
                pFramesFormat[iFrames] = mal_format_f32;
            }

            mal_dsp_mix_channels((float*)(pFrames[(iFrames + 1) % 2]), pDSP->config.channelsOut, (const float*)(pFrames[iFrames]), pDSP->config.channelsIn, framesRead, mal_channel_mix_mode_blend);
            iFrames = (iFrames + 1) % 2;
            pFramesFormat[iFrames] = mal_format_f32;
        }


        // Channel mapping.
        if (pDSP->isChannelMappingRequired) {
            for (mal_uint32 i = 0; i < framesRead; ++i) {
                mal_rearrange_channels(pFrames[iFrames] + (i * pDSP->config.channelsOut * mal_get_sample_size_in_bytes(pFramesFormat[iFrames])), pDSP->config.channelsOut, pDSP->channelShuffleTable, pFramesFormat[iFrames]);
            }
        }
        

        // Final conversion to output format.
        mal_pcm_convert(pFramesOut, pDSP->config.formatOut, pFrames[iFrames], pFramesFormat[iFrames], framesRead * pDSP->config.channelsOut);

        pFramesOut  = (mal_uint8*)pFramesOut + (framesRead * pDSP->config.channelsOut * mal_get_sample_size_in_bytes(pDSP->config.formatOut));
        frameCount -= framesRead;
        totalFramesRead += framesRead;
    }

    return totalFramesRead;
}




//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Miscellaneous Helpers
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void mal_blend_f32(float* pOut, float* pInA, float* pInB, float factor, mal_uint32 channels)
{
    for (mal_uint32 i = 0; i < channels; ++i) {
        pOut[i] = mal_mix_f32(pInA[i], pInB[i], factor);
    }
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//
//
// AUTO-GENERATED
//
//
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FORMAT CONVERSION
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if 0
#include "tools/malgen/bin/malgen_test0.c"
#else
void mal_pcm_u8_to_s16(short* pOut, const unsigned char* pIn, unsigned int count)
{
    int r;
    for (unsigned int i = 0; i < count; ++i) {
        int x = pIn[i];
        r = x - 128;
        r = r << 8;
        pOut[i] = (short)r;
    }
}

void mal_pcm_u8_to_s24(void* pOut, const unsigned char* pIn, unsigned int count)
{
    int r;
    for (unsigned int i = 0; i < count; ++i) {
        int x = pIn[i];
        r = x - 128;
        r = r << 16;
        ((unsigned char*)pOut)[(i*3)+0] = (unsigned char)(r & 0xFF); ((unsigned char*)pOut)[(i*3)+1] = (unsigned char)((r & 0xFF00) >> 8); ((unsigned char*)pOut)[(i*3)+2] = (unsigned char)((r & 0xFF0000) >> 16);
    }
}

void mal_pcm_u8_to_s32(int* pOut, const unsigned char* pIn, unsigned int count)
{
    int r;
    for (unsigned int i = 0; i < count; ++i) {
        int x = pIn[i];
        r = x - 128;
        r = r << 24;
        pOut[i] = (int)r;
    }
}

void mal_pcm_u8_to_f32(float* pOut, const unsigned char* pIn, unsigned int count)
{
    float r;
    for (unsigned int i = 0; i < count; ++i) {
        int x = pIn[i];
        r = x / 255.0f;
        r = r * 2;
        r = r - 1;
        pOut[i] = (float)r;
    }
}

void mal_pcm_s16_to_u8(unsigned char* pOut, const short* pIn, unsigned int count)
{
    int r;
    for (unsigned int i = 0; i < count; ++i) {
        int x = pIn[i];
        r = x >> 8;
        r = r + 128;
        pOut[i] = (unsigned char)r;
    }
}

void mal_pcm_s16_to_s24(void* pOut, const short* pIn, unsigned int count)
{
    int r;
    for (unsigned int i = 0; i < count; ++i) {
        int x = pIn[i];
        r = x << 8;
        ((unsigned char*)pOut)[(i*3)+0] = (unsigned char)(r & 0xFF); ((unsigned char*)pOut)[(i*3)+1] = (unsigned char)((r & 0xFF00) >> 8); ((unsigned char*)pOut)[(i*3)+2] = (unsigned char)((r & 0xFF0000) >> 16);
    }
}

void mal_pcm_s16_to_s32(int* pOut, const short* pIn, unsigned int count)
{
    int r;
    for (unsigned int i = 0; i < count; ++i) {
        int x = pIn[i];
        r = x << 16;
        pOut[i] = (int)r;
    }
}

void mal_pcm_s16_to_f32(float* pOut, const short* pIn, unsigned int count)
{
    float r;
    for (unsigned int i = 0; i < count; ++i) {
        int x = pIn[i];
        r = x + 32768.0f;
        r = r / 65536.0f;
        r = r * 2;
        r = r - 1;
        pOut[i] = (float)r;
    }
}

void mal_pcm_s24_to_u8(unsigned char* pOut, const void* pIn, unsigned int count)
{
    int r;
    for (unsigned int i = 0; i < count; ++i) {
        int x = ((int)(((unsigned int)(((unsigned char*)pIn)[i*3+0]) << 8) | ((unsigned int)(((unsigned char*)pIn)[i*3+1]) << 16) | ((unsigned int)(((unsigned char*)pIn)[i*3+2])) << 24)) >> 8;
        r = x >> 16;
        r = r + 128;
        pOut[i] = (unsigned char)r;
    }
}

void mal_pcm_s24_to_s16(short* pOut, const void* pIn, unsigned int count)
{
    int r;
    for (unsigned int i = 0; i < count; ++i) {
        int x = ((int)(((unsigned int)(((unsigned char*)pIn)[i*3+0]) << 8) | ((unsigned int)(((unsigned char*)pIn)[i*3+1]) << 16) | ((unsigned int)(((unsigned char*)pIn)[i*3+2])) << 24)) >> 8;
        r = x >> 8;
        pOut[i] = (short)r;
    }
}

void mal_pcm_s24_to_s32(int* pOut, const void* pIn, unsigned int count)
{
    int r;
    for (unsigned int i = 0; i < count; ++i) {
        int x = ((int)(((unsigned int)(((unsigned char*)pIn)[i*3+0]) << 8) | ((unsigned int)(((unsigned char*)pIn)[i*3+1]) << 16) | ((unsigned int)(((unsigned char*)pIn)[i*3+2])) << 24)) >> 8;
        r = x << 8;
        pOut[i] = (int)r;
    }
}

void mal_pcm_s24_to_f32(float* pOut, const void* pIn, unsigned int count)
{
    float r;
    for (unsigned int i = 0; i < count; ++i) {
        int x = ((int)(((unsigned int)(((unsigned char*)pIn)[i*3+0]) << 8) | ((unsigned int)(((unsigned char*)pIn)[i*3+1]) << 16) | ((unsigned int)(((unsigned char*)pIn)[i*3+2])) << 24)) >> 8;
        r = x + 8388608.0f;
        r = r / 16777215.0f;
        r = r * 2;
        r = r - 1;
        pOut[i] = (float)r;
    }
}

void mal_pcm_s32_to_u8(unsigned char* pOut, const int* pIn, unsigned int count)
{
    int r;
    for (unsigned int i = 0; i < count; ++i) {
        int x = pIn[i];
        r = x >> 24;
        r = r + 128;
        pOut[i] = (unsigned char)r;
    }
}

void mal_pcm_s32_to_s16(short* pOut, const int* pIn, unsigned int count)
{
    int r;
    for (unsigned int i = 0; i < count; ++i) {
        int x = pIn[i];
        r = x >> 16;
        pOut[i] = (short)r;
    }
}

void mal_pcm_s32_to_s24(void* pOut, const int* pIn, unsigned int count)
{
    int r;
    for (unsigned int i = 0; i < count; ++i) {
        int x = pIn[i];
        r = x >> 8;
        ((unsigned char*)pOut)[(i*3)+0] = (unsigned char)(r & 0xFF); ((unsigned char*)pOut)[(i*3)+1] = (unsigned char)((r & 0xFF00) >> 8); ((unsigned char*)pOut)[(i*3)+2] = (unsigned char)((r & 0xFF0000) >> 16);
    }
}

void mal_pcm_s32_to_f32(float* pOut, const int* pIn, unsigned int count)
{
    float r;
    for (unsigned int i = 0; i < count; ++i) {
        int x = pIn[i];
        int s;
        s = ((*((int*)&x)) & 0x80000000) >> 31;
        s = s + 2147483647;
        r = x / (float)(unsigned int)s;
        pOut[i] = (float)r;
    }
}

void mal_pcm_f32_to_u8(unsigned char* pOut, const float* pIn, unsigned int count)
{
    int r;
    for (unsigned int i = 0; i < count; ++i) {
        float x = pIn[i];
        float c;
        int s;
        c = ((x < -1) ? -1 : ((x > 1) ? 1 : x));
        s = ((*((int*)&x)) & 0x80000000) >> 31;
        s = s + 127;
        r = (int)(c * s);
        r = r + 128;
        pOut[i] = (unsigned char)r;
    }
}

void mal_pcm_f32_to_s16(short* pOut, const float* pIn, unsigned int count)
{
    int r;
    for (unsigned int i = 0; i < count; ++i) {
        float x = pIn[i];
        float c;
        int s;
        c = ((x < -1) ? -1 : ((x > 1) ? 1 : x));
        s = ((*((int*)&x)) & 0x80000000) >> 31;
        s = s + 32767;
        r = (int)(c * s);
        pOut[i] = (short)r;
    }
}

void mal_pcm_f32_to_s24(void* pOut, const float* pIn, unsigned int count)
{
    int r;
    for (unsigned int i = 0; i < count; ++i) {
        float x = pIn[i];
        float c;
        int s;
        c = ((x < -1) ? -1 : ((x > 1) ? 1 : x));
        s = ((*((int*)&x)) & 0x80000000) >> 31;
        s = s + 8388607;
        r = (int)(c * s);
        ((unsigned char*)pOut)[(i*3)+0] = (unsigned char)(r & 0xFF); ((unsigned char*)pOut)[(i*3)+1] = (unsigned char)((r & 0xFF00) >> 8); ((unsigned char*)pOut)[(i*3)+2] = (unsigned char)((r & 0xFF0000) >> 16);
    }
}

void mal_pcm_f32_to_s32(int* pOut, const float* pIn, unsigned int count)
{
    int r;
    for (unsigned int i = 0; i < count; ++i) {
        float x = pIn[i];
        float c;
        int s;
        c = ((x < -1) ? -1 : ((x > 1) ? 1 : x));
        s = ((*((int*)&x)) & 0x80000000) >> 31;
        s = s + 2147483647;
        r = (int)(c * s);
        pOut[i] = (int)r;
    }
}
#endif

#endif


// REVISION HISTORY
// ================
//
// v0.3 - 2017-06-19
//   - API CHANGE: Introduced the notion of a context. The context is the highest level object and is required for
//     enumerating and creating devices. Now, applications must first create a context, and then use that to
//     enumerate and create devices. The reason for this change is to ensure device enumeration and creation is
//     tied to the same backend. In addition, some backends are better suited to this design.
//   - API CHANGE: Removed the rewinding APIs because they're too inconsistent across the different backends, hard
//     to test and maintain, and just generally unreliable.
//   - Added helper APIs for initializing mal_device_config objects.
//   - Null Backend: Fixed a crash when recording.
//   - Fixed build for UWP.
//   - Added support for f32 formats to the OpenSL|ES backend.
//   - Added initial implementation of the WASAPI backend.
//   - Added initial implementation of the OpenAL backend.
//   - Added support for low quality linear sample rate conversion.
//   - Added early support for basic channel mapping.
//
// v0.2 - 2016-10-28
//   - API CHANGE: Add user data pointer as the last parameter to mal_device_init(). The rationale for this
//     change is to ensure the logging callback has access to the user data during initialization.
//   - API CHANGE: Have device configuration properties be passed to mal_device_init() via a structure. Rationale:
//     1) The number of parameters is just getting too much.
//     2) It makes it a bit easier to add new configuration properties in the future. In particular, there's a
//        chance there will be support added for backend-specific properties.
//   - Dropped support for f64, A-law and Mu-law formats since they just aren't common enough to justify the
//     added maintenance cost.
//   - DirectSound: Increased the default buffer size for capture devices.
//   - Added initial implementation of the OpenSL|ES backend.
//
// v0.1 - 2016-10-21
//   - Initial versioned release.


// TODO
// ====
// - Higher quality sample rate conversion.
//
//
// Optimizations
// -------------
// - SSE-ify format conversions
// - SSE-ify SRC
// - Optimize the DSP pipeline generally
//
//
// WASAPI
// ------
// - Add support for exclusive mode?
// - Look into event callbacks: AUDCLNT_STREAMFLAGS_EVENTCALLBACK
// - Clean up that terrible "__cplusplus" mess by implementing wrapper functions.
//
//
// ALSA
// ----
// - Use runtime linking for asound.
// - Finish mmap mode.
// - Tweak the default buffer size and period counts. Pretty sure the ALSA backend can support a much smaller
//   default buffer size.
//
//
// OpenSL|ES / Android
// -------------------
// - Test!


/*
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
*/
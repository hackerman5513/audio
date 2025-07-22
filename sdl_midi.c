#include <SDL2/SDL.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>

int sr = 44100, NOTE = 0;
int total = 22050, cur = 0;
volatile int play = 0; // Shared between audio thread and MIDI thread
int16_t* buffers[12];

float notes[12] = {
    523.25f, // C5
    554.37f, // C#5
    587.33f, // D5
    622.25f, // D#5
    659.26f, // E5
    698.46f, // F5
    739.99f, // F#5
    783.99f, // G5
    830.61f, // G#5
    880.00f, // A5
    932.33f, // A#5
    987.77f  // B5
};

void audio(void* u, int16_t* s, int len) {
    int16_t* out = (int16_t*)s;
    int n = len / 2;
    if (play && cur == total) cur = 0;
    int rem = play ? total - cur : 0;
    int copy = n < rem ? n : rem;
    for (int i = 0; i < copy; i++) out[i] = buffers[NOTE][cur++];
    for (int i = copy; i < n; i++) out[i] = 0;
}

// 🎧 MIDI listening thread
void* midi_listener(void* arg) {
    snd_seq_t *seq_handle;
    int port_id;

    if (snd_seq_open(&seq_handle, "default", SND_SEQ_OPEN_INPUT, 0) < 0) {
        perror("Failed to open ALSA sequencer");
        return NULL;
    }

    snd_seq_set_client_name(seq_handle, "MIDI Thread Listener");

    port_id = snd_seq_create_simple_port(seq_handle, "Input Port",
        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);

    // Adjust to match your MIDI keyboard port (check via `aconnect -l`)
    snd_seq_connect_from(seq_handle, port_id, 20, 0);

    int npfd = snd_seq_poll_descriptors_count(seq_handle, POLLIN);
    struct pollfd *pfd = alloca(npfd * sizeof(struct pollfd));
    snd_seq_poll_descriptors(seq_handle, pfd, npfd, POLLIN);

    while (1) {
        if (poll(pfd, npfd, -1) > 0) {
            snd_seq_event_t *event = NULL;
            while (snd_seq_event_input(seq_handle, &event) >= 0) {
                if (!event) continue;

                int midi_note = event->data.note.note;
                int note12 = midi_note % 12;

                if (event->type == SND_SEQ_EVENT_NOTEON && event->data.note.velocity > 0) {
                    NOTE = note12;
                    play = 1;
                } else if (event->type == SND_SEQ_EVENT_NOTEOFF ||
                          (event->type == SND_SEQ_EVENT_NOTEON && event->data.note.velocity == 0)) {
                    if (NOTE == note12) {
                        play = 0;
                    }
                }

                snd_seq_free_event(event);
            }
        }
    }

    snd_seq_close(seq_handle);
    return NULL;
}

int main() {
    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_VIDEO);
    SDL_CreateWindow("Keys", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480, 0);

    for (int h = 0; h < 12; h++) {
        buffers[h] = malloc(sizeof(int16_t) * total);
        for (int i = 0; i < total; i++)
            buffers[h][i] = sinf(6.2831f * notes[h] * i / sr) * 32767;
    }

    SDL_AudioSpec spec = {
        .freq = sr,
        .format = AUDIO_S16SYS,
        .channels = 1,
        .samples = 512,
        .callback = audio
    };
    SDL_OpenAudio(&spec, NULL);
    SDL_PauseAudio(0);

    // Start MIDI thread
    pthread_t midi_thread;
    pthread_create(&midi_thread, NULL, midi_listener, NULL);

    // Optional: keyboard control too
    SDL_Event e;
    int run = 1;
    while (run) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_KEYDOWN) {
                char key = e.key.keysym.sym;
                if (key == 'q') run = 0;
                play = 1;
                if (key == '1') NOTE = 0;
                if (key == '2') NOTE = 1;
                if (key == '3') NOTE = 2;
                if (key == '4') NOTE = 3;
                if (key == '5') NOTE = 4;
                if (key == '6') NOTE = 5;
                if (key == '7') NOTE = 6;
                if (key == '8') NOTE = 7;
                if (key == '9') NOTE = 8;
                if (key == '0') NOTE = 9;
                if (key == '-') NOTE = 10;
                if (key == '=') NOTE = 11;
            }
            if (e.type == SDL_KEYUP) play = 0;
        }
        SDL_Delay(10); // prevent 100% CPU usage
    }

    SDL_CloseAudio();
    SDL_Quit();
    return 0;
}


#include <SDL2/SDL.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>

#define VIS_SIZE 1024

int sr = 44100, NOTE = 0;
int total = 22050, cur = 0;
volatile int play = 0;
int16_t* buffers[12];
int16_t vis_buffer[VIS_SIZE];
int vis_pos = 0;

float notes[12] = {
    523.25f, 554.37f, 587.33f, 622.25f, 659.26f, 698.46f,
    739.99f, 783.99f, 830.61f, 880.00f, 932.33f, 987.77f
};

void audio(void* u, int16_t* s, int len) {
    int16_t* out = (int16_t*)s;
    int n = len / 2;
    if (play && cur == total) cur = 0;
    int rem = play ? total - cur : 0;
    int copy = n < rem ? n : rem;

    for (int i = 0; i < copy; i++) {
        int16_t sample = buffers[NOTE][cur++];
        out[i] = sample;
        vis_buffer[vis_pos] = sample;
        vis_pos = (vis_pos + 1) % VIS_SIZE;
    }
    for (int i = copy; i < n; i++) {
        out[i] = 0;
        vis_buffer[vis_pos] = 0;
        vis_pos = (vis_pos + 1) % VIS_SIZE;
    }
}

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

    snd_seq_connect_from(seq_handle, port_id, 20, 0); // adjust for your MIDI device

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
                    if (NOTE == note12) play = 0;
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

    int width = 640, height = 480;
    SDL_Window* win = SDL_CreateWindow("Sine Synth + Waveform",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    for (int h = 0; h < 12; h++) {
        buffers[h] = malloc(sizeof(int16_t) * total);
        for (int i = 0; i < total; i++) {
            buffers[h][i] = sinf(6.2831f * notes[h] * i / sr) * 32767;
        }
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

    pthread_t midi_thread;
    pthread_create(&midi_thread, NULL, midi_listener, NULL);

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

        // --- Draw waveform ---
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        for (int i = 1; i < VIS_SIZE; i++) {
            int idx0 = (vis_pos + i - 1) % VIS_SIZE;
            int idx1 = (vis_pos + i) % VIS_SIZE;

            int x0 = (i - 1) * width / VIS_SIZE;
            int x1 = i * width / VIS_SIZE;

            int y0 = (vis_buffer[idx0] / 32768.0f) * (height / 2) + (height / 2);
            int y1 = (vis_buffer[idx1] / 32768.0f) * (height / 2) + (height / 2);

            SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16); // ~60 FPS
    }

    SDL_CloseAudio();
    SDL_Quit();
    return 0;
}


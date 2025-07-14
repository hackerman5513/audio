#include <stdio.h>
#include <math.h>
#include <stdlib.h>

void main(){

int hz = 867;
int sec = 1;
int sample_rate = 44100;
int total = sample_rate*sec;
int16_t* pcm_buffer = malloc(sizeof(int16_t)*total);

for(int i = 0; i < total; i++){
 float delta_time = i*1.0/sample_rate;
 float sample = sin(6.28318*hz*delta_time);
 int16_t pcm = (int16_t)(sample * 32767);
 pcm_buffer[i] = pcm;
 printf("%c%c", pcm & 0xFF, (pcm>>8) & 0xFF);  
}

}

//gcc main.c -lm  && ./a.out | aplay -f S16_LE -r 44100
//or feed it to sox or ffmpeg player 
//or any other which can take raw pcm bytes and play 



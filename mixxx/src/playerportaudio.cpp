/***************************************************************************
                          playerportaudio.cpp  -  description
                             -------------------
    begin                : Wed Feb 20 2002
    copyright            : (C) 2002 by Tue and Ken Haste Andersen
    email                : 
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "playerportaudio.h"

PlayerPortAudio::PlayerPortAudio(int size, std::vector<EngineObject *> *engines) : Player(size, engines)
{
    PaError err = Pa_Initialize();
    if( err != paNoError ) qFatal("PortAudio initialization error");

    // Fill out devices list with info about available devices
    int no = Pa_CountDevices();
    const PaDeviceInfo *devInfo;
    for (int i=0; i<no; i++)
    {
        // Add new PlayerInfo object to devices list
        Player::Info *p = new Player::Info;
        devices.append(p);
        devInfo = Pa_GetDeviceInfo(i);

        // Name
        p->name = QString(devInfo->name);

        // Sample rates
        for (int i=0; i<devInfo->numSampleRates; i++)
            p->sampleRates.append((int)devInfo->sampleRates[i]);

        // Bits
        if (devInfo->nativeSampleFormats & paInt8)        p->bits.append(8);
        if (devInfo->nativeSampleFormats & paInt16)       p->bits.append(16);
        //if (devInfo->nativeSampleFormats & paPackedInt24) p->bits.append(24);
        if (devInfo->nativeSampleFormats & paInt24)       p->bits.append(24);
        if (devInfo->nativeSampleFormats & paInt32)       p->bits.append(32);
    }

    // Get device id of playback device
    PaDeviceID id = Pa_GetDefaultOutputDeviceID();

    // Ensure stereo is supported
    const PaDeviceInfo *info = Pa_GetDeviceInfo(id);
    if (info->maxOutputChannels < NO_CHANNELS)
        qFatal("Not enough channels available on default output device: %i",info->maxOutputChannels);

    // Set sample rate to 44100 if possible, otherwise highest possible
    int temp_sr = 0;
    for (int i=0; i<=info->numSampleRates; i++)
        if (info->sampleRates[i] == 44100.)
        temp_sr = 44100;
    if (temp_sr == 0)
        temp_sr = (int)info->sampleRates[info->numSampleRates-1];

    if (!open(QString(info->name),temp_sr,16,size))
        qFatal("PortAudio Error opening device");
}

PlayerPortAudio::~PlayerPortAudio()
{
	Pa_Terminate();
}

bool PlayerPortAudio::open(QString name, int srate, int bits, int bufferSize)
{
    // Extract bit information
    PaSampleFormat format;
    switch (bits)
    {
        case 8:  format = paInt8; break;
        case 16: format = paInt16; break;
        case 24: format = paInt24; break;
        case 32: format = paInt32; break;
        default: qFatal("Sample format not supported (%i bits)",bits);
    }

    // Extract device information
    unsigned int id;
    for (id=0; id<devices.count(); id++)
        if (name == devices.at(id)->name)
            break;

    PaError err = Pa_OpenStream(&stream,
                        paNoDevice,         // default input device
                        0,                  // no input
                        format,
                        NULL,
                        id,                 // default output device
                        NO_CHANNELS,        // stereo output
                        format,
                        NULL,
                        (double)srate,
                        bufferSize/NO_CHANNELS,   // frames per buffer per channel
                        0,                  // number of buffers, if zero then use default minimum
                        paClipOff,          // we won't output out of range samples so don't bother clipping them
                        paCallback,
                        this );

    if( err != paNoError )
    {
        qDebug("PortAudio open stream error: %s", Pa_GetErrorText(err));
        return false;
    }

    // Fill in active config information
    setParams(QString(Pa_GetDeviceInfo(id)->name), srate, 16, bufferSize);
    buffer_size = bufferSize;

    allocate();

    return true;
}

void PlayerPortAudio::close()
{
	PaError err = Pa_CloseStream( stream );
	if( err != paNoError )
        qFatal("PortAudio close stream error: %s", Pa_GetErrorText(err));

    deallocate();
}

void PlayerPortAudio::start(EngineObject *_reader)
{
    Player::start(_reader);

    PaError err = Pa_StartStream( stream );
    if( err != paNoError )
        qFatal("PortAudio start stream error: %s", Pa_GetErrorText(err));
}

void PlayerPortAudio::wait()
{
}

void PlayerPortAudio::stop()
{
	PaError err = Pa_StopStream( stream );
	if( err != paNoError ) exit(-1);
}

CSAMPLE *PlayerPortAudio::process(const CSAMPLE *, const int)
{
	return 0;
}

/* -------- ------------------------------------------------------
   Purpose: Wrapper function to call processing loop function,
            implemented as a method in a class. Used in PortAudio,
            which knows nothing about C++.
   Input:   .
   Output:  -
   -------- ------------------------------------------------------ */
static int paCallback(void *, void *outputBuffer,
                      unsigned long framesPerBuffer,
                      PaTimestamp, void *_player)
{
    Player *player = (Player *)_player;
    SAMPLE *out = (SAMPLE*)outputBuffer;
    player->prepareBuffer();
    for (unsigned int i=0; i<framesPerBuffer*NO_CHANNELS; i++)
        *out++=player->out_buffer[i];
    return 0;
}

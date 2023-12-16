// Copyright (c) 2023 Park Hyunwoo
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#ifndef XANTHINE_VOLUMERAII_H
#define XANTHINE_VOLUMERAII_H

#include <endpointvolume.h>
#include <memory>

class VolumeRAII
{
public:
    VolumeRAII(float newVolume);
    ~VolumeRAII();

private:
    std::shared_ptr<IAudioEndpointVolume> _endpointVolume = nullptr;
    float _oldVolume;
    BOOL _oldMute;
};

#endif // XANTHINE_VOLUMERAII_H

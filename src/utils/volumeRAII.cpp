// Copyright (c) 2023 Park Hyunwoo
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

#include "volumeRAII.h"
#include "WASAPIUtils.h"
#include "raiiUtils.h"
#include "debugLog.h"

VolumeRAII::VolumeRAII(float newVolume) {
    HRESULT hr;

    auto outputDevice = getDefaultOutputDevice();

    IAudioEndpointVolume *raw_endpointVolume;
    hr = outputDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, NULL,
                                reinterpret_cast<void **>(&raw_endpointVolume));
    if (FAILED(hr)) {
        logA("Cannot init IAudioEndpointVolume: %x", hr);
        return;
    }
    auto endpointVolume = make_autorelease(raw_endpointVolume);

    endpointVolume->GetMute(&_oldMute);
    hr = endpointVolume->GetMasterVolumeLevelScalar(&_oldVolume);
    if (FAILED(hr)) {
        logA("GetMasterVolumeLevelScalar failed: %x", hr);
        return;
    }

    hr = endpointVolume->SetMasterVolumeLevelScalar(newVolume, nullptr);
    logA("SetMasterVolumeLevelScalar: %f", newVolume);
    if (FAILED(hr)) {
        logA("SetMasterVolumeLevelScalar failed: %x", hr);
        return;
    }

    endpointVolume->SetMute(false, nullptr);

    _endpointVolume = endpointVolume;
}

VolumeRAII::~VolumeRAII() {
    if (_endpointVolume) {
        _endpointVolume->SetMasterVolumeLevelScalar(_oldVolume, nullptr);
        _endpointVolume->SetMute(_oldMute, nullptr);
        logA("SetMasterVolumeLevelScalar: %f", _oldVolume);
    }
}

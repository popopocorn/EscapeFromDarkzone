#include"stdafx.h"
#include "SoundManager.h"

void SoundManager::Init()
{
	FMOD::System_Create(&system);

	system->init(512, FMOD_INIT_NORMAL, nullptr);
}

void SoundManager::Update()
{
	if (system)
		system->update();
}

void SoundManager::Release()
{
	for (auto& pair : sounds)
	{
		pair.second->release();
	}
	sounds.clear();
	if (system)
	{
		system->close();
		system->release();
		system = nullptr;
	}

}

void SoundManager::BuildSound()
{

}

void SoundManager::LoadSound(SoundName name, const string& path, bool is3D, bool loop)
{
    if (sounds.find(name) != sounds.end()) return;

    FMOD::Sound* pSound = nullptr;
    FMOD_MODE mode = FMOD_DEFAULT;

    if (loop) mode |= FMOD_LOOP_NORMAL;
    else       mode |= FMOD_LOOP_OFF;

    if (is3D) 
    {
        mode |= FMOD_3D;
        mode |= FMOD_3D_LINEARROLLOFF;
    }
    else       mode |= FMOD_2D;

    system->createSound(path.c_str(), mode, nullptr, &pSound);

    if (pSound)
    {
        if (is3D) pSound->set3DMinMaxDistance(2.0f, 50.0f);

        sounds[name] = pSound;
    }
}

FMOD::Channel* SoundManager::Play(SoundName name, XMFLOAT3 position, XMFLOAT3 velocity)
{
    auto it = sounds.find(name);
    if (it == sounds.end()) return;

    FMOD::Channel* pChannel = nullptr;

    system->playSound(it->second, nullptr, true, &pChannel);

    if (pChannel)
    {
        
        FMOD_VECTOR fmodPos = ToFmodVec(position);
        FMOD_VECTOR vel = ToFmodVec(velocity);
        pChannel->set3DAttributes(&fmodPos, &vel);
        pChannel->setPaused(false);
    }
    return pChannel;
}

void SoundManager::UpdateListener(XMFLOAT3 position, XMFLOAT3 forward, XMFLOAT3 up)
{
    if (!system) return;

    FMOD_VECTOR fmodPos = ToFmodVec(position);
    FMOD_VECTOR fmodForward = ToFmodVec(forward);
    FMOD_VECTOR fmodUp = ToFmodVec(up);
    FMOD_VECTOR vel = { 0.0f, 0.0f, 0.0f };


    system->set3DListenerAttributes(0, &fmodPos, &vel, &fmodForward, &fmodUp);
}
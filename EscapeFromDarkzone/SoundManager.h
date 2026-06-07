#pragma once
#include<fmod.hpp>

inline FMOD_VECTOR ToFmodVec(const XMFLOAT3& v)
{
    return { v.x, v.y, v.z };
}

enum class SoundName {
    FOOSTEP,
    ENEMY_FOOSTEP,
};

class SoundManager
{
private:
    FMOD::System* system = NULL;

    unordered_map<SoundName, FMOD::Sound*> sounds;

    FMOD::Channel*   BGMChannel = NULL;

public:
    static SoundManager& Instance()
    {
        static SoundManager instance;
        return instance;
    }
    void Init();
    void Update();
    void Release();
    void BuildSound();
    void LoadSound(SoundName name, const string& path, bool is3D, bool loop);

    //loop하는 사운드의 경우 원하는 시점에 종료를 호출하기 위해 FMOD::Channel*반환
    FMOD::Channel* Play(SoundName name, XMFLOAT3 position, XMFLOAT3 vel = XMFLOAT3(0.0f, 0.0f, 0.0f));
    void UpdateListener(XMFLOAT3 position, XMFLOAT3 forward, XMFLOAT3 up);


private:
    SoundManager() {}
    ~SoundManager() {}
    SoundManager(const SoundManager& other) = delete;
    SoundManager& operator=(const SoundManager& other) = delete;

};

